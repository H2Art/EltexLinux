#include <stdio.h>
#include "calculator.h"

int debug() {
    char expression[] = "3 + 5 * 2 - 8 / 4";
    removeSpaces(expression);
    printf("%s\n", isValidExpression(expression) ? "Valid expression" : "Invalid expression");


    return 0;
}
int main() {
    printf("Calculator program started.\n");
    debug();
    return 0;
}