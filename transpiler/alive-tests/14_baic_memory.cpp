int foo(int* x) {
    return *(x + 2);
}

void bar(int* x) {
    *(x + 2) = 3;
}
