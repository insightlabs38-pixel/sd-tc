#include <string_view>
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

struct alignas(64) ProbeStats {
    uint64_t prepare=0,prepare_groups=0,prepare_busy=0,slow=0,slow_groups=0,slow_busy=0,cas_fail=0,overflow=0,bad=0;
};
static thread_local ProbeStats *probe_stats=nullptr;
struct SharedNames {
    static constexpr unsigned group_count=1<<15,capacity=group_count*4;
    static uint32_t spread(uint32_t h) { return h^(h>>11)^(h>>22); }
    static uint16_t tag(const Field &f) {
        return uint16_t((((f.raw_len<=32?f.raw_len:63U)<<9)|((f.hash>>17)&511))+1);
    }
    static uint64_t matches(uint64_t word,uint16_t value) {
        uint64_t x=word^(uint64_t(value)*0x0001000100010001ULL);
        return (x-0x0001000100010001ULL)&~x&0x8000800080008000ULL;
    }
    struct LongKey { const char *raw; uint32_t length; };
    struct Slot {
        char key[8]{};
        std::atomic<uint64_t> seen[4]{};
        const char *pointer() const { const char *p; std::memcpy(&p,key,sizeof(p)); return p; }
        bool matches(const Field &f,const char *) const {
            if (f.raw_len<=8) return !std::memcmp(key,f.raw,f.raw_len);
            const char *p=pointer();
            if (f.raw_len>32) {
                const auto *view=reinterpret_cast<const LongKey *>(p);
                if (view->length!=f.raw_len) return false;
                p=view->raw;
            }
            return !std::memcmp(p,f.raw,f.raw_len);
        }
        void prefetch(const Field &f) const { if (f.raw_len>8) __builtin_prefetch(pointer(),0,1); }
    };
    struct Overflow {
        std::mutex mutex;
        std::unordered_map<std::string_view,std::array<uint64_t,4>> keys;
        std::vector<std::unique_ptr<LongKey[]>> long_keys;
        unsigned used=32;
        const char *keep(const Field &name) {
            std::lock_guard<std::mutex> lock(mutex);
            if (used==32) { long_keys.emplace_back(new LongKey[32]); used=0; }
            auto &saved=long_keys.back()[used++]; saved={name.raw,name.raw_len};
            return reinterpret_cast<const char *>(&saved);
        }
    };
    std::array<Overflow,128> overflow;
    std::array<std::atomic<uint64_t>,group_count> controls{};
    Slot *slots;
    SharedNames() { static Slot storage[capacity]{}; slots=storage; }
    void prefetch(const Field &name) const {
        __builtin_prefetch(&controls[spread(name.hash)&(group_count-1)],0,1);
    }
    Slot *prepare(const Field &name) {
        ++probe_stats->prepare;
        uint16_t signature=tag(name);
        unsigned group=spread(name.hash)&(group_count-1);
        for (unsigned probe=0;probe<16;++probe,group=(group+1)&(group_count-1)) {
            ++probe_stats->prepare_groups;
            uint64_t word=controls[group].load(std::memory_order_acquire);
            if (matches(word,UINT16_MAX)) { ++probe_stats->prepare_busy; --probe; group=(group-1)&(group_count-1); std::this_thread::yield(); continue; }
            uint64_t equal=matches(word,signature);
            if (equal) return &slots[group*4+__builtin_ctzll(equal)/16];
            if (matches(word,0)) return nullptr;
        }
        return nullptr;
    }
    static bool mark(Slot &slot,unsigned group) {
        uint64_t bit=uint64_t(1)<<(group&63);
        auto &word=slot.seen[group>>6];
        return !(word.load(std::memory_order_relaxed)&bit) && !(word.fetch_or(bit,std::memory_order_relaxed)&bit);
    }
    bool slow_first(const Field &name,unsigned country,const char *end) {
        ++probe_stats->slow;
        uint16_t signature=tag(name);
        unsigned group=spread(name.hash)&(group_count-1);
        for (unsigned probe=0;probe<16;++probe,group=(group+1)&(group_count-1)) {
            ++probe_stats->slow_groups;
            auto &control=controls[group];
            uint64_t word=control.load(std::memory_order_acquire);
            if (matches(word,UINT16_MAX)) { ++probe_stats->slow_busy; --probe; group=(group-1)&(group_count-1); std::this_thread::yield(); continue; }
            uint64_t equal=matches(word,signature);
            while (equal) {
                unsigned lane=__builtin_ctzll(equal)/16; equal&=equal-1;
                if (uint16_t(word>>(lane*16))==signature && slots[group*4+lane].matches(name,end))
                    return mark(slots[group*4+lane],country);
            }
            uint64_t empty=matches(word,0);
            if (!empty) continue;
            unsigned lane=__builtin_ctzll(empty)/16,shift=lane*16;
            uint64_t mask=uint64_t(UINT16_MAX)<<shift;
            if (!control.compare_exchange_strong(word,word|mask,std::memory_order_acq_rel)) {
                ++probe_stats->cas_fail; --probe; group=(group-1)&(group_count-1); continue;
            }
            auto &slot=slots[group*4+lane];
            try {
                if (name.raw_len<=8) std::memcpy(slot.key,name.raw,name.raw_len);
                else {
                    const char *p=name.raw_len<=32?name.raw:overflow[name.hash&127].keep(name);
                    std::memcpy(slot.key,&p,sizeof(p));
                }
            } catch (...) {
                word=control.load(std::memory_order_relaxed);
                while (!control.compare_exchange_weak(word,word&~mask,std::memory_order_release)) {}
                throw;
            }
            word=control.load(std::memory_order_relaxed);
            while (!control.compare_exchange_weak(word,(word&~mask)|(uint64_t(signature)<<shift),std::memory_order_release)) {}
            return mark(slot,country);
        }
        ++probe_stats->overflow;
        auto &shard=overflow[name.hash&127];
        std::lock_guard<std::mutex> lock(shard.mutex);
        auto &seen=shard.keys[std::string_view(name.raw,name.raw_len)];
        uint64_t bit=uint64_t(1)<<(country&63);
        bool fresh=!(seen[country>>6]&bit); seen[country>>6]|=bit;
        return fresh;
    }
    bool first(const Field &name,unsigned country,Slot *slot,const char *end) {
        if (slot && slot->matches(name,end)) return mark(*slot,country);
        probe_stats->bad+=slot!=nullptr;
        return slow_first(name,country,end);
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
    ProbeStats probes;
    std::vector<Group> groups;
    std::exception_ptr error;
    explicit Worker(size_t group_count) : groups(group_count) { }
};

__attribute__((target("pclmul")))
static Field text_pair(const char *&p, const char *end, uint32_t &name_len,
                       uint32_t &note_len, SharedNames &shared) {
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
                shared.prefetch(name);
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
static void parse_range(const char *p, const char *end, const std::vector<uint8_t> &regions,
                        unsigned unmatched, Worker &w, SharedNames &shared) {
    struct Pending {
        uint64_t id;
        int64_t amount;
        Field name;
        uint32_t name_len, note_len;
        unsigned group;
    };
    static_assert(sizeof(Pending) == 48, "compact pending row");
    try {
        probe_stats=&w.probes;
        std::array<Pending, 32> batch;
        std::array<SharedNames::Slot *, 32> slots;
        while (p < end) {
            unsigned count = 0;
            do {
                bom(p, end);
                uint64_t id = number(p, end);
                comma(p, end);
                uint32_t name_len, note_len;
                Field name = text_pair(p, end, name_len, note_len, shared);
                uint64_t region = number(p, end);
                uint32_t region_key = region < regions.size()
                                          ? static_cast<uint32_t>(region) : UINT32_MAX;
                if (region_key != UINT32_MAX) __builtin_prefetch(&regions[region_key], 0, 1);
                comma(p, end);
                int64_t amount = cents(p, end);
                unsigned group = region_key != UINT32_MAX && regions[region_key] != UINT8_MAX
                                     ? regions[region_key] : unmatched;
                SharedNames::Slot *slot = shared.prepare(name);
                if (slot) {
                    __builtin_prefetch(slot, 0, 3);
                    __builtin_prefetch(&slot->seen[group >> 6], 1, 3);
                }
                newline(p, end);
                batch[count] = Pending{id, amount, name, name_len, note_len, group};
                slots[count++] = slot;
            } while (count < batch.size() && p < end);
            for (unsigned i=0;i<count;++i) if (slots[i]) slots[i]->prefetch(batch[i].name);
            for (unsigned i = 0; i < count; ++i) {
                const Pending &r = batch[i];
                w.groups[r.group].add(r.id, r.amount, r.name_len, r.note_len, shared.first(r.name, r.group, slots[i], end));
            }
        }
    } catch (...) { w.error = std::current_exception(); }
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
        ProbeStats sum;
        for (const auto &w:workers) {
            sum.prepare+=w.probes.prepare; sum.prepare_groups+=w.probes.prepare_groups;
            sum.prepare_busy+=w.probes.prepare_busy; sum.slow+=w.probes.slow;
            sum.slow_groups+=w.probes.slow_groups; sum.slow_busy+=w.probes.slow_busy;
            sum.cas_fail+=w.probes.cas_fail; sum.overflow+=w.probes.overflow; sum.bad+=w.probes.bad;
        }
        uint64_t occupied=0,overflow_names=0,max_full=0,run=0,long_names=0;
        for (const auto &shard:shared.overflow) overflow_names+=shard.keys.size();
        for (unsigned i=0;i<SharedNames::group_count*2;++i) {
            uint64_t word=shared.controls[i&(SharedNames::group_count-1)].load();
            unsigned full=0;
            for (unsigned lane=0;lane<4;++lane) if (uint16_t(word>>(lane*16))) {
                ++full;
                if (i<SharedNames::group_count) { ++occupied; long_names+=((uint16_t(word>>(lane*16))-1)>>9)==63; }
            }
            run=full==4?run+1:0; max_full=std::max(max_full,run);
        }
        std::fprintf(stderr,"PROBES rows=%llu prepare_groups=%llu prepare_busy=%llu slow=%llu slow_groups=%llu slow_busy=%llu cas_fail=%llu overflow_rows=%llu bad=%llu occupied=%llu overflow_names=%llu max_full_groups=%llu long_names=%llu\n",
            (unsigned long long)sum.prepare,(unsigned long long)sum.prepare_groups,(unsigned long long)sum.prepare_busy,
            (unsigned long long)sum.slow,(unsigned long long)sum.slow_groups,(unsigned long long)sum.slow_busy,
            (unsigned long long)sum.cas_fail,(unsigned long long)sum.overflow,(unsigned long long)sum.bad,
            (unsigned long long)occupied,(unsigned long long)overflow_names,(unsigned long long)max_full,(unsigned long long)long_names);
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
