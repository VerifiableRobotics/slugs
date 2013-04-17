#include <stdlib.h>
#include <stdio.h>

int main() {
// #if 1
#ifndef __x86_64
printf("-mtune=native -malign-double -DHAVE_IEEE_754 -DBSD -DCUDD_COMPILER_OPTIONS_SET\n");
#else
printf("-mtune=native -DHAVE_IEEE_754 -DBSD -DSIZEOF_VOID_P=8 -DSIZEOF_LONG=8 -DCUDD_COMPILER_OPTIONS_SET\n");
#endif
}
