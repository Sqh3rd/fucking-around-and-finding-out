#include <stdlib.h>
#include <stdio.h>

#include "calculate.h"

struct Exp *calculate(struct Exp *exp);

struct Exp *calculate(struct Exp *exp)
{
    if (exp == NULL)
        return NULL;

    if (exp->type == GROUP)
        return calculate(exp->grouped);

    if (exp->type == TERM)
        return exp;

    struct Exp *result = malloc(sizeof(struct Exp));
    struct Exp *left = calculate(exp->op->left);
    struct Exp *right = calculate(exp->op->right);

    if (left->type != TERM || left->term->type != NUM)
        return NULL;
    if (right->type != TERM || right->term->type != NUM)
        return NULL;

    if (left->term->type == NUM && right->term->type == NUM)
    {
        int left_num = left->term->num;
        int right_num = right->term->num;

        result->type = TERM;
        result->term = malloc(sizeof(struct Term));
        result->term->type = NUM;

        int calculated_num = 0;
        switch (exp->op->operand)
        {
        case '+':
            calculated_num = left_num + right_num;
            break;
        case '-':
            calculated_num = left_num - right_num;
            break;
        case '*':
            calculated_num = left_num * right_num;
            break;
        case '/':
            calculated_num = left_num / right_num;
            break;
        }

        printf("%d %c %d = %d\n", left_num, exp->op->operand, right_num, calculated_num);

        result->term->num = calculated_num;
        return result;
    }

    free(result);

    return NULL;
}