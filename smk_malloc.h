#ifndef SMK_MALLOC_H
#define SMK_MALLOC_H

#include <stdlib.h>
#include <stdio.h>

#define smk_malloc(p,x) { p = malloc(x); if (p == NULL) { fprintf(stderr,"smk_malloc:: ERROR: malloc(x) returned NULL (file: __FILE__, line: __LINE__)\n"); exit(EXIT_FAILURE); }}

#define smk_free(x) { if (x != NULL) { free(x); x = NULL; } }

#endif
