#include <stdlib.h>

#include "calculate.h"

Exp *calculate(Exp *exp);

Exp *calculate(Exp *exp)
{
    if (exp == NULL)
        return NULL;

    if (exp->type != OP)
        return exp;

    Exp *result = malloc(sizeof(Exp));
    Exp *left = calculate(exp->op->left);
    Exp *right = calculate(exp->op->right);

    if (left->type != TERM || left->term->type != NUM)
        return NULL;
    if (right->type != TERM || right->term->type != NUM)
        return NULL;

    if (left->term->type == NUM && right->term->type == NUM)
    {
        int left_num = left->term->num;
        int right_num = right->term->num;

        result->type = TERM;
        result->term = malloc(sizeof(Term));
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

        result->term->num = calculated_num;
        return result;
    }

    free(result)

    return NULL;
}