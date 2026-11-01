extern "C" int printf(char* fmt, ...);

void swap(int* a, int* b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

int main() {
    printf("=== Pointer & Array Test ===\n");

    int x = 100;
    int y = 200;
    printf("Before swap: x = %d, y = %d\n", x, y);
    swap(&x, &y);
    printf("After swap:  x = %d, y = %d\n", x, y);

    int arr[5];
    for (int i = 0; i < 5; ++i) {
        arr[i] = (i + 1) * 10;
    }

    printf("Array elements via indexing:\n");
    for (int i = 0; i < 5; ++i) {
        printf("  arr[%d] = %d\n", i, arr[i]);
    }

    printf("Array elements via pointer arithmetic:\n");
    int* p = arr;
    for (int i = 0; i < 5; ++i) {
        printf("  *(p + %d) = %d\n", i, *(p + i));
    }

    return 0;
}
