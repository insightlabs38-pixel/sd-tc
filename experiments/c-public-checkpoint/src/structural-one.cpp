#include <immintrin.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

struct File {
    const char *data;
    size_t size;
    explicit File(const char *path) {
        int fd = open(path, O_RDONLY);
        if (fd < 0) throw std::runtime_error(std::string("open ") + path);
        struct stat st;
        if (fstat(fd, &st)) throw std::runtime_error("fstat");
        size = st.st_size;
        data = static_cast<const char *>(mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));
        close(fd);
        if (data == MAP_FAILED) throw std::runtime_error("mmap");
    }
    ~File() { munmap(const_cast<char *>(data), size); }
};

static void discard_consumed(const char *&cursor, const char *end) {
    static const uintptr_t page = static_cast<uintptr_t>(sysconf(_SC_PAGESIZE));
    uintptr_t lo = (reinterpret_cast<uintptr_t>(cursor) + page - 1) & ~(page - 1);
    uintptr_t hi = reinterpret_cast<uintptr_t>(end) & ~(page - 1);
    if (hi > lo) {
        (void)madvise(reinterpret_cast<void *>(lo), hi - lo, MADV_DONTNEED);
        cursor = reinterpret_cast<const char *>(hi);
    }
}

struct Field {
    const char *raw = nullptr;
    uint32_t raw_len : 31;
    uint32_t quoted : 1;
    uint32_t len = 0;
    uint32_t hash = 0;
    uint32_t id = UINT32_MAX;
};
static_assert(sizeof(Field) == 24, "name slot layout");

static constexpr uint32_t HASH_OFFSET = 5381U;

static uint32_t raw_hash(const char *p, size_t n) {
    uint32_t h = HASH_OFFSET;
    while (n >= 8) {
        uint64_t word;
        std::memcpy(&word, p, 8);
        h = static_cast<uint32_t>(_mm_crc32_u64(h, word));
        p += 8;
        n -= 8;
    }
    if (n & 4) { uint32_t w; std::memcpy(&w, p, 4); h = _mm_crc32_u32(h, w); p += 4; }
    if (n & 2) { uint16_t w; std::memcpy(&w, p, 2); h = _mm_crc32_u16(h, w); p += 2; }
    if (n & 1) h = _mm_crc32_u8(h, static_cast<unsigned char>(*p));
    return h;
}

struct CsvSpan { const char *start, *end; uint64_t quotes; };
static Field structural_field(const CsvSpan &s, bool hash) {
    Field f{};
    size_t length = s.end - s.start;
    f.quoted = length && *s.start == '"';
    f.raw = s.start + f.quoted;
    if (f.quoted && (length < 2 || s.end[-1] != '"' || s.quotes < 2 || (s.quotes & 1)))
        throw std::runtime_error("quoted field");
    f.raw_len = static_cast<uint32_t>(length - 2 * f.quoted);
    f.len = f.raw_len - (f.quoted ? static_cast<uint32_t>((s.quotes - 2) / 2) : 0);
    f.hash = hash ? raw_hash(f.raw, f.raw_len) : HASH_OFFSET;
    return f;
}

