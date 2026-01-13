fn sum_array() -> i32 {
    let mut arr = [0i32; 10000];
    for i in 0..10000 { arr[i] = i as i32; }
    let mut total = 0i32;
    for _ in 0..1000 {
        for i in 0..10000 { total = total.wrapping_add(arr[i]); }
    }
    total
}
fn main() { sum_array(); }
