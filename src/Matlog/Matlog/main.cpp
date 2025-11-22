#include <iostream>
#include "bdd.h"

int main() {
    // Инициализация BuDDy
    bdd_init(1000, 100);
    bdd_setvarnum(2);   // две булевы переменные

    bdd x = bdd_ithvar(0);
    bdd y = bdd_ithvar(1);
    bdd f = x & !y;     // простая функция: x ∧ ¬y

    std::cout << "nodes: " << bdd_nodecount(f) << std::endl;

    bdd_done();         // завершение работы с библиотекой
    return 0;
}
