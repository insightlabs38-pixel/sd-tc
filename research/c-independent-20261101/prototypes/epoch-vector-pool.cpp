#include <functional>
#include <pthread.h>
#include <sys/prctl.h>
#include <immintrin.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

struct File {
    const char *data;
    size_t size, mapped;
    explicit File(const char *path) {
        int fd = open(path, O_RDONLY);
        if (fd < 0) throw std::runtime_error(std::string("open ") + path);
        struct stat st;
        if (fstat(fd, &st)) throw std::runtime_error("fstat");
        size = st.st_size;
        size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
        mapped = (size + page - 1) / page * page + page;
        data = static_cast<const char *>(mmap(nullptr, mapped, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        if (data == MAP_FAILED) { close(fd); throw std::runtime_error("reserve mmap"); }
        if (size && mmap(const_cast<char *>(data), size, PROT_READ, MAP_PRIVATE | MAP_FIXED, fd, 0) == MAP_FAILED) {
            munmap(const_cast<char *>(data), mapped); close(fd); throw std::runtime_error("file mmap");
        }
        close(fd);
    }
    ~File() { munmap(const_cast<char *>(data), mapped); }
};

static void discard_consumed(const char *&cursor, const char *end) {
    static const uintptr_t page = static_cast<uintptr_t>(sysconf(_SC_PAGESIZE));
    uintptr_t lo = (reinterpret_cast<uintptr_t>(cursor) + page - 1) & ~(page - 1);
    uintptr_t hi = reinterpret_cast<uintptr_t>(end) & ~(page - 1);
    if (hi > lo) {
        (void)madvise(reinterpret_cast<void *>(lo), hi - lo, MADV_DONTNEED);
        cursor = reinterpret_cast<const char *>(hi);
    }
}

struct Field {
    const char *raw = nullptr;
    uint32_t raw_len = 0;
    uint32_t hash = 0;
};
static_assert(sizeof(Field) == 16, "projected name metadata");

static constexpr uint32_t HASH_OFFSET = 5381U;

static uint32_t raw_hash(const char *p, size_t n, const char *end) {
    if (n <= 16) {
        alignas(16) static const unsigned char mask[32] = {
            255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
        __m128i key;
        if (end - p >= 16) {
            key = _mm_loadu_si128(reinterpret_cast<const __m128i *>(p));
        } else {
            alignas(16) unsigned char tail[16] = {};
            std::memcpy(tail, p, n);
            key = _mm_load_si128(reinterpret_cast<const __m128i *>(tail));
        }
        key = _mm_and_si128(key, _mm_loadu_si128(
            reinterpret_cast<const __m128i *>(mask + 16 - n)));
        uint32_t h = static_cast<uint32_t>(_mm_crc32_u64(HASH_OFFSET, static_cast<uint64_t>(_mm_cvtsi128_si64(key))));
        return static_cast<uint32_t>(_mm_crc32_u64(h, static_cast<uint64_t>(_mm_extract_epi64(key, 1))));
    }
    uint32_t h = HASH_OFFSET;
    while (n >= 8) {
        uint64_t word;
        std::memcpy(&word, p, 8);
        h = static_cast<uint32_t>(_mm_crc32_u64(h, word));
        p += 8;
        n -= 8;
    }
    if (n & 4) { uint32_t w; std::memcpy(&w, p, 4); h = _mm_crc32_u32(h, w); p += 4; }
    if (n & 2) { uint16_t w; std::memcpy(&w, p, 2); h = _mm_crc32_u16(h, w); p += 2; }
    if (n & 1) h = _mm_crc32_u8(h, static_cast<unsigned char>(*p));
    return h;
}

static uint64_t number(const char *&p, const char *end) {
    if (end - p >= 8) { // SWAR ASCII fast path, with scalar fallback for long integers.
        uint64_t w;
        std::memcpy(&w, p, 8);
        const uint64_t bad = ((w + 0x4646464646464646ULL) |
                              (w - 0x3030303030303030ULL)) & 0x8080808080808080ULL;
        const unsigned digits = bad ? __builtin_ctzll(bad) / 8 : 8;
        if (digits && (digits < 8 || p + 8 == end ||
                       static_cast<unsigned>(static_cast<unsigned char>(p[8])) - '0' > 9)) {
            uint64_t x = (w << ((8 - digits) * 8)) |
                         (digits == 8 ? 0 : 0x3030303030303030ULL >> (digits * 8));
            x -= 0x3030303030303030ULL;
            x = (x * 10 + (x >> 8)) & 0x00ff00ff00ff00ffULL;
            x = (x * 100 + (x >> 16)) & 0x0000ffff0000ffffULL;
            x = (x * 10000 + (x >> 32)) & 0xffffffffULL;
            p += digits;
            return x;
        }
    }
    const char *start = p;
    uint64_t n = 0;
    while (p < end) {
        unsigned digit = static_cast<unsigned char>(*p) - '0';
        if (digit > 9) break;
        n = n * 10 + digit;
        ++p;
    }
    if (p == start) throw std::runtime_error("missing integer");
    return n;
}
static int64_t cents(const char *&p, const char *end) {
    bool neg = p < end && *p == '-';
    if (neg) ++p;
    if (end - p >= 8) {
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
    }
    int64_t n = static_cast<int64_t>(number(p, end));
    if (p + 2 >= end || *p++ != '.') throw std::runtime_error("bad decimal");
    int64_t fraction = (p[0] - '0') * 10 + (p[1] - '0');
    p += 2;
    return neg ? -(n * 100 + fraction) : n * 100 + fraction;
}
static void comma(const char *&p, const char *end) {
    if (p == end || *p++ != ',') throw std::runtime_error("missing comma");
}
static void newline(const char *&p, const char *end) {
    p += (p < end && *p == '\r');
    if (p < end && *p++ != '\n') throw std::runtime_error("missing LF");
}
static void bom(const char *&p, const char *end) {
    if (end - p >= 3 && static_cast<unsigned char>(p[0]) == 0xef &&
        static_cast<unsigned char>(p[1]) == 0xbb && static_cast<unsigned char>(p[2]) == 0xbf) p += 3;
}

struct Group {
    uint64_t count = 0, id_sum = 0, name_bytes = 0, note_bytes = 0;
    int64_t sum = 0, min = 0, max = 0;
    uint32_t distinct = 0;
    void add(uint64_t id, int64_t amount, uint32_t name_len, uint32_t note_len, bool first) {
        if (!count || amount < min) min = amount;
        if (!count || amount > max) max = amount;
        ++count;
        id_sum += id;
        sum += amount;
        name_bytes += name_len;
        note_bytes += note_len;
        distinct += first;
    }
};
static std::string money(int64_t n) {
    bool negative = n < 0;
    uint64_t mag = negative ? static_cast<uint64_t>(-(n + 1)) + 1 : static_cast<uint64_t>(n);
    std::string s = (negative ? "-" : "") + std::to_string(mag / 100) + ".";
    if (mag % 100 < 10) s += '0';
    return s + std::to_string(mag % 100);
}

struct KeyArena {
    std::vector<std::unique_ptr<char[]>> arena;
    char *cursor = nullptr;
    size_t remaining = 0;
    const char *keep(const Field &f) {
        if (remaining < f.raw_len) {
            remaining = std::max<size_t>(32768, f.raw_len);
            arena.emplace_back(new char[remaining]);
            cursor = arena.back().get();
        }
        char *out = cursor;
        std::memcpy(out, f.raw, f.raw_len);
        cursor += f.raw_len;
        remaining -= f.raw_len;
        return out;
    }
};

struct PrivateNames {
    struct Slot {
        char key[16]{};
        uint32_t length, hash;
        std::array<uint64_t, 4> seen{};
        Field field() const {
            const char *p = key;
            if (length > 16) std::memcpy(&p, key, sizeof(p));
            return {p, length, hash};
        }
        bool matches(const Field &f) const {
            if (hash != f.hash || length != f.raw_len) return false;
            auto view = field();
            return !std::memcmp(view.raw, f.raw, length);
        }
    };
    std::vector<uint32_t> index = std::vector<uint32_t>(1 << 10);
    std::vector<Slot> slots;
    KeyArena *arena;
    explicit PrivateNames(KeyArena *a):arena(a) { slots.reserve(512); }
    static uint32_t hash(uint32_t h) { return h * 0x9e3779b1U; }
    void grow() {
        index.assign(index.size() * 2, 0);
        for (uint32_t n = 0; n < slots.size(); ++n) {
            size_t i = hash(slots[n].hash) & (index.size() - 1);
            while (index[i]) i = (i + 1) & (index.size() - 1);
            index[i] = n + 1;
        }
    }
    Slot &get(const Field &f) {
        if ((slots.size() + 1) * 4 >= index.size() * 3) grow();
        size_t i = hash(f.hash) & (index.size() - 1);
        while (index[i]) {
            Slot &s = slots[index[i] - 1];
            if (s.matches(f)) return s;
            i = (i + 1) & (index.size() - 1);
        }
        uint32_t id = slots.size();
        slots.emplace_back();
        auto &s = slots.back();
        s.length = f.raw_len;
        s.hash = f.hash;
        if (f.raw_len <= 16) std::memcpy(s.key, f.raw, f.raw_len);
        else { const char *p = arena->keep(f); std::memcpy(s.key, &p, sizeof(p)); }
        index[i] = id + 1;
        return s;
    }
    void prefetch(const Field &f) const {
        size_t i = hash(f.hash) & (index.size() - 1);
        __builtin_prefetch(&index[i], 0, 1);
    }
    uint32_t prepare(const Field &f) const {
        uint32_t id = index[hash(f.hash) & (index.size() - 1)];
        if (id) __builtin_prefetch(&slots[id - 1], 1, 3);
        return id;
    }
    void mark_prepared(const Field &f, unsigned group, uint32_t id) {
        Slot *slot = id ? &slots[id - 1] : nullptr;
        if (!slot || !slot->matches(f)) slot = &get(f);
        slot->seen[group >> 6] |= uint64_t(1) << (group & 63);
    }
    void mark(const Field &f, unsigned group) {
        get(f).seen[group >> 6] |= uint64_t(1) << (group & 63);
    }
};

struct Worker {
    struct Wide { Field name; unsigned group; };
    std::vector<Group> groups;
    std::array<std::vector<uint64_t>, 256> rows;
    std::array<std::vector<Wide>, 256> wide;
    const char *base = nullptr;
    std::exception_ptr error;
    explicit Worker(size_t n) : groups(n) {}
};

using Dictionaries = std::array<std::unique_ptr<PrivateNames>, 256>;

__attribute__((target("pclmul")))
static Field text_pair(const char *&p, const char *end, uint32_t &name_len,
                       uint32_t &note_len) {
    const char *start = p;
    Field name{};
    uint64_t quote_total = 0, name_quotes = 0, carry = 0;
    bool have_name = false;
    const __m256i quote = _mm256_set1_epi8('"'), comma = _mm256_set1_epi8(',');
    for (const char *block = p; block < end; block += std::min<ptrdiff_t>(64, end - block)) {
        unsigned bytes = std::min<ptrdiff_t>(64, end - block);
        alignas(32) char tail[64]{};
        const char *load = block;
        if (bytes < 64) { std::memcpy(tail, block, bytes); load = tail; }
        __m256i a = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(load));
        __m256i b = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(load + 32));
        uint64_t quotes = static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(a, quote))) |
            (uint64_t(static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(b, quote)))) << 32);
        uint64_t commas = static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(a, comma))) |
            (uint64_t(static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(b, comma)))) << 32);
        uint64_t inside = static_cast<uint64_t>(_mm_cvtsi128_si64(_mm_clmulepi64_si128(
            _mm_cvtsi64_si128(quotes), _mm_set1_epi32(-1), 0)));
        inside ^= carry;
        carry = 0 - (inside >> 63);
        uint64_t separators = commas & ~inside;
        if (bytes < 64) separators &= (uint64_t(1) << bytes) - 1;
        while (separators) {
            unsigned bit = __builtin_ctzll(separators);
            uint64_t lower = bit == 63 ? UINT64_MAX : (uint64_t(1) << (bit + 1)) - 1;
            uint64_t total = quote_total + __builtin_popcountll(quotes & lower);
            const char *finish = block + bit;
            if (!have_name) {
                bool quoted = *start == '"';
                name = {start + quoted, static_cast<uint32_t>(finish - start - 2 * quoted), 0};
                name_len = static_cast<uint32_t>(finish - start) - total / 2 - quoted;
                name.hash = raw_hash(name.raw, name.raw_len, end);

                name_quotes = total;
                start = finish + 1;
                have_name = true;
            } else {
                bool quoted = *start == '"';
                note_len = static_cast<uint32_t>(finish - start) - (total - name_quotes) / 2 - quoted;
                p = finish + 1;
                return name;
            }
            separators &= separators - 1;
        }
        quote_total += __builtin_popcountll(quotes);
    }
    throw std::runtime_error("missing text delimiters");
}

