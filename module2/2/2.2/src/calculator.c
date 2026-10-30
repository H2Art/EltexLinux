#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "calculator.h"
double add(double a, double b) {
    return a + b;
}

double subtract(double a, double b) {
    return a - b;
}

double multiply(double a, double b) {
    return a * b;
}

double divide(double a, double b) {
    if (b == 0) {
        return NAN; // Return NaN to indicate an error
    }
    return a / b;
}
double solve(double left, double right, Operation operation) {
    switch (operation) {
        case ADDITION:
            return add(left, right);
        case SUBTRACTION:
            return subtract(left, right);
        case MULTIPLICATION:
            return multiply(left, right);
        case DIVISION:
            return divide(left, right);
        default:
            return NAN; // Return NaN to indicate an error
    }
}



int removeSpaces(char* str) {
    char* dest = str;
    while (*str != '\0') {
        if (*str != ' ' && *str != '\t' && *str != '\n') { // Skip spaces, tabs, and newlines
            *dest++ = *str;
        }
        str++;
    }
    *dest = '\0'; // Null-terminate the modified string
    return 0; // Return 0 to indicate success
}
int isValidExpression(const char* expression) {
    if (expression == NULL || *expression == '\0') {
        return 0; // Invalid if the expression is NULL or empty
    }

    while (*expression != '\0') {
        if (!(isDigit(*expression) || isOperator(*expression) || *expression == '.')) {
            return 0; // Invalid character found
        }
        expression++;
    }
    return 1; // Valid expression
}

int isOperator(char c) {
    return (c == '+' || c == '-' || c == '*' || c == '/');
}
int isDigit(char c) {
    return (c >= '0' && c <= '9');
}

int tokenize(char *expr, CalculatorDoubleLinkedList *list) {
    double left, right;
    Operation op;

    if (!takeOneNumber(&expr, &left)) return 0;

    while (*expr != '\0') {
        if (!takeOneOperation(&expr, &op)) goto fail;
        if (!takeOneNumber(&expr, &right)) goto fail;

        CalculatorNode *node = malloc(sizeof *node);
        if (!node) goto fail;

        node->left = left;
        node->right = right;
        node->operation = op;
        node->next = NULL;
        node->prev = list->tail;

        if (list->tail) list->tail->next = node;
        else            list->head = node;
        list->tail = node;
        list->size++;

        left = right; // operands dublicates through nodes
    }
    return 1;

fail:
    freeList(list); //free all allocated nodes in case of failure
    return 0;
}

int takeOneNumber(char **expression, double *number) {
    char *endPtr;
    *number = strtod(*expression, &endPtr);
    if (endPtr == *expression) {
        return 0; // No number found
    }
    *expression = endPtr; // Move the pointer forward
    return 1; // Number successfully parsed
}

int takeOneOperation(char **expression, Operation *operation) {
    if (isOperator(**expression)) {
        *operation = (Operation)**expression;
        (*expression)++; // Move the pointer forward
        return 1; // Operation successfully parsed
    }
    return 0; // No valid operation found
}



int calculateExpression(CalculatorDoubleLinkedList *list, CalculationMode mode, double *result) {
    CalculatorNode *n = list->head;
    while (n) {
        CalculatorNode *next = n->next;
        int isMD = (n->operation == MULTIPLICATION || n->operation == DIVISION);

        if ((mode == MULTIPLICATION_DIVISION) == isMD) {
            double v = solve(n->left, n->right, n->operation);
            if (isnan(v)) return 0;              //handle division by zero or other errors
            if (n->prev) n->prev->right = v;     //updating links
            if (n->next) n->next->left = v;
            *result = v;
            removeNode(list, n);
        }
        n = next;
    }
    return 1;
}
static void removeNode(CalculatorDoubleLinkedList *list, CalculatorNode *n) {
    if (n->prev) n->prev->next = n->next; else list->head = n->next;
    if (n->next) n->next->prev = n->prev; else list->tail = n->prev;
    free(n);
    list->size--;
}

void freeList(CalculatorDoubleLinkedList *list) {
    while (list->head) removeNode(list, list->head);
}
