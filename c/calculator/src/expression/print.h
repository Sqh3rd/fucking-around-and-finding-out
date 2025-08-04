#ifndef PRINT_H
#define PRINT_H

#include "structs.h"

enum PrintStyle {
    POLISH,
    REVERSED_POLISH
};

void print(struct Exp* exp);
void print_styled(struct Exp* exp, enum PrintStyle style);

#endif