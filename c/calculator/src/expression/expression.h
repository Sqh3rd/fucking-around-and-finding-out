#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "structs.h"
#include "parse.h"
#include "print.h"
#include "calculate.h"

typedef enum EXPRESSION_ERRS {
    EXPRESSION_ERRS_OK = 0,
    EXPRESSION_ERRS_VALIDATION_FAILED,
    EXPRESSION_ERRS_PARSING_FAILED,
} EXPRESSION_ERRS;

#endif