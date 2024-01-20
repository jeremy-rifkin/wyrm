#include <cmath>
#include <limits>

float f1() {
    return INFINITY;
}
float f2() {
    return -INFINITY;
}
float f3() {
    return NAN;
}
double f4() {
    return INFINITY;
}
double f5() {
    return -INFINITY;
}
double f6() {
    return NAN;
}
// long double not supported by alive 2
// long double f7() {
//     return INFINITY;
// }
// long double f8() {
//     return -INFINITY;
// }
// long double f9() {
//     return NAN;
// }
_Float16 f10() {
    return INFINITY;
}
_Float16 f11() {
    return -INFINITY;
}
_Float16 f12() {
    return NAN;
}
__float128 f13() {
    return INFINITY;
}
__float128 f14() {
    return -INFINITY;
}
__float128 f15() {
    return NAN;
}
// TODO: Not sure if this is my fault or alive or clang or gcc
// _Float16 foo1() {
//     return std::numeric_limits<float>::signaling_NaN();
// }
float foo2() {
    return std::numeric_limits<float>::signaling_NaN();
}
// TODO: Not sure if this is my fault or alive or clang or gcc
// double foo3() {
//     return std::numeric_limits<float>::signaling_NaN();
// }
// long double not supported by alive 2
// long double foo4() {
//     return std::numeric_limits<float>::signaling_NaN();
// }
// TODO: Not sure if this is my fault or alive or clang or gcc
// __float128 foo5() {
//     return std::numeric_limits<float>::signaling_NaN();
// }
