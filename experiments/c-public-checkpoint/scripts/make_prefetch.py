from pathlib import Path
r=Path('/workspace/work/public-research');base=(r/'byte-short.cpp').read_text()
needle='__builtin_prefetch(&controls[i], 0, 3); __builtin_prefetch(&slots[i], 0, 3);'
assert needle in base
for name,distance,hint in [('byte-pf2',4,3),('byte-pf3',8,3),('byte-pf2-l2',4,2)]:
 new='__builtin_prefetch(&controls[i], 0, 3); __builtin_prefetch(&slots[i], 0, '+str(hint)+');'
 for d in range(4,distance+1,4):new+=' __builtin_prefetch(&slots[(i + '+str(d)+') & (capacity - 1)], 0, '+str(hint)+');'
 (r/(name+'.cpp')).write_text(base.replace(needle,new))
s=(r/'short-money.cpp').read_text().replace('probe(const Field &f, size_t *empty = nullptr)', 'probe(const Field &f, size_t *empty = nullptr, bool verify = true)').replace('if (slot.matches(f))','if (!verify || slot.matches(f))')
a=s.index('    bool first(const Field &name, unsigned group) {');b=s.index('\n};\n\nstatic uint64_t number',a)
s=s[:a]+'''    Slot *prepare(const Field &name) {
        Shard &s = shards[spread(name.hash) & (shards.size() - 1)];
        return s.probe(name, nullptr, false).first;
    }
    bool first(const Field &name, unsigned group, Slot *slot) {
        if (!slot || !slot->matches(name)) {
            Shard &s = shards[spread(name.hash) & (shards.size() - 1)];
            return slow_first(s, name, group);
        }
        const uint64_t mask = 1ULL << (group & 63);
        return !(slot->seen[group >> 6].fetch_or(mask, std::memory_order_relaxed) & mask);
    }
''' +s[b:]
s=s.replace('        std::array<Pending, 32> batch;', '        std::array<Pending, 32> batch;\n        std::array<SharedNames::Slot *, 32> slots;')
old='''            for (unsigned i = 0; i < count; ++i) {
                const Pending &r = batch[i];
                unsigned group = r.group < regions.size() && regions[r.group] != UINT8_MAX
                                     ? regions[r.group] : unmatched;
                w.groups[group].add(r.id, r.amount, r.name, r.note_len,
                                    shared.first(r.name, group));
            }'''
new='''            for (unsigned i = 0; i < count; ++i) {
                Pending &r = batch[i];
                r.group = r.group < regions.size() && regions[r.group] != UINT8_MAX ? regions[r.group] : unmatched;
                slots[i] = shared.prepare(r.name);
                if (slots[i]) {
                    __builtin_prefetch(slots[i], 0, 3);
                    __builtin_prefetch(&slots[i]->seen[r.group >> 6], PREFETCH_WRITE, 3);
                }
            }
            for (unsigned i = 0; i < count; ++i) {
                const Pending &r = batch[i];
                w.groups[r.group].add(r.id, r.amount, r.name, r.note_len, shared.first(r.name, r.group, slots[i]));
            }'''
assert old in s;s=s.replace(old,new)
(r/'short-prepare-read.cpp').write_text(s.replace('PREFETCH_WRITE','0'))
(r/'short-prepare-write.cpp').write_text(s.replace('PREFETCH_WRITE','1'))
