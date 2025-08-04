#include <stdlib.h>

#include "structs.h"

void free_exp(struct Exp *exp);

void free_op(struct Op *op)
{
    if (op == NULL)
        return;
    free_exp(op->left);
    free_exp(op->right);
    free(op);
}

void free_exp(struct Exp *exp)
{
    if (exp == NULL)
        return;
    switch (exp->type)
    {
    case OP:
        free_op(exp->op);
        break;
    case TERM:
        free(exp->term);
        break;
    case GROUP:
        free_exp(exp->grouped);
    }
    free(exp);
}

