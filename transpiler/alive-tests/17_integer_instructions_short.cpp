#define TOKENPASTE(x, y) x ## y
#define TOKENPASTE2(x, y) TOKENPASTE(x, y)
#define X(name) TOKENPASTE2(name, __COUNTER__)

short X(f)(short a, short b) {
    return a + b;
}
short X(f)(short a, short b) {
    return a - b;
}
short X(f)(short a, short b) {
    return a * b;
}
short X(f)(short a, short b) {
    return a / b;
}
short X(f)(short a, short b) {
    return a % b;
}
short X(f)(short a, short b) {
    return a & b;
}
short X(f)(short a, short b) {
    return a | b;
}
short X(f)(short a, short b) {
    return a ^ b;
}
short X(f)(short a, short b) {
    return a << b;
}
short X(f)(short a, short b) {
    return a >> b;
}
short X(f)(short a, short b) {
    return a && b;
}
short X(f)(short a, short b) {
    return a || b;
}
short X(f)(short a, short b) {
    return a < b;
}
short X(f)(short a, short b) {
    return a > b;
}
short X(f)(short a, short b) {
    return a <= b;
}
short X(f)(short a, short b) {
    return a >= b;
}
short X(f)(short a, short b) {
    return a == b;
}
short X(f)(short a, short b) {
    return a != b;
}
short X(f)(short a, short b) {
    return ~a;
}
short X(f)(short a, short b) {
    return !a;
}
short X(f)(short a, short b) {
    return +a;
}
short X(f)(short a, short b) {
    return -b;
}
