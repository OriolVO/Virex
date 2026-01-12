#include <stdio.h>

struct Point {
    int x;
    int y;
};

void print_point(struct Point p) {
    printf("C Point: x=%d, y=%d\n", p.x, p.y);
}

struct Point offset_point(struct Point p, int dx, int dy) {
    p.x += dx;
    p.y += dy;
    return p;
}