static __m128i finish_decimal4(__m256i bytes) {
    bytes = _mm256_and_si256(bytes, _mm256_set1_epi8(15));
    __m256i pairs = _mm256_maddubs_epi16(bytes, _mm256_set1_epi16(0x010a));
    __m256i quads = _mm256_madd_epi16(pairs, _mm256_set1_epi32(0x00010064));
    __m256i weighted = _mm256_mullo_epi32(quads, _mm256_setr_epi32(10000,1,10000,1,10000,1,10000,1));
    __m256i sums = _mm256_hadd_epi32(weighted, weighted);
    return _mm256_castsi256_si128(_mm256_permute4x64_epi64(sums, _MM_SHUFFLE(3,1,2,0)));
}

static uint64_t read8(const char *p) {
    uint64_t word;
    std::memcpy(&word, p, 8);
    return word;
}

static void decimal4(const char *const *p, const unsigned *n, uint64_t *out) {
    __m128i lengths=_mm_loadu_si128(reinterpret_cast<const __m128i *>(n));
    __m128i valid=_mm_and_si128(_mm_cmpgt_epi32(lengths,_mm_setzero_si128()),
                                _mm_cmpgt_epi32(_mm_set1_epi32(9),lengths));
    if (_mm_movemask_epi8(valid)!=0xffff) {
        for (unsigned i=0;i<4;++i) { const char *q=p[i]; out[i]=number(q,p[i]+n[i]); }
        return;
    }
    __m256i bytes = _mm256_set_epi64x(read8(p[3]),read8(p[2]),read8(p[1]),read8(p[0]));
    __m256i shifts=_mm256_slli_epi64(_mm256_cvtepu32_epi64(_mm_sub_epi32(_mm_set1_epi32(8),lengths)),3);
    __m128i values = finish_decimal4(_mm256_sllv_epi64(bytes,shifts));
    _mm256_storeu_si256(reinterpret_cast<__m256i *>(out),_mm256_cvtepu32_epi64(values));
}

