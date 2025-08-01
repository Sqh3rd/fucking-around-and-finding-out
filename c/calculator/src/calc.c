#include <stdio.h>
#include <string.h>
#include <stdlib.h>

enum TermType
{
    EXP,
    NUM,
    VAR
};

struct Term
{
    struct Term *left;
    struct Term *right;
    enum TermType type;
    union
    {
        char exp;
        char var;
        int num;
    };
};

const char MULT = '*';
const char ADD = '+';
const char SUB = '-';
const char DIV = '/';

struct Term *parse_expression(char *expression)
{
    int length = strlen(expression);
    int depth = 0;

    struct Term *term = malloc(sizeof(struct Term));

    for (int i = 0; i < length; i++)
    {
        switch (*(expression + i))
        {
        case '(':
            term->left = parse_expression(expression + i);
            depth = 0;
            break;

        default:
            break;
        }
    }
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

int unify(int length, char **strings)
{
    char *result = malloc(one_d_length(length, strings));

    concat_everything(length, strings, result);
    printf("Expression: %s\n", result);

    free(result);
    result = NULL;
}

int main(int argc, char *argv[])
{
    if (argc == 1)
    {
        printf("Nothing to calculate...\n");
        return 0;
    }

    unify(argc - 1, argv + 1);
    return 0;
}
