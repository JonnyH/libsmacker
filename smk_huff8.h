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
/** Build an 8-bit tree from a bitstream */
struct smk_huff8_t* smk_huff8_build(struct smk_bit_t* bs);

/** Look up an 8-bit value in the referenced tree by following a bitstream
	returns -1 on error */
short smk_huff8_lookup(struct smk_bit_t* bs, const struct smk_huff8_t* t);

/** Free an 8-bit tree */
void smk_huff8_free(struct smk_huff8_t* t);

#endif
