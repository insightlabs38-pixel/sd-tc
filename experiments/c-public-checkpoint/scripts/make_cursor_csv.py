from pathlib import Path
r=Path('/workspace/work/public-research');s=(r/'short-frame48.cpp').read_text()
a=s.index('static const char *find_quote(');b=s.index('struct SharedNames {',a);s=s[:a]+s[b:]
a=s.index('struct Worker {')
helper='''struct CsvCursor {
    const char *block, *next_block, *end;
    uint64_t quotes = 0, separators = 0, before = 0, total = 0, previous = 0, carry = 0;
    CsvCursor(const char *p, const char *limit) : block(p), next_block(p), end(limit) {}
    bool refill() {
        if (next_block == end) return false;
        block = next_block;
        size_t bytes = std::min<size_t>(64, end - block);
        alignas(32) char tail[64]{};
        const char *input = block;
        if (bytes < 64) { std::memcpy(tail, block, bytes); input = tail; }
        __m256i lo = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(input));
        __m256i hi = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(input + 32));
        auto mask = [&](char c) {
            __m256i v = _mm256_set1_epi8(c);
            return static_cast<uint64_t>(static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(lo, v)))) |
                   (static_cast<uint64_t>(static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(hi, v)))) << 32);
        };
        quotes = mask('"'); uint64_t inside = quotes;
        inside ^= inside << 1; inside ^= inside << 2; inside ^= inside << 4;
        inside ^= inside << 8; inside ^= inside << 16; inside ^= inside << 32;
        inside ^= carry; carry = 0ULL - (inside >> 63);
        separators = (mask(',') | mask('\\n')) & ~inside;
        before = total; total += __builtin_popcountll(quotes); next_block += bytes; return true;
    }
    const char *next() {
        while (!separators) if (!refill()) {
            if (carry) throw std::runtime_error("unclosed quote");
            return end;
        }
        unsigned bit = __builtin_ctzll(separators); separators &= separators - 1;
        return block + bit;
    }
    Field text(const char *&p, bool hash, uint32_t &length) {
        const char *q = next();
        if (q == end || *q != ',') throw std::runtime_error("text delimiter");
        uint64_t cumulative = before + __builtin_popcountll(quotes & (UINT64_MAX >> (63 - (q - block))));
        uint64_t count = cumulative - previous; previous = cumulative;
        bool quoted = p < q && *p == '"';
        if (quoted && (q - p < 2 || q[-1] != '"' || count < 2 || (count & 1)))
            throw std::runtime_error("quoted field");
        Field f{}; f.raw = p + quoted; f.raw_len = static_cast<uint32_t>(q - p - 2 * quoted);
        length = f.raw_len - (quoted ? static_cast<uint32_t>((count - 2) / 2) : 0);
        f.hash = hash ? raw_hash(f.raw, f.raw_len) : HASH_OFFSET;
        p = q + 1; return f;
    }
};
static uint64_t field_number(const char *p, const char *end) {
    size_t digits = end - p;
    if (digits && digits <= 8) {
        uint64_t word; std::memcpy(&word, p, 8);
        uint64_t x = (word << ((8 - digits) * 8)) & 0x0f0f0f0f0f0f0f0fULL;
        x = (x * 10 + (x >> 8)) & 0x00ff00ff00ff00ffULL;
        x = (x * 100 + (x >> 16)) & 0x0000ffff0000ffffULL;
        return (x * 10000 + (x >> 32)) & 0xffffffffULL;
    }
    uint64_t n = number(p, end);
    if (p != end) throw std::runtime_error("integer suffix");
    return n;
}

'''
s=s[:a]+helper+s[a:]
a=s.index('    uint64_t word; std::memcpy(&word, p, 8);',s.index('static int64_t cents'));b=s.index('        uint64_t lower',a)
s=s[:a]+'''    size_t dot = end - p >= 3 ? static_cast<size_t>(end - p - 3) : 0;
    if (dot && dot <= 5 && p[dot] == '.') {
        uint64_t word; std::memcpy(&word, p, 8);
''' +s[b:]
s=s.replace('        const char *reclaimed = p;\n        while (p < end)', '        const char *reclaimed = p;\n        CsvCursor csv(p, end);\n        while (p < end)')
a=s.index('                bom(p, end);',s.index('static void parse_range'));b=s.index('                batch[count++]',a)
s=s[:a]+'''                bom(p, end);
                const char *delimiter = csv.next();
                if (delimiter == end || *delimiter != ',') throw std::runtime_error("id delimiter");
                uint64_t id = field_number(p, delimiter); p = delimiter + 1;
                uint32_t name_len, note_len;
                Field name = csv.text(p, true, name_len);
                uint32_t spread = SharedNames::spread(name.hash);
                auto &shard = shared.shards[spread & 127];
                __builtin_prefetch(&shard.index[(spread >> 7) & (SharedNames::slots_per_shard - 1)], 0, 1);
                (void)csv.text(p, false, note_len);
                delimiter = csv.next();
                if (delimiter == end || *delimiter != ',') throw std::runtime_error("region delimiter");
                uint64_t region = field_number(p, delimiter); p = delimiter + 1;
                uint32_t region_key = region < regions.size() ? static_cast<uint32_t>(region) : UINT32_MAX;
                if (region_key != UINT32_MAX) __builtin_prefetch(&regions[region_key], 0, 1);
                delimiter = csv.next();
                if (delimiter < end && *delimiter != '\\n') throw std::runtime_error("record delimiter");
                const char *amount_end = delimiter;
                if (amount_end > p && amount_end[-1] == '\\r') --amount_end;
                int64_t amount = cents(p, amount_end);
                if (p != amount_end) throw std::runtime_error("amount suffix");
                p = delimiter + (delimiter < end);
''' +s[b:]
(r/'csv-cursor.cpp').write_text(s)
