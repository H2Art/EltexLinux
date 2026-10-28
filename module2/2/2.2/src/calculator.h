#include <stdio.h>
#define CALCULATOR_H
double add(double a, double b);
double subtract(double a, double b);
double multiply(double a, double b);
double divide(double a, double b);
typedef struct Operation {
    double (*func)(double, double);
    char symbol;
} Operation;