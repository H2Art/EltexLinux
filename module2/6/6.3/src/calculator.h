#include <stdio.h>
#ifndef CALCULATOR_H
#define CALCULATOR_H

#include "loader.h"

typedef struct CalculatorNode {
    double left;
    double right;
    char operation;             //operator symbol
    int  priority;              //1 = evaluated first, 2 = evaluated second
    BinaryOp func;              //function that performs the operation
    struct CalculatorNode* next;
    struct CalculatorNode* prev;
} CalculatorNode;

typedef struct CalculatorDoubleLinkedList {
    CalculatorNode* head;
    CalculatorNode* tail;
    size_t size;
} CalculatorDoubleLinkedList;

//interface function to solve an operation
double solve(double left, double right, BinaryOp func);

//functions to work with char*
int removeSpaces(char* str);
int isValidExpression(const char* expression, const OperationTable *ops);
int isDigit(char c);

//functions to work with CalculatorDoubleLinkedList
int tokenize(char* expression, CalculatorDoubleLinkedList *list, const OperationTable *ops);
int takeOneNumber(char **expression, double *number);
int takeOneOperation(char **expression, char *operation, const OperationTable *ops);
int calculateExpression(CalculatorDoubleLinkedList *list, int priority, double *result);
void freeList(CalculatorDoubleLinkedList *list);

#endif
