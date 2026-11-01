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
#include <sys/prctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

struct File {
    const char *data;
    size_t size, mapped;
    explicit File(const char *path) {
        int fd = open(path, O_RDONLY);
        if (fd < 0) throw std::runtime_error(std::string("open ") + path);
        struct stat st;
        if (fstat(fd, &st)) throw std::runtime_error("fstat");
        size = st.st_size;
        size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
        mapped = (size + page - 1) / page * page + page;
        data = static_cast<const char *>(mmap(nullptr, mapped, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        if (data == MAP_FAILED) { close(fd); throw std::runtime_error("reserve mmap"); }
        if (size && mmap(const_cast<char *>(data), size, PROT_READ, MAP_PRIVATE | MAP_FIXED, fd, 0) == MAP_FAILED) {
            munmap(const_cast<char *>(data), mapped); close(fd); throw std::runtime_error("file mmap");
        }
        close(fd);
    }
    ~File() { munmap(const_cast<char *>(data), mapped); }
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
    uint32_t raw_len = 0;
    uint32_t hash = 0;
};
static_assert(sizeof(Field) == 16, "projected name metadata");

static constexpr uint32_t HASH_OFFSET = 5381U;

static uint32_t raw_hash(const char *p, size_t n) {
    if (n <= 16) {
        alignas(16) static const unsigned char mask[32] = {
            255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
        __m128i key = _mm_and_si128(_mm_loadu_si128(reinterpret_cast<const __m128i *>(p)),
                                  _mm_loadu_si128(reinterpret_cast<const __m128i *>(mask + 16 - n)));
        uint32_t h = static_cast<uint32_t>(_mm_crc32_u64(HASH_OFFSET, static_cast<uint64_t>(_mm_cvtsi128_si64(key))));
        return static_cast<uint32_t>(_mm_crc32_u64(h, static_cast<uint64_t>(_mm_extract_epi64(key, 1))));
    }
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

static const char *find_quote(const char *p, const char *end) {
    const __m256i quote32 = _mm256_set1_epi8('"');
    while (end - p >= 32) {
        __m256i block = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(p));
        unsigned mask = static_cast<unsigned>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(block, quote32)));
        if (mask) return p + __builtin_ctz(mask);
        p += 32;
    }
    const __m128i quote16 = _mm_set1_epi8('"');
    while (end - p >= 16) {
        __m128i block = _mm_loadu_si128(reinterpret_cast<const __m128i *>(p));
        unsigned mask = static_cast<unsigned>(_mm_movemask_epi8(_mm_cmpeq_epi8(block, quote16)));
        if (mask) return p + __builtin_ctz(mask);
        p += 16;
    }
    for (; p < end; ++p) if (*p == '"') return p;
    return nullptr;
}

static Field text_field(const char *&p, const char *end, bool need_hash, uint32_t &length) {
    Field f{};
    length = 0;
    bool quoted = p < end && *p == '"';
    if (quoted) {
        ++p;
        f.raw = p;
        for (;;) {
            const char *q = find_quote(p, end);
            if (!q) throw std::runtime_error("unclosed quote");
            length += static_cast<uint32_t>(q - p);
            if (q + 1 < end && q[1] == '"') {
                ++length;
                p = q + 2;
                continue;
            }
            f.raw_len = static_cast<uint32_t>(q - f.raw);
            f.hash = need_hash ? raw_hash(f.raw, f.raw_len) : HASH_OFFSET;
            p = q + 1;
            return f;
        }
    }
    f.raw = p;
    while (p < end && *p != ',' && *p != '\r' && *p != '\n') {
        ++p;
        ++length;
    }
    f.raw_len = length;
    f.hash = need_hash ? raw_hash(f.raw, f.raw_len) : HASH_OFFSET;
    return f;
}

struct NameArena {
    std::vector<std::unique_ptr<char[]>> blocks;
    char *cursor = nullptr;
    size_t remaining = 0;
    const char *keep(const Field &f) {
        if (!f.raw_len) return f.raw;
        constexpr size_t block_size = 1 << 15;
        if (f.raw_len > block_size) {
            blocks.emplace_back(new char[f.raw_len]);
            std::memcpy(blocks.back().get(), f.raw, f.raw_len);
            return blocks.back().get();
        }
        if (f.raw_len > remaining) {
            blocks.emplace_back(new char[block_size]);
            cursor = blocks.back().get(); remaining = block_size;
        }
        char *out = cursor; std::memcpy(out, f.raw, f.raw_len);
        cursor += f.raw_len; remaining -= f.raw_len; return out;
    }
};
struct DirectControl {
    std::atomic<uint8_t> value;
    constexpr DirectControl() noexcept : value(0) {}
};
struct DirectWord {
    std::atomic<uint64_t> value;
    constexpr DirectWord() noexcept : value(0) {}
};
struct alignas(64) DirectSlot { char key[16]{}; DirectWord seen[4]; };
static_assert(sizeof(DirectSlot) == 64, "one cache line per name");
struct SharedNames {
    static constexpr unsigned shard_bits = 6, shard_count = 1 << shard_bits;
    static constexpr uint32_t slots_per_shard = 2048, capacity = shard_count * slots_per_shard;
    alignas(4096) inline static DirectControl controls[capacity]{};
    alignas(4096) inline static DirectSlot slots[capacity]{};
    struct Overflow { Field field; std::array<uint64_t, 4> seen{}; };
    struct Shard { std::mutex mutex; std::vector<Overflow> overflow; };
    std::array<Shard, shard_count> shards;
    static uint32_t spread(uint32_t h) { return h ^ (h >> 11) ^ (h >> 22); }
    static uint8_t tag(const Field &f) { return static_cast<uint8_t>(((f.raw_len <= 16 ? f.raw_len + 1 : 31) << 3) | (f.hash >> 29)); }
    static bool matches(const DirectSlot &s, const Field &f) {
        if (f.raw_len <= 16) {
            __m128i key = _mm_loadu_si128(reinterpret_cast<const __m128i *>(s.key));
            unsigned same = static_cast<unsigned>(_mm_movemask_epi8(_mm_cmpeq_epi8(key, _mm_loadu_si128(reinterpret_cast<const __m128i *>(f.raw)))));
            unsigned valid = 0xffffU >> (16 - f.raw_len); return (same & valid) == valid;
        }
        const char *saved; uint32_t length;
        std::memcpy(&saved, s.key, sizeof(saved)); std::memcpy(&length, s.key + 8, 4);
        return length == f.raw_len && std::memcmp(s.key + 12, f.raw, 4) == 0 && std::memcmp(saved + 4, f.raw + 4, f.raw_len - 4) == 0;
    }
    static void prefetch(const Field &f) {
        uint32_t h = spread(f.hash), base = (h & (shard_count - 1)) * slots_per_shard;
        uint32_t i = (h >> shard_bits) & (slots_per_shard - 1);
        __builtin_prefetch(&controls[base + i], 0, 3);
        for (unsigned n = 0; n < 1; ++n) __builtin_prefetch(&slots[base + ((i + n) & (slots_per_shard - 1))], 1, 3);
    }
    __attribute__((noinline, cold)) bool overflow(const Field &f, unsigned group, NameArena &arena) {
        Shard &s = shards[spread(f.hash) & (shard_count - 1)]; std::lock_guard<std::mutex> lock(s.mutex);
        const uint64_t bit = 1ULL << (group & 63);
        for (auto &x : s.overflow) if (x.field.hash == f.hash && x.field.raw_len == f.raw_len && std::memcmp(x.field.raw, f.raw, f.raw_len) == 0) {
            uint64_t &seen = x.seen[group >> 6]; bool fresh = !(seen & bit); seen |= bit; return fresh;
        }
        s.overflow.emplace_back(); auto &x = s.overflow.back(); x.field = f; x.field.raw = arena.keep(f); x.seen[group >> 6] = bit; return true;
    }
    bool first(const Field &f, unsigned group, NameArena &arena) {
        uint32_t h = spread(f.hash), base = (h & (shard_count - 1)) * slots_per_shard;
        uint32_t i = (h >> shard_bits) & (slots_per_shard - 1);
        const uint8_t signature = tag(f); const uint64_t mask = 1ULL << (group & 63);
        const char *saved = nullptr;
        for (unsigned n = 0; n < slots_per_shard; ++n, i = (i + 1) & (slots_per_shard - 1)) {
            auto &control = controls[base + i].value;
            uint8_t state = control.load(std::memory_order_acquire);
            while (state == 1) { _mm_pause(); state = control.load(std::memory_order_acquire); }
            DirectSlot &slot = slots[base + i];
            if (!state) {
                if (f.raw_len > 16 && !saved) saved = arena.keep(f);
                uint8_t expected = 0;
                if (!control.compare_exchange_strong(expected, 1, std::memory_order_acquire)) { --n; i = (i - 1) & (slots_per_shard - 1); continue; }
                if (f.raw_len <= 16) std::memcpy(slot.key, f.raw, f.raw_len);
                else {
                    uint32_t length = f.raw_len; std::memcpy(slot.key, &saved, sizeof(saved));
                    std::memcpy(slot.key + 8, &length, 4); std::memcpy(slot.key + 12, f.raw, 4);
                }
                slot.seen[group >> 6].value.store(mask, std::memory_order_relaxed);
                control.store(signature, std::memory_order_release); return true;
            }
            if (state == signature && matches(slot, f)) return !(slot.seen[group >> 6].value.fetch_or(mask, std::memory_order_relaxed) & mask);
        }
        return overflow(f, group, arena);
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
    uint64_t word; std::memcpy(&word, p, 8);
    uint64_t dots = word ^ 0x2e2e2e2e2e2e2e2eULL;
    dots = (dots - 0x0101010101010101ULL) & ~dots & 0x8080808080808080ULL;
    unsigned dot = dots ? __builtin_ctzll(dots) / 8 : 8;
    if (dot && dot <= 5 && end - p >= dot + 3) {
        uint64_t lower = (1ULL << (dot * 8)) - 1;
        uint64_t x = ((word & lower) | ((word >> 8) & ~lower)) << ((6 - dot) * 8);
        x &= 0x0f0f0f0f0f0f0f0fULL;
        x = (x * 10 + (x >> 8)) & 0x00ff00ff00ff00ffULL;
        x = (x * 100 + (x >> 16)) & 0x0000ffff0000ffffULL;
        int64_t value = static_cast<int64_t>((x * 10000 + (x >> 32)) & 0xffffffffULL);
        p += dot + 3; return neg ? -value : value;
    }
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
    void add(uint64_t id, int64_t amount, uint32_t name_len, uint32_t note_len, bool first) {
        if (!count || amount < min) min = amount;
        if (!count || amount > max) max = amount;
        ++count;
        id_sum += id;
        sum += amount;
        name_bytes += name_len;
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
    NameArena arena;
    std::vector<Group> groups;
    std::exception_ptr error;
    explicit Worker(size_t group_count) : groups(group_count) { }
};

static void parse_range(const char *p, const char *end, const std::vector<uint8_t> &regions,
                        unsigned unmatched, Worker &w, SharedNames &shared) {
    struct Pending {
        uint64_t id;
        int64_t amount;
        Field name;
        uint32_t name_len, note_len;
        unsigned group;
    };
    static_assert(sizeof(Pending) == 48, "compact pending row");
    try {
        std::array<Pending, 32> batch;
        const char *reclaimed = p;
        while (p < end) {
            unsigned count = 0;
            do {
                bom(p, end);
                uint64_t id = number(p, end);
                comma(p, end);
                uint32_t name_len;
                Field name = text_field(p, end, true, name_len);
                SharedNames::prefetch(name);
                comma(p, end);
                uint32_t note_len;
                (void)text_field(p, end, false, note_len);
                comma(p, end);
                uint64_t region = number(p, end);
                uint32_t region_key = region < regions.size()
                                          ? static_cast<uint32_t>(region) : UINT32_MAX;
                if (region_key != UINT32_MAX) __builtin_prefetch(&regions[region_key], 0, 1);
                comma(p, end);
                int64_t amount = cents(p, end);
                newline(p, end);
                batch[count++] = Pending{id, amount, name, name_len, note_len, region_key};
            } while (count < batch.size() && p < end);
            for (unsigned i = 0; i < count; ++i) {
                const Pending &r = batch[i];
                unsigned group = r.group < regions.size() && regions[r.group] != UINT8_MAX
                                     ? regions[r.group] : unmatched;
                w.groups[group].add(r.id, r.amount, r.name_len, r.note_len,
                                    shared.first(r.name, group, w.arena));
            }
            if (p - reclaimed >= (1 << 20)) discard_consumed(reclaimed, p);
        }
    } catch (...) { w.error = std::current_exception(); }
}

int main() {
    try {
        (void)prctl(PR_SET_THP_DISABLE, 1, 0, 0, 0);
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
        unsigned threads = end - p < (1 << 20) ? 1 : 6;
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
        (void)madvise(SharedNames::controls, sizeof(SharedNames::controls), MADV_POPULATE_WRITE);
        (void)madvise(SharedNames::slots, sizeof(SharedNames::slots), MADV_POPULATE_WRITE);
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
