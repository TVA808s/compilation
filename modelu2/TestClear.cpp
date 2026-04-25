#include <iostream>
using std::cout;
using std::endl;
bool is_even(short x) {
return x % 2 == 0;
}
int main() {
short sum = 0, count = 0, limit = 10;
for (short i = 0; i <= limit; i++) {
short squared = i * i;
if (is_even(i) && squared > 10) {
cout << i << " is even number and its square > 10" << endl;
sum += squared;
count++;
}
else {
cout << i << " is odd or square <= 10" << endl;
}
}
short i = 1;
while (i <= 3) {
i++;
}
short final_avg = count > 0 ? sum / count : 0;
cout << "Average: " << final_avg << endl;
}