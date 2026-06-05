#include <iostream>

using std::cout;
using std::endl;

/* 
    Функция проверки четности числа
    Возвращает true если число четное, false иначе
*/
bool is_even(short x) {
    // Оператор % возвращает остаток от деления
    // Если остаток от деления на 2 равен 0, то число четное
    return (x % 2) == 0;
}

// Главная функция программы
int main() {     
    // Инициализация переменных
    short sum = 0, count = 0, limit = 10;
    
    // Цикл for: перебираем числа от 0 до limit
    for (short i = 0; i <= limit; i++) {
        // Вычисляем квадрат текущего числа
        short squared = i * i;

        if (is_even(i) && squared > 10) {
            // Вывод информации о числе
            cout << i << " is even number and its square > 10" << endl;
            // Добавляем квадрат к сумме
            sum += squared;
            // Увеличиваем счетчик
            count++;
        }
        else {
            // Вывод для нечетных чисел или квадрат которых <= 10
            cout << i << " is odd or square <= 10" << endl;
        }
    }
    
    // Повторное объявление переменной i (новая область видимости)
    short i = 1;
    
    // Цикл while: выполняется пока i <= 3
    while (i <= 3) {
        // Инкремент переменной
        i++;
    }
    
    short final_avg = (count > 0) ? (sum / count) : 0;
    
    // Вывод результата
    cout << "Average: " << final_avg << endl;
    
    // Завершение программы
    return 0;
}