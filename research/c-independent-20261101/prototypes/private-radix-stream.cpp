#include <condition_variable>
#include <deque>
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
        uint32_t length=0,next=0;
        std::array<uint64_t,4> seen{};
        bool matches(const Field &name) const {
            if (length!=name.raw_len) return false;
            const char *raw=key;
            if (length>16) std::memcpy(&raw,key,sizeof(raw));
            return !std::memcmp(raw,name.raw,length);
        }
    };
    std::vector<uint16_t> small=std::vector<uint16_t>(4096);
    std::vector<uint32_t> large;
    std::deque<Slot> slots;
    KeyArena *arena;
    explicit PrivateNames(KeyArena *a):arena(a) {}
    uint32_t head(uint32_t hash) const { return large.empty()?small[hash&4095]:large[hash&4095]; }
    void set_head(uint32_t hash,uint32_t id) {
        if (id>UINT16_MAX && large.empty()) { large.assign(small.begin(),small.end()); std::vector<uint16_t>().swap(small); }
        if (large.empty()) small[hash&4095]=uint16_t(id);
        else large[hash&4095]=id;
    }
    Slot &get(const Field &name) {
        uint32_t first=head(name.hash);
        for (uint32_t id=first;id;id=slots[id-1].next)
            if (slots[id-1].matches(name)) return slots[id-1];
        slots.emplace_back(); auto &slot=slots.back();
        slot.length=name.raw_len; slot.next=first;
        if (name.raw_len<=16) std::memcpy(slot.key,name.raw,name.raw_len);
        else { const char *raw=arena->keep(name); std::memcpy(slot.key,&raw,sizeof(raw)); }
        set_head(name.hash,slots.size());
        return slot;
    }
    uint32_t prepare(const Field &name) const {
        uint32_t id=head(name.hash);
        if (id) __builtin_prefetch(&slots[id-1],1,3);
        return id;
    }
    bool mark_prepared(const Field &name,unsigned group,uint32_t id) {
        Slot *slot=id?&slots[id-1]:nullptr;
        if (!slot || !slot->matches(name)) slot=&get(name);
        auto &word=slot->seen[group>>6]; uint64_t bit=uint64_t(1)<<(group&63);
        bool fresh=!(word&bit); word|=bit; return fresh;
    }
};

struct Worker {
    std::vector<Group> groups;
    std::exception_ptr error;
    explicit Worker(size_t n):groups(n) {}
};

