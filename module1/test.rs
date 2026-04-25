// Function declaration and definition
fn is_even(x: i32) -> bool {
    x % 2 == 0
}
/* 
    example of multiline comment
    3rw \r {} 

*/
fn main() {     
    // Variable declaration and assignment
    let mut sum =    0;
    let mut         count = 0;
    let limit = 10;     
  
    // Loop: for loop
    for num in 1..=limit {
        // Arithmetic expression
        let squared = num * num;

        // Logical expression inside if-else
        if is_even(num) && squared > 10 {
            println!("{} is even and its square ({}) > 10", num, squared);
            sum += squared;
            count += 1;
        } else {
            println!("{} is odd or square <= 10", num);
        }
    }
  
    // While loop
    let mut i = 1;
    while i <= 3 {
        println!("While loop iteration {}", i);
        i += 1;
    }

    // Using function call
    let final_avg = if count > 0 {
        sum / count
    } else {
        0
    };
/*   */
    println!("Sum of qualifying squares: {}", sum);
    println!("Count: {}", count);
    println!("Average: {}", final_avg);
}