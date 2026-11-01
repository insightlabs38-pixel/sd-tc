from pathlib import Path
r=Path('/workspace/work/public-research');s=(r/'byte-short.cpp').read_text()
a=s.index('        uint64_t n = 0, last = UINT64_MAX;');b=s.index('        while (p < end) {',a)
s=s[:a]+'''        unsigned k = 0;
        const char *reclaimed = p;
        auto resolve = [&](const Pending &r) {
            unsigned group = r.region < regions.size() && regions[r.region] != UINT8_MAX ? regions[r.region] : unmatched;
            w.groups[group].add(r.id, r.amount, r.name, r.note_len, false);
            uint32_t id = shared.intern(r.name, group, w.groups[group].distinct, w.arena);
            if (id != UINT32_MAX) CountryBits::prefetch(group, id);
            return Mark{group, id};
        };
        auto mark = [](Mark m) { if (m.id != UINT32_MAX) CountryBits::mark(m.group, m.id); };
''' +s[b:]
a=s.index('            advance(); pending[n &');b=s.index('        discard_consumed(reclaimed, p);',a)
s=s[:a]+'''            mark(marks[k]);
            if (pending[k].name.raw) marks[k] = resolve(pending[k]);
            pending[k] = Pending{id, name, note.len, amount, key};
            k = (k + 1) & (lag - 1);
            const char *safe = pending[k].name.raw;
            if (safe && safe - reclaimed >= (1 << 20)) discard_consumed(reclaimed, safe);
        }
        for (Mark m : marks) mark(m);
        for (const Pending &r : pending) if (r.name.raw) mark(resolve(r));
''' +s[b:]
(r/'byte-short-ring.cpp').write_text(s)
for name,config in [('byte-ring8','#define C_LAG 8\n'),('byte-ring32','#define C_LAG 32\n'),('byte-ring-prefill','#define C_PREFILL 2\n'),('byte-ring-group','#define C_PREFILL 2\n')]:
 t=s
 if name.endswith('group'):t=t.replace('struct Group {','struct alignas(64) Group {')
 (r/(name+'.cpp')).write_text(config+t)
