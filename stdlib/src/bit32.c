#define uint32_t unsigned int

uint32_t _bit32__band(uint32_t a, uint32_t b) {
    return a & b;
}

uint32_t _bit32__bor(uint32_t a, uint32_t b) {
    return a | b;
}

uint32_t _bit32__bxor(uint32_t a, uint32_t b) {
    return a ^ b;
}

uint32_t _bit32__bnot(uint32_t a) {
    return ~a;
}

uint32_t _bit32__lshift(uint32_t a, int shift) {
    return a << (shift & 31);
}

uint32_t _bit32__rshift(uint32_t a, int shift) {
    return a >> (shift & 31);
}

uint32_t _bit32__arshift(int a, int shift) {
    return a >> (shift & 31);
}

uint32_t _bit32__rol(uint32_t a, int shift) {
    shift &= 31;
    return (a << shift) | (a >> (32 - shift));
}

uint32_t _bit32__ror(uint32_t a, int shift) {
    shift &= 31;
    return (a >> shift) | (a << (32 - shift));
}

uint32_t _bit32__extract(uint32_t a, int field, int width) {
    if (width <= 0 || field < 0 || field + width > 32)
        return 0;
    return (a >> field) & ((1u << width) - 1);
}

uint32_t _bit32__replace(uint32_t a, uint32_t v, int field, int width) {
    if (width <= 0 || field < 0 || field + width > 32)
        return a;
    uint32_t mask = ((1u << width) - 1) << field;
    return (a & ~mask) | ((v << field) & mask);
}
