#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "expression.h"

int do_stuff(int length, char **strings);

int main(int argc, char *argv[])
{
    if (argc == 1)
    {
        printf("Nothing to calculate...\n");
        return 0;
    }

    do_stuff(argc - 1, argv + 1);
    return 0;
}

int one_d_length(int length, char **strings)
{
    int total_length = 0;
    for (int i = 0; i < length; i++)
    {
        total_length += strlen(*(strings + i));
    }
    return total_length;
}

void concat_everything(int length, char **strings, char *result)
{
    strcpy(result, *strings);
    for (int i = 1; i < length; i++)
    {
        strcat(result, *(strings + i));
    }
}

int do_stuff(int length, char **strings)
{
    char *result = malloc(one_d_length(length, strings));

    concat_everything(length, strings, result);
    printf("[EXP] %s\n", result);

    struct Exp *exp = parse(result);

    free(result);
    result = NULL;

    printf("Term:\n");
    print(exp);
    printf("\n");

    printf("Polish:\n");
    print_styled(exp, POLISH);
    printf("\n");

    struct Exp *calculated_result = calculate(exp);

    printf("Result:\n");
    print(calculated_result);
    printf("\n");

    free_exp(exp);
    free_exp(calculated_result);
    exp = NULL;
}
