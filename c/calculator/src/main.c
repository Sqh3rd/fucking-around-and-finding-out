#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "expression.h"
#include "flags.h"

int do_stuff(int length, char **strings, short flags);

int main(int argc, char **argv)
{
    short flags = parse_flags(argc, argv);
    if (flags == -1) return 0;

    print_flags(flags);

    short flag_positions = 1;

    if (flags & 0b1) flag_positions += 1;

    do_stuff(argc - flag_positions, argv + flag_positions, flags);
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

int do_stuff(int length, char **strings, short flags)
{
    if (length <= 0) return -1;
    char *result = malloc(one_d_length(length, strings));

    concat_everything(length, strings, result);
    printf("[EXP] %s\n", result);

    struct Exp *exp = parse(result);

    free(result);
    result = NULL;

    printf("Notated:\n");
    if (flags & 0b1) {
        if ((flags >> 15) & 0b1 > 0) {
            print_styled(exp, REVERSED_POLISH);
        } else {
            print_styled(exp, POLISH);
        }
    } else {
        print(exp);
    }
    printf("\n");

    struct Exp *calculated_result = calculate(exp);

    printf("Result:\n");
    print(calculated_result);
    printf("\n");

    free_exp(exp);
    free_exp(calculated_result);
}
