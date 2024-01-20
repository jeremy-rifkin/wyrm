int factorial(int n) {
    int r = 1;
    while(n > 0) {
        r *= n;
        n--;
    }
    return r;
}

// DECL: int factorial(int n);
// TEST: VERIFY(factorial(5) == 120);
// TEST: VERIFY(factorial(6) == 720);
