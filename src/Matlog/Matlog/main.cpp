#include <iostream>
#include "bdd.h"

// My variant

#define N 9
#define M 4

#define LOG2N 4

#define n1 7
#define n2 4
#define n3 2
#define n4 6

// - - -
// - 0 -
// x x -
// мой вид соседства

// horizontal connect



using namespace std;


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