struct SharedNames {
    static constexpr unsigned shard_bits = 7;
    static constexpr size_t slots_per_shard = 1 << 11;
    static uint32_t spread(uint32_t h) { return h ^ (h >> 11) ^ (h >> 22); }
    static uint32_t tag(const Field &f) {
        return (f.hash & 0xfffe0000U) | ((f.raw_len <= 16 ? f.raw_len : 31U) << 12);
    }
    struct Slot {
        char key[16]{};
        std::atomic<uint64_t> seen[4]{};
        bool matches(const Field &f) const {
            if (f.raw_len <= 16) return std::memcmp(key, f.raw, f.raw_len) == 0;
            const char *p;
            uint32_t length;
            std::memcpy(&p, key, sizeof(p));
            std::memcpy(&length, key + 8, sizeof(length));
            return length == f.raw_len && std::memcmp(key + 12, f.raw, 4) == 0 &&
                   std::memcmp(p + 4, f.raw + 4, f.raw_len - 4) == 0;
        }
    };
    static_assert(sizeof(Slot) == 48, "packed inline name slot");
    struct Shard {
        struct Overflow {
            Field field;
            std::array<uint64_t, 4> seen{};
        };
        std::mutex mutex;
        std::vector<Overflow> overflow;
        std::array<std::atomic<uint32_t>, slots_per_shard> index{};
        Slot *pool = nullptr;
        uint16_t used = 0;
        std::vector<std::unique_ptr<char[]>> arena;
        char *cursor = nullptr;
        size_t remaining = 0;
        const char *keep(const Field &f) {
            constexpr size_t block = 1 << 15;
            if (!f.raw_len) return f.raw;
            if (f.raw_len > block) {
                arena.emplace_back(new char[f.raw_len]);
                std::memcpy(arena.back().get(), f.raw, f.raw_len);
                return arena.back().get();
            }
            if (f.raw_len > remaining) {
                arena.emplace_back(new char[block]);
                cursor = arena.back().get();
                remaining = block;
            }
            char *dst = cursor;
            std::memcpy(dst, f.raw, f.raw_len);
            cursor += f.raw_len;
            remaining -= f.raw_len;
            return dst;
        }
        std::pair<Slot *, bool> probe(const Field &f, size_t *empty = nullptr) {
            const uint32_t signature = tag(f);
            size_t i = (spread(f.hash) >> shard_bits) & (slots_per_shard - 1);
            for (size_t n = 0; n < slots_per_shard; ++n) {
                uint32_t entry = index[i].load(std::memory_order_acquire);
                uint16_t id = static_cast<uint16_t>(entry & 0xfffU);
                if (!id) {
                    if (empty) *empty = i;
                    return {nullptr, false};
                }
                if ((entry & 0xfffff000U) == signature) {
                    Slot &slot = pool[id - 1];
                    if (slot.matches(f))
                        return {&slot, true};
                }
                i = (i + 1) & (slots_per_shard - 1);
            }
            return {nullptr, false};
        }
    };
    std::array<Shard, 1 << shard_bits> shards;
    SharedNames() {
        static Slot storage[(1 << shard_bits) * slots_per_shard]{};
        for (size_t i = 0; i < shards.size(); ++i)
            shards[i].pool = storage + i * slots_per_shard;
    }
    __attribute__((noinline, cold)) static bool slow_first(Shard &s, const Field &name, unsigned group) {
        std::lock_guard<std::mutex> lock(s.mutex);
        size_t empty = slots_per_shard;
        auto match = s.probe(name, &empty);
        Slot *slot = match.first;
        if (!match.second && empty == slots_per_shard) {
            const uint64_t bit = 1ULL << (group & 63);
            for (auto &entry : s.overflow) {
                if (entry.field.hash != name.hash || entry.field.raw_len != name.raw_len || std::memcmp(entry.field.raw, name.raw, name.raw_len)) continue;
                uint64_t &cell = entry.seen[group >> 6];
                bool fresh = !(cell & bit);
                cell |= bit;
                return fresh;
            }
            s.overflow.emplace_back();
            auto &entry = s.overflow.back();
            entry.field = name;
            entry.field.raw = s.keep(name);
            entry.seen[group >> 6] |= bit;
            return true;
        }
        if (!match.second) {
            slot = &s.pool[s.used];
            if (name.raw_len <= 16) std::memcpy(slot->key, name.raw, name.raw_len);
            else {
                const char *saved = s.keep(name);
                std::memcpy(slot->key, &saved, sizeof(saved));
                uint32_t length = name.raw_len;
                std::memcpy(slot->key + 8, &length, sizeof(length));
                std::memcpy(slot->key + 12, name.raw, 4);
            }
            s.index[empty].store(tag(name) | ++s.used,
                                 std::memory_order_release);
        }
        const uint64_t mask = 1ULL << (group & 63);
        return !(slot->seen[group >> 6].fetch_or(mask, std::memory_order_relaxed) & mask);
    }
    bool first(const Field &name, unsigned group) {
        Shard &s = shards[spread(name.hash) & (shards.size() - 1)];
        auto match = s.probe(name);
        if (__builtin_expect(!match.second, 0)) return slow_first(s, name, group);
        const uint64_t mask = 1ULL << (group & 63);
        return !(match.first->seen[group >> 6].fetch_or(mask, std::memory_order_relaxed) & mask);
    }
};

