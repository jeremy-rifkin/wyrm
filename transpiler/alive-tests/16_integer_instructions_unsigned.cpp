#define TOKENPASTE(x, y) x ## y
#define TOKENPASTE2(x, y) TOKENPASTE(x, y)
#define X(name) TOKENPASTE2(name, __COUNTER__)

unsigned X(f)(unsigned a, unsigned b) {
    return a + b;
}
unsigned X(f)(unsigned a, unsigned b) {
    return a - b;
}
unsigned X(f)(unsigned a, unsigned b) {
    return a * b;
}
unsigned X(f)(unsigned a, unsigned b) {
    return a / b;
}
unsigned X(f)(unsigned a, unsigned b) {
    return a % b;
}
unsigned X(f)(unsigned a, unsigned b) {
    return a & b;
}
unsigned X(f)(unsigned a, unsigned b) {
    return a | b;
}
unsigned X(f)(unsigned a, unsigned b) {
    return a ^ b;
}
unsigned X(f)(unsigned a, unsigned b) {
    return a << b;
}
unsigned X(f)(unsigned a, unsigned b) {
    return a >> b;
}
unsigned X(f)(unsigned a, unsigned b) {
    return a && b;
}
unsigned X(f)(unsigned a, unsigned b) {
    return a || b;
}
unsigned X(f)(unsigned a, unsigned b) {
    return a < b;
}
unsigned X(f)(unsigned a, unsigned b) {
    return a > b;
}
unsigned X(f)(unsigned a, unsigned b) {
    return a <= b;
}
unsigned X(f)(unsigned a, unsigned b) {
    return a >= b;
}
unsigned X(f)(unsigned a, unsigned b) {
    return a == b;
}
unsigned X(f)(unsigned a, unsigned b) {
    return a != b;
}
unsigned X(f)(unsigned a, unsigned b) {
    return ~a;
}
unsigned X(f)(unsigned a, unsigned b) {
    return !a;
}
unsigned X(f)(unsigned a, unsigned b) {
    return +a;
}
unsigned X(f)(unsigned a, unsigned b) {
    return -b;
}