static void money4(const char *const *start, const char *const *end, int64_t *out) {
    static const std::array<uint64_t,9> masks = [] {
        std::array<uint64_t,9> result{};
        for (unsigned n=4;n<=8;++n) for (unsigned i=0;i<8;++i) {
            int digit=int(i)-int(9-n);
            unsigned source=digit<0?128U:unsigned(digit)+(unsigned(digit)>=n-3);
            result[n] |= uint64_t(source)<<(8*i);
        }
        return result;
    }();
    std::array<const char *,4> p;
    std::array<unsigned,4> n;
    std::array<int,4> sign;
    for (unsigned i=0;i<4;++i) {
        sign[i]=*start[i]=='-'?-1:1;
        p[i]=start[i]+(sign[i]<0);
        n[i]=end[i]-p[i];
    }
    __m128i lengths=_mm_loadu_si128(reinterpret_cast<const __m128i *>(n.data()));
    __m128i valid=_mm_and_si128(_mm_cmpgt_epi32(lengths,_mm_set1_epi32(3)),
                                _mm_cmpgt_epi32(_mm_set1_epi32(9),lengths));
    if (_mm_movemask_epi8(valid)!=0xffff) {
        for (unsigned i=0;i<4;++i) { const char *q=start[i]; out[i]=cents(q,end[i]); }
        return;
    }
    __m256i bytes = _mm256_set_epi64x(read8(p[3]),read8(p[2]),read8(p[1]),read8(p[0]));
    __m256i shuffle = _mm256_set_epi64x(masks[n[3]]+0x0808080808080808ULL,masks[n[2]],
                                       masks[n[1]]+0x0808080808080808ULL,masks[n[0]]);
    __m128i values=finish_decimal4(_mm256_shuffle_epi8(bytes,shuffle));
    values=_mm_sign_epi32(values,_mm_set_epi32(sign[3],sign[2],sign[1],sign[0]));
    _mm256_storeu_si256(reinterpret_cast<__m256i *>(out),_mm256_cvtepi32_epi64(values));
}

