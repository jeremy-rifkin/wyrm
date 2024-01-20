int factorial(int n) {
    if(n == 0) return 1;
    else return n * factorial(n - 1);
}
int square(int num) {
    return num * num + factorial(5);
}

// DECL: int square(int n);
// TEST: VERIFY(square(2) == 2 * 2 + 120);
