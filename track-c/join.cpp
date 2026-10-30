#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
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

struct Field {
    const char *raw = nullptr;
    uint32_t raw_len : 31;
    uint32_t quoted : 1;
    uint32_t len = 0;
    uint32_t hash = 0;
    uint32_t id = UINT32_MAX;
};
static_assert(sizeof(Field) == 24, "name slot layout");

static constexpr uint32_t FNV_OFFSET = 2166136261U;
static constexpr uint32_t FNV_PRIME = 16777619U;

static Field text_field(const char *&p, const char *end, bool need_hash) {
    Field f{};
    f.quoted = p < end && *p == '"';
    if (f.quoted) {
        ++p;
        f.raw = p;
        if (!need_hash) {
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
                p = q + 1;
                return f;
            }
        }
        uint32_t h = FNV_OFFSET;
        while (p < end) {
            unsigned char c = static_cast<unsigned char>(*p++);
            if (c == '"') {
                if (p < end && *p == '"') ++p;
                else {
                    f.raw_len = static_cast<uint32_t>(p - f.raw - 1);
                    f.hash = h;
                    return f;
                }
            }
            ++f.len;
            if (need_hash) h = (h ^ c) * FNV_PRIME;
        }
        throw std::runtime_error("unclosed quote");
    }
    f.raw = p;
    uint32_t h = FNV_OFFSET;
    while (p < end && *p != ',' && *p != '\r' && *p != '\n') {
        unsigned char c = static_cast<unsigned char>(*p++);
        ++f.len;
        if (need_hash) h = (h ^ c) * FNV_PRIME;
    }
    f.raw_len = f.len;
    f.hash = h;
    return f;
}

static bool equal_field(const Field &f, const Field &g) {
    if (f.len != g.len) return false;
    if (f.raw_len == f.len && g.raw_len == g.len)
        return std::memcmp(f.raw, g.raw, f.len) == 0;
    const char *a = f.raw, *b = g.raw;
    const char *ae = a + f.raw_len, *be = b + g.raw_len;
    while (a < ae && b < be) {
        unsigned char x = *a++, y = *b++;
        if (f.quoted && x == '"' && a < ae && *a == '"') ++a;
        if (g.quoted && y == '"' && b < be && *b == '"') ++b;
        if (x != y) return false;
    }
    return a == ae && b == be;
}

struct Names {
    std::vector<Field> table;
    std::vector<std::array<uint64_t, 4>> seen;
    size_t used = 0;
    explicit Names(size_t capacity = 1 << 18) : table(capacity) { seen.reserve(capacity); }
    void grow() {
        std::vector<Field> next(table.size() * 2);
        for (const auto &slot : table) {
            if (slot.id == UINT32_MAX) continue;
            size_t i = slot.hash & (next.size() - 1);
            while (next[i].id != UINT32_MAX) i = (i + 1) & (next.size() - 1);
            next[i] = slot;
        }
        table.swap(next);
    }
    uint32_t intern(const Field &f) {
        if ((used + 1) * 10 >= table.size() * 9) grow();
        size_t i = f.hash & (table.size() - 1);
        while (table[i].id != UINT32_MAX) {
            if (table[i].hash == f.hash && equal_field(table[i], f)) return table[i].id;
            i = (i + 1) & (table.size() - 1);
        }
        uint32_t id = static_cast<uint32_t>(seen.size());
        table[i] = f;
        table[i].id = id;
        seen.push_back({0, 0, 0, 0});
        ++used;
        return id;
    }
    bool first_in_group(uint32_t id, unsigned group) {
        uint64_t &cell = seen[id][group >> 6];
        uint64_t mask = 1ULL << (group & 63);
        if (cell & mask) return false;
        cell |= mask;
        return true;
    }
};