struct NamePacket { unsigned used=0; std::array<uint64_t,4096> values{}; };
class NamePipeline {
    struct alignas(64) Owner {
        KeyArena arena;
        std::array<std::unique_ptr<PrivateNames>,256> dictionaries;
        std::array<uint32_t,256> distinct{};
        std::mutex mutex;
        std::condition_variable ready,space;
        std::array<NamePacket,4> ring;
        unsigned read=0,write=0,count=0;
        bool closed=false;
    };
    std::vector<std::unique_ptr<Owner>> owners;
    std::vector<std::thread> jobs;
    std::atomic<bool> aborted{false};
    std::mutex error_mutex;
    std::exception_ptr error;
    void fail() {
        { std::lock_guard<std::mutex> lock(error_mutex); if (!error) error=std::current_exception(); }
        aborted.store(true,std::memory_order_release);
        for (auto &owner:owners) {
            std::lock_guard<std::mutex> lock(owner->mutex);
            owner->space.notify_all(); owner->ready.notify_all();
        }
    }
    void consume(Owner &owner,const NamePacket &packet) {
        std::array<uint16_t,256> counts{},next{};
        std::array<uint16_t,257> ranges{};
        std::array<uint16_t,4096> order;
        for (unsigned i=0;i<packet.used;) {
            uint64_t row=packet.values[i]; ++counts[row>>56];
            i+=((row>>36)&255)==255?3:1;
        }
        unsigned total=0;
        for (unsigned bucket=0;bucket<256;++bucket) {
            next[bucket]=ranges[bucket]=total; total+=counts[bucket];
        }
        ranges[256]=total;
        for (unsigned i=0;i<packet.used;) {
            uint64_t row=packet.values[i]; order[next[row>>56]++]=i;
            i+=((row>>36)&255)==255?3:1;
        }
        for (unsigned bucket=0;bucket<256;++bucket) {
            if (!counts[bucket]) continue;
            if (!owner.dictionaries[bucket]) owner.dictionaries[bucket]=std::make_unique<PrivateNames>(&owner.arena);
            auto &names=*owner.dictionaries[bucket];
            for (unsigned start=ranges[bucket];start<ranges[bucket+1];start+=64) {
                unsigned count=std::min<unsigned>(64,ranges[bucket+1]-start);
                std::array<Field,64> fields;
                std::array<uint32_t,64> prepared;
                std::array<unsigned,64> groups;
                for (unsigned i=0;i<count;++i) {
                    unsigned index=order[start+i]; uint64_t row=packet.values[index];
                    unsigned length=(row>>36)&255;
                    const char *raw=base+((row>>8)&0xfffffff);
                    if (length==255) {
                        raw=reinterpret_cast<const char *>(packet.values[index+1]);
                        length=uint32_t(packet.values[index+2]);
                    }
                    fields[i]={raw,length,uint32_t(row>>44)}; groups[i]=row&255;
                    __builtin_prefetch(raw,0,1);
                    if (length>64) __builtin_prefetch(raw+64,0,1);
                }
                for (unsigned i=0;i<count;++i) prepared[i]=names.prepare(fields[i]);
                for (unsigned i=0;i<count;++i)
                    owner.distinct[groups[i]]+=names.mark_prepared(fields[i],groups[i],prepared[i]);
            }
        }
    }
public:
    const char *base;
    unsigned owner_shift;
    NamePipeline(const char *input,unsigned count):base(input),owner_shift(0) {
        owners.reserve(count); jobs.reserve(count);
        for (unsigned i=0;i<count;++i) owners.emplace_back(new Owner);
        try {
            for (auto &entry:owners) {
                Owner *owner=entry.get();
                jobs.emplace_back([&,owner] {
                    try {
                        NamePacket packet;
                        for (;;) {
                            {
                                std::unique_lock<std::mutex> lock(owner->mutex);
                                owner->ready.wait(lock,[&]{return owner->count || owner->closed || aborted.load(std::memory_order_acquire);});
                                if (aborted.load(std::memory_order_acquire) || !owner->count) return;
                                packet=owner->ring[owner->read]; owner->read=(owner->read+1)%owner->ring.size(); --owner->count;
                                owner->space.notify_one();
                            }
                            consume(*owner,packet);
                        }
                    } catch (...) { fail(); }
                });
            }
        } catch (...) { fail(); finish(); throw; }
    }
    unsigned count() const { return owners.size(); }
    void push(unsigned index,const NamePacket &packet) {
        auto &owner=*owners[index];
        std::unique_lock<std::mutex> lock(owner.mutex);
        owner.space.wait(lock,[&]{return owner.count<owner.ring.size() || aborted.load(std::memory_order_acquire);});
        if (aborted.load(std::memory_order_acquire)) throw std::runtime_error("name owner failed");
        owner.ring[owner.write]=packet; owner.write=(owner.write+1)%owner.ring.size(); ++owner.count;
        owner.ready.notify_one();
    }
    void finish() {
        for (auto &owner:owners) {
            std::lock_guard<std::mutex> lock(owner->mutex);
            owner->closed=true; owner->ready.notify_all();
        }
        for (auto &job:jobs) if (job.joinable()) job.join();
    }
    void cancel() { fail(); }
    void check() { if (error) std::rethrow_exception(error); }
    void add_distinct(std::vector<Group> &groups) const {
        for (const auto &owner:owners) for (unsigned g=0;g<groups.size();++g) groups[g].distinct+=owner->distinct[g];
    }
    ~NamePipeline() { finish(); }
};
struct NameRouter {
    NamePipeline &pipeline;
    std::vector<NamePacket> packets;
    explicit NameRouter(NamePipeline &p):pipeline(p),packets(p.count()) {}
    void emit(const Field &name,unsigned group) {
        uint32_t hash=name.hash&0xfffff;
        unsigned owner=unsigned((uint64_t(hash)*packets.size())>>20);
        auto &packet=packets[owner];
        size_t offset=name.raw-pipeline.base;
        bool wide=offset>0xfffffff || name.raw_len>=255;
        unsigned words=wide?3:1;
        if (packet.used+words>packet.values.size()) { pipeline.push(owner,packet); packet.used=0; }
        uint64_t metadata=(uint64_t(hash)<<44)|(uint64_t(wide?255:name.raw_len)<<36)|group;
        packet.values[packet.used++]=metadata|(wide?0:uint64_t(offset)<<8);
        if (wide) { packet.values[packet.used++]=reinterpret_cast<uint64_t>(name.raw); packet.values[packet.used++]=name.raw_len; }
    }
    void flush() {
        for (unsigned owner=0;owner<packets.size();++owner) if (packets[owner].used) {
            pipeline.push(owner,packets[owner]); packets[owner].used=0;
        }
    }
};

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

