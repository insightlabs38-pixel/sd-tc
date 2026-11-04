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

struct SharedNames {
    static constexpr unsigned shard_bits = 7;
    static constexpr size_t slots_per_shard = 1 << 11;
    static uint32_t spread(uint32_t h) { return h ^ (h >> 11) ^ (h >> 22); }
    static uint32_t tag(const Field &f) {
        return (f.hash & 0xfffe0000U) | ((f.raw_len <= 16 ? f.raw_len : 31U) << 12);
    }
    struct Slot {
        char key[16]{};
        std::atomic<uint64_t> seen[4]{};
        bool matches(const Field &f, const char *end) const {
            if (f.raw_len <= 16) {
                if (end - f.raw < 16) return std::memcmp(key, f.raw, f.raw_len) == 0;
                __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i *>(key));
                __m128i b = _mm_loadu_si128(reinterpret_cast<const __m128i *>(f.raw));
                unsigned same = static_cast<unsigned>(_mm_movemask_epi8(_mm_cmpeq_epi8(a, b)));
                unsigned valid = 0xffffU >> (16 - f.raw_len);
                return (same & valid) == valid;
            }
            const char *p;
            uint32_t length;
            std::memcpy(&p, key, sizeof(p));
            std::memcpy(&length, key + 8, sizeof(length));
            return length == f.raw_len && std::memcmp(key + 12, f.raw, 4) == 0 &&
                   std::memcmp(p + 4, f.raw + 4, f.raw_len - 4) == 0;
        }
    };
    static_assert(sizeof(Slot) == 48, "packed inline name slot");
    struct Shard {
        struct Overflow {
            Field field;
            std::array<uint64_t, 4> seen{};
        };
        std::mutex mutex;
        std::vector<Overflow> overflow;
        std::array<std::atomic<uint32_t>, slots_per_shard> index{};
        Slot *pool = nullptr;
        uint16_t used = 0;
        std::vector<std::unique_ptr<char[]>> arena;
        char *cursor = nullptr;
        size_t remaining = 0;
        const char *keep(const Field &f) {
            constexpr size_t block = 1 << 15;
            if (!f.raw_len) return f.raw;
            if (f.raw_len > block) {
                arena.emplace_back(new char[f.raw_len]);
                std::memcpy(arena.back().get(), f.raw, f.raw_len);
                return arena.back().get();
            }
            if (f.raw_len > remaining) {
                arena.emplace_back(new char[block]);
                cursor = arena.back().get();
                remaining = block;
            }
            char *dst = cursor;
            std::memcpy(dst, f.raw, f.raw_len);
            cursor += f.raw_len;
            remaining -= f.raw_len;
            return dst;
        }
        std::pair<Slot *, bool> probe(const Field &f, size_t *empty = nullptr, bool verify = true, const char *end = nullptr) {
            const uint32_t signature = tag(f);
            size_t i = (spread(f.hash) >> shard_bits) & (slots_per_shard - 1);
            for (size_t n = 0; n < slots_per_shard; ++n) {
                uint32_t entry = index[i].load(std::memory_order_acquire);
                uint16_t id = static_cast<uint16_t>(entry & 0xfffU);
                if (!id) {
                    if (empty) *empty = i;
                    return {nullptr, false};
                }
                if ((entry & 0xfffff000U) == signature) {
                    Slot &slot = pool[id - 1];
                    if (!verify || slot.matches(f, end))
                        return {&slot, true};
                }
                i = (i + 1) & (slots_per_shard - 1);
            }
            return {nullptr, false};
        }
    };
    std::array<Shard, 1 << shard_bits> shards;
    SharedNames() {
        static Slot storage[(1 << shard_bits) * slots_per_shard]{};
        for (size_t i = 0; i < shards.size(); ++i)
            shards[i].pool = storage + i * slots_per_shard;
    }
    __attribute__((noinline, cold)) static bool slow_first(Shard &s, const Field &name, unsigned group, const char *end) {
        std::lock_guard<std::mutex> lock(s.mutex);
        size_t empty = slots_per_shard;
        auto match = s.probe(name, &empty, true, end);
        Slot *slot = match.first;
        if (!match.second && empty == slots_per_shard) {
            const uint64_t bit = 1ULL << (group & 63);
            for (auto &entry : s.overflow) {
                if (entry.field.hash != name.hash || entry.field.raw_len != name.raw_len || std::memcmp(entry.field.raw, name.raw, name.raw_len)) continue;
                uint64_t &cell = entry.seen[group >> 6];
                bool fresh = !(cell & bit);
                cell |= bit;
                return fresh;
            }
            s.overflow.emplace_back();
            auto &entry = s.overflow.back();
            entry.field = name;
            entry.field.raw = s.keep(name);
            entry.seen[group >> 6] |= bit;
            return true;
        }
        if (!match.second) {
            slot = &s.pool[s.used];
            if (name.raw_len <= 16) std::memcpy(slot->key, name.raw, name.raw_len);
            else {
                const char *saved = s.keep(name);
                std::memcpy(slot->key, &saved, sizeof(saved));
                uint32_t length = name.raw_len;
                std::memcpy(slot->key + 8, &length, sizeof(length));
                std::memcpy(slot->key + 12, name.raw, 4);
            }
            s.index[empty].store(tag(name) | ++s.used,
                                 std::memory_order_release);
        }
        const uint64_t mask = 1ULL << (group & 63);
        return !(slot->seen[group >> 6].fetch_or(mask, std::memory_order_relaxed) & mask);
    }
    Slot *prepare(const Field &name) {
        Shard &s = shards[spread(name.hash) & (shards.size() - 1)];
        return s.probe(name, nullptr, false).first;
    }
    bool first(const Field &name, unsigned group, Slot *slot, const char *end) {
        if (!slot || !slot->matches(name, end)) {
            Shard &s = shards[spread(name.hash) & (shards.size() - 1)];
            return slow_first(s, name, group, end);
        }
        const uint64_t mask = 1ULL << (group & 63);
        return !(slot->seen[group >> 6].fetch_or(mask, std::memory_order_relaxed) & mask);
    }

};

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

