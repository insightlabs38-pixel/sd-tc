from pathlib import Path
p=Path('/workspace/sd-tc/track-c/join.cpp');s=p.read_text()
a=s.index('static const char *find_quote('); b=s.index('\nstruct SharedNames',a)
s=s[:a]+'''struct CsvSpan { const char *start, *end; uint64_t quotes; };
static Field structural_field(const CsvSpan &s, bool hash) {
    Field f{};
    size_t length = s.end - s.start;
    f.quoted = length && *s.start == '"';
    f.raw = s.start + f.quoted;
    if (f.quoted && (length < 2 || s.end[-1] != '"' || s.quotes < 2 || (s.quotes & 1)))
        throw std::runtime_error("quoted field");
    f.raw_len = static_cast<uint32_t>(length - 2 * f.quoted);
    f.len = f.raw_len - (f.quoted ? static_cast<uint32_t>((s.quotes - 2) / 2) : 0);
    f.hash = hash ? raw_hash(f.raw, f.raw_len) : HASH_OFFSET;
    return f;
}
''' +s[b:]
a=s.index('static void parse_range('); b=s.index('\nint main()',a)
s=s[:a]+'''static void parse_range(const char *p, const char *end, const std::vector<uint8_t> &regions,
                        unsigned unmatched, Worker &w, SharedNames &shared) {
    struct Pending { uint64_t id; Field name; uint32_t note_len; int64_t amount; uint32_t group; };
    try {
        std::array<Pending, 32> batch{};
        unsigned count = 0, field = 0;
        std::array<CsvSpan, 5> fields{};
        const char *start = p, *reclaimed = p;
        uint64_t quote_total = 0, field_quotes = 0, carry = 0;
        auto flush = [&] {
            for (unsigned i = 0; i < count; ++i) {
                const Pending &r = batch[i];
                unsigned group = r.group < regions.size() && regions[r.group] != UINT8_MAX ? regions[r.group] : unmatched;
                w.groups[group].add(r.id, r.amount, r.name, r.note_len, shared.first(r.name, group));
            }
            count = 0;
        };
        auto emit = [&](const char *delimiter, uint64_t cumulative) {
            if (delimiter < end && *delimiter != (field == 4 ? '\\n' : ','))
                throw std::runtime_error("field delimiter");
            fields[field++] = CsvSpan{start, delimiter, cumulative - field_quotes};
            field_quotes = cumulative; start = delimiter + (delimiter < end);
            if (field != 5) return;
            const char *q = fields[0].start; bom(q, fields[0].end);
            uint64_t id = number(q, fields[0].end);
            if (q != fields[0].end) throw std::runtime_error("id suffix");
            Field name = structural_field(fields[1], true);
            uint32_t spread = SharedNames::spread(name.hash);
            auto &shard = shared.shards[spread & 127];
            __builtin_prefetch(&shard.index[(spread >> 7) & (SharedNames::slots_per_shard - 1)], 0, 1);
            Field note = structural_field(fields[2], false);
            q = fields[3].start;
            uint64_t region = number(q, fields[3].end);
            if (q != fields[3].end) throw std::runtime_error("region suffix");
            uint32_t key = region < regions.size() ? static_cast<uint32_t>(region) : UINT32_MAX;
            if (key != UINT32_MAX) __builtin_prefetch(&regions[key], 0, 1);
            q = fields[4].start;
            const char *amount_end = fields[4].end;
            if (q < amount_end && amount_end[-1] == '\\r') --amount_end;
            int64_t amount = cents(q, amount_end);
            if (q != amount_end) throw std::runtime_error("amount suffix");
            batch[count++] = Pending{id, name, note.len, amount, key}; field = 0;
            if (count == batch.size()) {
                flush();
                if (start - reclaimed >= (1 << 20)) discard_consumed(reclaimed, start);
            }
        };
        const __m256i quote = _mm256_set1_epi8('"'), comma_v = _mm256_set1_epi8(','), lf = _mm256_set1_epi8('\\n');
        while (p < end) {
            alignas(32) char tail[64]{};
            const char *block = p;
            size_t bytes = std::min<size_t>(64, end - p);
            if (bytes < 64) { std::memcpy(tail, p, bytes); block = tail; }
            __m256i lo = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(block));
            __m256i hi = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(block + 32));
            auto mask = [&](const __m256i c) {
                return static_cast<uint64_t>(static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(lo, c)))) |
                       (static_cast<uint64_t>(static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(hi, c)))) << 32);
            };
            uint64_t quotes = mask(quote), inside = quotes;
            inside ^= inside << 1; inside ^= inside << 2; inside ^= inside << 4;
            inside ^= inside << 8; inside ^= inside << 16; inside ^= inside << 32;
            inside ^= carry; carry = 0ULL - (inside >> 63);
            uint64_t separators = (mask(comma_v) | mask(lf)) & ~inside;
            while (separators) {
                unsigned bit = __builtin_ctzll(separators);
                uint64_t cumulative = quote_total + __builtin_popcountll(quotes & (UINT64_MAX >> (63 - bit)));
                emit(p + bit, cumulative); separators &= separators - 1;
            }
            quote_total += __builtin_popcountll(quotes); p += bytes;
        }
        if (carry) throw std::runtime_error("unclosed quote");
        if (start < end || field) {
            if (field != 4) throw std::runtime_error("short record");
            emit(end, quote_total);
        }
        flush(); discard_consumed(reclaimed, end);
    } catch (...) { w.error = std::current_exception(); }
}
''' +s[b:]
p.write_text(s)
