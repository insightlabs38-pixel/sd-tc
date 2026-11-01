from pathlib import Path
r=Path('/workspace/work/public-research');s=(r/'byte-short.cpp').read_text()
a=s.index('    if (n <= 16) {',s.index('static uint32_t raw_hash'));b=s.index('    uint32_t h = HASH_OFFSET;',a);s=s[:a]+s[b:]
a=s.index('static const char *find_quote(')
helpers='''static __m128i short_key(const char *p, unsigned n) {
    alignas(16) static const unsigned char mask[32] = {
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    __m128i key = _mm_and_si128(_mm_loadu_si128(reinterpret_cast<const __m128i *>(p)),
                              _mm_loadu_si128(reinterpret_cast<const __m128i *>(mask + 16 - n)));
    return n < 16 ? _mm_insert_epi8(key, n, 15) : key;
}
static __m128i packed_key(const char *p, unsigned n, bool *valid = nullptr) {
    alignas(32) static const unsigned char mask[64] = {
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    __m256i bytes = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(p));
    if (valid) *valid = !(static_cast<unsigned>(_mm256_movemask_epi8(bytes)) & ((1U << n) - 1));
    bytes = _mm256_and_si256(bytes, _mm256_loadu_si256(reinterpret_cast<const __m256i *>(mask + 32 - n)));
    uint64_t a = _pext_u64(static_cast<uint64_t>(_mm256_extract_epi64(bytes, 0)), 0x7f7f7f7f7f7f7f7fULL);
    uint64_t b = _pext_u64(static_cast<uint64_t>(_mm256_extract_epi64(bytes, 1)), 0x7f7f7f7f7f7f7f7fULL);
    uint64_t c = _pext_u64(static_cast<uint64_t>(_mm256_extract_epi64(bytes, 2)), 0x7f7fULL);
    return _mm_set_epi64x(static_cast<long long>((b >> 8) | (c << 48) | (static_cast<uint64_t>(n - 17) << 62)),
                         static_cast<long long>(a | (b << 56)));
}
static __m128i inline_key(const Field &f) { return f.id == 2 ? packed_key(f.raw, f.raw_len) : short_key(f.raw, f.raw_len); }
static uint32_t name_hash(Field &f) {
    __m128i key;
    if (f.raw_len <= 16) { f.id = f.raw_len < 16 ? 0 : 1; key = short_key(f.raw, f.raw_len); }
    else if (f.raw_len <= 18) {
        bool valid; key = packed_key(f.raw, f.raw_len, &valid);
        if (!valid) { f.id = 3; return raw_hash(f.raw, f.raw_len); }
        f.id = 2;
    } else { f.id = 3; return raw_hash(f.raw, f.raw_len); }
    uint32_t h = static_cast<uint32_t>(_mm_crc32_u64(HASH_OFFSET, static_cast<uint64_t>(_mm_cvtsi128_si64(key))));
    return static_cast<uint32_t>(_mm_crc32_u64(h, static_cast<uint64_t>(_mm_extract_epi64(key, 1))));
}
'''
s=s[:a]+helpers+'\n'+s[a:]
s=s.replace('need_hash ? raw_hash(f.raw, f.raw_len) : HASH_OFFSET','need_hash ? name_hash(f) : HASH_OFFSET')
a=s.index('    static uint8_t tag(');b=s.index('\n    static bool matches',a)
s=s[:a]+'''    static uint8_t tag(const Field &f) { return static_cast<uint8_t>(0x80 | (f.id << 5) | (f.hash >> 27)); }''' +s[b:]
a=s.index('        if (f.raw_len <= 16) {',s.index('static bool matches'));b=s.index('        const char *saved;',a)
s=s[:a]+'''        if (f.id < 3) return _mm_movemask_epi8(_mm_cmpeq_epi8(
            _mm_loadu_si128(reinterpret_cast<const __m128i *>(s.key)), inline_key(f))) == 0xffff;
''' +s[b:]
s=s.replace('if (f.raw_len > 16 && !saved)', 'if (f.id == 3 && !saved)')
s=s.replace('if (f.raw_len <= 16) std::memcpy(s.key, f.raw, f.raw_len);','if (f.id < 3) _mm_storeu_si128(reinterpret_cast<__m128i *>(s.key), inline_key(f));')
(r/'byte-exact18.cpp').write_text(s)
# Same canonical short encoding without the optional17/18-byte packing.
a=s.index('static __m128i packed_key(');b=s.index('static uint32_t name_hash(',a)
s=s[:a]+'''static __m128i inline_key(const Field &f) { return short_key(f.raw, f.raw_len); }
''' +s[b:]
a=s.index('    else if (f.raw_len <= 18) {');b=s.index('    uint32_t h = static_cast',a)
s=s[:a]+'''    else { f.id = 3; return raw_hash(f.raw, f.raw_len); }
''' +s[b:]
(r/'byte-exact16.cpp').write_text(s)
