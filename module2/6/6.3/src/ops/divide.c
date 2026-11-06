#include <math.h>

double operation(double a, double b) {
    if (b == 0) {
        return NAN; // Return NaN to indicate an error
    }
    return a / b;
}

const char symbol = '/';
