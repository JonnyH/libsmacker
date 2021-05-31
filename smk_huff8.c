/*
	libsmacker - A C library for decoding .smk Smacker Video files
	Copyright (C) 2012-2021 Greg Kennedy

	See smacker.h for more information.

	smk_huff8.c
		Implementation of Smacker Huffman coding trees.
		This is the 8-bit (small) tree.
*/

#include "smk_huff8.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#define SMK_HUFF8_BRANCH 0x8000
#define SMK_HUFF8_LEAF_MASK 0x7FFF

/**
	8-bit Tree node structure.
*/
struct smk_huff8_t
{
	/* Unfortunately, smk files do not store the alloc size of a small tree.
		511 entries is the pessimistic case (N codes and N-1 branches,
		with N=256 for 8 bits) */
	unsigned int tree_size;
	unsigned short tree[511];
};

/* Recursive sub-func for building a tree into an array. */
static int _smk_huff8_build_rec(struct smk_huff8_t *t, struct smk_bit_t* bs)
{
	/* Read the bit */
	char bit;
	short value;

	bit = smk_bs_read_1(bs);
	if (bit < 0)
	{
		fputs("libsmacker::_smk_huff8_build_rec(t,bs) - ERROR: get_bit returned -1\n", stderr);
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
		if (! _smk_huff8_build_rec(t, bs)) {
			fputs("libsmacker::_smk_huff8_build_rec(t,bs) - ERROR: failed to build left sub-tree\n", stderr);
			return 0;
		}

		/* now go back to our current location, and
			mark our location as a "jump" */
		t->tree[value] = SMK_HUFF8_BRANCH | t->tree_size;

		/* continue building the right side */
		if (! _smk_huff8_build_rec(t, bs)) {
			fputs("libsmacker::_smk_huff8_build_rec(t,bs) - ERROR: failed to build right sub-tree\n", stderr);
			return 0;
		}
	} else {
		/* Bit unset signifies a Leaf node. */
		/* Attempt to read value */
		value = smk_bs_read_8(bs);
		if (value < 0)
		{
			fputs("libsmacker::_smk_huff8_build_rec(t,bs) - ERROR: get_byte returned -1\n", stderr);
			return 0;
		}

		/* store to tree */
		t->tree[t->tree_size] = value;
		t->tree_size ++;
	}

	return 1;
}

/**
*/
struct smk_huff8_t* _smk_huff8_build(struct smk_bit_t* bs)
{
	struct smk_huff8_t* ret;
	char bit;

	/* sanity check */
	assert(bs);

	/* Smacker huff trees begin with a set-bit. */
	bit = smk_bs_read_1(bs);

	if (bit < 0)
	{
		fputs("libsmacker::smk_huff8_build(bs) - ERROR: initial get_bit returned -1\n", stderr);
		return NULL;
	}

	/* OK to allocate the struct now */
	ret = malloc(sizeof(struct smk_huff8_t));
	if (ret == NULL)
	{
		perror("libsmacker::smk_huff8_build(bs) - ERROR: malloc() returned NULL");
		return NULL;
	}

	ret->tree_size = 0;

	/* First bit indicates whether a tree is present or not. */
	/*  Very small or audio-only files may have no tree. */
	if (bit)
	{
		if (! _smk_huff8_build_rec(ret, bs)) {
			fputs("libsmacker::smk_huff8_build(bs) - ERROR: tree build failed\n", stderr);
			smk_huff8_free(ret);
			return NULL;
		}
	}
	else
		ret->tree[0] = 0;

	/* huff trees end with an unset-bit */
	bit = smk_bs_read_1(bs);

	if (bit < 0)
	{
		fputs("libsmacker::smk_huff8_build(bs) - ERROR: final get_bit returned -1\n", stderr);
		smk_huff8_free(ret);
		return NULL;
	} else if (bit)
	{
		fputs("libsmacker::_smk_huff8_build(bs) - ERROR: final get_bit returned 1\n", stderr);
		smk_huff8_free(ret);
		return NULL;
	}

	return ret;
}

/* Look up an 8-bit value from a basic huff tree.
	Return -1 on error. */
short _smk_huff8_lookup(struct smk_bit_t* bs, const struct smk_huff8_t* t)
{
	char bit;
	int index = 0;

	/* sanity check */
	assert(bs);
	assert(t);

	while (t->tree[index] & SMK_HUFF8_BRANCH) {
		bit = smk_bs_read_1(bs);
		if (bit < 0)
		{
			fputs("libsmacker::smk_huff8_build(bs) - ERROR: final get_bit returned -1\n", stderr);
			return -1;
		} else if (bit) {
			/* take the right branch */
			index = t->tree[index] & SMK_HUFF8_LEAF_MASK;
		} else {
			/* take the left branch */
			index ++;
		}
	}

	return t->tree[index];
}

/*
	Free a basic tree.  The structure is not complex so just free() is fine.
*/
void smk_huff8_free(struct smk_huff8_t* const t)
{
	assert(t);
	free(t);
}
