fn nested_loops() -> i32 {
    let mut sum = 0;
    for _ in 0..1000 {
        for _ in 0..1000 { sum += 1; }
    }
    sum
}
fn main() { nested_loops(); }
