from pathlib import Path
r=Path('/workspace/work/public-research');s=(r/'short-packet40.cpp').read_text()
a=s.index('                if (__builtin_expect(amount < INT32_MIN');b=s.index('                batch[count++]',a);s=s[:a]+s[b:]
s=s.replace('''        uint64_t id;
        Field name;
        int32_t amount;''', '''        uint64_t id;
        int64_t amount;
        Field name;''').replace('sizeof(Pending) == 40','sizeof(Pending) == 48').replace('Pending{id, name, static_cast<int32_t>(amount), name_len, note_len, region_key}', 'Pending{id, amount, name, name_len, note_len, region_key}')
(r/'short-frame48.cpp').write_text(s)