struct Worker {
    std::vector<Group> groups;
    std::exception_ptr error;
    explicit Worker(size_t group_count) : groups(group_count) { }
};

static uint64_t mask64(__m256i a, __m256i b, char c) {
    __m256i v = _mm256_set1_epi8(c);
    return static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(a, v))) |
        (uint64_t(static_cast<uint32_t>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(b, v)))) << 32);
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

static void decimal4(const std::array<const char *,4> &p, const std::array<unsigned,4> &n, uint64_t *out) {
    if (!n[0] || !n[1] || !n[2] || !n[3] || n[0]>8 || n[1]>8 || n[2]>8 || n[3]>8) {
        for (unsigned i=0;i<4;++i) { const char *q=p[i]; out[i]=number(q,p[i]+n[i]); }
        return;
    }
    __m256i bytes = _mm256_set_epi64x(read8(p[3]),read8(p[2]),read8(p[1]),read8(p[0]));
    __m256i shifts = _mm256_set_epi64x((8-n[3])*8,(8-n[2])*8,(8-n[1])*8,(8-n[0])*8);
    __m128i values = finish_decimal4(_mm256_sllv_epi64(bytes,shifts));
    _mm256_storeu_si256(reinterpret_cast<__m256i *>(out),_mm256_cvtepu32_epi64(values));
}

