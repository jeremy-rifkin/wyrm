int factorial(int n) {
    int r = 1;
    while(n > 0) {
        r *= n;
        n--;
    }
    return r;
}
