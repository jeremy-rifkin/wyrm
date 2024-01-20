#define TOKENPASTE(x, y) x ## y
#define TOKENPASTE2(x, y) TOKENPASTE(x, y)
#define X(name) TOKENPASTE2(name, __COUNTER__)

int X(f)(int a, int b) {
    return a + b;
}
int X(f)(int a, int b) {
    return a - b;
}
int X(f)(int a, int b) {
    return a * b;
}
int X(f)(int a, int b) {
    return a / b;
}
int X(f)(int a, int b) {
    return a % b;
}
int X(f)(int a, int b) {
    return a & b;
}
int X(f)(int a, int b) {
    return a | b;
}
int X(f)(int a, int b) {
    return a ^ b;
}
int X(f)(int a, int b) {
    return a << b;
}
int X(f)(int a, int b) {
    return a >> b;
}
int X(f)(int a, int b) {
    return a && b;
}
int X(f)(int a, int b) {
    return a || b;
}
int X(f)(int a, int b) {
    return a < b;
}
int X(f)(int a, int b) {
    return a > b;
}
int X(f)(int a, int b) {
    return a <= b;
}
int X(f)(int a, int b) {
    return a >= b;
}
int X(f)(int a, int b) {
    return a == b;
}
int X(f)(int a, int b) {
    return a != b;
}
int X(f)(int a, int b) {
    return ~a;
}
int X(f)(int a, int b) {
    return !a;
}
int X(f)(int a, int b) {
    return +a;
}
int X(f)(int a, int b) {
    return -b;
}