__attribute__((target("pclmul")))
static void parse_stream(const char *p,const char *end,const std::vector<uint8_t> &regions,
                         unsigned unmatched,Worker &worker,NamePipeline &pipeline) {
    struct Row { uint64_t id; int64_t amount; Field name; uint32_t nlen,xlen,group; };
    try {
        NameRouter router(pipeline);
        std::array<Row,32> rows;
        while (p<end) {
            unsigned count=0;
            do {
                bom(p,end); uint64_t id=number(p,end); comma(p,end);
                uint32_t nlen,xlen; Field name=text_pair(p,end,nlen,xlen);
                uint64_t region=number(p,end); comma(p,end);
                if (region<regions.size()) __builtin_prefetch(&regions[region],0,1);
                int64_t amount=cents(p,end); newline(p,end);
                unsigned group=region<regions.size() && regions[region]!=UINT8_MAX?regions[region]:unmatched;
                rows[count++]={id,amount,name,nlen,xlen,group};
            } while (count<rows.size() && p<end);
            for (unsigned i=0;i<count;++i) {
                const auto &row=rows[i];
                worker.groups[row.group].add(row.id,row.amount,row.nlen,row.xlen,false);
                router.emit(row.name,row.group);
            }
        }
        router.flush();
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
        unsigned threads = end - p < (1 << 20) ? 1 : 6;
        unsigned owners=threads>=12?4:threads>=6?2:1;
        unsigned parsers=std::max(1U,threads-owners);
        std::vector<const char *> cuts(parsers+1);
        cuts[0]=record_start; cuts[parsers]=end;
        for (unsigned t=1;t<parsers;++t) cuts[t]=next_boundary(record_start+(end-record_start)*t/parsers,end);
        std::vector<Worker> workers; workers.reserve(parsers);
        for (unsigned t=0;t<parsers;++t) workers.emplace_back(unmatched+1);
        NamePipeline pipeline(records.data,owners);
        std::vector<std::thread> jobs;
        try {
            jobs.reserve(parsers);
            for (unsigned t=0;t<parsers;++t) jobs.emplace_back([&,t]{parse_stream(cuts[t],cuts[t+1],regions,unmatched,workers[t],pipeline);});
        } catch (...) { pipeline.cancel(); for (auto &job:jobs) job.join(); throw; }
        for (auto &job:jobs) job.join();
        pipeline.finish(); pipeline.check();
        for (const auto &worker:workers) if (worker.error) std::rethrow_exception(worker.error);
        std::vector<Group> groups(unmatched+1);
        for (const auto &worker:workers) for (unsigned g=0;g<=unmatched;++g) {
            const auto &src=worker.groups[g]; if (!src.count) continue;
            auto &dst=groups[g];
            if (!dst.count || src.min<dst.min) dst.min=src.min;
            if (!dst.count || src.max>dst.max) dst.max=src.max;
            dst.count+=src.count; dst.id_sum+=src.id_sum; dst.sum+=src.sum;
            dst.name_bytes+=src.name_bytes; dst.note_bytes+=src.note_bytes;
        }
        pipeline.add_distinct(groups);
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
