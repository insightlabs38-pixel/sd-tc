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
    static constexpr unsigned shard_bits = 7, slots_per_shard = 2048;
    static constexpr unsigned name_capacity = (1U << shard_bits) * slots_per_shard;
    static uint32_t spread(uint32_t h) { return h ^ (h >> 11) ^ (h >> 22); }
    struct Slot {
        char prefix[24]{};
        const char *whole = nullptr;
        uint32_t len = 0, id = 0;
        bool matches(const Field &f, const char *) const {
            if (len != f.raw_len || std::memcmp(prefix, f.raw, std::min<size_t>(24, len)))
                return false;
            return len <= 24 || std::memcmp(whole + 24, f.raw + 24, len - 24) == 0;
        }
    };
    static_assert(sizeof(Slot) == 40);
    struct Shard {
        struct Overflow {
            Field field;
            std::array<uint64_t, 4> seen{};
        };
        std::array<std::atomic<uint64_t>, slots_per_shard> index{};
        std::array<Slot, slots_per_shard> pool{};
        std::atomic<uint32_t> used{};
        std::mutex mutex;
        std::vector<Overflow> overflow;
        std::vector<std::unique_ptr<char[]>> arena;
        char *cursor = nullptr;
        size_t remaining = 0;
        const char *keep_unlocked(const Field &f) {
            constexpr size_t block = 1 << 15;
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
            const char *dst = cursor;
            std::memcpy(cursor, f.raw, f.raw_len);
            cursor += f.raw_len;
            remaining -= f.raw_len;
            return dst;
        }
    };
    std::array<Shard, 1 << shard_bits> shards;
    std::atomic<uint32_t> next_id{};
    std::atomic<uint64_t> *marks;
    SharedNames() {
        constexpr size_t bytes = size_t(name_capacity) * 4 * sizeof(uint64_t);
        marks = static_cast<std::atomic<uint64_t> *>(
            mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        if (marks == MAP_FAILED) throw std::bad_alloc();
    }
    ~SharedNames() { munmap(marks, size_t(name_capacity) * 4 * sizeof(uint64_t)); }
    Slot *prepare(const Field &f) {
        const uint32_t h = spread(f.hash), si = h & ((1U << shard_bits) - 1);
        Shard &s = shards[si];
        const uint32_t sig = f.hash ^ (f.raw_len * 0x9e3779b9U);
        uint32_t bucket = (h >> shard_bits) & (slots_per_shard - 1);
        for (unsigned n = 0; n != slots_per_shard; ++n) {
            auto &cell = s.index[bucket];
            uint64_t word = cell.load(std::memory_order_acquire);
            if (!word) {
                uint64_t expected = 0;
                if (cell.compare_exchange_strong(expected, 1, std::memory_order_acq_rel)) {
                    uint32_t local = s.used.fetch_add(1, std::memory_order_relaxed);
                    if (local >= slots_per_shard) {
                        cell.store(0, std::memory_order_release);
                        return nullptr;
                    }
                    try {
                        Slot &slot = s.pool[local];
                        slot.len = f.raw_len;
                        slot.id = next_id.fetch_add(1, std::memory_order_relaxed);
                        if (f.raw_len > 24) {
                            std::lock_guard<std::mutex> lock(s.mutex);
                            slot.whole = s.keep_unlocked(f);
                        }
                        std::memcpy(slot.prefix, f.raw, std::min<size_t>(24, f.raw_len));
                        cell.store((uint64_t(sig) << 32) | uint64_t(local + 2),
                                   std::memory_order_release);
                        return &slot;
                    } catch (...) {
                        cell.store(0, std::memory_order_release);
                        throw;
                    }
                }
                word = expected;
            }
            while (word == 1) word = cell.load(std::memory_order_acquire);
            if (uint32_t(word >> 32) == sig && uint32_t(word) >= 2) {
                Slot *slot = &s.pool[uint32_t(word) - 2];
                if (slot->matches(f, nullptr)) return slot;
            }
            bucket = (bucket + 1) & (slots_per_shard - 1);
        }
        return nullptr;
    }
    bool overflow_first(const Field &f, unsigned group) {
        Shard &s = shards[spread(f.hash) & ((1U << shard_bits) - 1)];
        std::lock_guard<std::mutex> lock(s.mutex);
        const uint64_t bit = uint64_t(1) << (group & 63);
        for (auto &entry : s.overflow) {
            if (entry.field.hash != f.hash || entry.field.raw_len != f.raw_len ||
                std::memcmp(entry.field.raw, f.raw, f.raw_len)) continue;
            auto &w = entry.seen[group >> 6];
            bool first = !(w & bit);
            w |= bit;
            return first;
        }
        s.overflow.emplace_back();
        auto &entry = s.overflow.back();
        entry.field = f;
        entry.field.raw = s.keep_unlocked(f);
        entry.seen[group >> 6] |= bit;
        return true;
    }
    bool first(const Field &f, unsigned group, Slot *slot, const char *) {
        if (!slot) return overflow_first(f, group);
        const size_t ix = size_t(slot->id) * 4 + (group >> 6);
        uint64_t bit = uint64_t(1) << (group & 63);
        return !(marks[ix].fetch_or(bit, std::memory_order_relaxed) & bit);
    }
    void prefetch_seen(Slot *slot, unsigned group) {
        __builtin_prefetch(&marks[size_t(slot->id) * 4 + (group >> 6)], 1, 3);
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
                uint32_t spread = SharedNames::spread(name.hash);
                auto &shard = shared.shards[spread & 127];
                __builtin_prefetch(&shard.index[(spread >> 7) & (SharedNames::slots_per_shard - 1)], 0, 1);
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
    struct Pending {
        uint64_t id;
        int64_t amount;
        Field name;
        uint32_t name_len, note_len;
        unsigned group;
    };
    struct Entry {
        Pending row{};
        SharedNames::Slot *slot = nullptr;
    };
    constexpr unsigned lag = 32, window = lag * 2;
    static_assert((window & (window - 1)) == 0);
    std::array<Entry, window> ring{};
    const char *range_begin = p;
    const char *reclaimed = p;
    const char *safe = p;
    size_t n = 0;
    auto retire = [&](size_t i) {
        Entry &e = ring[i & (window - 1)];
        const Pending &r = e.row;
        w.groups[r.group].add(r.id, r.amount, r.name_len, r.note_len,
                               shared.first(r.name, r.group, e.slot, end));
        safe = r.name.raw;
    };
    auto intern = [&](size_t i) {
        Entry &e = ring[i & (window - 1)];
        e.slot = shared.prepare(e.row.name);
        if (e.slot) {
            __builtin_prefetch(e.slot, 0, 3);
            shared.prefetch_seen(e.slot, e.row.group);
        }
    };
    try {
        while (p < end) {
            if (n >= window) retire(n - window);
            if (n >= lag) intern(n - lag);
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
            ring[n & (window - 1)].row = Pending{id, amount, name, name_len, note_len, group};
            ++n;
            if (safe - reclaimed >= (1 << 20)) discard_consumed(reclaimed, safe);
        }
        for (size_t i = n; i < n + window; ++i) {
            if (i >= window && i - window < n) retire(i - window);
            if (i >= lag && i - lag < n) intern(i - lag);
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
