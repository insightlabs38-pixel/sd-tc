from pathlib import Path
r=Path('/workspace/work/public-research');s=(r/'byte-short.cpp').read_text()
s=s.replace('#ifndef C_LAG\n','#ifndef C_SHARD_BITS\n#define C_SHARD_BITS 6\n#endif\n#ifndef C_LAG\n',1)
s=s.replace('static constexpr uint32_t capacity = CountryBits::capacity, limit = capacity / 8 * 7;', 'static constexpr uint32_t capacity = CountryBits::capacity, shard_bits = C_SHARD_BITS,\n                              shard_count = 1 << shard_bits, slots_per_shard = capacity / shard_count;')
s=s.replace('    std::atomic<uint64_t> used{0};\n','')
s=s.replace('std::array<Shard, 64> shards;', 'std::array<Shard, shard_count> shards;')
s=s.replace('uint32_t i = spread(f.hash) & (capacity - 1);\n        __builtin_prefetch', 'uint32_t h = spread(f.hash);\n        uint32_t i = (h & (shard_count - 1)) * slots_per_shard + ((h >> shard_bits) & (slots_per_shard - 1));\n        __builtin_prefetch',1)
s=s.replace('shards[spread(f.hash) & 63]', 'shards[spread(f.hash) & (shard_count - 1)]')
a=s.index('    __attribute__((noinline, cold)) uint32_t full_lookup');b=s.index('    uint32_t intern(',a);s=s[:a]+s[b:]
s=s.replace('''        uint32_t i = spread(f.hash) & (capacity - 1);
        for (uint32_t n = 0; n < capacity; ++n, i = (i + 1) & (capacity - 1)) {
            NameSlot &s = slots[i];
            auto &cell = controls[i].value;''', '''        uint32_t h = spread(f.hash), base = (h & (shard_count - 1)) * slots_per_shard;
        uint32_t i = (h >> shard_bits) & (slots_per_shard - 1);
        for (uint32_t n = 0; n < slots_per_shard; ++n, i = (i + 1) & (slots_per_shard - 1)) {
            NameSlot &s = slots[base + i];
            auto &cell = controls[base + i].value;''')
a=s.index('                // Reprobe after the acquire:');b=s.index('                if (f.raw_len > 16',a);s=s[:a]+s[b:]
s=s.replace('i = (i - 1) & (capacity - 1);', 'i = (i - 1) & (slots_per_shard - 1);')
a=s.index('                if (used.fetch_add(');b=s.index('                if (f.raw_len <= 16)',a);s=s[:a]+s[b:]
s=s.replace('cell.store(signature, std::memory_order_release); return i;','cell.store(signature, std::memory_order_release); return base + i;')
s=s.replace('if (control == signature && matches(s, f)) return i;','if (control == signature && matches(s, f)) return base + i;')
assert 'used.' not in s and 'full_lookup' not in s
(r/'byte-sharded.cpp').write_text(s)
(r/'byte-sharded128.cpp').write_text('#define C_SHARD_BITS 7\n'+s)
