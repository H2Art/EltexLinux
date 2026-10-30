#include <stdio.h>
#include "calculator.h"
#include <string.h>

int debug() {
    char expression[] = "3 + 5 * 2 - 8 / 4";
    removeSpaces(expression);
    printf("%s\n", isValidExpression(expression) ? "Valid expression" : "Invalid expression");


    return 0;
}
int main() {
    printf("Calculator program started.\n");
    char input[256];
    CalculatorDoubleLinkedList list = {NULL, NULL, 0};
    while (true) {
        printf("Enter an expression (or 'quit' to exit): ");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        // Process the input expression
        if (strncmp(input, "quit", 4) == 0) {
            break;
        }
        removeSpaces(input);
        if (!isValidExpression(input)) {
            printf("Invalid expression. Please try again.\n");
            continue;
        }
        tokenize(input, list);
        double result = 0;
        calculateExpression(list, MULTIPLICATION_DIVISION, &result);
        calculateExpression(list, ADDITION_SUBTRACTION, &result);
        printf("Result: %lf\n", result);
    }
    debug();
    return 0;
}