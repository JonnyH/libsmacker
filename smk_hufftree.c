/*
	libsmacker - A C library for decoding .smk Smacker Video files
	Copyright (C) 2012-2013 Greg Kennedy

	See smacker.h for more information.

	smk_hufftree.c
		Implementation of Smacker Huffman coding trees.
*/

#include "smk_hufftree.h"

/* malloc and friends */
#include "smk_malloc.h"

/* function to recursively delete a huffman tree */
void smk_huff_free(struct smk_huff_t *t)
{
	/* Sanity check: do not double-free */
	if (t == NULL)
	{
		fprintf(stderr,"libsmacker::smk_huff_free(p) - Warning: attempt to free NULL Huffman tree\n");
		return;
	}

	/* If this is not a leaf node, free child trees first */
	if (t->b0 != NULL)
	{
		smk_huff_free(t->b0);
		smk_huff_free(t->u.b1);
	}

	/* Safe-delete tree node. */
	smk_free(t);
}

/* Recursive tree-building function. */
static struct smk_huff_t *smk_huff_build_rec(struct smk_bit_t *bs)
{
	struct smk_huff_t *ret = NULL;
	char bit;
	short byte;

	/* sanity check */
	if (bs == NULL)
	{
		fprintf(stderr,"libsmacker::smk_huff_build_rec(bs) - ERROR: bitstream is NULL.\n");
		return NULL;
	}

	/* Read the bit */
	bit = smk_bs_read_1(bs);
	if (bit < 0)
	{
		/* Error code came back from bs_read_1 */
		fprintf(stderr,"libsmacker::smk_huff_build_rec(bs) - ERROR: Bad return code while reading from bitstream.\n");
		return NULL;
	}
	else if (bit)
	{
		/* Bit set: this forms a Branch node. */
		smk_malloc(ret,sizeof(struct smk_huff_t));
		/* Recursively attempt to build the Left branch. */
		ret->b0 = smk_huff_build_rec(bs);
		/* Failed to build left branch.  Free current node, return NULL. */
		if (ret->b0 == NULL)
		{
			smk_free(ret);
			return NULL;
		}
		/* Everything is still OK: attempt to build the Right branch. */
		ret->u.b1 = smk_huff_build_rec(bs);
		/* Failed to build right branch.  Free left bracnh, free current node, return NULL. */
		if (ret->u.b1 == NULL)
		{
			smk_huff_free(ret->b0);
			smk_free(ret);
			return NULL;
		}
	}
	else
	{
		/* Bit unset signifies a Leaf node. */
		/* Attempt to read value */
		byte = smk_bs_read_8(bs);
		if (byte < 0)
		{
			fprintf(stderr,"libsmacker::smk_huff_build_rec(bs) - ERROR: Bad return code reading value byte.\n");
			return NULL;
		}
		else
		{
			/* Allocate a node for our leaf. */
			smk_malloc(ret,sizeof(struct smk_huff_t));
			ret->b0 = NULL;
			ret->u.leaf.value = (byte & 0x00FF);
			ret->u.leaf.escapecode = 0xFF;
		}
	}

	return ret;
}

/*
	Entry point for huff_build.  Basically just checks the start/end tags
	and calls smk_huff_build_rec recursive function.
*/
struct smk_huff_t *smk_huff_build(struct smk_bit_t* bs)
{
	struct smk_huff_t *ret = NULL;
	char bit;

	/* sanity check */
	if (bs == NULL)
	{
		fprintf(stderr,"libsmacker::smk_huff_build(bs)- ERROR: bitstream is NULL.\n");
		return NULL;
	}