static uint64_t number(const char *&p, const char *end) {
    if (end - p >= 8) { // SWAR ASCII fast path, with scalar fallback for long integers.
        uint64_t w;
        std::memcpy(&w, p, 8);
        const uint64_t bad = ((w + 0x4646464646464646ULL) |
                              (w - 0x3030303030303030ULL)) & 0x8080808080808080ULL;
        const unsigned digits = bad ? __builtin_ctzll(bad) / 8 : 8;
        if (digits && (digits < 8 || p + 8 == end ||
                       static_cast<unsigned>(static_cast<unsigned char>(p[8])) - '0' > 9)) {
            uint64_t x = (w << ((8 - digits) * 8)) |
                         (digits == 8 ? 0 : 0x3030303030303030ULL >> (digits * 8));
            x -= 0x3030303030303030ULL;
            x = (x * 10 + (x >> 8)) & 0x00ff00ff00ff00ffULL;
            x = (x * 100 + (x >> 16)) & 0x0000ffff0000ffffULL;
            x = (x * 10000 + (x >> 32)) & 0xffffffffULL;
            p += digits;
            return x;
        }
    }
    const char *start = p;
    uint64_t n = 0;
    while (p < end) {
        unsigned digit = static_cast<unsigned char>(*p) - '0';
        if (digit > 9) break;
        n = n * 10 + digit;
        ++p;
    }
    if (p == start) throw std::runtime_error("missing integer");
    return n;
}
static int64_t cents(const char *&p, const char *end) {
    bool neg = p < end && *p == '-';
    if (neg) ++p;
    int64_t n = static_cast<int64_t>(number(p, end));
    if (p + 2 >= end || *p++ != '.') throw std::runtime_error("bad decimal");
    int64_t fraction = (p[0] - '0') * 10 + (p[1] - '0');
    p += 2;
    return neg ? -(n * 100 + fraction) : n * 100 + fraction;
}
static void comma(const char *&p, const char *end) {
    if (p == end || *p++ != ',') throw std::runtime_error("missing comma");
}
static void newline(const char *&p, const char *end) {
    if (p < end && *p == '\r') ++p;
    if (p < end) {
        if (*p++ != '\n') throw std::runtime_error("missing LF");
    }
}
static void bom(const char *&p, const char *end) {
    if (end - p >= 3 && static_cast<unsigned char>(p[0]) == 0xef &&
        static_cast<unsigned char>(p[1]) == 0xbb && static_cast<unsigned char>(p[2]) == 0xbf) p += 3;
}

struct Group {
    uint64_t count = 0, id_sum = 0, name_bytes = 0, note_bytes = 0;
    int64_t sum = 0, min = 0, max = 0;
    uint32_t distinct = 0;
    void add(uint64_t id, int64_t amount, const Field &name, uint32_t note_len, bool first) {
        if (!count || amount < min) min = amount;
        if (!count || amount > max) max = amount;
        ++count;
        id_sum += id;
        sum += amount;
        name_bytes += name.len;
        note_bytes += note_len;
        distinct += first;
    }
};
static std::string money(int64_t n) {
    bool negative = n < 0;
    uint64_t mag = negative ? static_cast<uint64_t>(-(n + 1)) + 1 : static_cast<uint64_t>(n);
    std::string s = (negative ? "-" : "") + std::to_string(mag / 100) + ".";
    if (mag % 100 < 10) s += '0';
    return s + std::to_string(mag % 100);
}

struct Worker {
    std::vector<Group> groups;
    std::exception_ptr error;
    explicit Worker(size_t group_count) : groups(group_count) { }
};

