from pathlib import Path
import subprocess
r=Path('/workspace/work/public-research');base=(r/'short-money.cpp').read_text()
(r/'short-group64.cpp').write_text(base.replace('struct Group {','struct alignas(64) Group {'))
s=base.replace('''        uint64_t id;
        Field name;
        uint32_t note_len;
        int64_t amount;
        unsigned group;''','''        uint64_t id;
        int64_t amount;
        Field name;
        uint32_t note_len;
        unsigned group;''').replace('Pending{id, name, note.len, amount, region_key}','Pending{id, amount, name, note.len, region_key}')
(r/'short-row48.cpp').write_text(s)
(r/'short-slot64.cpp').write_text(base.replace('    struct Slot {','    struct alignas(64) Slot {').replace('sizeof(Slot) == 48','sizeof(Slot) == 64'))
packet=Path('/workspace/work/packet40.cpp').read_text()
s=base
for start,end in [('struct Field {','static constexpr uint32_t HASH_OFFSET'),('static Field text_field','struct SharedNames {'),('struct Group {','static std::string money'),('static void parse_range','int main()')]:
 a=s.index(start);b=s.index(end,a);c=packet.index(start);d=packet.index(end,c)
 s=s[:a]+packet[c:d]+s[b:]
s=s.replace('static_cast<const char *>(memchr(p, \'"\', end - p))','find_quote(p, end)')
s=s.replace('shared.shards[spread & 63]','shared.shards[spread & 127]').replace('(spread >> 6) &','(spread >> 7) &')
(r/'short-packet40.cpp').write_text(s)
byte=subprocess.check_output(['git','-C','/workspace/sd-tc','show','frontier/c-byte-country-pipeline:track-c/join.cpp'],text=True)
s=byte
for start,end in [('struct File {','static void discard_consumed'),('static uint32_t raw_hash','static const char *find_quote'),('static int64_t cents','static void comma')]:
 a=s.index(start);b=s.index(end,a);c=base.index(start);d=base.index(end,c)
 s=s[:a]+base[c:d]+s[b:]
a='if (f.raw_len <= 16) return std::memcmp(s.key, f.raw, f.raw_len) == 0;'
b='''if (f.raw_len <= 16) {
            __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i *>(s.key));
            __m128i b = _mm_loadu_si128(reinterpret_cast<const __m128i *>(f.raw));
            unsigned same = static_cast<unsigned>(_mm_movemask_epi8(_mm_cmpeq_epi8(a, b)));
            unsigned valid = 0xffffU >> (16 - f.raw_len);
            return (same & valid) == valid;
        }'''
assert a in s;s=s.replace(a,b)
(r/'byte-short.cpp').write_text(s)
# Batch pipeline retains the qualified byte table but removes the per-record ring.
a=s.index('static void parse_range');b=s.index('int main()',a);c=base.index('static void parse_range');d=base.index('int main()',c)
parse=base[c:d]
parse=parse.replace('        std::array<Pending, 32> batch;', '        std::array<Pending, 32> batch;\n        std::array<std::pair<unsigned, uint32_t>, 32> marks;')
old='''                uint32_t spread = SharedNames::spread(name.hash);
                auto &shard = shared.shards[spread & 127];
                __builtin_prefetch(&shard.index[(spread >> 7) & (SharedNames::slots_per_shard - 1)], 0, 1);'''
assert old in parse;parse=parse.replace(old,'                SharedNames::prefetch(name);')
old='''                w.groups[group].add(r.id, r.amount, r.name, r.note_len,
                                    shared.first(r.name, group));'''
new='''                w.groups[group].add(r.id, r.amount, r.name, r.note_len, false);
                uint32_t id = shared.intern(r.name, group, w.groups[group].distinct, w.arena);
                marks[i] = {group, id};
                if (id != UINT32_MAX) CountryBits::prefetch(group, id);'''
assert old in parse;parse=parse.replace(old,new)
parse=parse.replace('            if (p - reclaimed >= (1 << 20))', '''            for (unsigned i = 0; i < count; ++i)
                if (marks[i].second != UINT32_MAX) CountryBits::mark(marks[i].first, marks[i].second);
            if (p - reclaimed >= (1 << 20))''')
s=s[:a]+parse+s[b:];(r/'byte-short-batch.cpp').write_text(s)
