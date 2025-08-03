#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "structs.h"
#include "parse.h"
#include "util.h"

Exp *parse(char *input);
Exp *parse_recursive(char *input, int lowerbound, int upperbound);

Exp *parse(char *input)
{
    return parse_recursive(input, 0, strlen(input));
}

Exp *parse_num(char *nums, unsigned int start, unsigned int length)
{
    if (nums == NULL)
        return NULL;

    Exp *num_as_term = malloc(sizeof(struct Exp));
    num_as_term->type = TERM;
    num_as_term->term = malloc(sizeof(struct Term));

    num_as_term->term->type = NUM;
    num_as_term->term->num = to_int(nums, start, length);

    return num_as_term;
}

Exp *parse_var(char var)
{
    if (var == 0)
        return NULL;
    Exp *var_as_term = malloc(sizeof(Exp));
    var_as_term->type = TERM;

    var_as_term->term = malloc(sizeof(struct Term));

    var_as_term->term->type = VAR;
    var_as_term->term->var = var;

    return var_as_term;
}

Exp *parse_term(char *input, int lowerbound, int upperbound)
{
    if (lowerbound >= upperbound)
        return NULL;

    printf("Parsing term: ");
    for (int i = lowerbound; i < upperbound; i++) {
        printf("%c", *(input + i));
    }
    printf("\n");

    char *nums = malloc(upperbound - lowerbound);
    int num_index = 0;

    struct Exp *result = malloc(sizeof(struct Exp));
    result->type = TERM;

    char potential_var = '\0';

    for (int i = lowerbound; i < upperbound; i++)
    {
        char cur = *(input + i);
        if (potential_var != '\0')
        {
            result->type = OP;
            result->op = malloc(sizeof(struct Op));
            result->op->operand = '*';

            result->op->left = parse_var(potential_var);

            result->op->right = parse_recursive(input, i, upperbound);
            return result;
        }

        if (cur - '0' >= 0 && cur - '0' < 10)
        {
            *(nums + num_index++) = cur;
        }
        else if (cur - 'a' >= 0 && cur - 'z' <= 0)
        {
            if (num_index != 0)
            {
                result->type = OP;
                result->op = malloc(sizeof(struct Op));
                result->op->operand = '*';
                result->op->left = parse_num(input, lowerbound, num_index);
                result->op->right = parse_recursive(input, i, upperbound);
                return result;
            }
            potential_var = cur;
        } else {
            printf("Un-parseable character encountered: %c\n", cur);
        }
    }
    if (potential_var != '\0')
        return parse_var(potential_var);

    if (num_index != 0)
        return parse_num(input, lowerbound, num_index);
    return NULL;
}

struct Exp *parse_simple(char *input, int lowerbound, int upperbound, char operand)
{
    if (lowerbound >= upperbound)
        return NULL;

    struct Exp *result = malloc(sizeof(struct Exp));
    result->type = OP;

    for (int i = lowerbound; i < upperbound; i++)
    {
        char cur = *(input + i);

        if (cur == operand)
        {
            result->op = malloc(sizeof(struct Op));
            result->op->left = parse_recursive(input, lowerbound, i);
            result->op->right = parse_recursive(input, i + 1, upperbound);
            result->op->operand = operand;
            return result;
        }
    }

    free(result);
    return NULL;
}

struct Exp *parse_recursive(char *input, int lowerbound, int upperbound)
{
    if (lowerbound >= upperbound)
        return NULL;
    struct Exp *add = parse_simple(input, lowerbound, upperbound, '+');
    if (add != NULL)
        return add;
    struct Exp *sub = parse_simple(input, lowerbound, upperbound, '-');
    if (sub != NULL)
        return sub;
    struct Exp *mult = parse_simple(input, lowerbound, upperbound, '*');
    if (mult != NULL)
        return mult;
    struct Exp *div = parse_simple(input, lowerbound, upperbound, '/');
    if (div != NULL)
        return div;
    struct Exp *term = parse_term(input, lowerbound, upperbound);
    return term;
}
