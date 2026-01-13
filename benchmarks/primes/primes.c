#include <stdbool.h>
int sieve_primes(int limit) {
    bool is_prime[10000];
    for (int i = 0; i < limit; i++) is_prime[i] = true;
    is_prime[0] = is_prime[1] = false;
    for (int i = 2; i * i < limit; i++) {
        if (is_prime[i]) {
            for (int j = i * i; j < limit; j += i) is_prime[j] = false;
        }
    }
    int count = 0;
    for (int i = 0; i < limit; i++) if (is_prime[i]) count++;
    return count;
}
int main() {
    int total = 0;
    for (int iter = 0; iter < 1000; iter++) {
        total += sieve_primes(10000);
    }
    return 0;
}