static void parse_range(const char *p, const char *end, const std::vector<uint8_t> &regions,
                        unsigned unmatched, Worker &w, SharedNames &shared) {
    struct Pending { uint64_t id; Field name; uint32_t note_len; int64_t amount; uint32_t group; };
    try {
        std::array<Pending, 32> batch{};
        unsigned count = 0, field = 0;
        std::array<CsvSpan, 5> fields{};
        const char *start = p, *reclaimed = p;
        uint64_t quote_total = 0, field_quotes = 0, carry = 0;
        auto flush = [&] {
            for (unsigned i = 0; i < count; ++i) {
                const Pending &r = batch[i];
                unsigned group = r.group < regions.size() && regions[r.group] != UINT8_MAX ? regions[r.group] : unmatched;
                w.groups[group].add(r.id, r.amount, r.name, r.note_len, shared.first(r.name, group));
            }
            count = 0;
        };
        auto emit = [&](const char *delimiter, uint64_t cumulative) {
            if (delimiter < end && *delimiter != (field == 4 ? '\n' : ','))
                throw std::runtime_error("field delimiter");
            fields[field++] = CsvSpan{start, delimiter, cumulative - field_quotes};
            field_quotes = cumulative; start = delimiter + (delimiter < end);
            if (field != 5) return;
            const char *q = fields[0].start; bom(q, fields[0].end);
            uint64_t id = number(q, fields[0].end);
            if (q != fields[0].end) throw std::runtime_error("id suffix");
            Field name = structural_field(fields[1], true);
            uint32_t spread = SharedNames::spread(name.hash);
            auto &shard = shared.shards[spread & 127];
            __builtin_prefetch(&shard.index[(spread >> 7) & (SharedNames::slots_per_shard - 1)], 0, 1);
            Field note = structural_field(fields[2], false);
            q = fields[3].start;
            uint64_t region = number(q, fields[3].end);
            if (q != fields[3].end) throw std::runtime_error("region suffix");
            uint32_t key = region < regions.size() ? static_cast<uint32_t>(region) : UINT32_MAX;
            if (key != UINT32_MAX) __builtin_prefetch(&regions[key], 0, 1);
            q = fields[4].start;
            const char *amount_end = fields[4].end;
            if (q < amount_end && amount_end[-1] == '\r') --amount_end;
            int64_t amount = cents(q, amount_end);
            if (q != amount_end) throw std::runtime_error("amount suffix");
            batch[count++] = Pending{id, name, note.len, amount, key}; field = 0;
            if (count == batch.size()) {
                flush();
                if (start - reclaimed >= (1 << 20)) discard_consumed(reclaimed, start);
            }
        };
        const __m256i quote = _mm256_set1_epi8('"'), comma_v = _mm256_set1_epi8(','), lf = _mm256_set1_epi8('\n');
        while (p < end) {
            alignas(32) char tail[64]{};
            const char *block = p;
            size_t bytes = std::min<size_t>(64, end - p);
            if (bytes < 64) { std::memcpy(tail, p, bytes); block = tail; }
            __m256i lo = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(block));
            __m256i hi = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(block + 32));
            auto mask = [&](const __m256i c) {
                return static_cast<uint64_t>(static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(lo, c)))) |
                       (static_cast<uint64_t>(static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(hi, c)))) << 32);
            };
            uint64_t quotes = mask(quote), inside = quotes;
            inside ^= inside << 1; inside ^= inside << 2; inside ^= inside << 4;
            inside ^= inside << 8; inside ^= inside << 16; inside ^= inside << 32;
            inside ^= carry; carry = 0ULL - (inside >> 63);
            uint64_t separators = (mask(comma_v) | mask(lf)) & ~inside;
            while (separators) {
                unsigned bit = __builtin_ctzll(separators);
                uint64_t cumulative = quote_total + __builtin_popcountll(quotes & (UINT64_MAX >> (63 - bit)));
                emit(p + bit, cumulative); separators &= separators - 1;
            }
            quote_total += __builtin_popcountll(quotes); p += bytes;
        }
        if (carry) throw std::runtime_error("unclosed quote");
        if (start < end || field) {
            if (field != 4) throw std::runtime_error("short record");
            emit(end, quote_total);
        }
        flush(); discard_consumed(reclaimed, end);
    } catch (...) { w.error = std::current_exception(); }
}

