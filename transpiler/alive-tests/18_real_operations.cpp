#define TOKENPASTE(x, y) x ## y
#define TOKENPASTE2(x, y) TOKENPASTE(x, y)
#define X(name) TOKENPASTE2(name, __COUNTER__)

float X(f)(float a, float b) {
    return a + b;
}
float X(f)(float a, float b) {
    return a - b;
}
float X(f)(float a, float b) {
    return a * b;
}
float X(f)(float a, float b) {
    return a / b;
}
float X(f)(float a, float b) {
    return a && b;
}
float X(f)(float a, float b) {
    return a || b;
}
float X(f)(float a, float b) {
    return a < b;
}
float X(f)(float a, float b) {
    return a > b;
}
float X(f)(float a, float b) {
    return a <= b;
}
float X(f)(float a, float b) {
    return a >= b;
}
float X(f)(float a, float b) {
    return a == b;
}
float X(f)(float a, float b) {
    return a != b;
}
float X(f)(float a, float b) {
    return !a;
}
float X(f)(float a, float b) {
    return +a;
}
float X(f)(float a, float b) {
    return -b;
}
