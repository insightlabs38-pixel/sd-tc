#include <condition_variable>
#include <string_view>
#include <sched.h>
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

class ExactNames {
public:
    static constexpr unsigned capacity=1<<17,words=capacity/64;
    static constexpr uint32_t missing=UINT32_MAX;
private:
    struct Overflow {
        std::mutex mutex;
        std::unordered_map<std::string_view,std::array<uint64_t,4>> names;
    };
    std::array<Overflow,128> overflow;
    std::atomic<uint64_t> *keys,*bits;
    const char **canonical;
    struct Masks { unsigned equal,empty,busy; };
    static uint64_t fingerprint(const Field &name) {
        uint64_t key=(uint64_t(name.raw_len)<<32)|name.hash;
        return key<2?key+2:key;
    }
    static unsigned first(const Field &name) { return (name.hash^(name.hash>>16))&(capacity-4); }
    Masks inspect(unsigned at,uint64_t key) const {
        uint64_t a=keys[at].load(std::memory_order_acquire),b=keys[at+1].load(std::memory_order_acquire);
        uint64_t c=keys[at+2].load(std::memory_order_acquire),d=keys[at+3].load(std::memory_order_acquire);
        auto block=_mm256_set_epi64x(d,c,b,a);
        auto mask=[](__m256i x) { return unsigned(_mm256_movemask_pd(_mm256_castsi256_pd(x))); };
        return {mask(_mm256_cmpeq_epi64(block,_mm256_set1_epi64x(key))),
                mask(_mm256_cmpeq_epi64(block,_mm256_setzero_si256())),
                mask(_mm256_cmpeq_epi64(block,_mm256_set1_epi64x(1)))};
    }
    bool same(unsigned id,const Field &name,uint64_t key) const {
        (void)key; const char *saved=canonical[id];
        return saved==name.raw || !std::memcmp(saved,name.raw,name.raw_len);
    }
    bool claim(unsigned id,const Field &name,uint64_t key) {
        uint64_t expected=0;
        if (!keys[id].compare_exchange_strong(expected,1,std::memory_order_acq_rel)) return false;
        canonical[id]=name.raw; keys[id].store(key,std::memory_order_release);
        return true;
    }
    void prefetch_canonical(unsigned id,uint64_t key) const {
        (void)key; __builtin_prefetch(canonical[id],0,1);
    }
public:
    ExactNames() {
        alignas(4096) static std::atomic<uint64_t> key_storage[capacity]{};
        alignas(4096) static const char *canonical_storage[capacity]{};
        alignas(4096) static std::atomic<uint64_t> membership[256*words]{};
        keys=key_storage; canonical=canonical_storage; bits=membership;
    }
    void populate(unsigned task,unsigned tasks,unsigned groups) {
        for (unsigned page=task;page<capacity/512;page+=tasks) {
            keys[page*512].store(0,std::memory_order_relaxed); canonical[page*512]=nullptr;
        }
        for (unsigned page=task;page<groups*words/512;page+=tasks) bits[page*512].store(0,std::memory_order_relaxed);
    }
    void prefetch(const Field &name) const {
        unsigned at=first(name); __builtin_prefetch(&keys[at],0,1); __builtin_prefetch(&canonical[at],0,1);
    }
    uint32_t prepare(const Field &name) {
        uint64_t key=fingerprint(name); if (!key) return missing;
        unsigned at=first(name);
        for (unsigned probe=0;probe<16;++probe,at=(at+4)&(capacity-1)) {
            auto found=inspect(at,key);
            if (found.equal) {
                unsigned id=at+__builtin_ctz(found.equal); prefetch_canonical(id,key); return id;
            }
            if (found.busy) return missing;
            if (found.empty) {
                unsigned id=at+__builtin_ctz(found.empty);
                return claim(id,name,key)?id:missing;
            }
        }
        return missing;
    }
    uint32_t resolve(const Field &name,uint32_t hint) {
        uint64_t key=fingerprint(name); if (!key) return missing;
        if (hint<capacity && keys[hint].load(std::memory_order_acquire)==key && same(hint,name,key)) return hint;
        unsigned at=first(name),probe=0,spins=0;
        while (probe<16) {
            auto found=inspect(at,key);
            for (unsigned equal=found.equal;equal;equal&=equal-1) {
                unsigned id=at+__builtin_ctz(equal); if (same(id,name,key)) return id;
            }
            if (found.busy) {
                if (++spins%64) _mm_pause(); else std::this_thread::yield();
                continue;
            }
            if (found.empty) {
                unsigned id=at+__builtin_ctz(found.empty); if (claim(id,name,key)) return id;
                continue;
            }
            ++probe; at=(at+4)&(capacity-1);
        }
        return missing;
    }
    void prefetch_bit(unsigned id,unsigned group) const {
        if (id<capacity) __builtin_prefetch(&bits[group*words+(id>>6)],1,3);
    }
    void mark(unsigned id,unsigned group) {
        uint64_t bit=uint64_t(1)<<(id&63); auto &word=bits[group*words+(id>>6)];
        if (!(word.load(std::memory_order_relaxed)&bit)) word.fetch_or(bit,std::memory_order_relaxed);
    }
    bool overflow_first(const Field &name,unsigned group) {
        auto &shard=overflow[name.hash&127]; std::lock_guard<std::mutex> lock(shard.mutex);
        auto &seen=shard.names[std::string_view(name.raw,name.raw_len)];
        uint64_t bit=uint64_t(1)<<(group&63); bool fresh=!(seen[group>>6]&bit); seen[group>>6]|=bit;
        return fresh;
    }
    uint32_t count(unsigned group) const {
        uint32_t total=0;
        for (unsigned i=0;i<words;++i) total+=__builtin_popcountll(bits[group*words+i].load(std::memory_order_relaxed));
        return total;
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

static const char *fast_comma(const char *p,const char *end) {
    if (end-p>=8) {
        uint64_t value; std::memcpy(&value,p,8); value^=0x2c2c2c2c2c2c2c2cULL;
        uint64_t hits=(value-0x0101010101010101ULL)&~value&0x8080808080808080ULL;
        if (hits) return p+__builtin_ctzll(hits)/8;
        p+=8;
    }
    auto result=static_cast<const char *>(std::memchr(p,',',end-p));
    if (!result) throw std::runtime_error("missing comma");
    return result;
}
static uint64_t fast_decimal(const char *p,size_t n,const char *end) {
    if (!n || n>8) return number(p,end);
    uint64_t x; std::memcpy(&x,p,8); x=(x<<((8-n)*8))&0x0f0f0f0f0f0f0f0fULL;
    x=(x*10+(x>>8))&0x00ff00ff00ff00ffULL;
    x=(x*100+(x>>16))&0x0000ffff0000ffffULL;
    return (x*10000+(x>>32))&0xffffffffULL;
}
static int64_t fast_money(const char *p,const char *end) {
    bool negative=*p=='-'; const char *start=p; p+=negative;
    size_t n=end-p;
    if (n<4 || n>8) return cents(start,end);
    uint64_t word; std::memcpy(&word,p,8); unsigned dot=n-3;
    uint64_t lower=(uint64_t(1)<<(dot*8))-1;
    uint64_t x=(((word&lower)|((word>>8)&~lower))<<((9-n)*8))&0x0f0f0f0f0f0f0f0fULL;
    x=(x*10+(x>>8))&0x00ff00ff00ff00ffULL;
    x=(x*100+(x>>16))&0x0000ffff0000ffffULL;
    int64_t value=int64_t((x*10000+(x>>32))&0xffffffffULL);
    return negative?-value:value;
}
static Field fast_text(const char *&p,const char *end,uint32_t &length) {
    if (*p!='"') {
        const char *start=p,*finish=fast_comma(p,end);
        length=finish-start; p=finish+1; return {start,length,0};
    }
    const char *raw=++p,*block=raw;
    uint32_t escaped=0;
    for (;;) {
        if (block>=end) throw std::runtime_error("unclosed text");
        auto bytes=_mm256_loadu_si256(reinterpret_cast<const __m256i *>(block));
        unsigned mask=_mm256_movemask_epi8(_mm256_cmpeq_epi8(bytes,_mm256_set1_epi8('"')));
        while (mask) {
            unsigned bit=__builtin_ctz(mask);
            if (bit==31 && block+32<end && block[32]=='"') { ++escaped; block+=33; goto next_block; }
            if (bit<31 && ((mask>>(bit+1))&1)) { ++escaped; mask&=mask-1; mask&=mask-1; continue; }
            const char *close=block+bit;
            uint32_t raw_length=close-raw; length=raw_length-escaped; p=close+2;
            return {raw,raw_length,0};
        }
        block+=32;
        next_block:;
    }
}

struct CompactRow { uint64_t id; int64_t amount; Field name; uint32_t nlen,xlen; uint64_t region; };
class RecordPipe {
    static constexpr unsigned lag=16;
    struct Name { Field name; uint32_t group,hint; };
    struct Mark { Field name; uint32_t group,id; };
    std::array<CompactRow,lag> rows;
    std::array<Name,lag> names;
    std::array<Mark,lag> marks;
    uint64_t tick=0;
    ExactNames &registry;
    const std::vector<uint8_t> &regions;
    unsigned unmatched;
    Worker &worker;
    void advance(uint64_t last) {
        unsigned i=tick&(lag-1);
        if (tick>=3*lag && tick-3*lag<last) {
            auto &mark=marks[i];
            if (mark.id<ExactNames::capacity) registry.mark(mark.id,mark.group);
            else worker.groups[mark.group].distinct+=registry.overflow_first(mark.name,mark.group);
        }
        if (tick>=2*lag && tick-2*lag<last) {
            const auto &name=names[i]; uint32_t id=registry.resolve(name.name,name.hint);
            registry.prefetch_bit(id,name.group); marks[i]={name.name,name.group,id};
        }
        if (tick>=lag && tick-lag<last) {
            const auto &row=rows[i];
            unsigned group=row.region<regions.size() && regions[row.region]!=UINT8_MAX?regions[row.region]:unmatched;
            worker.groups[group].add(row.id,row.amount,row.nlen,row.xlen,false);
            names[i]={row.name,group,registry.prepare(row.name)};
        }
    }
public:
    RecordPipe(ExactNames &r,const std::vector<uint8_t> &lookup,unsigned unknown,Worker &w)
        :registry(r),regions(lookup),unmatched(unknown),worker(w) {}
    void push(const CompactRow &row) { advance(UINT64_MAX); rows[tick&(lag-1)]=row; ++tick; }
    void finish() { uint64_t last=tick; for (unsigned i=0;i<3*lag;++i,++tick) advance(last); }
};
struct Gate {
    std::atomic<unsigned> left;
    std::mutex mutex;
    std::condition_variable ready;
    explicit Gate(unsigned n):left(n) {}
    void done() {
        if (left.fetch_sub(1,std::memory_order_acq_rel)==1) { std::lock_guard<std::mutex> lock(mutex); ready.notify_all(); }
    }
    void wait() {
        if (!left.load(std::memory_order_acquire)) return;
        std::unique_lock<std::mutex> lock(mutex);
        ready.wait(lock,[&]{return !left.load(std::memory_order_acquire);});
    }
};
static unsigned allowed_threads() {
    cpu_set_t set;
    unsigned n=sched_getaffinity(0,sizeof(set),&set)?1:CPU_COUNT(&set);
    return std::clamp(n,1U,12U);
}
static void drop_pages(const File &file,unsigned task,unsigned tasks) {
    size_t page=size_t(sysconf(_SC_PAGESIZE)),pages=(file.size+page-1)/page;
    size_t first=pages*task/tasks,last=pages*(task+1)/tasks;
    if (last>first) (void)madvise(const_cast<char *>(file.data)+first*page,(last-first)*page,MADV_DONTNEED);
}

static void parse_range(const char *p,const char *end,const std::vector<uint8_t> &regions,
                        unsigned unmatched,Worker &worker,ExactNames &registry) {
    try {
        RecordPipe pipe(registry,regions,unmatched,worker);
        while (p<end) {
            _mm_prefetch(p+2048,_MM_HINT_T0);
            bom(p,end); const char *id_end=fast_comma(p,end);
            uint64_t id=fast_decimal(p,id_end-p,end); p=id_end+1;
            uint32_t nlen,xlen; Field name=fast_text(p,end,nlen);
            name.hash=raw_hash(name.raw,name.raw_len,end); registry.prefetch(name);
            (void)fast_text(p,end,xlen);
            auto tail=_mm256_loadu_si256(reinterpret_cast<const __m256i *>(p));
            unsigned cm=_mm256_movemask_epi8(_mm256_cmpeq_epi8(tail,_mm256_set1_epi8(',')));
            unsigned lf=_mm256_movemask_epi8(_mm256_cmpeq_epi8(tail,_mm256_set1_epi8('\n')));
            const char *region_end=cm?p+__builtin_ctz(cm):fast_comma(p,end);
            uint64_t region=fast_decimal(p,region_end-p,end);
            if (region<regions.size()) __builtin_prefetch(&regions[region],0,1);
            const char *eol=lf?p+__builtin_ctz(lf):static_cast<const char *>(std::memchr(p,'\n',end-p));
            if (!eol) eol=end;
            const char *amount_end=eol;
            if (amount_end>region_end && amount_end[-1]=='\r') --amount_end;
            int64_t amount=fast_money(region_end+1,amount_end);
            p=eol+(eol<end); pipe.push({id,amount,name,nlen,xlen,region});
        }
        pipe.finish();
    } catch (...) { worker.error=std::current_exception(); }
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
        unsigned threads = end - p < (1 << 20) ? 1 : 12;
        threads=std::min(threads,allowed_threads());
        unsigned pieces=threads*8,initial=threads*3;
        std::vector<const char *> cuts(pieces+1);
        cuts[0]=record_start; cuts[pieces]=end;
        for (unsigned i=1;i<pieces;++i) cuts[i]=next_boundary(record_start+(end-record_start)*i/pieces,end);
        ExactNames registry;
        std::vector<Worker> workers;
        for (unsigned t=0;t<threads;++t) workers.emplace_back(unmatched+1);
        Gate populated(initial),parsed(pieces);
        std::atomic<unsigned> next_init{0},next_parse{0},next_count{0},next_drop{0};
        std::atomic<bool> failed{false};
        std::vector<std::thread> jobs; jobs.reserve(threads);
        try {
            for (unsigned t=0;t<threads;++t) jobs.emplace_back([&,t] {
                for (;;) {
                    unsigned i=next_init.fetch_add(1,std::memory_order_relaxed); if (i>=initial) break;
                    registry.populate(i,initial,unmatched+1); populated.done();
                }
                populated.wait();
                for (;;) {
                    unsigned i=next_parse.fetch_add(1,std::memory_order_relaxed); if (i>=pieces) break;
                    if (!failed.load(std::memory_order_relaxed)) {
                        parse_range(cuts[i],cuts[i+1],regions,unmatched,workers[t],registry);
                        if (workers[t].error) failed.store(true,std::memory_order_relaxed);
                    }
                    parsed.done();
                }
                parsed.wait();
                for (;;) {
                    unsigned group=next_count.fetch_add(1,std::memory_order_relaxed); if (group>unmatched) break;
                    workers[t].groups[group].distinct+=registry.count(group);
                }
                for (;;) {
                    unsigned i=next_drop.fetch_add(1,std::memory_order_relaxed); if (i>=pieces) break;
                    drop_pages(records,i,pieces);
                }
            });
        } catch (...) {
            for (auto &job:jobs) job.join();
            throw;
        }
        for (auto &job:jobs) job.join();
        for (const auto &worker:workers) if (worker.error) std::rethrow_exception(worker.error);
        std::vector<Group> groups(unmatched + 1);
        for (const auto &worker:workers) for (unsigned g=0;g<=unmatched;++g) groups[g].distinct+=worker.groups[g].distinct;
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
