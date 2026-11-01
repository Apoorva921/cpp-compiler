extern "C" int printf(char* fmt, ...);

class Point {
public:
    int x;
    int y;

    Point(int initX, int initY) {
        x = initX;
        y = initY;
    }

    void print() {
        printf("Point: (%d, %d)\n", x, y);
    }

    int sum() {
        return x + y;
    }

    void translate(int dx, int dy) {
        x += dx;
        y += dy;
    }
};

int main() {
    printf("=== Object-Oriented C++ Class Test ===\n");

    Point* p1 = new Point(15, 25);
    p1->print();

    int total = p1->sum();
    printf("Sum of coordinates: %d\n", total);

    printf("Translating by (5, -10)...\n");
    p1->translate(5, -10);
    p1->print();

    Point* p2 = new Point(100, 200);
    p2->print();

    return 0;
}