	bit = smk_bs_read_1(bs);
	/* Smacker huff trees begin with a set-bit. */
	if (bit < 0)
	{
		/* Error code came back from bs_read_1 */
		fprintf(stderr,"libsmacker::smk_huff_build_rec(bs) - ERROR: Bad return code while reading from bitstream.\n");
		return NULL;
	}
	else if (!bit)
	{
		fprintf(stderr,"libsmacker::smk_huff_build(bs) - ERROR: initial get_bit returned 0\n");
		return NULL;
	}
	else
	{
		/* Begin parsing the tree data. */
		ret = smk_huff_build_rec(bs);
		if (ret == NULL)
		{
			fprintf(stderr,"libsmacker::smk_huff_build(bs) - ERROR: NULL return code from recursive tree build\n");
			return NULL;
		}

		bit = smk_bs_read_1(bs);
		/* Smacker huff trees end with a set-bit. */
		if (bit < 0)
		{
			/* Error code came back from bs_read_1 */
			fprintf(stderr,"libsmacker::smk_huff_build_rec(bs) - ERROR: Bad return code while reading from bitstream.\n");
			smk_huff_free(ret);
			return NULL;
		}
		else if (bit)
		{
			fprintf(stderr,"libsmacker::smk_huff_build(bs) - ERROR: final get_bit returned 1\n");
			smk_huff_free(ret);
			return NULL;
		}
	}

	return ret;
}

/* Look up an 8-bit value from a basic huff tree.
	Return -1 on error. */
short smk_huff_lookup (struct smk_bit_t *bs, struct smk_huff_t *t)
{
	char bit;

	/* sanity check */
	if (bs == NULL)
	{
		fprintf(stderr,"libsmacker::smk_huff_lookup(bs) - ERROR: bitstream is NULL.\n");
		return -1;
	}

	if (t->b0 == NULL)
	{
		/* Reached a Leaf node.  Return its value. */
		return t->u.leaf.value;
	}
	else
	{
		bit = smk_bs_read_1(bs);
		if (bit < 0)
		{
			fprintf(stderr,"libsmacker::smk_huff_lookup(bs,t) - ERROR: out of bits in bs when reading.  Returning bogus value.\n");
			return -1;
		}
		else if (bit)
		{
			/* get_bit returned Set, follow Right branch. */
			return smk_huff_lookup(bs,t->u.b1);
		}
		else
		{
			return smk_huff_lookup(bs,t->b0);
		}
	}
}

/* Recursively builds a Big tree. */
static struct smk_huff_t *smk_huff_big_build_rec(struct smk_bit_t *bs, struct smk_huff_big_t *big, struct smk_huff_t *low8, struct smk_huff_t *hi8)
{
	struct smk_huff_t *ret = NULL;

	char bit;
	short lowval, hival;

	/* sanity check */
	if (bs == NULL)
	{
		fprintf(stderr,"libsmacker::smk_huff_lookup(big,low8,hi8,bs) - ERROR: bitstream is NULL.\n");
		return NULL;
	}
	if (big == NULL)
	{
		fprintf(stderr,"libsmacker::smk_huff_lookup(big,low8,hi8,bs) - ERROR: big (cache) is NULL.\n");
		return NULL;
	}
	if (low8 == NULL)
	{
		fprintf(stderr,"libsmacker::smk_huff_lookup(big,low8,hi8,bs) - ERROR: low8 tree is NULL.\n");
		return NULL;
	}
	if (hi8 == NULL)
	{
		fprintf(stderr,"libsmacker::smk_huff_lookup(big,low8,hi8,bs) - ERROR: hi8 tree is NULL.\n");
		return NULL;
	}

