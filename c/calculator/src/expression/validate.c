#include <stdio.h>
#include <stdlib.h>

#include "validate.h"

int pre_parse_validation(char *input) {
    int parentheses = 0;
    for (int i = 0; i < strlen(input); i++) {
        char cur = *(input + i);

        if (cur != '(' && cur != ')' && (cur - '0' < 0 || cur - '0' >= 10)) {
            printf("Error: ")
        }
    }
}

int post_parse_validation(struct Exp *exp) {}
