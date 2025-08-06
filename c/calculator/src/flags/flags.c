#include <stdio.h>
#include <stdbool.h>

#include "flags.h"

void display_help();
void inform_of_misuse(char flag);

void print_flags(short flags) {
    printf("Flags: ");
    for (int i = 15; i >= 0; i--) {
        printf("%d", (flags & (0b1 << i)) > 0 ? 1 : 0);
    }
    printf("\n");
}

bool is_valid_notation(char c);

short parse_flags(int argc, char **argv)
{
    if (argc == 1)
    {
        display_help();
        return -1;
    }
    short result = 0b0;

    for (int i = 1; i < argc; i++)
    {
        char *cur = *(argv + i);
        if (*cur != '-')
            break;
        switch (*(cur + 1))
        {
        case 'n':
            if (*(cur + 2) != '=' || !is_valid_notation(*(cur + 3)))
            {
                inform_of_misuse('n');
                return -1;
            }
            if (*(cur + 3) == 'r')
                result |= (0b1 << 15);
            result |= 0b1;
            break;
        default:
            display_help();
            return -1;
        }
    }

    return result;
}

void display_usage();
void display_flags();

void display_help()
{
    printf("Solves the given equation\n\n");
    display_usage();
    display_flags();
}

void display_usage()
{
    printf("Usage:\n\tCLI_Calculator {flags} ...(equation)\n\n");
}

void display_flags()
{
    printf("Flags:\n");
    printf("\t-h, --help: Displays the help message for this command.\n");
    printf("\t-n, --notation: Converts the expression to a different notation.\n");
    printf("\t-s, --simplify: Simplifies the expression if it can't be solved.\n");
    printf("\n");
}

void inform_of_misuse(char flag)
{
    switch (flag)
    {
    case 'n':
        printf("Expected either -n=p (polish) or -n=r (reverse polish) as a value!\n");
        break;
    }
}

bool is_valid_notation(char param)
{
    return param == 'r' || param == 'p';
}