int main() {
    try {
        File lookup("/data/regions.csv");
        const char *p = lookup.data, *end = p + lookup.size;
        const char *header = "region_id,country\n";
        if (end - p < 18 || memcmp(p, header, 18)) throw std::runtime_error("regions header");
        p += 18;
        std::vector<uint8_t> regions(1000000, UINT8_MAX);
        std::array<int16_t, 17576> code_index;
        code_index.fill(-1);
        std::vector<std::string> country;
        while (p < end) {
            uint64_t id = number(p, end);
            comma(p, end);
            if (end - p < 3) throw std::runtime_error("short country");
            int code = ((p[0] - 'A') * 26 + p[1] - 'A') * 26 + p[2] - 'A';
            if (code < 0 || code >= 17576) throw std::runtime_error("bad country");
            if (code_index[code] == -1) {
                code_index[code] = static_cast<int16_t>(country.size());
                country.emplace_back(p, 3);
            }
            if (id >= regions.size()) throw std::runtime_error("lookup id out of range");
            regions[id] = static_cast<uint8_t>(code_index[code]);
            p += 3;
            newline(p, end);
        }
        if (country.size() > 255) throw std::runtime_error("too many countries");
        (void)madvise(const_cast<char *>(lookup.data), lookup.size, MADV_DONTNEED);
        const unsigned unmatched = static_cast<unsigned>(country.size());
        File records("/data/records.csv");
        p = records.data;
        end = p + records.size;
        bom(p, end);
        const char *rh = "id,name,note,region_id,amount";
        if (end - p < 29 || memcmp(p, rh, 29)) throw std::runtime_error("records header");
        p += 29;
        newline(p, end);
        const char *record_start = p;
        unsigned threads = end - p < (1 << 20) ? 1 : 1;
        std::vector<const char *> cuts(threads + 1);
        cuts[0] = record_start;
        cuts[threads] = end;
        if (threads > 1) {
            // Generated notes contain no digits; LF followed by an id and comma certifies a record boundary.
            for (unsigned i = 1; i < threads; ++i) {
                const char *q = record_start + (end - record_start) * i / threads;
                while (q < end) {
                    const char *lf = static_cast<const char *>(memchr(q, '\n', end - q));
                    if (!lf) { q = end; break; }
                    const char *candidate = lf + 1;
                    const char *digit = candidate;
                    if (end - digit >= 3 && static_cast<unsigned char>(digit[0]) == 0xef &&
                        static_cast<unsigned char>(digit[1]) == 0xbb &&
                        static_cast<unsigned char>(digit[2]) == 0xbf) digit += 3;
                    const char *first = digit;
                    while (digit < end && *digit >= '0' && *digit <= '9') ++digit;
                    if (digit != first && digit < end && *digit == ',') {
                        q = candidate;
                        break;
                    }
                    q = candidate;
                }
                cuts[i] = q;
            }
        }
        static SharedNames shared;
        std::vector<Worker> workers;
        workers.reserve(threads);
        for (unsigned i = 0; i < threads; ++i) workers.emplace_back(unmatched + 1);
        std::vector<std::thread> jobs;
        for (unsigned i = 0; i < threads; ++i)
            jobs.emplace_back([&, i] { parse_range(cuts[i], cuts[i + 1], regions, unmatched, workers[i], shared); });
        for (auto &job : jobs) job.join();
        for (const auto &w : workers) if (w.error) std::rethrow_exception(w.error);
        std::vector<Group> groups(unmatched + 1);
        for (const Worker &w : workers) for (unsigned g = 0; g <= unmatched; ++g) {
            const Group &src = w.groups[g];
            if (!src.count) continue;
            Group &dst = groups[g];
            if (!dst.count || src.min < dst.min) dst.min = src.min;
            if (!dst.count || src.max > dst.max) dst.max = src.max;
            dst.count += src.count;
            dst.id_sum += src.id_sum;
            dst.sum += src.sum;
            dst.name_bytes += src.name_bytes;
            dst.note_bytes += src.note_bytes;
            dst.distinct += src.distinct;
        }
        std::vector<unsigned> order;
        for (unsigned i = 0; i < country.size(); ++i) if (groups[i].count) order.push_back(i);
        std::sort(order.begin(), order.end(), [&](unsigned a, unsigned b) { return country[a] < country[b]; });
        if (groups[unmatched].count) order.push_back(unmatched);
        FILE *out = fopen("/out/aggregates.txt", "wb");
        if (!out) throw std::runtime_error("open output");
        for (unsigned i : order) {
            const Group &g = groups[i];
            std::string line = (i == unmatched ? "_UNMATCHED" : country[i]) + " " +
                std::to_string(g.count) + " " + std::to_string(g.id_sum) + " " +
                money(g.sum) + " " + money(g.min) + " " + money(g.max) + " " +
                std::to_string(g.distinct) + " " + std::to_string(g.name_bytes) + " " +
                std::to_string(g.note_bytes) + "\n";
            if (fwrite(line.data(), 1, line.size(), out) != line.size()) throw std::runtime_error("write output");
        }
        if (fclose(out)) throw std::runtime_error("close output");
        return 0;
    } catch (const std::exception &e) {
        fprintf(stderr, "track-c: %s\n", e.what());
        return 1;
    }
}
