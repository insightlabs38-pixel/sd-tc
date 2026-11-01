from pathlib import Path
r=Path('/workspace/work/public-research');s=(r/'padded.cpp').read_text()
fast='''    if (n <= 16) {
        alignas(16) static const unsigned char mask[32] = {
            255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
        __m128i key = _mm_and_si128(_mm_loadu_si128(reinterpret_cast<const __m128i *>(p)),
                                  _mm_loadu_si128(reinterpret_cast<const __m128i *>(mask + 16 - n)));
        uint32_t h = static_cast<uint32_t>(_mm_crc32_u64(HASH_OFFSET, static_cast<uint64_t>(_mm_cvtsi128_si64(key))));
        return static_cast<uint32_t>(_mm_crc32_u64(h, static_cast<uint64_t>(_mm_extract_epi64(key, 1))));
    }
'''
s=s.replace('static uint32_t raw_hash(const char *p, size_t n) {\n','static uint32_t raw_hash(const char *p, size_t n) {\n'+fast)
(r/'padded-hash.cpp').write_text(s)
equal=(r/'padded-equal.cpp').read_text()
equal=equal.replace('static uint32_t raw_hash(const char *p, size_t n) {\n','static uint32_t raw_hash(const char *p, size_t n) {\n'+fast)
(r/'padded-hash-equal.cpp').write_text(equal)
for name in ['control-main','padded','padded-equal','padded-hash','padded-hash-equal']:
 t=(r/(name+'.cpp')).read_text().replace('unsigned threads = end - p < (1 << 20) ? 1 : 6;', 'unsigned threads = end - p < (1 << 20) ? 1 : 1;')
 (r/(name+'-short-one.cpp')).write_text(t)
