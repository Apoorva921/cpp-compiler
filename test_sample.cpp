extern "C" int printf(char* fmt, ...);

class Calculator {
public:
    int base;

    Calculator(int b) {
        base = b;
    }

    int add(int x) {
        return base + x;
    }

    int multiply(int x) {
        return base * x;
    }
};

int main() {
    printf("Welcome to Micro C++ (mcxx)!\n\n");

    Calculator* calc = new Calculator(10);

    int sum = calc->add(5);
    int prod = calc->multiply(5);

    printf("Base value: 10\n");
    printf("10 + 5 = %d\n", sum);
    printf("10 * 5 = %d\n\n", prod);

    printf("Looping through numbers:\n");
    for (int i = 1; i <= 5; ++i) {
        printf("  calc->add(%d) = %d\n", i, calc->add(i));
    }

    return 0;
}
