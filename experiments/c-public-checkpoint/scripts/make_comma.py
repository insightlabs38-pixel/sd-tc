from pathlib import Path
import re
r=Path('/workspace/work/public-research')
helper='''static uint64_t record_number(const char *&p, const char *end) {
    uint64_t word; std::memcpy(&word, p, 8);
    uint64_t diff = word ^ 0x2c2c2c2c2c2c2c2cULL;
    uint64_t mask = (diff - 0x0101010101010101ULL) & ~diff & 0x8080808080808080ULL;
    if (mask) {
        unsigned digits = __builtin_ctzll(mask) / 8;
        if (digits) {
            uint64_t x = (word << ((8 - digits) * 8)) & 0x0f0f0f0f0f0f0f0fULL;
            x = (x * 10 + (x >> 8)) & 0x00ff00ff00ff00ffULL;
            x = (x * 100 + (x >> 16)) & 0x0000ffff0000ffffULL;
            p += digits + 1; return (x * 10000 + (x >> 32)) & 0xffffffffULL;
        }
    }
    uint64_t value = number(p, end); comma(p, end); return value;
}
'''
for name,out in [('short-money','short-comma'),('byte-short-ring','byte-comma')]:
 s=(r/(name+'.cpp')).read_text()
 s=s.replace('static void newline(',helper+'static void newline(')
 s,n=re.subn(r'uint64_t id = number\(p, end\);\s*comma\(p, end\);','uint64_t id = record_number(p, end);',s);assert n==2,n
 s=s.replace('uint64_t region = number(p, end);','uint64_t region = record_number(p, end);')
 s,n=re.subn(r'comma\(p, end\);\s*int64_t amount = cents', 'int64_t amount = cents',s);assert n==1,n
 (r/(out+'.cpp')).write_text(s)
