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

typedef enum CalculationMode {
    ADDITION_SUBTRACTION,
    MULTIPLICATION_DIVISION
} CalculationMode;

double add(double a, double b);
double subtract(double a, double b);
double multiply(double a, double b);
double divide(double a, double b);

double solve(double left, double right, Operation operation);

//functions to work with char*
int removeSpaces(char* str);
int isValidExpression(const char* expression);
int isDigit(char c);
int isOperator(char c);
//functions to work with CalculatorDoubleLinkedList
int tokenize(char* expression, CalculatorDoubleLinkedList list);
int takeOneNumber(char **expression, double *number);
int takeOneOperation(char **expression, Operation *operation);
int calculateExpression(CalculatorDoubleLinkedList list, CalculationMode mode, double *result);


#endif