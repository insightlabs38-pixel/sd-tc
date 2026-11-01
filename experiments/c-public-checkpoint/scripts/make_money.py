from pathlib import Path
r=Path('/workspace/work/public-research')
s=(r/'padded-hash-equal.cpp').read_text();control=(r/'control-main.cpp').read_text()
a=s.index('static const char *find_quote(');b=s.index('\nstatic Field text_field',a)
c=control.index('static const char *find_quote(');d=control.index('\nstatic Field text_field',c)
s=s[:a]+control[c:d]+s[b:]
(r/'short-guarded.cpp').write_text(s)
needle="    if (neg) ++p;\n    int64_t n = static_cast<int64_t>(number(p, end));"
fast='''    if (neg) ++p;
    uint64_t word; std::memcpy(&word, p, 8);
    uint64_t dots = word ^ 0x2e2e2e2e2e2e2e2eULL;
    dots = (dots - 0x0101010101010101ULL) & ~dots & 0x8080808080808080ULL;
    unsigned dot = dots ? __builtin_ctzll(dots) / 8 : 8;
    if (dot && dot <= 5 && end - p >= dot + 3) {
        uint64_t lower = (1ULL << (dot * 8)) - 1;
        uint64_t x = ((word & lower) | ((word >> 8) & ~lower)) << ((6 - dot) * 8);
        x &= 0x0f0f0f0f0f0f0f0fULL;
        x = (x * 10 + (x >> 8)) & 0x00ff00ff00ff00ffULL;
        x = (x * 100 + (x >> 16)) & 0x0000ffff0000ffffULL;
        int64_t value = static_cast<int64_t>((x * 10000 + (x >> 32)) & 0xffffffffULL);
        p += dot + 3; return neg ? -value : value;
    }
    int64_t n = static_cast<int64_t>(number(p, end));'''
assert needle in s
s=s.replace(needle,fast)
(r/'short-money.cpp').write_text(s)
for name in ['short-guarded','short-money']:
 t=(r/(name+'.cpp')).read_text().replace('unsigned threads = end - p < (1 << 20) ? 1 : 6;', 'unsigned threads = end - p < (1 << 20) ? 1 : 1;')
 (r/(name+'-one.cpp')).write_text(t)
