#include <sys/prctl.h>
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
#include <new>
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

static uint32_t raw_hash(const char *p, size_t n, const char *end) {
    if (n <= 16) {
        alignas(16) static const unsigned char mask[32] = {
            255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
        __m128i key;
        if (end - p >= 16) {
            key = _mm_loadu_si128(reinterpret_cast<const __m128i *>(p));
        } else {
            alignas(16) unsigned char tail[16] = {};
            std::memcpy(tail, p, n);
            key = _mm_load_si128(reinterpret_cast<const __m128i *>(tail));
        }
        key = _mm_and_si128(key, _mm_loadu_si128(
            reinterpret_cast<const __m128i *>(mask + 16 - n)));
        uint32_t h = static_cast<uint32_t>(_mm_crc32_u64(HASH_OFFSET, static_cast<uint64_t>(_mm_cvtsi128_si64(key))));
        return static_cast<uint32_t>(_mm_crc32_u64(h, static_cast<uint64_t>(_mm_extract_epi64(key, 1))));
    }
    uint64_t first, last;
    std::memcpy(&first, p, 8);
    std::memcpy(&last, p + n - 8, 8);
    uint32_t h = static_cast<uint32_t>(_mm_crc32_u64(HASH_OFFSET, first));
    h = static_cast<uint32_t>(_mm_crc32_u64(h, last));
    return _mm_crc32_u32(h, static_cast<uint32_t>(n));
}


struct SharedNames {
    static constexpr unsigned bits = 17;
    static constexpr size_t capacity = size_t(1) << bits, mask = capacity - 1;
    static uint32_t spread(uint32_t h) { return h ^ (h >> 11) ^ (h >> 22); }
    struct alignas(64) Slot {
        // One cache line holds exact identity, publication and membership.
        std::atomic<uint64_t> signature{};
        char key[24]{};
        std::atomic<uint64_t> seen[4]{};
        bool matches(const Field &f, const char *) const {
            if (key[23] != char(-1)) {
                return static_cast<unsigned char>(key[23]) == f.raw_len &&
                       std::memcmp(key, f.raw, f.raw_len) == 0;
            }
            uint32_t len;
            const char *saved;
            std::memcpy(&len, key + 8, sizeof(len));
            if (len != f.raw_len || std::memcmp(key + 12, f.raw, 11)) return false;
            std::memcpy(&saved, key, sizeof(saved));
            return std::memcmp(saved + 11, f.raw + 11, len - 11) == 0;
        }
    };
    static_assert(sizeof(Slot) == 64, "exact one-cacheline name+presence");
    struct Overflow {
        std::array<uint64_t,4> seen{};
    };
    struct ArenaShard {
        std::mutex mutex;
        std::vector<std::unique_ptr<char[]>> storage;
        std::unordered_map<std::string,Overflow> overflow;
        char *cursor = nullptr;
        size_t remain = 0;
        const char *keep(const Field &f) {
            constexpr size_t block = 1 << 15;
            std::lock_guard<std::mutex> lock(mutex);
            if (f.raw_len > block) {
                storage.emplace_back(new char[f.raw_len]);
                std::memcpy(storage.back().get(), f.raw, f.raw_len);
                return storage.back().get();
            }
            if (f.raw_len > remain) {
                storage.emplace_back(new char[block]);
                cursor = storage.back().get();
                remain = block;
            }
            const char *ret = cursor;
            std::memcpy(cursor, f.raw, f.raw_len);
            cursor += f.raw_len;
            remain -= f.raw_len;
            return ret;
        }
        bool overflow_first(const Field &f, unsigned group) {
            std::lock_guard<std::mutex> lock(mutex);
            auto &words = overflow[std::string(f.raw, f.raw_len)].seen;
            uint64_t bit = uint64_t(1) << (group & 63);
            uint64_t &word = words[group >> 6];
            bool fresh = !(word & bit);
            word |= bit;
            return fresh;
        }
    };
    // Empty and publishing states are distinct. The payload hash is a filter,
    // never an identity proof: matches() compares every relevant input byte.
    static uint64_t signature(const Field &f) {
        uint64_t h = (uint64_t(f.hash) << 32) | f.raw_len;
        return h < 2 ? h + 2 : h;
    }
    std::array<Slot, capacity> table{};
    std::array<ArenaShard, 128> arenas;
    Slot *prepare(const Field &f) {
        size_t pos = spread(f.hash) & mask;
        uint64_t tag = signature(f);
        for (unsigned probe = 0; probe < 64; ++probe) {
            Slot &slot = table[pos];
            uint64_t state = slot.signature.load(std::memory_order_acquire);
            while (state == 1) {
                _mm_pause();
                state = slot.signature.load(std::memory_order_acquire);
            }
            if (!state) return nullptr;
            if (state == tag) return &slot;
            pos = (pos + 1) & mask;
        }
        return nullptr;
    }
    bool first(const Field &f, unsigned group, Slot *slot, const char *end) {
        if (!slot || !slot->matches(f, end)) {
            slot = find_or_publish(f);
            if (!slot) return arenas[spread(f.hash) & 127].overflow_first(f, group);
        }
        uint64_t bit = uint64_t(1) << (group & 63);
        return !(slot->seen[group >> 6].fetch_or(bit, std::memory_order_relaxed) & bit);
    }
    Slot *find_or_publish(const Field &f) {
        size_t pos = spread(f.hash) & mask;
        uint64_t tag = signature(f);
        for (unsigned probe = 0; probe < 64; ++probe) {
            Slot &slot = table[pos];
            uint64_t state = slot.signature.load(std::memory_order_acquire);
            if (!state) {
                uint64_t expected = 0;
                if (slot.signature.compare_exchange_strong(expected, 1, std::memory_order_acq_rel)) {
                    try {
                        if (f.raw_len <= 23) {
                            std::memcpy(slot.key, f.raw, f.raw_len);
                            slot.key[23] = static_cast<char>(f.raw_len);
                        } else {
                            const char *stored = arenas[spread(f.hash) & 127].keep(f);
                            std::memcpy(slot.key, &stored, sizeof(stored));
                            std::memcpy(slot.key + 8, &f.raw_len, sizeof(f.raw_len));
                            std::memcpy(slot.key + 12, f.raw, 11);
                            slot.key[23] = char(-1);
                        }
                        slot.signature.store(tag, std::memory_order_release);
                        return &slot;
                    } catch (...) {
                        slot.signature.store(0, std::memory_order_release);
                        throw;
                    }
                }
                state = expected;
            }
            while (state == 1) {
                _mm_pause();
                state = slot.signature.load(std::memory_order_acquire);
            }
            if (state == tag && slot.matches(f, nullptr)) return &slot;
            pos = (pos + 1) & mask;
        }
        return nullptr;
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
    if (end - p >= 8) {
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
    p += (p < end && *p == '\r');
    if (p < end && *p++ != '\n') throw std::runtime_error("missing LF");
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
    std::vector<Group> groups;
    std::exception_ptr error;
    explicit Worker(size_t group_count) : groups(group_count) { }
};

__attribute__((target("pclmul")))
static Field text_pair(const char *&p, const char *end, uint32_t &name_len,
                       uint32_t &note_len, SharedNames &shared) {
    const char *start = p;
    Field name{};
    uint64_t quote_total = 0, name_quotes = 0, carry = 0;
    bool have_name = false;
    const __m256i quote = _mm256_set1_epi8('"'), comma = _mm256_set1_epi8(',');
    for (const char *block = p; block < end; block += std::min<ptrdiff_t>(64, end - block)) {
        unsigned bytes = std::min<ptrdiff_t>(64, end - block);
        alignas(32) char tail[64]{};
        const char *load = block;
        if (bytes < 64) { std::memcpy(tail, block, bytes); load = tail; }
        __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(load));
        __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(load + 32));
        uint64_t quotes = static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(a, quote))) |
            (uint64_t(static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(b, quote)))) << 32);
        uint64_t commas = static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(a, comma))) |
            (uint64_t(static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(b, comma)))) << 32);
        uint64_t inside = static_cast<uint64_t>(_mm_cvtsi128_si64(_mm_clmulepi64_si128(
            _mm_cvtsi64_si128(quotes), _mm_set1_epi32(-1), 0)));
        inside ^= carry;
        carry = 0 - (inside >> 63);
        uint64_t separators = commas & ~inside;
        if (bytes < 64) separators &= (uint64_t(1) << bytes) - 1;
        while (separators) {
            unsigned bit = __builtin_ctzll(separators);
            uint64_t lower = bit == 63 ? UINT64_MAX : (uint64_t(1) << (bit + 1)) - 1;
            uint64_t total = quote_total + __builtin_popcountll(quotes & lower);
            const char *finish = block + bit;
            if (!have_name) {
                bool quoted = *start == '"';
                name = {start + quoted, static_cast<uint32_t>(finish - start - 2 * quoted), 0};
                name_len = static_cast<uint32_t>(finish - start) - total / 2 - quoted;
                name.hash = raw_hash(name.raw, name.raw_len, end);
                __builtin_prefetch(&shared.table[SharedNames::spread(name.hash) & SharedNames::mask], 0, 1);
                name_quotes = total;
                start = finish + 1;
                have_name = true;
            } else {
                bool quoted = *start == '"';
                note_len = static_cast<uint32_t>(finish - start) - (total - name_quotes) / 2 - quoted;
                p = finish + 1;
                return name;
            }
            separators &= separators - 1;
        }
        quote_total += __builtin_popcountll(quotes);
    }
    throw std::runtime_error("missing text delimiters");
}