static uint64_t number(const char *&p, const char *end) {
    uint64_t n = 0;
    if (p == end || *p < '0' || *p > '9') throw std::runtime_error("missing integer");
    while (p < end && *p >= '0' && *p <= '9') n = n * 10 + (*p++ - '0');
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
    void add(uint64_t id, int64_t amount, const Field &name, const Field &note, bool first) {
        if (!count || amount < min) min = amount;
        if (!count || amount > max) max = amount;
        ++count;
        id_sum += id;
        sum += amount;
        name_bytes += name.len;
        note_bytes += note.len;
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
    Names names;
    std::exception_ptr error;
    explicit Worker(size_t group_count) : groups(group_count), names(1 << 17) { }
};

static void parse_range(const char *p, const char *end, const std::vector<uint16_t> &regions,
                        unsigned unmatched, Worker &w) {
    struct Pending {
        uint64_t id;
        Field name, note;
        int64_t amount;
        unsigned group;
    };
    try {
        std::array<Pending, 8> batch;
        while (p < end) {
            unsigned count = 0;
            const size_t mask = w.names.table.size() - 1;
            do {
                bom(p, end);
                uint64_t id = number(p, end);
                comma(p, end);
                Field name = text_field(p, end, true);
                comma(p, end);
                Field note = text_field(p, end, false);
                comma(p, end);
                uint64_t region = number(p, end);
                comma(p, end);
                int64_t amount = cents(p, end);
                newline(p, end);
                unsigned group = region < regions.size() && regions[region] != UINT16_MAX ? regions[region] : unmatched;
                batch[count++] = Pending{id, name, note, amount, group};
                __builtin_prefetch(&w.names.table[name.hash & mask], 0, 1);
            } while (count < batch.size() && p < end);
            for (unsigned i = 0; i < count; ++i) {
                const Pending &r = batch[i];
                uint32_t name_id = w.names.intern(r.name);
                w.groups[r.group].add(r.id, r.amount, r.name, r.note,
                                      w.names.first_in_group(name_id, r.group));
            }
        }
    } catch (...) { w.error = std::current_exception(); }
}

static std::vector<Group> merge_workers(const std::vector<Worker> &workers, unsigned group_count) {
    std::vector<Group> groups(group_count);
    for (const Worker &w : workers) {
        for (unsigned g = 0; g < group_count; ++g) {
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
        }
    }
    if (workers.size() == 1) {
        for (unsigned g = 0; g < group_count; ++g)
            groups[g].distinct = workers.front().groups[g].distinct;
        return groups;
    }
    constexpr unsigned shards = 6;
    std::array<std::array<uint32_t, 256>, shards> distinct{};
    std::array<std::exception_ptr, shards> errors{};
    std::vector<std::thread> jobs;
    jobs.reserve(shards);
    for (unsigned shard = 0; shard < shards; ++shard) {
        jobs.emplace_back([&, shard] {
            try {
                Names merged(1 << 15);
                for (const Worker &w : workers) {
                    for (const Field &slot : w.names.table) {
                        if (slot.id == UINT32_MAX || ((slot.hash >> 16) % shards) != shard) continue;
                        uint32_t id = merged.intern(slot);
                        for (unsigned word = 0; word < 4; ++word) {
                            uint64_t &dst = merged.seen[id][word];
                            uint64_t fresh = w.names.seen[slot.id][word] & ~dst;
                            dst |= fresh;
                            while (fresh) {
                                ++distinct[shard][word * 64 + __builtin_ctzll(fresh)];
                                fresh &= fresh - 1;
                            }
                        }
                    }
                }
            } catch (...) { errors[shard] = std::current_exception(); }
        });
    }
    for (auto &job : jobs) job.join();
    for (const auto &error : errors) if (error) std::rethrow_exception(error);
    for (unsigned shard = 0; shard < shards; ++shard)
        for (unsigned g = 0; g < group_count; ++g) groups[g].distinct += distinct[shard][g];
    return groups;
}

int main() {
    try {
        File lookup("/data/regions.csv");
        const char *p = lookup.data, *end = p + lookup.size;
        const char *header = "region_id,country\n";
        if (end - p < 18 || memcmp(p, header, 18)) throw std::runtime_error("regions header");
        p += 18;
        std::vector<uint16_t> regions(1000000, UINT16_MAX);
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
            regions[id] = static_cast<uint16_t>(code_index[code]);
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
                    for (const char *q = a; q < b; ++q) x ^= (*q == '"');
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
        std::vector<Worker> workers;
        workers.reserve(threads);
        for (unsigned i = 0; i < threads; ++i) workers.emplace_back(unmatched + 1);
        std::vector<std::thread> jobs;
        for (unsigned i = 0; i < threads; ++i)
            jobs.emplace_back([&, i] { parse_range(cuts[i], cuts[i + 1], regions, unmatched, workers[i]); });
        for (auto &job : jobs) job.join();
        for (const auto &w : workers) if (w.error) std::rethrow_exception(w.error);
        std::vector<Group> groups = merge_workers(workers, unmatched + 1);
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
