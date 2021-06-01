/**
	libsmacker - A C library for decoding .smk Smacker Video files
	Copyright (C) 2012-2017 Greg Kennedy

	See smacker.h for more information.

	smk_huff16.c
		Implementation of Smacker Huffman coding trees.
		This is the 16-bit (big) tree.
*/

#include "smk_huff16.h"

#include "smk_huff8.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#define SMK_HUFF16_BRANCH 0x80000000
#define SMK_HUFF16_CACHE 0x40000000
#define SMK_HUFF16_LEAF_MASK 0x3FFFFFFF

/**
	16-bit Tree node structure.
*/
struct smk_huff16_t
{
	unsigned int * tree;
	unsigned int tree_size;

	/* recently-used values cache */
	unsigned short cache[3];
};

/* delete a (big) huffman tree */
void smk_huff16_free(struct smk_huff16_t* const big)
{
	/* Sanity check: do not double-free */
	assert(big);
	assert(big->tree);

	free(big->tree);
	free(big);
}

/* Recursive sub-func for building a tree into an array. */
static int _smk_huff16_build_rec(struct smk_huff16_t * const t, struct smk_bit_t* const bs, const struct smk_huff8_t* const low8, const struct smk_huff8_t* const hi8)
{
	/* Read the bit */
	char bit;
	int value, value2;

	bit = smk_bs_read_1(bs);
	if (bit < 0)
	{
		fputs("libsmacker::_smk_huff16_build_rec() - ERROR: get_bit returned -1\n", stderr);
		return 0;
	} else if (bit) {
		/* Bit set: this forms a Branch node.
			what we have to do is build the left-hand branch,
			assign the "jump" address,
			then build the right hand branch from there.
		*/

		/* track the current index */
		value = t->tree_size;
		t->tree_size ++;

		/* go build the left branch */
		if (! _smk_huff16_build_rec(t, bs, low8, hi8)) {
			fputs("libsmacker::_smk_huff16_build_rec() - ERROR: failed to build left sub-tree\n", stderr);
			return 0;
		}

		/* now go back to our current location, and
			mark our location as a "jump" */
		t->tree[value] = SMK_HUFF16_BRANCH | t->tree_size;

		/* continue building the right side */
		if (! _smk_huff16_build_rec(t, bs, low8, hi8)) {
			fputs("libsmacker::_smk_huff16_build_rec() - ERROR: failed to build right sub-tree\n", stderr);
			return 0;
		}
	} else {
		/* Bit unset signifies a Leaf node. */
		/* Attempt to read LOW value */
		value = smk_huff8_lookup(bs, low8);
		if (value < 0)
		{
			fputs("libsmacker::_smk_huff16_build_rec() - ERROR: get LOW value returned -1\n", stderr);
			return 0;
		}

		/* now read HIGH value */
		value2 = smk_huff8_lookup(bs, hi8);
		if (value2 < 0)
		{
			fputs("libsmacker::_smk_huff16_build_rec() - ERROR: get HIGH value returned -1\n", stderr);
			return 0;
		}

		/* Looks OK: we got low and hi values. Return a new LEAF */
		t->tree[t->tree_size] = (value << 8) | value2;

		/* Last: when building the tree, some Values may correspond to cache positions.
			Identify these values and set the Escape code byte accordingly. */
		if (t->tree[t->tree_size] == t->cache[0])
		{
			t->tree[t->tree_size] = SMK_HUFF16_CACHE;
		}
		else if (t->tree[t->tree_size] == t->cache[1])
		{
			t->tree[t->tree_size] = SMK_HUFF16_CACHE | 1;
		}
		else if (t->tree[t->tree_size] == t->cache[2])
		{
			t->tree[t->tree_size] = SMK_HUFF16_CACHE | 2;
		}

		t->tree_size ++;
	}
	return 1;
}

/* Entry point for building a big 16-bit tree. */
struct smk_huff16_t* smk_huff16_build(struct smk_bit_t* const bs, const unsigned int alloc_size)
{
	struct smk_huff16_t* big;

	struct smk_huff8_t* low8, *hi8;

	int value, i;

	char bit;

	/* sanity check */
	assert(bs);

	/* Smacker huff trees begin with a set-bit. */
	bit = smk_bs_read_1(bs);

	if (bit < 0)
	{
		fputs("libsmacker::smk_huff16_build() - ERROR: initial get_bit returned -1\n", stderr);
		return NULL;
	}

