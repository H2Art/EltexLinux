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