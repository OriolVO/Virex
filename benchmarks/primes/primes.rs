fn sieve_primes(limit: usize) -> i32 {
    let mut is_prime = [true; 10000];
    is_prime[0] = false; is_prime[1] = false;
    let mut i = 2;
    while i * i < limit {
        if is_prime[i] {
            let mut j = i * i;
            while j < limit { is_prime[j] = false; j += i; }
        }
        i += 1;
    }
    let mut count = 0;
    for i in 0..limit { if is_prime[i] { count += 1; } }
    count
}
fn main() {
    let mut total = 0;
    for _ in 0..1000 {
        total += sieve_primes(10000);
    }
}
