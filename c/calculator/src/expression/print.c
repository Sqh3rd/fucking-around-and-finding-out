#include <stdlib.h>
#include <stdio.h>

#include "print.h"

void print(Exp *exp);

void print_op(Op *op);
void print_term(Term *term);

void print(Exp *exp)
{
    if (exp == NULL)
        return;
    if (exp->type == OP)
    {
        print_op(exp->op);
    }
    else if (exp->type == TERM)
    {
        print_term(exp->term);
    }
}

void print_op(Op *op)
{
    if (op == NULL)
        return;
    print(op->left);
    printf(" %c ", op->operand);
    print(op->right);
}

void print_term(Term *term)
{
    if (term == NULL)
        return;
    if (term->type == VAR)
        printf("%c", term->var);
    if (term->type == NUM)
        printf("%d", term->num);
}

void print_styled(Exp *exp, enum PrintStyle style);
void print_op_styled(Op *op, enum PrintStyle style);
void print_term_styled(Term *term, enum PrintStyle style);

void print_styled(Exp *exp, enum PrintStyle style)
{
    if (exp == NULL)
        return;
    if (exp->type == OP)
    {
        print_op_styled(exp->op, style);
    }
    else if (exp->type == TERM)
    {
        print_term_styled(exp->term, style);
    }
}

void print_op_styled(Op *op, enum PrintStyle style)
{
    if (op == NULL)
        return;

    switch (style)
    {
    case POLISH:
        printf("%c ", op->operand);
        print_styled(op->left, style);
        printf(" ");
        print_styled(op->right, style);
        printf(" ");
        break;
    case REVERSED_POLISH:
        print_styled(op->left, style);
        printf(" ");
        print_styled(op->right, style);
        printf(" %c ", op->operand);
        break;
    }
}

void print_term_styled(Term *term, enum PrintStyle style)
{
    if (term == NULL)
        return;
    if (term->type == VAR)
        printf("%c", term->var);
    if (term->type == NUM)
        printf("%d", term->num);
}