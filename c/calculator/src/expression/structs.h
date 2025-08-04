#ifndef STRUCTS_H
#define STRUCTS_H

struct Exp;
struct Op;
struct Term;

void free_exp(struct Exp *exp);

enum ExpType
{
    OP,
    TERM,
    GROUP,
    NA
};

struct Exp
{
    enum ExpType type;
    union
    {
        struct Op *op;
        struct Term *term;
        struct Exp *grouped;
    };
};

struct Op
{
    char operand;
    struct Exp *left;
    struct Exp *right;
};

enum TermType
{
    NUM,
    VAR,
    EMPTY
};

struct Term
{
    enum TermType type;
    union
    {
        char var;
        int num;
    };
};

#endif
