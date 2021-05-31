/**
	libsmacker - A C library for decoding .smk Smacker Video files
	Copyright (C) 2012-2021 Greg Kennedy

	See smacker.h for more information.

	smk_bitstream.c
		Implements a bitstream structure, which can extract and
		return a bit at a time from a raw block of bytes.
*/

#include "smk_bitstream.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

/*
	Bitstream structure
	Pointer to raw block of data and a size limit.
	Maintains internal pointers to byte_num and bit_number.
*/
struct smk_bit_t
{
	const unsigned char * buffer, * end;
	unsigned int bit_num;
};

/* BITSTREAM Functions */

/** Initialize a bitstream */
struct smk_bit_t* smk_bs_init(const unsigned char *const b, const unsigned long size)
{
	struct smk_bit_t* ret;

	/* sanity check */
	assert(b);

	/* allocate a bitstream struct */
	ret = malloc(sizeof(struct smk_bit_t));
	if (ret == NULL)
	{
		perror("libsmacker::smk_bs_init() - ERROR: malloc() returned NULL");
		return NULL;
	}

	/* set up the pointer to bitstream start and end, and set the bit pointer to 0 */
	ret->buffer = b;
	ret->end = b + size;
	ret->bit_num = 0;

	return ret;
}

/* Free a bitstream
	This is nothing special but may be useful for future expansion of bs */
void smk_bs_free(struct smk_bit_t * const bs)
{
	assert(bs);
	free(bs);
}

/* Reads a bit
	Returns -1 if error encountered */
char smk_bs_read_1(struct smk_bit_t* const bs)
{
	char ret;

	/* sanity check */
	assert(bs);

	/* don't die when running out of bits, but signal */
	if (bs->buffer >= bs->end)
	{
		fputs("libsmacker::smk_bs_read_1(bs): ERROR: bitstream exhausted.\n", stderr);
		return -1;
	}

	/* get next bit and return */
	ret = (*bs->buffer >> bs->bit_num) & 1;

	/* advance to next bit */
	/* Out of bits in this byte: next! */
	if (bs->bit_num > 6)
	{
		bs->buffer ++;
		bs->bit_num = 0;
	} else {
		bs->bit_num ++;
	}

	return ret;
}

/* Reads a byte
	Returns -1 if error. */
short smk_bs_read_8(struct smk_bit_t* const bs)
{
	/* sanity check */
	assert(bs);

	/* don't die when running out of bits, but signal */
	if (bs->buffer + (bs->bit_num > 0) >= bs->end)
	{
		fputs("libsmacker::smk_bs_read_8(bs): ERROR: bitstream exhausted.\n", stderr);
		return -1;
	}

	if (bs->bit_num)
	{
		/* unaligned read */
		unsigned char ret = *bs->buffer >> bs->bit_num;
		bs->buffer ++;
		ret |= (*bs->buffer << (8 - bs->bit_num));
		return ret;
	}

	/* aligned read */
	return *bs->buffer++;
}
