#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "validate.h"

int pre_parse_validation(char *input)
{
    int open_parantheses = 0;
    int parentheses = 0;
    for (int i = 0; i < strlen(input); i++)
    {
        char cur = *(input + i);

        if (cur == '(')
        {
            if (parentheses == 0)
                open_parantheses = i;
            parentheses++;
        }
        else if (cur == ')')
        {
            parentheses--;
            if (parentheses < 0)
            {
                printf("%s\n", input);
                for (int j = 0; j < i; j++)
                    printf(" ");
                printf("~\n");
                printf("Invalid ')' at pos: %d\n", i);
                return -1;
            }
        }
    }

    if (parentheses > 0)
    {
        printf("%s\n", input);
        for (int j = 0; j < open_parantheses; j++)
            printf(" ");
        printf("~\n");
        printf("Unclosed '(' at pos: %d\n", open_parantheses);
        return -1;
    }
}

int post_parse_validation(struct Exp *exp) {}
