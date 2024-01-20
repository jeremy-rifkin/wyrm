void foo(int* arr, int n) {
    for(int i = 0; i < n; i++) {
        arr[i] *= 2;
    }
}

// DECL: int foo(int* arr, int n);
// TEST: int arr1[] = {1, 2, 3, 4, 5, 6};
// TEST: int arr2[] = {1, 2, 3, 4, 5, 6};
// TEST: foo(arr2, 6);
// TEST: for(int i = 0; i < 6; i++) {
// TEST:     VERIFY(arr2[i] == 2 * arr1[i]);
// TEST: }