static const char *numeric_delimiter(const char *p, const char *end, char delimiter) {
    __m128i target=_mm_set1_epi8(delimiter);
    while (end-p>=16) {
        __m128i bytes=_mm_loadu_si128(reinterpret_cast<const __m128i *>(p));
        unsigned mask=static_cast<unsigned>(_mm_movemask_epi8(_mm_cmpeq_epi8(bytes,target)));
        if (mask) return p+__builtin_ctz(mask);
        p+=16;
    }
    for (;p<end;++p) if (*p==delimiter) return p;
    return end;
}

__attribute__((target("pclmul")))
static void parse_epoch(const char *p, const char *end, const std::vector<uint8_t> &regions,
                        unsigned unmatched, Worker &w) {
    try {
        w.error=nullptr;
        w.base=p;
        for (auto &v:w.rows) v.clear();
        for (auto &v:w.wide) v.clear();
        size_t hint=(end-p)/60/256+64;
        std::array<Field,32> names;
        std::array<const char *,32> idp,regp,price,price_end;
        std::array<unsigned,32> idlen,reglen,nlen,xlen,groups;
        std::array<uint64_t,32> ids,rids;
        std::array<int64_t,32> amounts;
        unsigned used=0;
        auto flush=[&] {
            static const char zero[16]="0.00";
            unsigned rounded=(used+3)&~3U;
            for (unsigned i=used;i<rounded;++i) {
                idp[i]=regp[i]=price[i]=zero; price_end[i]=zero+4;
                idlen[i]=reglen[i]=1;
            }
            for (unsigned base=0;base<rounded;base+=4) {
                decimal4(idp.data()+base,idlen.data()+base,ids.data()+base);
                decimal4(regp.data()+base,reglen.data()+base,rids.data()+base);
                for (unsigned j=0;j<4;++j) if (rids[base+j]<regions.size())
                    __builtin_prefetch(&regions[rids[base+j]],0,1);
                money4(price.data()+base,price_end.data()+base,amounts.data()+base);
                for (unsigned j=0;j<4;++j) {
                    unsigned i=base+j;
                    groups[i]=rids[i]<regions.size() && regions[rids[i]]!=UINT8_MAX?regions[rids[i]]:unmatched;
                }
            }
            for (unsigned i=0;i<used;++i) {
                auto name=names[i];
                unsigned group=groups[i];
                w.groups[group].add(ids[i],amounts[i],nlen[i],xlen[i],false);
                unsigned bucket=(name.hash>>16)&255;
                name.hash&=0xffffffU;
                size_t offset=name.raw-w.base;
                if (offset>0xffffff || name.raw_len>=255) w.wide[bucket].push_back({name,group});
                else {
                    auto &v=w.rows[bucket];
                    if (!v.capacity()) v.reserve(hint);
                    uint64_t metadata=(uint64_t(name.hash)<<8)|name.raw_len;
                    v.push_back((metadata<<32)|(offset<<8)|group);
                }
            }
            used=0;
        };
        while (p<end) {
            bom(p,end);
            idp[used]=p;
            const char *id_end=numeric_delimiter(p,end,',');
            if (id_end==end) throw std::runtime_error("id delimiter");
            idlen[used]=id_end-p;
            p=id_end+1;
            names[used]=text_pair(p,end,nlen[used],xlen[used]);
            regp[used]=p;
            const char *region_end=numeric_delimiter(p,end,',');
            if (region_end==end) throw std::runtime_error("region delimiter");
            reglen[used]=region_end-p;
            price[used]=region_end+1;
            const char *lf=numeric_delimiter(price[used],end,'\n');
            price_end[used]=lf;
            if (lf>price[used] && lf[-1]=='\r') --price_end[used];
            p=lf<end?lf+1:end;
            if (++used==names.size()) flush();
        }
        flush();
    } catch (...) { w.error=std::current_exception(); }
}

