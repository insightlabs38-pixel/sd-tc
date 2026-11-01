from pathlib import Path
s=Path('/workspace/work/public-research/control-main.cpp').read_text()
a=s.index('static const char *find_quote(');b=s.index('\nstatic Field text_field',a)
s=s[:a]+'''#ifndef C_QUOTE_BYTES
#define C_QUOTE_BYTES 64
#endif
struct QuoteCursor {
    const char *window = nullptr, *limit;
    uint64_t quotes = 0;
    explicit QuoteCursor(const char *end) : limit(end) {}
    const char *next(const char *p) {
        constexpr size_t width = C_QUOTE_BYTES;
        static_assert(width == 32 || width == 64, "quote window");
        if (window && p >= window && p - window < static_cast<ptrdiff_t>(width)) {
            quotes &= UINT64_MAX << (p - window);
            if (quotes) return window + __builtin_ctzll(quotes);
            p = window + std::min<size_t>(width, limit - window);
        }
        const __m256i q = _mm256_set1_epi8('"');
        while (p < limit) {
            alignas(32) char tail[width]{};
            size_t bytes = std::min<size_t>(width, limit - p);
            const char *block = p;
            if (bytes < width) { std::memcpy(tail, p, bytes); block = tail; }
            window = p;
            __m256i lo = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(block));
            quotes = static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(lo, q)));
            if constexpr (width == 64) {
                __m256i hi = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(block + 32));
                quotes |= static_cast<uint64_t>(static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(hi, q)))) << 32;
            }
            if (quotes) return p + __builtin_ctzll(quotes);
            p += bytes;
        }
        return nullptr;
    }
};
''' +s[b:]
s=s.replace('bool need_hash) {','bool need_hash, QuoteCursor &cursor) {').replace('find_quote(p, end)','cursor.next(p)')
s=s.replace('        const char *reclaimed = p;','        const char *reclaimed = p;\n        QuoteCursor cursor(end);')
s=s.replace('text_field(p, end, true)','text_field(p, end, true, cursor)').replace('text_field(p, end, false)','text_field(p, end, false, cursor)')
Path('/workspace/work/public-research/quote-cache.cpp').write_text(s)