	bit = smk_bs_read_1(bs);
	/* OK, we have enough bits, go on */
	if (bit < 0)
	{
		fprintf(stderr,"libsmacker::smk_huff_big_build_rec(bs) - ERROR: out of bits in bs when building a bigtree (node/leaf).\n");
	}
	else if (bit)
	{
		/* Bit set: this forms a Branch node. */
		smk_malloc(ret,sizeof(struct smk_huff_t));
		/* Recursively attempt to build the Left branch. */
		ret->b0 = smk_huff_big_build_rec(bs,big,low8,hi8);
		/* Failed to build left branch.  Free current node, return NULL. */
		if (ret->b0 == NULL)
		{
			smk_free(ret);
			return NULL;
		}

		/* Everything is still OK: attempt to build the Right branch. */
		ret->u.b1 = smk_huff_big_build_rec(bs,big,low8,hi8);
		/* Failed to build right branch.  Free left bracnh, free current node, return NULL. */
		if (ret->u.b1 == NULL)
		{
			smk_huff_free(ret->b0);
			smk_free(ret);
			return NULL;
		}
	}
	else
	{
		/* Bit unset signifies a Leaf node. */
		lowval = smk_huff_lookup(bs,low8);
		/* low8 is an 8-byte tree: if we get something in the top bytes, that's an error */
		if (lowval < 0)
		{
			fprintf(stderr,"libsmacker::smk_huff_big_build_rec(bs) - ERROR: bad return code from smk_huff_lookup(bs,low8).\n");
			return NULL;
		}

		hival = smk_huff_lookup(bs,hi8);
		/* hi8 is an 8-byte tree: if we get something in the top bytes, that's an error */
		if (hival < 0)
		{
			fprintf(stderr,"libsmacker::smk_huff_big_build_rec(bs) - ERROR: bad return code from smk_huff_lookup(bs,hi8).\n");
			return NULL;
		}

		/* Looks OK: we got low and hi values.  Return a new LEAF */
		smk_malloc(ret,sizeof(struct smk_huff_t));
		ret->b0 = NULL;
		ret->u.leaf.value = lowval | (hival << 8);

		/* Last: when building the tree, some Values may correspond to cache positions.
			Identify these values and set the Escape code byte accordingly. */
		if (ret->u.leaf.value == big->cache[0])
		{
			ret->u.leaf.escapecode = 0;
		}
		else if (ret->u.leaf.value == big->cache[1])
		{
			ret->u.leaf.escapecode = 1;
		}
		else if (ret->u.leaf.value == big->cache[2])
		{
			ret->u.leaf.escapecode = 2;
		}
		else
		{
			ret->u.leaf.escapecode = 0xFF;
		}
	}

	return ret;
}

/* Entry point for building a big 16-bit tree. */
struct smk_huff_big_t *smk_huff_big_build(struct smk_bit_t* bs)
{
	struct smk_huff_big_t *big = NULL;

	struct smk_huff_t *low8 = NULL;
	struct smk_huff_t *hi8 = NULL;

	short lowval, hival;
	char bit;
	unsigned char i;

	/* sanity check */
	if (bs == NULL)
	{
		fprintf(stderr,"libsmacker::smk_huff_lookup(bs) - ERROR: bitstream is NULL.\n");
		return NULL;
	}

	bit = smk_bs_read_1(bs);
	/* Smacker huff trees begin with a set-bit. */
	if (bit < 0)
	{
		fprintf(stderr,"libsmacker::smk_huff_build_big(bs) - ERROR: out of bits in bs when building a tree (node/leaf).\n");
		return NULL;
	}
	else if (bit)
	{
		low8 = smk_huff_build(bs);
		if (low8 == NULL)
		{
			fprintf(stderr,"libsmacker::smk_huff_build_big(bs) - ERROR: failed to build low8 tree.\n");
			return NULL;
		}
		hi8 = smk_huff_build(bs);
		if (hi8 == NULL)
		{
			fprintf(stderr,"libsmacker::smk_huff_build_big(bs) - ERROR: failed to build hi8 tree.\n");
			smk_huff_free(low8);
			return NULL;
		}

		/* Both 8-bit trees built OK.  Allocate room for a new bigtree. */
		smk_malloc(big,sizeof(struct smk_huff_big_t));

		/* Retrieve three 16-bit values.  That means 6 bytes should be available. */
		if (bs->byte_num + 7 >= bs->size) {
			fprintf(stderr,"libsmacker::smk_huff_build_big(bs) - ERROR: out of bits in bs when retrieving escape codes.\n");
			smk_free(big);
			smk_huff_free(hi8);
			smk_huff_free(low8);
			return NULL;
		}

		/* Init the escape code cache. */
		for (i = 0; i < 3; i ++)
		{
			lowval = smk_bs_read_8(bs);
			if (lowval < 0)
			{
				fprintf(stderr,"libsmacker::smk_huff_build_big(bs) - ERROR: failed to read lowval of cache %u.\n",i);
				smk_free(big);
				smk_huff_free(hi8);
				smk_huff_free(low8);
				return NULL;
			}
			hival = smk_bs_read_8(bs);
			if (hival < 0)
			{
				fprintf(stderr,"libsmacker::smk_huff_build_big(bs) - ERROR: failed to read lowval of cache %u.\n",i);
				smk_free(big);
				smk_huff_free(hi8);
				smk_huff_free(low8);
				return NULL;
			}
			big->cache[i] = lowval | (hival << 8);
		}

		/* Finally, call recursive function to retrieve the Bigtree. */
		big->t = smk_huff_big_build_rec(bs,big,low8,hi8);

		/* Done with 8-bit hufftrees, free them. */
		smk_huff_free(hi8);
		smk_huff_free(low8);

		/* Check final end tag. */
		if (big->t == NULL)
		{
			fprintf(stderr,"libsmacker::smk_huff_build_big(bs) - ERROR: NULL return code from recursive tree build\n");
			smk_free(big);
			return NULL;
		}

		bit = smk_bs_read_1(bs);
		if ( bit < 0 )
		{
			fprintf(stderr,"libsmacker::smk_huff_build_big(bs) - ERROR: out of bits in bs while looking for end tag\n");
			smk_huff_free(big->t);
			smk_free(big);
			return NULL;
		}
		else if (bit)
		{
			fprintf(stderr,"libsmacker::smk_huff_build_big(bs) - ERROR: final get_bit returned 1\n");
			smk_huff_free(big->t);
			smk_free(big);
			return NULL;
		}
	} else {
		fprintf(stderr,"libsmacker::smk_huff_build_big(bs) - ERROR: initial get_bit returned 0\n");
		return NULL;
	}

