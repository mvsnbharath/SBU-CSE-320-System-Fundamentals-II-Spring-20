#include <stdio.h>
#include "custom_constants.h"
#include "debug.h"
#include "const.h"

void printHeader(unsigned int i,FILE* out);

void printInUtf8FromInt(unsigned int i,FILE * out);

void formRulesAndCompressData(FILE *in, FILE *out, int bsize);

void createRulesWithHeaders(FILE* out);

void printRule(SYMBOL* ruleHead,FILE* out);