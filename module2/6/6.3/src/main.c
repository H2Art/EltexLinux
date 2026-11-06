#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "calculator.h"
#include "loader.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <libraries directory>\n", argv[0]);
        return 1;
    }

    OperationTable ops = {0};
    Loader loader = {0};
    int loaded = loadOperations(argv[1], &ops, &loader);
    if (loaded < 0) { fprintf(stderr, "Cannot open directory '%s'\n", argv[1]); return 1; }
    if (loaded == 0) { fprintf(stderr, "No operations found in '%s'\n", argv[1]); return 1; }

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
        if (!isValidExpression(input, &ops) || !tokenize(input, &list, &ops)) {
            printf("Invalid expression or operation is not loaded.\n");
            freeList(&list);
            continue;
        }
        double result = 0;
        if (list.size == 0) {
            result = strtod(input, NULL);
        } else if (!calculateExpression(&list, PRIORITY_HIGH, &result) ||
                !calculateExpression(&list, PRIORITY_LOW, &result)) {
            printf("Error (division by zero).\n");
            freeList(&list);
            continue;
        }
        printf("Result: %g\n", result);
        freeList(&list);
    }
    unloadOperations(&loader);
    return 0;
}
