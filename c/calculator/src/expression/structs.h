typedef struct Exp Exp;
typedef struct Op Op;
typedef struct Term Term;

void free_exp(Exp *exp);

enum ExpType
{
    OP,
    TERM,
    GROUP,
    NA
};

typedef struct Exp
{
    enum ExpType type;
    union
    {
        struct Op *op;
        struct Term *term;
        struct Exp *grouped;
    };
} Exp;

typedef struct Op
{
    char operand;
    struct Exp *left;
    struct Exp *right;
} Op;

enum TermType
{
    NUM,
    VAR,
    EMPTY
};

typedef struct Term
{
    enum TermType type;
    union
    {
        char var;
        int num;
    };
} Term;


