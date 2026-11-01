extern "C" int printf(char* fmt, ...);

int main() {
    int a = 10;
    int b = 25;
    int sum = a + b;
    printf("Hello from Micro C++ Compiler!\n");
    printf("Calculation: %d + %d = %d\n", a, b, sum);
    return 0;
}
