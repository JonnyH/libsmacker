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
struct smk_bit_t *smk_bs_init(unsigned char *b, unsigned int size)
{
    /* allocate a bitstream struct */
    struct smk_bit_t *ret;
    smk_malloc(ret,sizeof(struct smk_bit_t));

    /* set up the pointer to bitstream, and the size counter */
    ret->bitstream = b;
    ret->size = size;

    /* point to initial byte */
    ret->byte_num = -1;
    ret->bit_num = 7;

    return ret;
}

/* aligns struct to start of next byte */
void smk_bs_align(struct smk_bit_t *bs)
{
    bs->bit_num = 7;
}

/* returns "enough bits left for a byte?" */
unsigned char smk_bytes_left(struct smk_bit_t *bs)
{
    return (bs->byte_num + 1 < bs->size);
}

/* returns "any bits left?" */
unsigned char smk_bits_left(struct smk_bit_t *bs)
{
    return (bs->bit_num < 7 || smk_bytes_left(bs));
}

unsigned char smk_bs_1(struct smk_bit_t *bs)
{
    /* don't die when running out of bits, but signal */
    if (!smk_bits_left(bs))
    {
        fprintf(stderr,"libsmacker::smk_bs_1 - WARNING: bitstream exhausted.\n");
        return 0xFF;
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

unsigned char smk_bs_8(struct smk_bit_t *bs)
{
    unsigned char ret = 0, i;

    /* don't die when running out of bits, but signal */
    if (!smk_bytes_left(bs))
    {
        fprintf(stderr,"libsmacker::smk_bs_1 - WARNING: bitstream exhausted.\n");
        return 0xFF;
    }

    for (i = 0; i < 8; i ++)
    {
        ret = ret >> 1;
        ret |= (smk_bs_1(bs) << 7);
    }
    return ret;
}