static void reconcile_epoch(unsigned owner, unsigned owners, const std::vector<Worker> &workers,
                             Dictionaries &dicts, std::vector<KeyArena> &arenas) {
    for (unsigned bucket = owner; bucket < 256; bucket += owners) {
        bool active = false;
        for (const auto &w : workers) active |= !w.rows[bucket].empty() || !w.wide[bucket].empty();
        if (!active) continue;
        if (!dicts[bucket]) dicts[bucket] = std::make_unique<PrivateNames>(&arenas[owner]);
        auto &names = *dicts[bucket];
        for (const auto &w : workers) {
            const auto &rows = w.rows[bucket];
            for (size_t start = 0; start < rows.size(); start += 64) {
                unsigned n = std::min<size_t>(64, rows.size() - start);
                std::array<Field, 64> fields;
                std::array<uint32_t, 64> prepared;
                for (unsigned i = 0; i < n; ++i) {
                    uint64_t r = rows[start + i];
                    fields[i] = {w.base + ((r >> 8) & 0xffffff), uint32_t(r >> 32) & 255, uint32_t(r >> 40)};
                    __builtin_prefetch(fields[i].raw, 0, 1);
                    if (fields[i].raw_len > 64) __builtin_prefetch(fields[i].raw + 64, 0, 1);
                }
                for (unsigned i = 0; i < n; ++i) prepared[i] = names.prepare(fields[i]);
                for (unsigned i = 0; i < n; ++i)
                    names.mark_prepared(fields[i], rows[start + i] & 255, prepared[i]);
            }
            for (const auto &r : w.wide[bucket]) names.mark(r.name, r.group);
        }
    }
}

