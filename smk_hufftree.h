/*
	libsmacker - A C library for decoding .smk Smacker Video files
	Copyright (C) 2012-2013 Greg Kennedy

	See smacker.h for more information.

	smk_hufftree.h
		SMK huffmann trees.  There are two types - a basic tree,
		and a "big" tree which includes a cache for recently
		searched values.
*/
	
#ifndef SMK_HUFFTREE_H
#define SMK_HUFFTREE_H

#include "smk_bitstream.h"

struct smk_huff_t
{
    struct smk_huff_t *b0;
    struct smk_huff_t *b1;
    unsigned short value;
    unsigned char escapecode;
};

struct smk_huff_big_t
{
    struct smk_huff_t *t;
    unsigned short s16[3];
};


/* function to recursively delete a huffman tree */
void smk_del_huffman(struct smk_huff_t *);

/* Build a tree from a bitstream */
struct smk_huff_t *smk_build_tree(struct smk_bit_t *);

/* Look up a 16-bit value in the referenced tree by following a bitstream */
unsigned short smk_tree_lookup (struct smk_bit_t *, struct smk_huff_t *);

/* Build a bigtree from a bitstream */
struct smk_huff_big_t *smk_build_bigtree(struct smk_bit_t *);

/* Look up a 16-bit value in the bigtree by following a bitstream */
unsigned short smk_bigtree_lookup (struct smk_bit_t *, struct smk_huff_big_t *);

/* Reset the cache in a bigtree */
void smk_bigtree_reset (struct smk_huff_big_t *);

#endif
