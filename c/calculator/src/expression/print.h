#ifndef PRINT_H
#define PRINT_H

#include "structs.h"

enum PrintStyle {
    POLISH,
    REVERSED_POLISH
};

void print(Exp* exp);
void print_styled(Exp* exp, enum PrintStyle style);

#endif