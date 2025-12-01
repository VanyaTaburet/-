#pragma comment(lib, "bdd.lib")

#include "bdd.h"
#include <fstream>
#include <iostream>

using namespace std;

// ------------------------- Параметры варианта -------------------------
//
// Здесь задаются константы задачи. Комментарии явно поясняют смысл каждой
// константы и как они используются далее в коде.
#define N       5   // количество объектов (элементов, позиций)
#define M       5   // количество свойств (атрибутов) у каждого объекта
#define LOG2N   4   // число бит, требуемое для кодирования значения 0..N-1
                      // ceil(log2(9)) = 4, т.е. каждое свойство кодируется 4 битами

// Общее число булевых переменных (битов) в BDD:
// для каждого из N объектов, для каждого из M свойств — LOG2N бит.
#define N_VAR   (N * M * LOG2N)

// количество ограничений по типам (используются в отчёте/комментариях)
#define n1 7    // тип 1 — конкретные задания значений
#define n2 4    // тип 2 — отношения между свойствами в одном объекте
#define n3 2    // тип 3 — направленные соседские связи
#define n4 6    // тип 4 — смежное соседство (рядом)

// Параметры сетки 3x3 (с индексированием row*SIDE+col)
#define SIDE 3

// Горизонтальная склейка краёв поля: если включено, левая граница
// считается соседней с правой (циклическая по горизонтали).
#define HORIZONTAL_GLUE 1

// -------------------- Глобальные структуры для вывода -----------------
//
// Файл вывода решений и массив текущих битовых значений для печати
//
ofstream out;
char varBits[N_VAR]; // хранит текущую конкретную расстановку битов (0/1) при перечислении решений

// p[k][i][j] : логическая формула (BDD), обозначающая "для объекта i свойство k = j".
// Реализовано как конъюнкция LOG2N литералов (бит кодирования значения j).
// Индексы:
//   k = 0..M-1   — индекс свойства,
//   i = 0..N-1   — индекс объекта,
//   j = 0..N-1   — значение свойства (кодируемое LOG2N битами).
bdd p[M][N][N];

// ------------------------- Служебные функции -------------------------
// Печать одного полного решения из varBits (по объектам и свойствам)
//
// Формат вывода:
//  i: v0 v1 v2 v3
// где для объекта i перечислены значения всех его M свойств.
// Разбор значения выполняется как суммирование битов (младший бит имеет вес 1).
void print_solution()
{
    for (unsigned i = 0; i < N; ++i)
    {
        out << i << ": ";
        // у объекта i для каждого свойства k декодируем j по LOG2N битам
        for (unsigned k = 0; k < M; ++k)
        {
            unsigned base = (i * M + k) * LOG2N; // базовый индекс бита для (i,k)
            unsigned value = 0;
            for (unsigned t = 0; t < LOG2N; ++t)
            {
                // varBits хранит 0/1 для каждого бита — суммируем с весом 2^t
                value += (unsigned)(varBits[base + t] << t);
            }
            out << value << ' ';
        }
        out << '\n';
    }
    out << '\n';
}

// varset[idx] ∈ {0,1,-1}; -1 = don't care
void build_assignments(char* varset, int n, int idx)
{
    if (idx == n - 1)
    {
        if (varset[idx] == 0 || varset[idx] == 1)
        {
            varBits[idx] = (int)varset[idx];
            print_solution();
            return;
        }

        // don't care → две ветки
        varBits[idx] = 0;
        print_solution();
        varBits[idx] = 1;
        print_solution();
        return;
    }

    if (varset[idx] == 0 || varset[idx] == 1)
    {
        varBits[idx] = (int)varset[idx];
        build_assignments(varset, n, idx + 1);
        return;
    }

    // don't care
    varBits[idx] = 0;
    build_assignments(varset, n, idx + 1);
    varBits[idx] = 1;
    build_assignments(varset, n, idx + 1);
}

void allsat_handler(char* varset, int size)
{
    build_assignments(varset, size, 0);
}