__attribute__((target("pclmul")))
static void parse_range(const char *p, const char *end, const std::vector<uint8_t> &regions,
                        unsigned unmatched, Worker &w, SharedNames &shared) {
    const char *range_begin = p;
    struct Pending {
        uint64_t id;
        int64_t amount;
        Field name;
        uint32_t name_len, note_len;
        unsigned group;
    };
    static_assert(sizeof(Pending) == 48, "compact pending row");
    try {
        std::array<Pending, 64> batch;
        std::array<SharedNames::Slot *, 64> slots;
        const char *reclaimed = p;
        while (p < end) {
            unsigned count = 0;
            do {
                bom(p, end);
                uint64_t id = number(p, end);
                comma(p, end);
                uint32_t name_len, note_len;
                Field name = text_pair(p, end, name_len, note_len, shared);
                uint64_t region = number(p, end);
                uint32_t region_key = region < regions.size()
                                          ? static_cast<uint32_t>(region) : UINT32_MAX;
                if (region_key != UINT32_MAX) __builtin_prefetch(&regions[region_key], 0, 1);
                comma(p, end);
                int64_t amount = cents(p, end);
                unsigned group = region_key != UINT32_MAX && regions[region_key] != UINT8_MAX
                                     ? regions[region_key] : unmatched;
                newline(p, end);
                batch[count++] = Pending{id, amount, name, name_len, note_len, group};
            } while (count < batch.size() && p < end);
            // Finish the parse burst before blocking on dictionary index loads.
            for (unsigned i = 0; i < count; ++i) {
                slots[i] = shared.prepare(batch[i].name);
                if (slots[i]) __builtin_prefetch(slots[i], 1, 3);
            }
            for (unsigned i = 0; i < count; ++i) {
                const Pending &r = batch[i];
                w.groups[r.group].add(r.id, r.amount, r.name_len, r.note_len, shared.first(r.name, r.group, slots[i], end));
            }
            if (p - reclaimed >= (1 << 20)) discard_consumed(reclaimed, p);
        }
        const uintptr_t page = static_cast<uintptr_t>(sysconf(_SC_PAGESIZE));
        const uintptr_t first = (reinterpret_cast<uintptr_t>(range_begin) + page - 1) & ~(page - 1);
        const uintptr_t last = reinterpret_cast<uintptr_t>(end) & ~(page - 1);
        if (last > first && munmap(reinterpret_cast<void *>(first), last - first))
            throw std::runtime_error("worker munmap");
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
        threads = std::min(threads, std::max(1u, std::thread::hardware_concurrency()));
        unsigned pieces = threads == 1 ? 1 : threads * 8;
        std::vector<const char *> cuts(pieces + 1);
        cuts[0] = record_start;
        cuts[pieces] = end;
        if (pieces > 1) {
            // Generated notes contain no digits; LF followed by an id and comma certifies a record boundary.
            for (unsigned i = 1; i < pieces; ++i) {
                const char *q = record_start + (end - record_start) * i / pieces;
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
        std::atomic<unsigned> next_piece{0};
        std::vector<std::thread> jobs;
        for (unsigned i = 0; i < threads; ++i)
            jobs.emplace_back([&, i] {
                for (unsigned k; (k = next_piece.fetch_add(1, std::memory_order_relaxed)) < pieces;)
                    parse_range(cuts[k], cuts[k + 1], regions, unmatched, workers[i], shared);
            });
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