	/* OK to allocate the struct now */
	big = malloc(sizeof(struct smk_huff16_t));
	if (big == NULL)
	{
		perror("libsmacker::smk_huff16_build() - ERROR: malloc() returned NULL");
		return NULL;
	}

	big->tree_size = 0;

	/* First bit indicates whether a tree is present or not. */
	/*  Very small or audio-only files may have no tree. */
	if (bit)
	{
		/* build low-8-bits tree */
		low8 = smk_huff8_build(bs);
		if (low8 == NULL) {
			fputs("libsmacker::smk_huff16_build() - ERROR: failed to build LOW tree\n", stderr);
			free(big);
			return NULL;
		}
		/* build hi-8-bits tree */
		hi8 = smk_huff8_build(bs);
		if (hi8 == NULL) {
			fputs("libsmacker::smk_huff16_build() - ERROR: failed to build HIGH tree\n", stderr);
			free(low8);
			free(big);
			return NULL;
		}

		/* Init the escape code cache. */
		for (i = 0; i < 3; i ++)
		{
			value = smk_bs_read_8(bs);
			if (value < 0)
			{
				fprintf(stderr, "libsmacker::smk_huff16_build() - ERROR: get LOW value for cache %d returned -1\n", i);
				free(low8);
				free(big);
				return NULL;
			}
			big->cache[i] = value;

			/* now read HIGH value */
			value = smk_bs_read_8(bs);
			if (value < 0)
			{
				fprintf(stderr, "libsmacker::smk_huff16_build() - ERROR: get HIGH value for cache %d returned -1\n", i);
				free(low8);
				free(big);
				return NULL;
			}
			big->cache[i] |= (value << 8);
		}

		/* Everything looks OK so far. Time to malloc structure. */
		big->tree = malloc((alloc_size - 12) / 4 * sizeof(unsigned int));

		/* Finally, call recursive function to retrieve the Bigtree. */
		_smk_huff16_build_rec(big, bs, low8, hi8);

		/* Done with 8-bit hufftrees, free them. */
		smk_huff8_free(hi8);
		smk_huff8_free(low8);
	}
	else
	{
		big->tree = malloc(sizeof(unsigned int));
		big->tree[0] = 0;
		big->cache[0] = big->cache[1] = big->cache[2] = 0;
	}

	/* Check final end tag. */
	bit = smk_bs_read_1(bs);

	if (bit < 0)
	{
		fputs("libsmacker::smk_huff16_build() - ERROR: final get_bit returned -1\n", stderr);
		smk_huff16_free(big);
		return NULL;
	} else if (bit)
	{
		fputs("libsmacker::smk_huff16_build() - ERROR: final get_bit returned 1\n", stderr);
		smk_huff16_free(big);
		return NULL;
	}

	return big;
}

/* Look up a 16-bit value from a large huff tree.
	Return -1 on error.
	Note that this also updates the recently-used-values cache. */
long smk_huff16_lookup(struct smk_bit_t* const bs, struct smk_huff16_t* const t)
{
	char bit;
	int index = 0;
	int value;

	/* sanity check */
	assert(bs);
	assert(t);

	while (t->tree[index] & SMK_HUFF16_BRANCH) {
		bit = smk_bs_read_1(bs);
		if (bit < 0)
		{
			fputs("libsmacker::smk_huff16_build() - ERROR: final get_bit returned -1\n", stderr);
			return -1;
		} else if (bit) {
			/* take the right branch */
			index = t->tree[index] & SMK_HUFF16_LEAF_MASK;
		} else {
			/* take the left branch */
			index ++;
		}
	}

	if (t->tree[index] & SMK_HUFF16_CACHE) {
		/* uses cached value instead of actual value */
		value = t->cache[t->tree[index] & SMK_HUFF16_LEAF_MASK];
	} else {
		/* Use value directly. */
		value = t->tree[index];
	}

	if (t->cache[0] != value)
	{
		/* Update the cache, by moving val to the front of the queue,
			if it isn't already there. */
		t->cache[2] = t->cache[1];
		t->cache[1] = t->cache[0];
		t->cache[0] = value;
	}

	return value;
}

/* Resets a Big hufftree cache */
void smk_huff16_reset(struct smk_huff16_t* const t)
{
	/* sanity check */
	assert(t);

	t->cache[0] = t->cache[1] = t->cache[2] = 0;
}
