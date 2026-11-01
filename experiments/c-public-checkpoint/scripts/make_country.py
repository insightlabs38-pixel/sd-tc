from pathlib import Path
s=Path('/workspace/work/public-research/control-main.cpp').read_text()
s='#include <sys/prctl.h>\n'+s
pos=s.index('struct SharedNames {');end=s.index('static uint64_t number',pos)
cls='''struct ZeroWord {
    std::atomic<uint64_t> value;
    constexpr ZeroWord() noexcept : value(0) {}
};
static_assert(sizeof(ZeroWord) == 8, "atomic index word");
struct NameBlock { uint32_t next = 0, end = 0; };
struct SharedNames {
    static constexpr unsigned shard_bits = 6;
    static constexpr uint32_t capacity = 1 << 17, slots_per_shard = 1 << 11;
    static constexpr uint64_t id_mask = (1ULL << 24) - 1;
    struct Key { char bytes[16]{}; };
    struct Shard {
        struct Overflow { Field field; std::array<uint64_t, 4> seen{}; };
        std::mutex mutex;
        std::vector<Overflow> overflow;
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
                arena.emplace_back(new char[block]); cursor = arena.back().get(); remaining = block;
            }
            char *out = cursor; std::memcpy(out, f.raw, f.raw_len);
            cursor += f.raw_len; remaining -= f.raw_len; return out;
        }
    };
    std::array<Shard, 1 << shard_bits> shards;
    alignas(4096) inline static ZeroWord index[capacity]{};
    alignas(4096) inline static Key keys[capacity]{};
    std::atomic<uint32_t> allocated{0};
    static uint32_t spread(uint32_t h) { return h ^ (h >> 11) ^ (h >> 22); }
    static uint64_t tag(const Field &f) {
        return (static_cast<uint64_t>(f.hash) << 32) |
               (static_cast<uint64_t>(f.raw_len <= 16 ? f.raw_len : 31U) << 24);
    }
    static uint32_t id(uint64_t entry) { return static_cast<uint32_t>((entry & id_mask) - 1); }
    static bool matches(uint32_t key_id, const Field &f) {
        const Key &key = keys[key_id];
        if (f.raw_len <= 16) return std::memcmp(key.bytes, f.raw, f.raw_len) == 0;
        const char *saved; uint32_t length;
        std::memcpy(&saved, key.bytes, sizeof(saved));
        std::memcpy(&length, key.bytes + 8, sizeof(length));
        return length == f.raw_len && std::memcmp(key.bytes + 12, f.raw, 4) == 0 &&
               std::memcmp(saved + 4, f.raw + 4, f.raw_len - 4) == 0;
    }
    static void prefetch(const Field &f) {
        uint32_t h = spread(f.hash);
        size_t home = (h & 63) * slots_per_shard + ((h >> 6) & (slots_per_shard - 1));
        __builtin_prefetch(&index[home], 0, 3);
    }
    static uint64_t probe(const Field &f, bool verify, size_t *empty = nullptr) {
        uint32_t h = spread(f.hash);
        size_t base = (h & 63) * slots_per_shard, i = (h >> 6) & (slots_per_shard - 1);
        const uint64_t signature = tag(f);
        for (unsigned n = 0; n < slots_per_shard; ++n) {
            uint64_t entry = index[base + i].value.load(std::memory_order_acquire);
            if (!entry) { if (empty) *empty = base + i; return 0; }
            if ((entry & ~id_mask) == signature && (!verify || matches(id(entry), f))) return entry;
            i = (i + 1) & (slots_per_shard - 1);
        }
        return 0;
    }
    uint32_t reserve(NameBlock &block) {
        if (block.next < block.end) return block.next++;
        uint32_t begin = allocated.load(std::memory_order_relaxed);
        while (begin < capacity) {
            uint32_t finish = std::min(begin + 32, capacity);
            if (allocated.compare_exchange_weak(begin, finish, std::memory_order_relaxed)) {
                block.next = begin + 1; block.end = finish; return begin;
            }
        }
        return UINT32_MAX;
    }
    __attribute__((noinline, cold)) uint32_t resolve(const Field &f, unsigned group, NameBlock &block,
                                                  uint32_t &overflow_distinct) {
        Shard &s = shards[spread(f.hash) & 63];
        std::lock_guard<std::mutex> lock(s.mutex);
        size_t empty = capacity;
        uint64_t found = probe(f, true, &empty);
        if (found) return id(found);
        const uint64_t bit = 1ULL << (group & 63);
        for (auto &entry : s.overflow) if (entry.field.hash == f.hash &&
             entry.field.raw_len == f.raw_len && std::memcmp(entry.field.raw, f.raw, f.raw_len) == 0) {
            uint64_t &word = entry.seen[group >> 6];
            overflow_distinct += !(word & bit); word |= bit; return UINT32_MAX;
        }
        uint32_t key_id = empty < capacity ? reserve(block) : UINT32_MAX;
        if (key_id == UINT32_MAX) {
            s.overflow.emplace_back(); auto &entry = s.overflow.back();
            entry.field = f; entry.field.raw = s.keep(f); entry.seen[group >> 6] = bit;
            ++overflow_distinct; return UINT32_MAX;
        }
        Key &key = keys[key_id];
        if (f.raw_len <= 16) std::memcpy(key.bytes, f.raw, f.raw_len);
        else {
            const char *saved = s.keep(f); uint32_t length = f.raw_len;
            std::memcpy(key.bytes, &saved, sizeof(saved)); std::memcpy(key.bytes + 8, &length, 4);
            std::memcpy(key.bytes + 12, f.raw, 4);
        }
        index[empty].value.store(tag(f) | (key_id + 1), std::memory_order_release);
        return key_id;
    }
};
struct CountryBits {
    static constexpr unsigned words = SharedNames::capacity / 64;
    alignas(4096) inline static ZeroWord bits[256 * words]{};
    static std::atomic<uint64_t> &word(unsigned group, uint32_t id) { return bits[group * words + id / 64].value; }
    static void prefetch(unsigned group, uint32_t id) { __builtin_prefetch(&word(group, id), 1, 3); }
    static void mark(unsigned group, uint32_t id) { word(group, id).fetch_or(1ULL << (id & 63), std::memory_order_relaxed); }
    static uint32_t count(unsigned group) {
        uint32_t total = 0;
        for (unsigned i = 0; i < words; ++i) total += __builtin_popcountll(bits[group * words + i].value.load(std::memory_order_relaxed));
        return total;
    }
};
#ifndef C_LAG
#define C_LAG 16
#endif
#ifndef C_PREFILL
#define C_PREFILL 0
#endif
#ifndef C_THPOFF
#define C_THPOFF 1
#endif

'''
s=s[:pos]+cls+s[end:]
s=s.replace('struct Worker {','struct Worker {\n    NameBlock block;')
pos=s.index('static void parse_range');end=s.index('\nint main()',pos)
parser='''static void parse_range(const char *p, const char *end, const std::vector<uint8_t> &regions,
                        unsigned unmatched, Worker &w, SharedNames &shared) {
    constexpr unsigned lag = C_LAG;
    static_assert(lag && !(lag & (lag - 1)), "pipeline distance");
    struct Pending { uint64_t id; int64_t amount; Field name; uint32_t note_len, region; };
    struct Candidate { Field name; uint32_t group = 0, id = UINT32_MAX; bool verified = false; };
    struct Mark { uint32_t group = 0, id = UINT32_MAX; };
    try {
        std::array<Pending, lag> pending{};
        std::array<Candidate, lag> candidates{};
        std::array<Mark, lag> marks{};
        uint64_t n = 0, last = UINT64_MAX;
        const char *reclaimed = p;
        auto advance = [&]() __attribute__((always_inline)) {
            unsigned k = n & (lag - 1);
            if (n >= 3 * lag && n - 3 * lag < last && marks[k].id != UINT32_MAX)
                CountryBits::mark(marks[k].group, marks[k].id);
            if (n >= 2 * lag && n - 2 * lag < last) {
                const Candidate &c = candidates[k];
                uint32_t id = c.id;
                if (!c.verified && !SharedNames::matches(id, c.name))
                    id = shared.resolve(c.name, c.group, w.block, w.groups[c.group].distinct);
                marks[k] = Mark{c.group, id};
                if (id != UINT32_MAX) CountryBits::prefetch(c.group, id);
            }
            if (n >= lag && n - lag < last) {
                const Pending &r = pending[k];
                unsigned group = r.region < regions.size() && regions[r.region] != UINT8_MAX ? regions[r.region] : unmatched;
                w.groups[group].add(r.id, r.amount, r.name, r.note_len, false);
                uint64_t found = SharedNames::probe(r.name, false);
                uint32_t id;
                if (found) {
                    id = SharedNames::id(found);
                    __builtin_prefetch(&SharedNames::keys[id], 0, 3);
                } else id = shared.resolve(r.name, group, w.block, w.groups[group].distinct);
                candidates[k] = Candidate{r.name, group, id, !found};
            }
        };
        while (p < end) {
            bom(p, end);
            uint64_t id = number(p, end); comma(p, end);
            Field name = text_field(p, end, true); SharedNames::prefetch(name);
            comma(p, end); Field note = text_field(p, end, false); comma(p, end);
            uint64_t region = number(p, end);
            uint32_t key = region < regions.size() ? static_cast<uint32_t>(region) : UINT32_MAX;
            if (key != UINT32_MAX) __builtin_prefetch(&regions[key], 0, 3);
            comma(p, end); int64_t amount = cents(p, end); newline(p, end);
            advance(); pending[n & (lag - 1)] = Pending{id, amount, name, note.len, key}; ++n;
            if (n >= 2 * lag) {
                const char *safe = candidates[n & (lag - 1)].name.raw;
                if (safe && safe - reclaimed >= (1 << 20)) discard_consumed(reclaimed, safe);
            }
        }
        for (last = n; n < last + 3 * lag; ++n) advance();
        discard_consumed(reclaimed, p);
    } catch (...) { w.error = std::current_exception(); }
}
'''
s=s[:pos]+parser+s[end:]
s=s.replace('    try {\n        File lookup', '    try {\n        if (C_THPOFF) (void)prctl(PR_SET_THP_DISABLE, 1, 0, 0, 0);\n        File lookup',1)
s=s.replace('        static SharedNames shared;', '''        static SharedNames shared;
        if (C_PREFILL) {
            (void)madvise(SharedNames::index, sizeof(SharedNames::index), MADV_POPULATE_WRITE);
            (void)madvise(CountryBits::bits, (unmatched + 1) * CountryBits::words * sizeof(ZeroWord), MADV_POPULATE_WRITE);
            if (C_PREFILL > 1) (void)madvise(SharedNames::keys, sizeof(SharedNames::keys), MADV_POPULATE_WRITE);
        }''')
s=s.replace('        std::vector<unsigned> order;','        for (unsigned g = 0; g <= unmatched; ++g) groups[g].distinct += CountryBits::count(g);\n        std::vector<unsigned> order;')
Path('/workspace/sd-tc/track-c/join.cpp').write_text(s)