	return big;
}

static int smk_huff_big_lookup_rec (struct smk_bit_t *bs, struct smk_huff_big_t *big, struct smk_huff_t *t)
{
	unsigned short val;
	char bit;

	/* sanity check */
	if (bs == NULL)
	{
		fprintf(stderr,"libsmacker::smk_huff_lookup(bs,big,t) - ERROR: bitstream is NULL.\n");
		return -1;
	}
	if (big == NULL)
	{
		fprintf(stderr,"libsmacker::smk_huff_big_lookup_rec(bs,big,t) - ERROR: big (cache) is NULL.\n");
		return -1;
	}
	if (t == NULL)
	{
		fprintf(stderr,"libsmacker::smk_huff_big_lookup_rec(bs,big,t) - ERROR: t (tree) is NULL.\n");
		return -1;
	}

	/* Reached a Leaf node */
	if (t->b0 == NULL)
	{
		if (t->u.leaf.escapecode != 0xFF)
		{
			/* Found escape code. Retrieve value from Cache. */
			val = big->cache[t->u.leaf.escapecode];
		}
		else
		{
			/* Use value directly. */
			val = t->u.leaf.value;
		}

		if ( big->cache[0] != val)
		{
			/* Update the cache, by moving val to the front of the queue,
				if it isn't already there. */
			if (big->cache[1] == val)
			{
			/* Special case where val is the second item in the queue. */
				big->cache[1] = big->cache[0];
				big->cache[0] = val;
			} else {
				big->cache[2] = big->cache[1];
				big->cache[1] = big->cache[0];
				big->cache[0] = val;
			}
		}

		return val;
	}
	else
	{
		bit = smk_bs_read_1(bs);
		if (bit < 0)
		{
			fprintf(stderr,"libsmacker::smk_huff_big_lookup_rec(bs,big,t) - ERROR: smk_bs_read_1 returned error code.\n");
			return -1;
		}
		else if (bit)
		{
			/* get_bit returned Set, follow Right branch. */
			return smk_huff_big_lookup_rec(bs,big,t->u.b1);
		}
		else
		{
			return smk_huff_big_lookup_rec(bs,big,t->b0);
		}
	}
}

/* Convenience call-out for recursive bigtree lookup function */
long smk_huff_big_lookup (struct smk_bit_t *bs, struct smk_huff_big_t *big)
{
	return smk_huff_big_lookup_rec(bs,big,big->t);
}

/* Resets a Big hufftree cache */
void smk_huff_big_reset (struct smk_huff_big_t *big)
{
	big->cache[0] = 0;
	big->cache[1] = 0;
	big->cache[2] = 0;
}
