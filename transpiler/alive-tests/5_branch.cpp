unsigned baz(unsigned x, unsigned y) {
    if(x < y) {
        return x * y / (x + y);
    } else {
        return y / x;
    }
}
