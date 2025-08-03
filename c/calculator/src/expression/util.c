#include <string.h>
#include <stdio.h>

#include "util.h"

int powi(int base, unsigned int exp)
{
    if (base == 1)
        return 1;
    if (base == 2)
        return base << exp;

    int p = base;
    int r = 1;
    while (exp > 0)
    {
        if (exp % 2 == 1)
            r *= p;
        p *= p;
        exp /= 2;
    }

    return r;
}

int to_int(char* input, unsigned int start, unsigned int length) {
    int result = 0;

    for (int i = 0; i < length; i++) {
        result *= 10;
        result += *(input + start + i) - '0';
    }

    return result;
}