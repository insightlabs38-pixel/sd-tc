from pathlib import Path
r=Path('/workspace/work/public-research');s=(r/'short-frame48.cpp').read_text();prep=(r/'short-prepare-read.cpp').read_text()
a=s.index('struct SharedNames {');b=s.index('static uint64_t number',a);c=prep.index('struct SharedNames {');d=prep.index('static uint64_t number',c);s=s[:a]+prep[c:d]+s[b:]
s=s.replace('        std::array<Pending, 32> batch;', '        std::array<Pending, 32> batch;\n        std::array<SharedNames::Slot *, 32> slots;')
a=s.index('            for (unsigned i = 0; i < count; ++i) {',s.index('static void parse_range'));b=s.index('            if (p - reclaimed',a)
s=s[:a]+'''            for (unsigned i = 0; i < count; ++i) {
                Pending &r = batch[i];
                r.group = r.group < regions.size() && regions[r.group] != UINT8_MAX ? regions[r.group] : unmatched;
                slots[i] = shared.prepare(r.name);
                if (slots[i]) {
                    __builtin_prefetch(slots[i], 0, 3);
                    __builtin_prefetch(&slots[i]->seen[r.group >> 6], 0, 3);
                }
            }
            for (unsigned i = 0; i < count; ++i) {
                const Pending &r = batch[i];
                w.groups[r.group].add(r.id, r.amount, r.name_len, r.note_len, shared.first(r.name, r.group, slots[i]));
            }
''' +s[b:]
(r/'short-frame-prepare.cpp').write_text(s)
t=s.replace('#include <sys/mman.h>','#include <sys/mman.h>\n#include <sys/prctl.h>').replace('    try {\n        File lookup','    try {\n        (void)prctl(PR_SET_THP_DISABLE, 1, 0, 0, 0);\n        File lookup')
(r/'short-frame-prepare-thp.cpp').write_text(t)
