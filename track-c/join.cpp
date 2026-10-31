#include <condition_variable>
#include <nmmintrin.h>
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

[[maybe_unused]] static void discard_consumed(const char *&cursor, const char *end) {
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

struct NameKey {
    uint64_t first = 0;
    uint32_t middle = 0, hash = 0;
};
static_assert(sizeof(NameKey) == 16, "exact projected name key");

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

// For fixed lengths <=16, the first 12 bytes plus CRC32C are injective:
// a CRC of at most four unknown suffix bytes has a full-rank 32-bit transform.
static NameKey encode_name(const char *raw, uint32_t length) {
    NameKey key;
    if (length > 16) {
        key.first = reinterpret_cast<uintptr_t>(raw);
        key.middle = length;
        key.hash = raw_hash(raw, length);
        return key;
    }
    uint32_t h = HASH_OFFSET, remaining = length;
    uint64_t word = 0;
    if (length >= 8) {
        std::memcpy(&word, raw, 8);
        key.first = word;
        h = static_cast<uint32_t>(_mm_crc32_u64(h, word));
        remaining -= 8;
        if (remaining) {
            // A successfully parsed name has at least eight following record bytes.
            std::memcpy(&word, raw + 8, 8);
            unsigned take = std::min(remaining, 4U);
            key.middle = static_cast<uint32_t>(word) & (take == 4 ? UINT32_MAX : (1U << (take * 8)) - 1);
        }
    } else if (length) {
        std::memcpy(&word, raw, 8);
        key.first = word & (UINT64_MAX >> ((8 - length) * 8));
    }
    if (remaining == 8) h = static_cast<uint32_t>(_mm_crc32_u64(h, word));
    else {
        if (remaining & 4) { h = _mm_crc32_u32(h, static_cast<uint32_t>(word)); word >>= 32; }
        if (remaining & 2) { h = _mm_crc32_u16(h, static_cast<uint16_t>(word)); word >>= 16; }
        if (remaining & 1) h = _mm_crc32_u8(h, static_cast<uint8_t>(word));
    }
    key.hash = h;
    return key;
}

static Field text_field(const char *&p, const char *end, bool need_hash) {
    Field f{};
    f.quoted = p < end && *p == '"';
    if (f.quoted) {
        ++p;
        f.raw = p;
        for (;;) {
            const char *q = static_cast<const char *>(memchr(p, '"', end - p));
            if (!q) throw std::runtime_error("unclosed quote");
            f.len += static_cast<uint32_t>(q - p);
            if (q + 1 < end && q[1] == '"') {
                ++f.len;
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
        ++f.len;
    }
    f.raw_len = f.len;
    f.hash = need_hash ? raw_hash(f.raw, f.raw_len) : HASH_OFFSET;
    return f;
}

struct BucketNames {
    struct Entry { NameKey key; std::array<uint64_t, 4> seen{}; };
    static bool matches(const NameKey &a, const NameKey &b, unsigned shape) {
        if (shape <= 16) return std::memcmp(&a, &b, sizeof(NameKey)) == 0;
        return a.hash == b.hash && a.middle == b.middle &&
               std::memcmp(reinterpret_cast<const char *>(a.first), reinterpret_cast<const char *>(b.first), a.middle) == 0;
    }
    struct Shard {
        struct Overflow { NameKey key; unsigned shape; std::array<uint64_t, 4> seen{}; };
        std::array<uint32_t, 2048> index{};
        Entry *pool = nullptr;
        uint16_t used = 0;
        std::vector<Overflow> overflow;
    };
    std::array<Shard, 64> shards;
    explicit BucketNames(unsigned owner) {
        static Entry storage[12 * 64 * 1024]{};
        for (unsigned i = 0; i < 64; ++i) shards[i].pool = storage + (owner * 64 + i) * 1024;
    }
    static uint32_t spread(uint32_t h) { return h ^ (h >> 11) ^ (h >> 22); }
    static uint32_t tag(const NameKey &key, unsigned shape) { return (key.hash & 0xfffe0000U) | (shape << 12); }
    void prefetch(uint32_t hash) {
        uint32_t h = spread(hash);
        __builtin_prefetch(&shards[h & 63].index[(h >> 6) & 2047], 0, 1);
    }
    __attribute__((noinline, cold)) bool insert(Shard &s, const NameKey &key, unsigned shape, unsigned group, size_t empty) {
        uint64_t bit = 1ULL << (group & 63);
        if (s.used == 1024) {
            for (auto &e : s.overflow) if (e.shape == shape && matches(e.key, key, shape)) {
                uint64_t &word = e.seen[group >> 6]; bool fresh = !(word & bit); word |= bit; return fresh;
            }
            s.overflow.emplace_back(); auto &e = s.overflow.back();
            e.key = key; e.shape = shape; e.seen[group >> 6] = bit; return true;
        }
        Entry &e = s.pool[s.used]; e.key = key; e.seen[group >> 6] |= bit;
        s.index[empty] = tag(key, shape) | ++s.used;
        return true;
    }
    bool first(const NameKey &key, unsigned shape, unsigned group) {
        uint32_t h = spread(key.hash), signature = tag(key, shape);
        Shard &s = shards[h & 63];
        size_t i = (h >> 6) & 2047;
        for (;;) {
            uint32_t control = s.index[i];
            if (!control) return insert(s, key, shape, group, i);
            if ((control & 0xfffff000U) == signature && matches(s.pool[(control & 0xfffU) - 1].key, key, shape)) {
                uint64_t &word = s.pool[(control & 0xfffU) - 1].seen[group >> 6];
                uint64_t bit = 1ULL << (group & 63); bool fresh = !(word & bit); word |= bit; return fresh;
            }
            i = (i + 1) & 2047;
        }
    }
};
static constexpr unsigned owner_count = 2;
struct NamePipe {
    static constexpr unsigned blocks = 4, batch_size = 512;
    struct Message { NameKey key; uint32_t group_shape = 0; };
    struct Block { unsigned count = 0; std::array<Message, batch_size> rows; };
    std::array<Block, blocks> data;
    unsigned written = 0, filled = 0, read = 0;
    std::mutex mutex;
    std::condition_variable writable;
    std::atomic<bool> done{false};
    void send(const Message &row) {
        Block &b = data[written % blocks];
        if (!filled) {
            std::unique_lock<std::mutex> guard(mutex);
            writable.wait(guard, [&] { return b.count == 0; });
        }
        b.rows[filled++] = row;
    }
};
struct OwnerSignal { std::mutex mutex; std::condition_variable ready; uint64_t epoch = 0; };
struct OwnerPipeline {
    std::vector<std::unique_ptr<NamePipe[]>> pipes;
    std::array<OwnerSignal, owner_count> signals;
    const char *base, *end;
    bool compact;
    OwnerPipeline(unsigned parsers, const char *p, const char *q, bool mode) : base(p), end(q), compact(mode) {
        for (unsigned i = 0; i < parsers; ++i) pipes.emplace_back(new NamePipe[owner_count]);
    }
    void notify(unsigned owner) {
        std::lock_guard<std::mutex> lock(signals[owner].mutex);
        ++signals[owner].epoch;
        signals[owner].ready.notify_one();
    }
    void flush(NamePipe &pipe, unsigned owner) {
        if (!pipe.filled) return;
        {
            std::lock_guard<std::mutex> guard(pipe.mutex);
            pipe.data[pipe.written++ % NamePipe::blocks].count = pipe.filled;
            pipe.filled = 0;
        }
        notify(owner);
    }
    void send(const NameKey &key, unsigned shape, unsigned group, unsigned parser) {
        unsigned owner = key.hash >> 31;
        NamePipe::Message row{};
        row.key = key; row.group_shape = group | (shape << 8);
        NamePipe &pipe = pipes[parser][owner]; pipe.send(row);
        if (pipe.filled == NamePipe::batch_size) flush(pipe, owner);
    }
    void finish(unsigned parser) {
        for (unsigned o = 0; o < owner_count; ++o) {
            flush(pipes[parser][o], o);
            pipes[parser][o].done.store(true, std::memory_order_release);
            notify(o);
        }
    }
    void consume(unsigned owner, std::array<uint32_t, 256> &counts) {
        BucketNames names(owner);
        for (;;) {
            uint64_t before;
            { std::lock_guard<std::mutex> lock(signals[owner].mutex); before = signals[owner].epoch; }
            bool progress = false, finished = true;
            for (auto &parser : pipes) {
                NamePipe &pipe = parser[owner];
                auto &b = pipe.data[pipe.read % NamePipe::blocks];
                unsigned count;
                { std::lock_guard<std::mutex> lock(pipe.mutex); count = b.count; }
                if (count) {
                    progress = true;
                    for (unsigned i = 0; i < count; ++i) {
                        if (i + 16 < count) {
                            const auto &future = b.rows[i + 16];
                            if ((future.group_shape >> 8) == 31) __builtin_prefetch(reinterpret_cast<const char *>(future.key.first), 0, 1);
                            names.prefetch(future.key.hash);
                        }
                        const auto &row = b.rows[i];
                        unsigned group = row.group_shape & 255, shape = row.group_shape >> 8;
                        counts[group] += names.first(row.key, shape, group);

                    }
                    { std::lock_guard<std::mutex> lock(pipe.mutex); b.count = 0; }
                    ++pipe.read;
                    pipe.writable.notify_one();
                }
                if (!pipe.done.load(std::memory_order_acquire)) finished = false;
                { std::lock_guard<std::mutex> lock(pipe.mutex); if (pipe.data[pipe.read % NamePipe::blocks].count) finished = false; }
            }
            if (finished) break;
            if (!progress) {
                std::unique_lock<std::mutex> lock(signals[owner].mutex);
                signals[owner].ready.wait(lock, [&] { return signals[owner].epoch != before; });
            }
        }
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

static void parse_range(const char *p, const char *end, const char *,
                        const std::vector<uint8_t> &regions, unsigned unmatched, Worker &w, bool, OwnerPipeline &pipeline, unsigned worker_id) {
    struct Pending {
        uint64_t id;
        int64_t amount;
        NameKey name; uint32_t name_len, shape, note_len, region;
    };
    try {
        std::array<Pending, 32> batch;
        while (p < end) {
            unsigned count = 0;
            do {
                bom(p, end);
                uint64_t id = number(p, end);
                comma(p, end);
                Field parsed = text_field(p, end, false);
                unsigned shape = parsed.raw_len <= 16 ? parsed.raw_len : 31U;
                NameKey name = encode_name(parsed.raw, parsed.raw_len);
                comma(p, end);
                Field note = text_field(p, end, false);
                comma(p, end);
                uint64_t region = number(p, end);
                uint32_t key = region < regions.size() ? static_cast<uint32_t>(region) : UINT32_MAX;
                if (key != UINT32_MAX) __builtin_prefetch(&regions[key], 0, 1);
                comma(p, end);
                int64_t amount = cents(p, end);
                newline(p, end);
                batch[count++] = Pending{id, amount, name, parsed.len, shape, note.len, key};
            } while (count < batch.size() && p < end);
            for (unsigned i = 0; i < count; ++i) {
                const Pending &r = batch[i];
                unsigned group = r.region < regions.size() && regions[r.region] != UINT8_MAX
                                     ? regions[r.region] : unmatched;
                w.groups[group].add(r.id, r.amount, r.name_len, r.note_len, false);
                pipeline.send(r.name, r.shape, group, worker_id);
            }
        }
        pipeline.finish(worker_id);
    } catch (...) { w.error = std::current_exception(); pipeline.finish(worker_id); }
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
        unsigned threads = end - p < (1 << 20) ? 1 : 4;
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
        if (records.size > UINT32_MAX) throw std::runtime_error("record offsets exceed 32 bits");
        bool compact = records.size < (1U << 28);
        std::vector<Worker> workers;
        workers.reserve(threads);
        for (unsigned i = 0; i < threads; ++i) workers.emplace_back(unmatched + 1);
        OwnerPipeline pipeline(threads, records.data, end, compact);
        std::array<std::array<uint32_t, 256>, owner_count> distinct{};
        std::vector<std::thread> jobs;
        for (unsigned i = 0; i < owner_count; ++i) jobs.emplace_back([&, i] { pipeline.consume(i, distinct[i]); });
        for (unsigned i = 0; i < threads; ++i)
            jobs.emplace_back([&, i] { parse_range(cuts[i], cuts[i + 1], records.data, regions, unmatched, workers[i], compact, pipeline, i); });
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
        for (const auto &d : distinct) for (unsigned g = 0; g <= unmatched; ++g) groups[g].distinct += d[g];
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
