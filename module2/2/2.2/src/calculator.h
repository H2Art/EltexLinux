#include <stdio.h>
#ifndef CALCULATOR_H
#define CALCULATOR_H

typedef enum Operation {
    ADDITION = '+',
    SUBTRACTION = '-',
    MULTIPLICATION = '*',
    DIVISION = '/'
} Operation;

typedef struct CalculatorNode {
    double left;
    double right;
    enum Operation operation;
    struct CalculatorNode* next;
    struct CalculatorNode* prev;
} CalculatorNode;

typedef struct CalculatorDoubleLinkedList {
    CalculatorNode* head;
    CalculatorNode* tail;
    size_t size;
} CalculatorDoubleLinkedList;

double add(double a, double b);
double subtract(double a, double b);
double multiply(double a, double b);
double divide(double a, double b);

double solve(double left, double right, Operation operation);

//functions to work with char*
int removeSpaces(char* str);
int isValidExpression(const char* expression);

#endif