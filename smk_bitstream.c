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

/*
	Bitstream structure
	Pointer to raw block of data and a size limit.
	Maintains internal pointers to byte_num and bit_number.
*/
struct smk_bit_t
{
	const unsigned char* bitstream;
	unsigned long size;

	unsigned long byte_num;
	char bit_num;
};

/* BITSTREAM Functions */
struct smk_bit_t* smk_bs_init(const unsigned char* b, const unsigned long size)
{
	struct smk_bit_t* ret = NULL;

	/* sanity check */
	smk_assert(b);

	/* allocate a bitstream struct */
	smk_malloc(ret, sizeof(struct smk_bit_t));

	/* set up the pointer to bitstream, and the size counter */
	ret->bitstream = b;
	ret->size = size;

	/* point to initial byte */
	ret->byte_num = -1;
	ret->bit_num = 7;

	/* return ret or NULL if error : ) */
error:
	return ret;
}

/* Internal function: gets next bit.
	No safety checking. */
static inline char smk_bs_next(struct smk_bit_t* bs)
{
	/* advance to next bit */
	bs->bit_num ++;

	/* Out of bits in this byte: next! */
	if (bs->bit_num > 7)
	{
		bs->byte_num ++;
		bs->bit_num = 0;
	}

	return (((bs->bitstream[bs->byte_num]) & (1 << bs->bit_num)) != 0);
}

/* Reads a bit
	Returns -1 if error encountered */
char smk_bs_read_1(struct smk_bit_t* bs)
{
	/* sanity check */
	smk_assert(bs);

	/* don't die when running out of bits, but signal */
	if (! (bs->bit_num < 7 || (bs->byte_num + 1 < bs->size)) )
	{
		fputs("libsmacker::smk_bs_read_1(bs) - ERROR: bitstream exhausted.\n", stderr);
		goto error;
	}

	/* get next bit and return */
	return smk_bs_next(bs);

error:
	return -1;
}

/* Reads a byte
	Returns -1 if error. */
short smk_bs_read_8(struct smk_bit_t* bs)
{
	unsigned char ret = 0, i;

	/* sanity check */
	smk_assert(bs);

	/* don't die when running out of bits, but signal */
	if (! (bs->byte_num + 1 < bs->size) )
	{
		fputs("libsmacker::smk_bs_read_8(bs) - ERROR: bitstream exhausted.\n", stderr);
		goto error;
	}

	for (i = 0; i < 8; i ++)
	{
		ret >>= 1;
		ret |= (smk_bs_next(bs) << 7);
	}
	return ret;

error:
	return -1;
}

/* aligns struct to start of next byte */
/*
void smk_bs_align(struct smk_bit_t* bs)
{
	bs->bit_num = 7;
}
*/
