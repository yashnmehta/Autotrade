//endianess conversion functions
inline uint16_t be16toh(uint16_t x) {
    return (x << 8) | (x >> 8);
}

inline uint32_t be32toh(uint32_t x) {
    return ((x << 24) & 0xff000000 ) |
           ((x <<  8) & 0x00ff0000 ) |
           ((x >>  8) & 0x0000ff00 ) |
           ((x >> 24) & 0x000000ff );
}


//print hex dump of data
inline void print_hex_dump(const char* data, int len) {
    for (int i = 0; i < len; i++) {
        printf("%02x ", (unsigned char)data[i]);
    }
    printf("\n");
}