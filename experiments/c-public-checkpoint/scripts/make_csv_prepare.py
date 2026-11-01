from pathlib import Path
r=Path('/workspace/work/public-research');s=(r/'csv-cursor.cpp').read_text();prep=(r/'short-frame-prepare.cpp').read_text()
a=s.index('struct SharedNames {');b=s.index('static uint64_t number',a);c=prep.index('struct SharedNames {');d=prep.index('static uint64_t number',c);s=s[:a]+prep[c:d]+s[b:]
s=s.replace('        std::array<Pending, 32> batch;', '        std::array<Pending, 32> batch;\n        std::array<SharedNames::Slot *, 32> slots;')
a=s.index('            for (unsigned i = 0; i < count; ++i) {',s.index('static void parse_range'));b=s.index('            if (p - reclaimed',a)
c=prep.index('            for (unsigned i = 0; i < count; ++i) {',prep.index('static void parse_range'));d=prep.index('            if (p - reclaimed',c);s=s[:a]+prep[c:d]+s[b:]
(r/'csv-prepare.cpp').write_text(s)
helper='''__attribute__((target("pclmul"))) static uint64_t csv_prefix(uint64_t q) {
    return static_cast<uint64_t>(_mm_cvtsi128_si64(_mm_clmulepi64_si128(_mm_cvtsi64_si128(static_cast<long long>(q)), _mm_set1_epi8(-1), 0)));
}
'''
s=s.replace('struct CsvCursor {',helper+'struct CsvCursor {\n    const bool hardware = __builtin_cpu_supports("pclmul");')
a=s.index('        quotes = mask(\'"\'); uint64_t inside = quotes;');b=s.index('        inside ^= carry;',a)
s=s[:a]+'''        quotes = mask('"'); uint64_t inside;
        if (hardware) inside = csv_prefix(quotes);
        else {
            inside = quotes;
            inside ^= inside << 1; inside ^= inside << 2; inside ^= inside << 4;
            inside ^= inside << 8; inside ^= inside << 16; inside ^= inside << 32;
        }
''' +s[b:]
(r/'csv-prepare-pclmul.cpp').write_text(s)
