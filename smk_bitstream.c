/*
	libsmacker - A C library for decoding .smk Smacker Video files
	Copyright (C) 2012-2013 Greg Kennedy

	See smacker.h for more information.

	smk_bitstream.c
		Implements a bitstream structure, which can extract and
		return a bit at a time from a raw block of bytes.
*/

#include "smk_bitstream.h"

/* malloc and friends */
#include "smk_malloc.h"

/* BITSTREAM Functions */
struct smk_bit_t *smk_bs_init(unsigned char *b, unsigned long size)
{
	struct smk_bit_t *ret = NULL;

	if (b == NULL)
	{
		fprintf(stderr,"libsmacker::smk_bs_init(b,%lu) - ERROR: passed a NULL pointer for b.\n",size);
		return NULL;
	}

	/* allocate a bitstream struct */
	smk_malloc(ret,sizeof(struct smk_bit_t));

	/* set up the pointer to bitstream, and the size counter */
	ret->bitstream = b;
	ret->size = size;

	/* point to initial byte */
	ret->byte_num = -1;
	ret->bit_num = 7;

	return ret;
}

/* returns "enough bits left for a byte?" */
static unsigned char smk_bs_query_8(struct smk_bit_t *bs)
{
	/* sanity check */
	if (bs == NULL)
	{
		fprintf(stderr,"libsmacker::smk_bs_query_8(bs) - ERROR: bs is NULL.\n");
		return 0;
	}
	return (bs->byte_num + 1 < bs->size);
}

/* returns "any bits left?" */
static unsigned char smk_bs_query_1(struct smk_bit_t *bs)
{
	/* sanity check */
	if (bs == NULL)
	{
		fprintf(stderr,"libsmacker::smk_bs_query_1(bs) - ERROR: bs is NULL.\n");
		return 0;
	}
	return (bs->bit_num < 7 || smk_bs_query_8(bs));
}

/* Reads a bit
	Returns -1 if error encountered */
char smk_bs_read_1(struct smk_bit_t *bs)
{
	/* sanity check */
	if (bs == NULL)
	{
		fprintf(stderr,"libsmacker::smk_bs_read_1(bs) - ERROR: bs is NULL.\n");
		return -1;
	}

	/* don't die when running out of bits, but signal */
	if (!smk_bs_query_1(bs))
	{
		fprintf(stderr,"libsmacker::smk_bs_read_1(bs) - ERROR: bitstream exhausted.\n");
		return -1;
	}

	/* advance to next bit */
	bs->bit_num ++;

	/* Out of bits in this byte: next! */
	if (bs->bit_num > 7)
	{
		bs->byte_num ++;
		bs->bit_num = 0;
	}

	return (((bs->bitstream[bs->byte_num]) & (0x01 << bs->bit_num)) != 0);
}

/* Reads a byte
	Returns -1 if error. */
short smk_bs_read_8(struct smk_bit_t *bs)
{
	unsigned char ret = 0, i;

	/* sanity check */
	if (bs == NULL)
	{
		fprintf(stderr,"libsmacker::smk_bs_read_8(bs) - ERROR: bs is NULL.\n");
		return -1;
	}

	/* don't die when running out of bits, but signal */
	if (!smk_bs_query_8(bs))
	{
		fprintf(stderr,"libsmacker::smk_bs_read_8(bs) - ERROR: bitstream exhausted.\n");
		return -1;
	}

	for (i = 0; i < 8; i ++)
	{
		ret = ret >> 1;
		ret |= (smk_bs_read_1(bs) << 7);
	}
	return ret;
}

/* aligns struct to start of next byte */
/*
void smk_bs_align(struct smk_bit_t *bs)
{
	bs->bit_num = 7;
}
*/
