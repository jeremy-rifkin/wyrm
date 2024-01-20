#include <stdint.h>

int8_t a(int32_t x) {
    return x;
}
int64_t b(int32_t x) {
    return x;
}
uint8_t c(int32_t x) {
    return x;
}
uint64_t d(int32_t x) {
    return x;
}

int8_t a(uint32_t x) {
    return x;
}
int64_t b(uint32_t x) {
    return x;
}
uint8_t c(uint32_t x) {
    return x;
}
uint64_t d(uint32_t x) {
    return x;
}

int32_t foo(uint32_t x) {
    return x;
}
uint32_t bar(int32_t x) {
    return x;
}
