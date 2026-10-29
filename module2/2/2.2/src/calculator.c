#include <stdio.h>
#include <math.h>
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
        fprintf(stderr, "Error: Division by zero is not allowed.\n");
        return NAN; // Return NaN to indicate an error
    }
    return a / b;
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
        if (!((*expression >= '0' && *expression <= '9') || 
              *expression == '+' || 
              *expression == '-' || 
              *expression == '*' || 
              *expression == '/' || 
              *expression == '.')) {
            return 0; // Invalid character found
        }
        expression++;
    }
    return 1; // Valid expression
}

int isOperator(char c) {
    return (c == '+' || c == '-' || c == '*' || c == '/');
}


