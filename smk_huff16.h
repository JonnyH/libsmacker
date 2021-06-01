/**
	libsmacker - A C library for decoding .smk Smacker Video files
	Copyright (C) 2012-2017 Greg Kennedy

	See smacker.h for more information.

	smk_huff16.h
		SMK huffmann trees.  There are two types:
		- a basic 8-bit tree, and
		- a "big" 16-bit tree which includes a cache for recently
			searched values.
*/

#ifndef SMK_HUFF16_H
#define SMK_HUFF16_H

#include "smk_bitstream.h"

/** Tree node structures - Forward declaration */
struct smk_huff16_t;

/************************ 16-BIT HUFF-TREE FUNCTIONS ************************/
/** Build a 16-bit tree from a bitstream */
struct smk_huff16_t* smk_huff16_build(struct smk_bit_t* bs, unsigned int alloc_size);

/** Look up a 16-bit value in the bigtree by following a bitstream
	returns -1 on error */
long smk_huff16_lookup(struct smk_bit_t* bs, struct smk_huff16_t* big);

/** Reset the cache in a 16-bit tree */
void smk_huff16_reset(struct smk_huff16_t* big);

/** function to recursively delete a 16-bit huffman tree */
void smk_huff16_free(struct smk_huff16_t* big);

#endif
