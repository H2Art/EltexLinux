#include <stdio.h>
#define CALCULATOR_H
double add(double a, double b);
double subtract(double a, double b);
double multiply(double a, double b);
double divide(double a, double b);

double solve(double left, double right, Operation operation);

typedef enum {
    ADDITION = '+',
    SUBTRACTION = '-',
    MULTIPLICATION = '*',
    DIVISION = '/'
} Operation;

typedef struct {
    double left;
    double right;
    Operation operation;
} CalculatorNode;

typedef struct {
    CalculatorNode* head;
    CalculatorNode* tail;
    size_t size;
} CalculatorDoubleLinkedList;