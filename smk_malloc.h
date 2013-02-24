/*
	libsmacker - A C library for decoding .smk Smacker Video files
	Copyright (C) 2012-2013 Greg Kennedy

	See smacker.h for more information.

	smk_malloc.h
		"Safe" implementations of malloc and free.
*/
	
#ifndef SMK_MALLOC_H
#define SMK_MALLOC_H

#include <stdlib.h>
#include <stdio.h>

#define smk_malloc(p,x) { p = malloc(x); if (p == NULL) { fprintf(stderr,"smk_malloc:: ERROR: malloc(x) returned NULL (file: __FILE__, line: __LINE__)\n"); exit(EXIT_FAILURE); } }

#define smk_free(x) { if (x != NULL) { free(x); x = NULL; } }

#endif
