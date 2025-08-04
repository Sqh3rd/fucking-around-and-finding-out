#include <stdlib.h>
#include <stdio.h>

#include "print.h"

void print(struct Exp *exp);

void print_op(struct Op *op);
void print_term(struct Term *term);

void print(struct Exp *exp)
{
    if (exp == NULL)
        return;
    switch (exp->type)
    {
    case OP:
        print_op(exp->op);
        break;
    case TERM:
        print_term(exp->term);
        break;
    case GROUP:
        printf("(");
        print(exp->grouped);
        printf(")");
    }
}

void print_op(struct Op *op)
{
    if (op == NULL)
        return;
    print(op->left);
    printf(" %c ", op->operand);
    print(op->right);
}

void print_term(struct Term *term)
{
    if (term == NULL)
        return;
    if (term->type == VAR)
        printf("%c", term->var);
    if (term->type == NUM)
        printf("%d", term->num);
}

void print_styled(struct Exp *exp, enum PrintStyle style);
void print_op_styled(struct Op *op, enum PrintStyle style);
void print_term_styled(struct Term *term, enum PrintStyle style);

void print_styled(struct Exp *exp, enum PrintStyle style)
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
    else if (exp->type == GROUP)
    {
        print_styled(exp->grouped, style);
    }
}

void print_op_styled(struct Op *op, enum PrintStyle style)
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

void print_term_styled(struct Term *term, enum PrintStyle style)
{
    if (term == NULL)
        return;
    if (term->type == VAR)
        printf("%c", term->var);
    if (term->type == NUM)
        printf("%d", term->num);
}