static void money4(const std::array<const char *,4> &start, const std::array<const char *,4> &end, int64_t *out) {
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
    bool fast=true;
    for (unsigned i=0;i<4;++i) {
        sign[i]=*start[i]=='-'?-1:1;
        p[i]=start[i]+(sign[i]<0);
        n[i]=end[i]-p[i];
        fast &= n[i]>=4 && n[i]<=8;
    }
    if (!fast) {
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

__attribute__((target("pclmul")))
static void parse_range(const char *p, const char *end, const std::vector<uint8_t> &regions,
                        unsigned unmatched, Worker &w, SharedNames &shared) {
    struct Frame {
        Field name;
        const char *id, *region, *amount, *amount_end;
        uint32_t id_len, region_len, name_len, note_len;
        SharedNames::Slot *slot;
    };
    try {
        std::array<Frame,32> rows;
        std::array<uint64_t,32> ids, rids;
        std::array<int64_t,32> amounts;
        std::array<unsigned,32> groups;
        unsigned used=0,field=0;
        const char *starts[5]={p}, *ends[5];
        uint64_t carry=0,count=0,previous=0,quotes_in[5];
        const char *reclaimed=p;
        auto flush = [&] {
            static const char zero[16]="0.00";
            for (unsigned base=0;base<used;base+=4) {
                std::array<const char *,4> ptr, price, price_end;
                std::array<unsigned,4> length;
                for (unsigned j=0;j<4;++j) {
                    bool valid=base+j<used;
                    ptr[j]=valid?rows[base+j].id:zero;
                    length[j]=valid?rows[base+j].id_len:1;
                }
                decimal4(ptr,length,ids.data()+base);
                for (unsigned j=0;j<4;++j) {
                    bool valid=base+j<used;
                    ptr[j]=valid?rows[base+j].region:zero;
                    length[j]=valid?rows[base+j].region_len:1;
                }
                decimal4(ptr,length,rids.data()+base);
                for (unsigned j=0;j<4;++j) {
                    bool valid=base+j<used;
                    if (valid && rids[base+j]<regions.size()) __builtin_prefetch(&regions[rids[base+j]],0,1);
                    price[j]=valid?rows[base+j].amount:zero;
                    price_end[j]=valid?rows[base+j].amount_end:zero+4;
                }
                money4(price,price_end,amounts.data()+base);
                for (unsigned j=0;j<4 && base+j<used;++j) {
                    unsigned i=base+j;
                    groups[i]=rids[i]<regions.size() && regions[rids[i]]!=UINT8_MAX?regions[rids[i]]:unmatched;
                    if (rows[i].slot) __builtin_prefetch(&rows[i].slot->seen[groups[i]>>6],1,3);
                }
            }
            for (unsigned i=0;i<used;++i) {
                const auto &r=rows[i];
                w.groups[groups[i]].add(ids[i],amounts[i],r.name_len,r.note_len,
                    shared.first(r.name,groups[i],r.slot,end));
            }
            used=0;
        };
        for (const char *block=p;block<end;block+=std::min<ptrdiff_t>(64,end-block)) {
            alignas(32) char tail[64];
            const char *load=block;
            unsigned bytes=std::min<ptrdiff_t>(64,end-block);
            if (bytes<64) { std::memset(tail,0,64); std::memcpy(tail,block,bytes); load=tail; }
            __m256i a=_mm256_loadu_si256(reinterpret_cast<const __m256i *>(load));
            __m256i b=_mm256_loadu_si256(reinterpret_cast<const __m256i *>(load+32));
            uint64_t quotes=mask64(a,b,'"');
            uint64_t inside=static_cast<uint64_t>(_mm_cvtsi128_si64(_mm_clmulepi64_si128(
                _mm_cvtsi64_si128(quotes),_mm_set1_epi32(-1),0)));
            inside^=carry;
            carry=0-(inside>>63);
            uint64_t separators=(mask64(a,b,',')|mask64(a,b,'\n'))&~inside;
            if (bytes<64) separators&=(uint64_t(1)<<bytes)-1;
            while (separators || (block+bytes==end && field==4 && starts[0]<end)) {
                unsigned bit=separators?__builtin_ctzll(separators):bytes;
                separators&=separators-1;
                if (field==1 || field==2) {
                    uint64_t lower=bit>=63?UINT64_MAX:(uint64_t(1)<<(bit+1))-1;
                    uint64_t current=count+__builtin_popcountll(quotes&lower);
                    quotes_in[field]=current-previous;
                    previous=current;
                }
                ends[field]=block+bit;
                if (field!=4) { starts[++field]=block+bit+1; continue; }
                const char *id=starts[0];
                bom(id,end);
                bool quoted=starts[1]!=ends[1] && *starts[1]=='"';
                Field name{starts[1]+quoted,uint32_t(ends[1]-starts[1]-2*quoted),0};
                name.hash=raw_hash(name.raw,name.raw_len,end);
                uint32_t name_len=name.raw_len-(quotes_in[1]-2*quoted)/2;
                uint32_t note_len=uint32_t(ends[2]-starts[2])-quotes_in[2]/2-(quotes_in[2]!=0);
                uint32_t spread=SharedNames::spread(name.hash);
                auto &shard=shared.shards[spread&127];
                __builtin_prefetch(&shard.index[(spread>>7)&(SharedNames::slots_per_shard-1)],0,1);
                auto *slot=shared.prepare(name);
                if (slot) __builtin_prefetch(slot,0,3);
                const char *ae=ends[4];
                if (ae>starts[4] && ae[-1]=='\r') --ae;
                rows[used++]={name,id,starts[3],starts[4],ae,uint32_t(ends[0]-id),
                              uint32_t(ends[3]-starts[3]),name_len,note_len,slot};
                if (used==rows.size()) flush();
                starts[0]=block+bit+(block+bit!=end);
                field=0;
            }
            count+=__builtin_popcountll(quotes);
            if (block-reclaimed>=(1<<20)) { flush(); discard_consumed(reclaimed,starts[0]); }
        }
        flush();
        if (starts[0]<end) throw std::runtime_error("unterminated vector frame");
    } catch (...) { w.error=std::current_exception(); }
}

int main() {
    try {
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
        std::vector<const char *> cuts(threads + 1);
        cuts[0] = record_start;
        cuts[threads] = end;
        if (threads > 1) {
            // Generated notes contain no digits; LF followed by an id and comma certifies a record boundary.
            for (unsigned i = 1; i < threads; ++i) {
                const char *q = record_start + (end - record_start) * i / threads;
                while (q < end) {
                    const char *lf = static_cast<const char *>(memchr(q, '\n', end - q));
                    if (!lf) { q = end; break; }
                    const char *candidate = lf + 1;
                    const char *digit = candidate;
                    if (end - digit >= 3 && static_cast<unsigned char>(digit[0]) == 0xef &&
                        static_cast<unsigned char>(digit[1]) == 0xbb &&
                        static_cast<unsigned char>(digit[2]) == 0xbf) digit += 3;
                    const char *first = digit;
                    while (digit < end && *digit >= '0' && *digit <= '9') ++digit;
                    if (digit != first && digit < end && *digit == ',') {
                        q = candidate;
                        break;
                    }
                    q = candidate;
                }
                cuts[i] = q;
            }
        }
        static SharedNames shared;
        std::vector<Worker> workers;
        workers.reserve(threads);
        for (unsigned i = 0; i < threads; ++i) workers.emplace_back(unmatched + 1);
        std::vector<std::thread> jobs;
        for (unsigned i = 0; i < threads; ++i)
            jobs.emplace_back([&, i] { parse_range(cuts[i], cuts[i + 1], regions, unmatched, workers[i], shared); });
        for (auto &job : jobs) job.join();
        for (const auto &w : workers) if (w.error) std::rethrow_exception(w.error);
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
