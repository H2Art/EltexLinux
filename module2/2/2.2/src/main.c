#include <stdio.h>
#include "calculator.h"
#include <string.h>
#include <stdlib.h>
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
        if (!isValidExpression(input) || !tokenize(input, &list)) {
            printf("Invalid expression.\n");
            freeList(&list);
            continue;
        }
        double result = 0;
        if (list.size == 0) {
            result = strtod(input, NULL);
        } else if (!calculateExpression(&list, MULTIPLICATION_DIVISION, &result) ||
                !calculateExpression(&list, ADDITION_SUBTRACTION, &result)) {
            printf("Error (division by zero).\n");
            freeList(&list);
            continue;
        }
        printf("Result: %g\n", result);
        freeList(&list);
    }
    return 0;
}