from pathlib import Path
r=Path('/workspace/work/public-research');s=(r/'short-frame48.cpp').read_text();byte=(r/'byte-short.cpp').read_text()
s=s.replace('#include <sys/mman.h>','#include <sys/mman.h>\n#include <sys/prctl.h>')
a=s.index('struct SharedNames {');b=s.index('static uint64_t number',a)
c=byte.index('struct NameArena {');d=byte.index('struct NameSlot',c)
new=byte[c:d]+'''struct DirectControl {
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
        for (unsigned n = 0; n < 4; ++n) __builtin_prefetch(&slots[base + ((i + n) & (slots_per_shard - 1))], 1, 3);
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

'''
s=s[:a]+new+s[b:]
s=s.replace('struct Worker {','struct Worker {\n    NameArena arena;')
a=s.index('                uint32_t spread = SharedNames::spread(name.hash);',s.index('static void parse_range'));b=s.index('                comma(p, end);',a)
s=s[:a]+'                SharedNames::prefetch(name);\n'+s[b:]
s=s.replace('shared.first(r.name, group)', 'shared.first(r.name, group, w.arena)')
s=s.replace('    try {\n        File lookup','    try {\n        (void)prctl(PR_SET_THP_DISABLE, 1, 0, 0, 0);\n        File lookup')
s=s.replace('        static SharedNames shared;', '''        static SharedNames shared;
        (void)madvise(SharedNames::controls, sizeof(SharedNames::controls), MADV_POPULATE_WRITE);
        (void)madvise(SharedNames::slots, sizeof(SharedNames::slots), MADV_POPULATE_WRITE);''')
(r/'direct64.cpp').write_text(s)
(r/'direct64-pf1.cpp').write_text(s.replace('n < 4; ++n) __builtin_prefetch','n < 1; ++n) __builtin_prefetch'))
(r/'direct64-nofill.cpp').write_text(s.replace('        (void)madvise(SharedNames::controls, sizeof(SharedNames::controls), MADV_POPULATE_WRITE);\n','').replace('        (void)madvise(SharedNames::slots, sizeof(SharedNames::slots), MADV_POPULATE_WRITE);\n',''))