// ------------------------ Построение массива p ------------------------
// Кодирование p[k][i][j] через LOG2N битов:
// индекс булевой переменной: base = (i * M + k) * LOG2N + t
void build_p()
{
    for (int i = 0; i < N; ++i)
    {
        int base_i = i * M * LOG2N;

        for (int j = 0; j < N; ++j)   // значения 0..4
        {
            for (int k = 0; k < M; ++k)
            {
                bdd conj = bddtrue;

                for (int t = 0; t < LOG2N; ++t)
                {
                    int varIndex = base_i + k * LOG2N + t;
                    bool bit = ((j >> t) & 1) != 0;

                    if (bit)
                        conj &= bdd_ithvar(varIndex);
                    else
                        conj &= bdd_nithvar(varIndex);
                }

                p[k][i][j] = conj;
            }
        }
    }
}

// ----------------------- Общие ограничения ----------------------------

// 1) Для каждого (k, i) свойство должно принять хотя бы одно значение j
//    OR_{j=0..N-1} p[k][i][j]
void add_domain_constraints(bdd& F)
{
    for (int k = 0; k < M; ++k)
    {
        for (int i = 0; i < N; ++i)
        {
            bdd at_least_one = bddfalse;
            for (int j = 0; j < N; ++j)
            {
                at_least_one |= p[k][i][j];
            }
            F &= at_least_one;
        }
    }
}

// 2) Уникальность: одно и то же значение j одного свойства k
//    не может стоять у двух разных объектов i1 != i2.
//    Для каждого k,j и пары i1<i2:
//      ¬( p[k][i1][j] ∧ p[k][i2][j] )
void add_uniqueness_constraints(bdd& F)
{
    for (int k = 0; k < M; ++k)
    {
        for (int j = 0; j < N; ++j)
        {
            for (int i1 = 0; i1 < N; ++i1)
            {
                for (int i2 = i1 + 1; i2 < N; ++i2)
                {
                    bdd a = p[k][i1][j];
                    bdd b = p[k][i2][j];
                    F &= !(a & b);   // не могут оба одновременно
                }
            }
        }
    }
}

// ----------------------- Ограничения 1-го рода ------------------------
// Тип 1: "объект i имеет свойство k со значением j"
// просто добавляем конъюнкцию F &= p[k][i][j]
void add_type1_constraints(bdd& F)
{
    //
    // Object 0: property 0 = 1
    F &= p[0][0][1];

    // Object 0: property 1 = 2
    F &= p[1][0][2];

    // Object 0: property 1 = 2
    F &= p[2][0][4];

    // Object 1: property 2 = 3
    F &= p[2][1][3];

    // Object 2: property 4 = 0
    F &= p[4][2][0];

    // Object 3: property 1 = 4
    F &= p[1][3][4];

    // Object 4: property 3 = 2
    F &= p[3][4][2];
}

void add_type2_constraints(bdd& F)
{
    auto add_t2_c = [&](int k1, int j1, int k2, int j2)
        {
            for (int i = 0; i < N; i++) {
                F &= (!p[k1][i][k2]) | p[k2][i][j2];
            }
        };


	add_t2_c(0, 1, 3, 2); // If property 0 = 1 then property 3 = 2

	add_t2_c(1, 2, 4, 3); // If property 1 = 2 then property 4 = 3
    
	add_t2_c(2, 3, 0, 4); // If property 2 = 3 then property 0 = 4

	add_t2_c(3, 4, 1, 0); // If property 3 = 4 then property 1 = 0
    
}

// ----------------------------- main -----------------------------------

int main()
{
    // Инициализация BuDDy: для N=M=5 
    bdd_init(200'000'00, 200'000'00);
    bdd_setvarnum(N_VAR);

    // Строим p[k][i][j]
    build_p();

    // Исходная функция — истина, постепенно навешиваем ограничения
    bdd F = bddtrue;

    // Общие ограничения
    add_domain_constraints(F);
    add_uniqueness_constraints(F);

    // Ограничения 1-го рода
    add_type1_constraints(F);

    add_type2_constraints(F);

	//add_type3_constraints(F);

	//add_type4_constraints(F);

    // Считаем и выводим решения
    out.open("solutions_small.txt");
    double satcount = bdd_satcount(F);
    out << (long long)satcount << " solutions:\n\n";

    if (satcount > 0.0)
    {
        bdd_allsat(F, allsat_handler);
    }

    out.close();
    bdd_done();

    cout << "Done. Solutions written to solutions_small.txt\n";
    return 0;
}