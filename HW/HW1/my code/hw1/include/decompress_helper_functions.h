#include <stdio.h>
#include "custom_constants.h"
#include "debug.h"
#include "const.h"

int validateHeader(int fieldType,FILE *in);

int decompressBlock(FILE *in,FILE*out);

int decompressRule(FILE *in);

SYMBOL* addSymbol(int value);

void addSymbolToTheRule(SYMBOL* newSymbol, SYMBOL* ruleHead);

int get_utf8(FILE * in, int i);

void expandRule(SYMBOL* rule,FILE* out);