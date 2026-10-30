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

int tokenize(char *expression, CalculatorDoubleLinkedList list) {
    double leftNumber = 0;
    double rightNumber = 0;
    Operation operation = 0;

    while (*expression != '\0') {
        if (!takeOneNumber(&expression, &leftNumber)) {
            return 0; // Failed to parse a number
        }
        if (!takeOneOperation(&expression, &operation)) {
            return 0; // Failed to parse an operation
        }
        if (!takeOneNumber(&expression, &rightNumber)) {
            return 0; // Failed to parse a number
        }
        //taken one number, one operation, and one number, now we can create a node and add it to the list

        CalculatorNode* newNode = (CalculatorNode*)malloc(sizeof(CalculatorNode));
        if (newNode == NULL) {
            return 0; // Memory allocation failed 
        }

        newNode->left = leftNumber;
        newNode->right = rightNumber;
        newNode->operation = operation;

        if (list.size == 0) {
            newNode->next = NULL;
            newNode->prev = NULL;
            list.head = newNode;
            list.tail = newNode;
        } else {
            newNode->next = NULL;
            newNode->prev = list.tail;
            list.tail->next = newNode;
            list.tail = newNode;
        }
        list.size++;
        leftNumber = rightNumber; // Prepare for the next operation
    }
    return 1; // Successfully tokenized the expression

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



int calculateExpression(CalculatorDoubleLinkedList list, CalculationMode mode, double *result) {
    if (list.size == 0) {
        return 0; // No operations to perform
    }

    CalculatorNode* currentNode = list.head;
    *result = currentNode->left; // Start with the left number of the first node

    while (currentNode != NULL) {
        if (mode == ADDITION_SUBTRACTION && (currentNode->operation == MULTIPLICATION || currentNode->operation == DIVISION)) {
            // Skip multiplication and division in addition/subtraction mode
            currentNode = currentNode->next;
            continue;
        }
        if (mode == MULTIPLICATION_DIVISION && (currentNode->operation == ADDITION || currentNode->operation == SUBTRACTION)) {
            // Skip addition and subtraction in multiplication/division mode
            currentNode = currentNode->next;
            continue;
        }
        *result = solve(*result, currentNode->right, currentNode->operation);
        currentNode = currentNode->next;
    }
    return 1; // Successfully calculated the expression
}