static const char *next_boundary(const char *q, const char *end) {
    while (q < end) {
        const char *lf = static_cast<const char *>(std::memchr(q, '\n', end - q));
        if (!lf) return end;
        q = lf + 1;
        const char *digit = q;
        bom(digit, end);
        const char *first = digit;
        while (digit < end && *digit >= '0' && *digit <= '9') ++digit;
        if (digit != first && digit < end && *digit == ',') return q;
    }
    return end;
}

enum class EpochPhase { Parse, Reconcile, Count, Stop };

class EpochTeam {
    pthread_barrier_t begin, finish;
    std::vector<std::thread> jobs;
    std::function<void(unsigned,EpochPhase)> work;
    std::atomic<EpochPhase> phase{EpochPhase::Stop};
    std::atomic<unsigned> boot{0};
public:
    EpochTeam(unsigned n,std::function<void(unsigned,EpochPhase)> fn):work(std::move(fn)) {
        if (pthread_barrier_init(&begin,nullptr,n+1)) throw std::runtime_error("begin barrier");
        if (pthread_barrier_init(&finish,nullptr,n+1)) {
            pthread_barrier_destroy(&begin);
            throw std::runtime_error("finish barrier");
        }
        try {
            jobs.reserve(n);
            for (unsigned t=0;t<n;++t) jobs.emplace_back([&,t] {
                unsigned state;
                while (!(state=boot.load(std::memory_order_acquire))) std::this_thread::yield();
                if (state==2) return;
                for (;;) {
                    pthread_barrier_wait(&begin);
                    EpochPhase op=phase.load(std::memory_order_acquire);
                    if (op==EpochPhase::Stop) break;
                    work(t,op);
                    pthread_barrier_wait(&finish);
                }
            });
        } catch (...) {
            boot.store(2,std::memory_order_release);
            for (auto &job:jobs) job.join();
            pthread_barrier_destroy(&begin); pthread_barrier_destroy(&finish);
            throw;
        }
        boot.store(1,std::memory_order_release);
    }
    void run(EpochPhase op) {
        phase.store(op,std::memory_order_release);
        pthread_barrier_wait(&begin);
        pthread_barrier_wait(&finish);
    }
    ~EpochTeam() {
        phase.store(EpochPhase::Stop,std::memory_order_release);
        pthread_barrier_wait(&begin);
        for (auto &job:jobs) job.join();
        pthread_barrier_destroy(&begin); pthread_barrier_destroy(&finish);
    }
};

