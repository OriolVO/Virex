int sum_array() {
    int arr[10000];
    for (int i = 0; i < 10000; i++) arr[i] = i;
    int total = 0;
    for (int iter = 0; iter < 1000; iter++)
        for (int i = 0; i < 10000; i++) total += arr[i];
    return total;
}
int main() { sum_array(); return 0; }
