#include <stdio.h>
#ifndef CALCULATOR_H
#define CALCULATOR_H

typedef enum Operation {
    ADDITION = '+',
    SUBTRACTION = '-',
    MULTIPLICATION = '*',
    DIVISION = '/'
} Operation;

//pointer to a math function loaded from a library
typedef double (*BinaryOp)(double, double);

//loaded functions, indexed by operator symbol: func['+'] etc. NULL = not loaded
typedef struct OperationTable {
    BinaryOp func[256];
} OperationTable;

typedef struct CalculatorNode {
    double left;
    double right;
    enum Operation operation;   //symbol, used to choose the priority
    BinaryOp func;              //function that performs the operation
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

//interface function to solve an operation
double solve(double left, double right, BinaryOp func);

//functions to work with char*
int removeSpaces(char* str);
int isValidExpression(const char* expression);
int isDigit(char c);
int isOperator(char c);

//functions to work with CalculatorDoubleLinkedList
int tokenize(char* expression, CalculatorDoubleLinkedList *list, const OperationTable *ops);
int takeOneNumber(char **expression, double *number);
int takeOneOperation(char **expression, Operation *operation, const OperationTable *ops);
int calculateExpression(CalculatorDoubleLinkedList *list, CalculationMode mode, double *result);
void freeList(CalculatorDoubleLinkedList *list);

#endif