int main() {
    try {
        (void)prctl(PR_SET_THP_DISABLE, 1, 0, 0, 0);
        File lookup("/data/regions.csv");
        const char *p = lookup.data, *end = p + lookup.size;
        const char *header = "region_id,country\n";
        if (end - p < 18 || memcmp(p, header, 18)) throw std::runtime_error("regions header");
        p += 18;
        std::vector<uint8_t> regions(1000000, UINT8_MAX);
        std::array<int16_t, 17576> code_index;
        code_index.fill(-1);
        std::vector<std::string> country;
        while (p < end) {
            uint64_t id = number(p, end);
            comma(p, end);
            if (end - p < 3) throw std::runtime_error("short country");
            int code = ((p[0] - 'A') * 26 + p[1] - 'A') * 26 + p[2] - 'A';
            if (code < 0 || code >= 17576) throw std::runtime_error("bad country");
            if (code_index[code] == -1) {
                code_index[code] = static_cast<int16_t>(country.size());
                country.emplace_back(p, 3);
            }
            if (id >= regions.size()) throw std::runtime_error("lookup id out of range");
            regions[id] = static_cast<uint8_t>(code_index[code]);
            p += 3;
            newline(p, end);
        }
        if (country.size() > 255) throw std::runtime_error("too many countries");
        (void)madvise(const_cast<char *>(lookup.data), lookup.size, MADV_DONTNEED);
        const unsigned unmatched = static_cast<unsigned>(country.size());
        File records("/data/records.csv");
        p = records.data;
        end = p + records.size;
        bom(p, end);
        const char *rh = "id,name,note,region_id,amount";
        if (end - p < 29 || memcmp(p, rh, 29)) throw std::runtime_error("records header");
        p += 29;
        newline(p, end);
        const char *record_start = p;
        unsigned threads = end - p < (1 << 20) ? 1 : 6;
        std::vector<Worker> workers;
        workers.reserve(threads);
        for (unsigned t = 0; t < threads; ++t) workers.emplace_back(unmatched + 1);
        std::vector<KeyArena> arenas(threads);
        Dictionaries dictionaries;
        std::vector<const char *> cuts(threads+1);
        std::vector<std::array<uint32_t,256>> distinct(threads);
        EpochTeam team(threads,[&](unsigned t,EpochPhase op) {
            try {
                if (op==EpochPhase::Parse) parse_epoch(cuts[t],cuts[t+1],regions,unmatched,workers[t]);
                else if (op==EpochPhase::Reconcile) reconcile_epoch(t,threads,workers,dictionaries,arenas);
                else if (op==EpochPhase::Count) {
                    for (unsigned b=t;b<256;b+=threads) if (dictionaries[b])
                        for (const auto &slot:dictionaries[b]->slots) for (unsigned j=0;j<4;++j) {
                            uint64_t bits=slot.seen[j];
                            while (bits) { ++distinct[t][j*64+__builtin_ctzll(bits)]; bits&=bits-1; }
                        }
                }
            } catch (...) { workers[t].error=std::current_exception(); }
        });
        const char *reclaimed = record_start;
        for (const char *start = record_start; start < end;) {
            const char *target = start + std::min<ptrdiff_t>(16 << 20, end - start);
            const char *stop = target == end ? end : next_boundary(target, end);
            cuts[0] = start; cuts[threads] = stop;
            for (unsigned t = 1; t < threads; ++t)
                cuts[t] = next_boundary(start + (stop - start) * t / threads, stop);
            team.run(EpochPhase::Parse);
            for (const auto &w:workers) if (w.error) std::rethrow_exception(w.error);
            team.run(EpochPhase::Reconcile);
            for (const auto &w:workers) if (w.error) std::rethrow_exception(w.error);
            discard_consumed(reclaimed, stop);
            start = stop;
        }
        team.run(EpochPhase::Count);
        for (const auto &w:workers) if (w.error) std::rethrow_exception(w.error);
        std::vector<Group> groups(unmatched + 1);
        for (const Worker &w : workers) for (unsigned g = 0; g <= unmatched; ++g) {
            const Group &src = w.groups[g];
            if (!src.count) continue;
            Group &dst = groups[g];
            if (!dst.count || src.min < dst.min) dst.min = src.min;
            if (!dst.count || src.max > dst.max) dst.max = src.max;
            dst.count += src.count;
            dst.id_sum += src.id_sum;
            dst.sum += src.sum;
            dst.name_bytes += src.name_bytes;
            dst.note_bytes += src.note_bytes;
            dst.distinct += src.distinct;
        }
        for (const auto &row : distinct) for (unsigned g = 0; g <= unmatched; ++g) groups[g].distinct += row[g];
        std::vector<unsigned> order;
        for (unsigned i = 0; i < country.size(); ++i) if (groups[i].count) order.push_back(i);
        std::sort(order.begin(), order.end(), [&](unsigned a, unsigned b) { return country[a] < country[b]; });
        if (groups[unmatched].count) order.push_back(unmatched);
        FILE *out = fopen("/out/aggregates.txt", "wb");
        if (!out) throw std::runtime_error("open output");
        for (unsigned i : order) {
            const Group &g = groups[i];
            std::string line = (i == unmatched ? "_UNMATCHED" : country[i]) + " " +
                std::to_string(g.count) + " " + std::to_string(g.id_sum) + " " +
                money(g.sum) + " " + money(g.min) + " " + money(g.max) + " " +
                std::to_string(g.distinct) + " " + std::to_string(g.name_bytes) + " " +
                std::to_string(g.note_bytes) + "\n";
            if (fwrite(line.data(), 1, line.size(), out) != line.size()) throw std::runtime_error("write output");
        }
        if (fclose(out)) throw std::runtime_error("close output");
        return 0;
    } catch (const std::exception &e) {
        fprintf(stderr, "track-c: %s\n", e.what());
        return 1;
    }
}
