/**
	libsmacker - A C library for decoding .smk Smacker Video files
	Copyright (C) 2012-2021 Greg Kennedy

	See smacker.h for more information.

	smk_huff8.h
		SMK huffmann trees.  There are two types:
		- a basic 8-bit tree, and
		- a "big" 16-bit tree which includes a cache for recently
			searched values.
*/

#ifndef SMK_HUFF8_H
#define SMK_HUFF8_H

#include "smk_bitstream.h"

/* forward declaration */
struct smk_huff8_t;

/*********************** 8-BIT HUFF-TREE FUNCTIONS ***********************/
/** This macro checks return code from _smk_huff8_build and
	jumps to error label if problems occur. */
#define smk_huff8_build(bs,t) \
{ \
	if (!(t = _smk_huff8_build(bs))) \
	{ \
		fprintf(stderr, "libsmacker::smk_huff8_build(" #bs ", " #t ") - ERROR (file: %s, line: %lu)\n", __FILE__, (unsigned long)__LINE__); \
		goto error; \
	} \
}
/** Build an 8-bit tree from a bitstream */
struct smk_huff8_t* _smk_huff8_build(struct smk_bit_t* bs);

/** This macro checks return code from _smk_huff8_lookup and
	jumps to error label if problems occur. */
#define smk_huff8_lookup(bs,t,s) \
{ \
	if ((short)(s = _smk_huff8_lookup(bs, t)) < 0) \
	{ \
		fprintf(stderr, "libsmacker::smk_huff8_lookup(" #bs ", " #t ", " #s ") - ERROR (file: %s, line: %lu)\n", __FILE__, (unsigned long)__LINE__); \
		goto error; \
	} \
}
/** Look up an 8-bit value in the referenced tree by following a bitstream
	returns -1 on error */
short _smk_huff8_lookup(struct smk_bit_t* bs, const struct smk_huff8_t* t);

/** Free an 8-bit tree */
void smk_huff8_free(struct smk_huff8_t* t);

#endif
