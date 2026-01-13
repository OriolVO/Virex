int nested_loops() {
    int sum = 0;
    for (int i = 0; i < 1000; i++)
        for (int j = 0; j < 1000; j++) sum++;
    return sum;
}
int main() { nested_loops(); return 0; }
