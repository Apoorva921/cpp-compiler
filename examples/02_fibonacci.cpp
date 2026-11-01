extern "C" int printf(char* fmt, ...);

int fib_recursive(int n) {
    if (n <= 1) {
        return n;
    }
    return fib_recursive(n - 1) + fib_recursive(n - 2);
}

int fib_iterative(int n) {
    if (n <= 1) return n;
    int a = 0;
    int b = 1;
    for (int i = 2; i <= n; ++i) {
        int next = a + b;
        a = b;
        b = next;
    }
    return b;
}

int main() {
    printf("=== Fibonacci Test ===\n");
    for (int i = 0; i <= 10; ++i) {
        int r = fib_recursive(i);
        int it = fib_iterative(i);
        printf("fib(%d): recursive = %d, iterative = %d\n", i, r, it);
    }
    return 0;
}
