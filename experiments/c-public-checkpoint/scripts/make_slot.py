from pathlib import Path
p=Path('/workspace/sd-tc/track-c/join.cpp');s=p.read_text().replace('#include <sys/mman.h>','#include <sys/mman.h>\n#include <sys/prctl.h>')
a=s.index('struct SharedNames {');b=s.index('\nstatic uint64_t number',a)
s=s[:a]+'''#ifndef C_NAME_BITS
#define C_NAME_BITS 17
#endif
#ifndef C_LAG
#define C_LAG 16
#endif
#ifndef C_PREFILL
#define C_PREFILL 1
#endif
#ifndef C_THPOFF
#define C_THPOFF 1
#endif
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
struct NameSlot {
    std::atomic<uint64_t> control;
    char key[16];
    constexpr NameSlot() noexcept : control(0), key{} {}
};
static_assert(sizeof(NameSlot) == 24, "exact slot");
struct ZeroWord {
    std::atomic<uint64_t> value;
    constexpr ZeroWord() noexcept : value(0) {}
};
struct CountryBits {
    static constexpr uint32_t capacity = 1 << C_NAME_BITS, words = capacity / 64;
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
struct SharedNames {
    static constexpr uint32_t capacity = CountryBits::capacity, limit = capacity / 8 * 7;
    static constexpr uint64_t busy = 2;
    alignas(4096) inline static NameSlot slots[capacity]{};
    std::atomic<uint64_t> used{0};
    struct Overflow { Field field; std::array<uint64_t, 4> seen{}; };
    struct Shard { std::mutex mutex; std::vector<Overflow> overflow; };
    std::array<Shard, 64> shards;
    static uint32_t spread(uint32_t h) { return h ^ (h >> 11) ^ (h >> 22); }
    static uint64_t tag(const Field &f) { return (static_cast<uint64_t>(f.hash) << 32) | (static_cast<uint64_t>(f.raw_len) << 1) | 1; }
    static bool matches(const NameSlot &s, const Field &f) {
        if (f.raw_len <= 16) return std::memcmp(s.key, f.raw, f.raw_len) == 0;
        const char *saved; std::memcpy(&saved, s.key, sizeof(saved));
        return std::memcmp(s.key + 8, f.raw, 8) == 0 && std::memcmp(saved + 8, f.raw + 8, f.raw_len - 8) == 0;
    }
    static void prefetch(const Field &f) { __builtin_prefetch(&slots[spread(f.hash) & (capacity - 1)], 0, 3); }
    __attribute__((noinline, cold)) uint32_t overflow(const Field &f, unsigned group, uint32_t &distinct, NameArena &arena) {
        Shard &s = shards[spread(f.hash) & 63]; std::lock_guard<std::mutex> lock(s.mutex);
        uint64_t bit = 1ULL << (group & 63);
        for (auto &x : s.overflow) if (x.field.hash == f.hash && x.field.raw_len == f.raw_len &&
                                     std::memcmp(x.field.raw, f.raw, f.raw_len) == 0) {
            uint64_t &seen = x.seen[group >> 6]; distinct += !(seen & bit); seen |= bit; return UINT32_MAX;
        }
        s.overflow.emplace_back(); auto &x = s.overflow.back(); x.field = f;
        x.field.raw = arena.keep(f); x.seen[group >> 6] = bit; ++distinct; return UINT32_MAX;
    }
    uint32_t intern(const Field &f, unsigned group, uint32_t &distinct, NameArena &arena) {
        const uint64_t signature = tag(f);
        uint32_t i = spread(f.hash) & (capacity - 1);
        for (uint32_t n = 0; n < capacity; ++n, i = (i + 1) & (capacity - 1)) {
            NameSlot &s = slots[i];
            uint64_t control = s.control.load(std::memory_order_acquire);
            while (control == busy) { _mm_pause(); control = s.control.load(std::memory_order_acquire); }
            if (!control) {
                if (used.load(std::memory_order_relaxed) >= limit) return overflow(f, group, distinct, arena);
                uint64_t expected = 0;
                if (!s.control.compare_exchange_strong(expected, busy, std::memory_order_acquire)) { --n; i = (i - 1) & (capacity - 1); continue; }
                if (used.fetch_add(1, std::memory_order_relaxed) >= limit) {
                    // Every reader waits on busy; nobody can probe through the restored empty slot.
                    s.control.store(0, std::memory_order_release); return overflow(f, group, distinct, arena);
                }
                if (f.raw_len <= 16) std::memcpy(s.key, f.raw, f.raw_len);
                else {
                    const char *saved = arena.keep(f); std::memcpy(s.key, &saved, sizeof(saved));
                    std::memcpy(s.key + 8, f.raw, 8);
                }
                s.control.store(signature, std::memory_order_release); return i;
            }
            if (control == signature && matches(s, f)) return i;
        }
        return overflow(f, group, distinct, arena);
    }
};
''' +s[b:]
s=s.replace('struct Worker {','struct Worker {\n    NameArena arena;')
a=s.index('static void parse_range(');b=s.index('\nint main()',a)
s=s[:a]+'''static void parse_range(const char *p, const char *end, const std::vector<uint8_t> &regions,
                        unsigned unmatched, Worker &w, SharedNames &shared) {
    constexpr unsigned lag = C_LAG;
    static_assert(lag && !(lag & (lag - 1)), "pipeline distance");
    struct Pending { uint64_t id; Field name; uint32_t note_len; int64_t amount; uint32_t region; };
    struct Mark { uint32_t group = 0, id = UINT32_MAX; };
    try {
        std::array<Pending, lag> pending{};
        std::array<Mark, lag> marks{};
        uint64_t n = 0, last = UINT64_MAX;
        const char *reclaimed = p;
        auto advance = [&]() __attribute__((always_inline)) {
            unsigned k = n & (lag - 1);
            if (n >= 2 * lag && n - 2 * lag < last && marks[k].id != UINT32_MAX)
                CountryBits::mark(marks[k].group, marks[k].id);
            if (n >= lag && n - lag < last) {
                const Pending &r = pending[k];
                unsigned group = r.region < regions.size() && regions[r.region] != UINT8_MAX ? regions[r.region] : unmatched;
                w.groups[group].add(r.id, r.amount, r.name, r.note_len, false);
                uint32_t id = shared.intern(r.name, group, w.groups[group].distinct, w.arena);
                marks[k] = Mark{group, id};
                if (id != UINT32_MAX) CountryBits::prefetch(group, id);
            }
        };
        while (p < end) {
            bom(p, end); uint64_t id = number(p, end); comma(p, end);
            Field name = text_field(p, end, true); SharedNames::prefetch(name);
            comma(p, end); Field note = text_field(p, end, false); comma(p, end);
            uint64_t region = number(p, end);
            uint32_t key = region < regions.size() ? static_cast<uint32_t>(region) : UINT32_MAX;
            if (key != UINT32_MAX) __builtin_prefetch(&regions[key], 0, 3);
            comma(p, end); int64_t amount = cents(p, end); newline(p, end);
            advance(); pending[n & (lag - 1)] = Pending{id, name, note.len, amount, key}; ++n;
            if (n >= lag) {
                const char *safe = pending[n & (lag - 1)].name.raw;
                if (safe && safe - reclaimed >= (1 << 20)) discard_consumed(reclaimed, safe);
            }
        }
        for (last = n; n < last + 2 * lag; ++n) advance();
        discard_consumed(reclaimed, p);
    } catch (...) { w.error = std::current_exception(); }
}
''' +s[b:]
s=s.replace('    try {\n        File lookup','    try {\n        if (C_THPOFF) (void)prctl(PR_SET_THP_DISABLE, 1, 0, 0, 0);\n        File lookup')
s=s.replace('        static SharedNames shared;','''        static SharedNames shared;
        if (C_PREFILL) {
            (void)madvise(SharedNames::slots, sizeof(SharedNames::slots), MADV_POPULATE_WRITE);
            if (C_PREFILL > 1) (void)madvise(CountryBits::bits, (unmatched + 1) * CountryBits::words * sizeof(ZeroWord), MADV_POPULATE_WRITE);
        }''')
s=s.replace('        std::vector<unsigned> order;', '        for (unsigned g = 0; g <= unmatched; ++g) groups[g].distinct += CountryBits::count(g);\n        std::vector<unsigned> order;')
p.write_text(s)
