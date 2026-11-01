from pathlib import Path
p=Path('/workspace/sd-tc/track-c/join.cpp');s=p.read_text()
s=s.replace('    size_t size;','    size_t size, mapped;')
a=s.index('        data = static_cast<const char *>(mmap(');b=s.index('\n    }\n    ~File()',a)
s=s[:a]+'''        size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
        mapped = (size + page - 1) / page * page + page;
        data = static_cast<const char *>(mmap(nullptr, mapped, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        if (data == MAP_FAILED) { close(fd); throw std::runtime_error("reserve mmap"); }
        if (size && mmap(const_cast<char *>(data), size, PROT_READ, MAP_PRIVATE | MAP_FIXED, fd, 0) == MAP_FAILED) {
            munmap(const_cast<char *>(data), mapped); close(fd); throw std::runtime_error("file mmap");
        }
        close(fd);''' +s[b:]
s=s.replace('~File() { munmap(const_cast<char *>(data), size); }','~File() { munmap(const_cast<char *>(data), mapped); }')
a=s.index('static const char *find_quote(');b=s.index('\nstatic Field text_field',a)
s=s[:a]+'''static const char *find_quote(const char *p, const char *end) {
    const __m256i quote = _mm256_set1_epi8('"');
    while (p < end) {
        __m256i block = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(p));
        unsigned mask = static_cast<unsigned>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(block, quote)));
        if (mask) { const char *q = p + __builtin_ctz(mask); return q < end ? q : nullptr; }
        p += 32;
    }
    return nullptr;
}
''' +s[b:]
p.write_text(s)
