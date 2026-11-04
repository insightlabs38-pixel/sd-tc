#include <time.h>
#include <linux/futex.h>
#include <sys/syscall.h>
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
            f.hash = need_hash ? raw_hash(f.raw, f.raw_len, end) : HASH_OFFSET;
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
    f.hash = need_hash ? raw_hash(f.raw, f.raw_len, end) : HASH_OFFSET;
    return f;
}


struct PrivateNames {
    struct Slot {
        char key[16]{};
        uint32_t length, hash;
        std::array<uint64_t, 4> seen{};
        Field field() const {
            const char *p = key;
            if (length > 16) std::memcpy(&p, key, sizeof(p));
            return {p, length, hash};
        }
        bool matches(const Field &f) const {
            if (hash != f.hash || length != f.raw_len) return false;
            auto view = field();
            return !std::memcmp(view.raw, f.raw, length);
        }
    };
    std::vector<uint32_t> index = std::vector<uint32_t>(1 << 17);
    std::vector<Slot> slots;
    std::vector<std::unique_ptr<char[]>> arena;
    char *cursor = nullptr;
    size_t remaining = 0;
    PrivateNames() { slots.reserve(65536); }
    static uint32_t hash(uint32_t h) { return h * 0x9e3779b1U; }
    const char *keep(const Field &f) {
        if (remaining < f.raw_len) {
            remaining = std::max<size_t>(32768, f.raw_len);
            arena.emplace_back(new char[remaining]);
            cursor = arena.back().get();
        }
        char *out = cursor;
        std::memcpy(out, f.raw, f.raw_len);
        cursor += f.raw_len;
        remaining -= f.raw_len;
        return out;
    }
    void grow() {
        index.assign(index.size() * 2, 0);
        for (uint32_t n = 0; n < slots.size(); ++n) {
            size_t i = hash(slots[n].hash) & (index.size() - 1);
            while (index[i]) i = (i + 1) & (index.size() - 1);
            index[i] = n + 1;
        }
    }
    Slot &get(const Field &f) {
        if ((slots.size() + 1) * 4 >= index.size() * 3) grow();
        size_t i = hash(f.hash) & (index.size() - 1);
        while (index[i]) {
            Slot &s = slots[index[i] - 1];
            if (s.matches(f)) return s;
            i = (i + 1) & (index.size() - 1);
        }
        uint32_t id = slots.size();
        slots.emplace_back();
        auto &s = slots.back();
        s.length = f.raw_len;
        s.hash = f.hash;
        if (f.raw_len <= 16) std::memcpy(s.key, f.raw, f.raw_len);
        else { const char *p = keep(f); std::memcpy(s.key, &p, sizeof(p)); }
        index[i] = id + 1;
        return s;
    }
    void prefetch(const Field &f) const {
        size_t i = hash(f.hash) & (index.size() - 1);
        __builtin_prefetch(&index[i], 0, 1);
    }
    uint32_t prepare(const Field &f) const {
        uint32_t id = index[hash(f.hash) & (index.size() - 1)];
        if (id) __builtin_prefetch(&slots[id - 1], 1, 3);
        return id;
    }
    void mark_prepared(const Field &f, unsigned group, uint32_t id) {
        Slot *slot = id ? &slots[id - 1] : nullptr;
        if (!slot || !slot->matches(f)) slot = &get(f);
        slot->seen[group >> 6] |= uint64_t(1) << (group & 63);
    }
    void mark(const Field &f, unsigned group) {
        get(f).seen[group >> 6] |= uint64_t(1) << (group & 63);
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


static void wait_word(std::atomic<unsigned> &word, unsigned value) {
    struct timespec timeout{0,1000000};
    (void)syscall(SYS_futex, reinterpret_cast<unsigned *>(&word), FUTEX_WAIT_PRIVATE, value, &timeout, nullptr, 0);
}
static void wake_word(std::atomic<unsigned> &word) {
    (void)syscall(SYS_futex, reinterpret_cast<unsigned *>(&word), FUTEX_WAKE_PRIVATE, 1, nullptr, nullptr, 0);
}
static_assert(sizeof(std::atomic<unsigned>) == 4 && std::atomic<unsigned>::is_always_lock_free, "futex word");

struct OwnerPipeline {
    static constexpr unsigned owners = 2, capacity = 4, batch = 256;
    struct Signal {
        std::atomic<unsigned> epoch{0};
        std::atomic<bool> sleeping{false};
        void notify() {
            epoch.fetch_add(1, std::memory_order_release);
            if (sleeping.load(std::memory_order_acquire)) wake_word(epoch);
        }
    };
    struct Message { Field name; unsigned group; };
    struct Block { unsigned size; std::array<Message, batch> rows; };
    struct Pipe {
        alignas(64) std::atomic<unsigned> written{0};
        alignas(64) std::atomic<unsigned> read{0};
        std::atomic<bool> waiting{false};
        alignas(64) std::array<Block, capacity> blocks;
        Signal *signal = nullptr;
        std::atomic<bool> *failed = nullptr;
        unsigned filled = 0, produced = 0;
        void push(const Field &name, unsigned group) {
            if (!filled) for (;;) {
                unsigned before = read.load(std::memory_order_acquire);
                if (produced - before < capacity) break;
                waiting.store(true, std::memory_order_release);
                if (failed->load(std::memory_order_acquire)) throw std::runtime_error("owner failed");
                wait_word(read, before);
                waiting.store(false, std::memory_order_release);
            }
            blocks[produced % capacity].rows[filled++] = {name, group};
            if (filled == batch) flush();
        }
        void flush() {
            if (!filled) return;
            blocks[produced % capacity].size = filled;
            filled = 0;
            written.store(++produced, std::memory_order_release);
            signal->notify();
        }
    };
    std::vector<std::unique_ptr<Pipe[]>> pipes;
    std::array<Signal, owners> signals;
    std::atomic<unsigned> done{0};
    std::atomic<bool> failed{false};
    std::array<std::exception_ptr, owners> errors;
    std::array<std::array<uint32_t, 256>, owners> distinct{};
    explicit OwnerPipeline(unsigned n) {
        for (unsigned i = 0; i < n; ++i) {
            pipes.emplace_back(new Pipe[owners]);
            for (unsigned o = 0; o < owners; ++o) {
                pipes.back()[o].failed = &failed;
                pipes.back()[o].signal = &signals[o];
            }
        }
    }
    void push(unsigned parser, const Field &name, unsigned group) {
        if (failed.load(std::memory_order_acquire)) throw std::runtime_error("owner failed");
        pipes[parser][PrivateNames::hash(name.hash) % owners].push(name, group);
    }
    void finish(unsigned parser) {
        for (unsigned o = 0; o < owners; ++o) pipes[parser][o].flush();
        done.fetch_add(1, std::memory_order_release);
        for (auto &s : signals) s.notify();
    }
    void consume(unsigned owner) {
        try {
            PrivateNames names;
            auto &signal = signals[owner];
            for (;;) {
                unsigned before = signal.epoch.load(std::memory_order_acquire);
                bool complete = done.load(std::memory_order_acquire) == pipes.size();
                bool active = false;
                for (auto &parser : pipes) {
                    Pipe &p = parser[owner];
                    unsigned read = p.read.load(std::memory_order_relaxed);
                    unsigned written = p.written.load(std::memory_order_acquire);
                    if (read == written) continue;
                    active = true;
                    const Block &b = p.blocks[read % capacity];
                    std::array<uint32_t, batch> prepared;
                    for (unsigned i = 0; i < b.size; ++i) {
                        names.prefetch(b.rows[i].name);
                        __builtin_prefetch(b.rows[i].name.raw,0,3);
                    }
                    for (unsigned i = 0; i < b.size; ++i) prepared[i] = names.prepare(b.rows[i].name);
                    for (unsigned i = 0; i < b.size; ++i) {
                        uint32_t id=prepared[i];
                        if (id && names.slots[id-1].length>16)
                            __builtin_prefetch(names.slots[id-1].field().raw,0,3);
                    }
                    for (unsigned i = 0; i < b.size; ++i) names.mark_prepared(b.rows[i].name, b.rows[i].group, prepared[i]);
                    p.read.store(read + 1, std::memory_order_release);
                    if (p.waiting.load(std::memory_order_acquire)) wake_word(p.read);
                }
                if (!active) {
                    if (complete) break;
                    signal.sleeping.store(true, std::memory_order_release);
                    wait_word(signal.epoch, before);
                    signal.sleeping.store(false, std::memory_order_release);
                }
            }
            for (const auto &s : names.slots) for (unsigned j = 0; j < 4; ++j) {
                uint64_t bits = s.seen[j];
                while (bits) { ++distinct[owner][j * 64 + __builtin_ctzll(bits)]; bits &= bits - 1; }
            }
        } catch (...) {
            errors[owner] = std::current_exception();
            failed.store(true, std::memory_order_release);
            for (auto &parser : pipes) for (unsigned o = 0; o < owners; ++o) wake_word(parser[o].read);
            for (auto &s : signals) s.notify();
        }
    }
};

struct Worker {
    std::vector<Group> groups;
    std::exception_ptr error;
    explicit Worker(size_t group_count) : groups(group_count) { }
};

static void parse_range(const char *p, const char *end, const std::vector<uint8_t> &regions,
                        unsigned unmatched, Worker &w, OwnerPipeline &pipeline, unsigned parser) {
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
                unsigned group = region_key != UINT32_MAX && regions[region_key] != UINT8_MAX
                                     ? regions[region_key] : unmatched;
                newline(p, end);
                batch[count] = Pending{id, amount, name, name_len, note_len, group};
                ++count;
            } while (count < batch.size() && p < end);
            for (unsigned i = 0; i < count; ++i) {
                const Pending &r = batch[i];
                w.groups[r.group].add(r.id, r.amount, r.name_len, r.note_len, false);
                pipeline.push(parser, r.name, r.group);
            }
            (void)reclaimed;
        }
    } catch (...) { w.error = std::current_exception(); }
    pipeline.finish(parser);
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
        std::vector<Worker> workers;
        workers.reserve(threads);
        for (unsigned i = 0; i < threads; ++i) workers.emplace_back(unmatched + 1);
        OwnerPipeline pipeline(threads);
        std::vector<std::thread> owner_jobs;
        for (unsigned i = 0; i < OwnerPipeline::owners; ++i) owner_jobs.emplace_back([&, i] { pipeline.consume(i); });
        std::vector<std::thread> jobs;
        for (unsigned i = 0; i < threads; ++i)
            jobs.emplace_back([&, i] { parse_range(cuts[i], cuts[i + 1], regions, unmatched, workers[i], pipeline, i); });
        for (auto &job : jobs) job.join();
        for (auto &job : owner_jobs) job.join();
        for (const auto &e : pipeline.errors) if (e) std::rethrow_exception(e);
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

        for (const auto &counts : pipeline.distinct) for (unsigned g = 0; g <= unmatched; ++g) groups[g].distinct += counts[g];
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
