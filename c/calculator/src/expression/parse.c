#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "parse.h"
#include "util.h"

struct Exp *parse(char *input);
struct Exp *parse_recursive(char *input, int lowerbound, int upperbound);

struct Exp *parse(char *input)
{
    return parse_recursive(input, 0, strlen(input));
}

struct Exp *parse_num(char *nums, unsigned int start, unsigned int length)
{
    if (nums == NULL)
        return NULL;

    struct Exp *num_as_term = malloc(sizeof(struct Exp));
    num_as_term->type = TERM;
    num_as_term->term = malloc(sizeof(struct Term));

    num_as_term->term->type = NUM;
    num_as_term->term->num = to_int(nums, start, length);

    return num_as_term;
}

struct Exp *parse_var(char var)
{
    if (var == 0)
        return NULL;
    
    printf("[PRSV] %c", var);
    struct Exp *var_as_term = malloc(sizeof(struct Exp));
    var_as_term->type = TERM;

    var_as_term->term = malloc(sizeof(struct Term));

    var_as_term->term->type = VAR;
    var_as_term->term->var = var;

    return var_as_term;
}

struct Exp *parse_term(char *input, int lowerbound, int upperbound)
{
    if (lowerbound >= upperbound)
        return NULL;

    int nums_length = 0;

    struct Exp *group = malloc(sizeof(struct Exp));
    group->type = NA;

    char potential_var = '\0';
    int group_start = 0;
    int depth = 0;

    printf("[PRST]");
    for (int i = 0; i < lowerbound; i++)
        printf("  ");
    for (int i = lowerbound; i < upperbound; i++)
        printf(" %c", *(input + i));
    printf("\n");


    for (int i = lowerbound; i < upperbound; i++)
    {
        char cur = *(input + i);

        // There was already a var parsed
        if (depth == 0 && (potential_var != '\0'
                           // There were numbers, but not anymore
                           || (nums_length != 0 && (cur - '0' < 0 || cur - '0' >= 10))
                           // The previous thing was a group
                           || (group->type == GROUP)))
        {
            struct Exp *result = malloc(sizeof(struct Exp));
            result->type = OP;
            result->op = malloc(sizeof(struct Op));
            result->op->operand = '*';

            if (potential_var != '\0')
                result->op->left = parse_var(potential_var);
            else if (nums_length != 0)
                result->op->left = parse_num(input, lowerbound, nums_length);
            else
                result->op->left = group;

            result->op->right = parse_recursive(input, i + 1, upperbound);

            if (group->type == NA)
                free_exp(group);
            return result;
        }

        if (cur == '(')
        {
            if (depth == 0)
                group_start = i;
            depth++;
            continue;
        }
        else if (cur == ')')
        {
            depth--;
            if (depth != 0)
                continue;
            group->type = GROUP;
            group->grouped = parse_recursive(input, group_start + 1, i);
            continue;
        }

        if (depth != 0)
            continue;

        if (cur - '0' >= 0 && cur - '0' < 10)
        {
            nums_length++;
        }
        else if (cur - 'a' >= 0 && cur - 'z' <= 0)
        {
            potential_var = cur;
        }
    }

    if (nums_length != 0)
        return parse_num(input, lowerbound, nums_length);

    if (potential_var != '\0')
        return parse_var(potential_var);

    if (group->type == GROUP)
        return group;

    free_exp(group);
    return NULL;
}

struct Exp *parse_operation(char *input, int lowerbound, int upperbound)
{
    if (lowerbound >= upperbound)
        return NULL;

    printf("[PRSO]");
    for (int i = 0; i < lowerbound; i++)
        printf("  ");
    for (int i = lowerbound; i < upperbound; i++)
        printf(" %c", *(input + i));
    printf("\n");

    struct Exp *result = malloc(sizeof(struct Exp));
    result->type = OP;

    struct Exp *candidate = malloc(sizeof(struct Exp));
    candidate->type = NA;
    int candidate_pos = 0;

    int depth = 0;

    for (int i = lowerbound; i < upperbound; i++)
    {
        char cur = *(input + i);

        if (cur == '(')
        {
            depth++;
        }
        else if (cur == ')')
        {
            depth--;
        }
        if (depth != 0)
            continue;

        if (cur == '+' || cur == '-')
        {
            result->op = malloc(sizeof(struct Op));
            if (candidate->type != NA)
            {
                result->op->left = candidate;
                candidate->op->right = parse_recursive(input, candidate_pos + 1, i);
            }
            else
            {
                result->op->left = parse_recursive(input, lowerbound, i);
            }
            result->op->right = parse_recursive(input, i + 1, upperbound);
            result->op->operand = cur;
            return result;
        }

        if (cur == '*' || cur == '/')
        {
            if (candidate->type == OP)
                continue;
            candidate_pos = i;
            candidate->type = OP;
            candidate->op = malloc(sizeof(struct Op));
            candidate->op->left = parse_recursive(input, lowerbound, i);
            candidate->op->operand = cur;
        }
    }

    free(result);
    if (candidate->type != NA) {
        candidate->op->right = parse_recursive(input, candidate_pos + 1, upperbound);
        return candidate;
    }
    free(candidate);
    return NULL;
}

struct Exp *parse_recursive(char *input, int lowerbound, int upperbound)
{
    if (lowerbound >= upperbound)
        return NULL;

    struct Exp *operation = parse_operation(input, lowerbound, upperbound);
    if (operation != NULL)
        return operation;
    struct Exp *term = parse_term(input, lowerbound, upperbound);
    return term;
}
