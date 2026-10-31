#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>
#include <thread>
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

static uint32_t raw_hash(const char *p, uint32_t len) {
    uint32_t h = HASH_OFFSET;
    for (uint32_t i = 0; i < len; ++i) h = ((h << 5) + h) ^ static_cast<uint8_t>(p[i]);
    return h;
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

struct Arena {
    std::vector<std::unique_ptr<char[]>> blocks;
    char *arena_cursor = nullptr;
    size_t arena_remaining = 0;
    const char *keep(const Field &f) {
        constexpr size_t block = 1 << 20;
        if (!f.raw_len) return f.raw;
        if (f.raw_len > block) {
            blocks.emplace_back(new char[f.raw_len]);
            std::memcpy(blocks.back().get(), f.raw, f.raw_len);
            return blocks.back().get();
        }
        if (f.raw_len > arena_remaining) {
            blocks.emplace_back(new char[block]);
            arena_cursor = blocks.back().get();
            arena_remaining = block;
        }
        char *dst = arena_cursor;
        std::memcpy(dst, f.raw, f.raw_len);
        arena_cursor += f.raw_len;
        arena_remaining -= f.raw_len;
        return dst;
    }
};

struct SharedNames {
    struct alignas(64) Entry {
        std::atomic<uint32_t> state{0};
        uint32_t hash = 0;
        uint32_t raw_len = 0;
        char key[16]{};
        std::atomic<uint64_t> seen[4]{};
    };
    static_assert(sizeof(Entry) == 64, "inline name slot");
    static constexpr size_t capacity = 1 << 18;
    std::unique_ptr<Entry[]> table{new Entry[capacity]};

    bool first_in_group(const Field &field, unsigned group, Arena &storage) {
        size_t i = field.hash & (capacity - 1);
        for (;;) {
            Entry &entry = table[i];
            uint32_t state = entry.state.load(std::memory_order_acquire);
            if (!state) {
                uint32_t expected = 0;
                if (entry.state.compare_exchange_strong(expected, 1, std::memory_order_acq_rel)) {
                    entry.hash = field.hash;
                    entry.raw_len = field.raw_len;
                    if (field.raw_len <= sizeof(entry.key)) {
                        std::memcpy(entry.key, field.raw, field.raw_len);
                    } else {
                        const char *saved = storage.keep(field);
                        std::memcpy(entry.key, &saved, sizeof(saved));
                        std::memcpy(entry.key + 8, field.raw, 8);
                    }
                    entry.state.store(2, std::memory_order_release);
                    state = 2;
                } else {
                    state = expected;
                }
            }
            while (state == 1) state = entry.state.load(std::memory_order_acquire);
            if (state == 2 && entry.hash == field.hash && entry.raw_len == field.raw_len) {
                bool same;
                if (field.raw_len <= sizeof(entry.key)) {
                    same = std::memcmp(entry.key, field.raw, field.raw_len) == 0;
                } else {
                    const char *saved;
                    std::memcpy(&saved, entry.key, sizeof(saved));
                    same = std::memcmp(entry.key + 8, field.raw, 8) == 0 &&
                           std::memcmp(saved + 8, field.raw + 8, field.raw_len - 8) == 0;
                }
                if (same) {
                    uint64_t mask = 1ULL << (group & 63);
                    return !(entry.seen[group >> 6].fetch_or(mask, std::memory_order_relaxed) & mask);
                }
            }
            i = (i + 1) & (capacity - 1);
        }
    }
};

static uint64_t number(const char *&p, const char *end) {
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
    Arena names;
    std::exception_ptr error;
    explicit Worker(size_t group_count) : groups(group_count) { }
};

static void parse_range(const char *p, const char *end, const std::vector<uint8_t> &regions,
                        unsigned unmatched, Worker &w, SharedNames &shared) {
    struct Pending {
        uint64_t id;
        Field name;
        uint32_t note_len;
        int64_t amount;
        unsigned group;
    };
    try {
        std::array<Pending, 32> batch;
        const char *reclaimed = p;
        while (p < end) {
            unsigned count = 0;
            do {
                bom(p, end);
                uint64_t id = number(p, end);
                comma(p, end);
                Field name = text_field(p, end, true);
                __builtin_prefetch(&shared.table[name.hash & (SharedNames::capacity - 1)], 0, 1);
                comma(p, end);
                Field note = text_field(p, end, false);
                comma(p, end);
                uint64_t region = number(p, end);
                uint32_t region_key = region < regions.size()
                                          ? static_cast<uint32_t>(region) : UINT32_MAX;
                if (region_key != UINT32_MAX) __builtin_prefetch(&regions[region_key], 0, 1);
                comma(p, end);
                int64_t amount = cents(p, end);
                newline(p, end);
                batch[count++] = Pending{id, name, note.len, amount, region_key};
            } while (count < batch.size() && p < end);
            for (unsigned i = 0; i < count; ++i) {
                const Pending &r = batch[i];
                unsigned group = r.group < regions.size() && regions[r.group] != UINT8_MAX
                                     ? regions[r.group] : unmatched;
                w.groups[group].add(r.id, r.amount, r.name, r.note_len,
                                    shared.first_in_group(r.name, group, w.names));
            }
            if (p - reclaimed >= (1 << 20)) discard_consumed(reclaimed, p);
        }
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
            std::vector<uint8_t> parity(threads);
            std::vector<std::thread> jobs;
            for (unsigned i = 0; i < threads; ++i) {
                const char *a = record_start + (end - record_start) * i / threads;
                const char *b = record_start + (end - record_start) * (i + 1) / threads;
                jobs.emplace_back([&, i, a, b] {
                    uint8_t x = 0;
                    const char *reclaimed = a;
                    for (const char *q = a; q < b;) {
                        const char *limit = b - q > (1 << 20) ? q + (1 << 20) : b;
                        for (; q < limit; ++q) x ^= (*q == '"');
                        discard_consumed(reclaimed, q);
                    }
                    parity[i] = x;
                });
            }
            for (auto &job : jobs) job.join();
            uint8_t in_quote = 0;
            for (unsigned i = 1; i < threads; ++i) {
                in_quote ^= parity[i - 1];
                uint8_t state = in_quote;
                const char *q = record_start + (end - record_start) * i / threads;
                for (; q < end; ++q) {
                    if (*q == '"') state ^= 1;
                    if (*q == '\n' && !state) { ++q; break; }
                }
                cuts[i] = q;
            }
        }
        SharedNames shared;
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
