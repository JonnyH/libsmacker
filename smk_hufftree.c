/* see smacker.h */
#include "smk_hufftree.h"

/* malloc and friends */
#include "smk_malloc.h"

/* function to recursively delete a huffman tree */
void smk_del_huffman(struct smk_huff_t *t)
{
    if (t == NULL) return;

    if (t->b0 != NULL) smk_del_huffman(t->b0);
    if (t->b1 != NULL) smk_del_huffman(t->b1);
    smk_free(t);
}

static struct smk_huff_t *smk_tree_rec(struct smk_bit_t *bs)
{
    struct smk_huff_t *ret;

    /* check out-of-bits condition */
    if (!smk_bits_left(bs))
    {
        fprintf(stderr,"libsmacker::smk_tree_rec - ERROR: out of bits in bs when building a tree (node/leaf).\n");
        return NULL;
    }

    /* OK, we have enough bits, go on */ 
    if (smk_bs_1(bs))
    {
        smk_malloc(ret,sizeof(struct smk_huff_t));
        ret->b0 = smk_tree_rec(bs);
        if (ret->b0 == NULL) { smk_free(ret); return NULL; }
        ret->b1 = smk_tree_rec(bs);
        if (ret->b1 == NULL) { smk_del_huffman(ret->b0); smk_free(ret); return NULL; }
        ret->value = 0;
        ret->escapecode = 0xFF;
    } else {
        /* Make sure we have at least a byte left */
        if (!smk_bytes_left(bs))
        {
            fprintf(stderr,"libsmacker::smk_tree_rec - ERROR: out of bytes in bs when building a tree LEAF.\n");
            return NULL;
        }

        smk_malloc(ret,sizeof(struct smk_huff_t));
        ret->b0 = NULL;
        ret->b1 = NULL;
        ret->value = smk_bs_8(bs);
        ret->escapecode = 0xFF;
    }
    return ret;
}

struct smk_huff_t *smk_build_tree(struct smk_bit_t* bs)
{
    struct smk_huff_t *ret;

    /* check for out-of-bits */
    if (!smk_bits_left(bs))
    {
        fprintf(stderr,"libsmacker::smk_build_tree - ERROR: out of bits in bs when building a tree HEAD.\n");
        return NULL;
    }

    if (smk_bs_1(bs))
    {
        ret = smk_tree_rec(bs);
        if ( !smk_bits_left(bs) || smk_bs_1(bs) )
        {
            fprintf(stderr,"libsmacker::smk_build_tree - ERROR: out of bits in bs when building a tree tail, OR final get_bit returned 1.\n");
            smk_del_huffman(ret);
            ret = NULL;
        }
    } else {
        fprintf(stderr,"libsmacker::smk_build_tree - ERROR: initial get_bit returned 0.\n");
        ret = NULL;
    }

    return ret;
}

unsigned short smk_tree_lookup (struct smk_bit_t *bs, struct smk_huff_t *t)
{
    if (t->b0 == NULL || t->b1 == NULL)
        return t->value;
    else
    { 
        if (!smk_bits_left(bs))
        {
            fprintf(stderr,"libsmacker::smk_tree_lookup - ERROR: out of bits in bs when reading.  Returning bogus value.\n");
            return -1;
        }

        if (smk_bs_1(bs))
            return smk_tree_lookup(bs,t->b1);
        else
            return smk_tree_lookup(bs,t->b0);
    }
}

static void btp(struct smk_huff_t *t, int d)
{
  int q = d;
  if (t == NULL) return;
  if (t->b0 != NULL)
  {
    btp(t->b0, d + 1);
  while ((q --) > 0) printf(" ");
    printf("|- NODE\n");
    btp(t->b1, d + 1);
  } else {
  while ((q --) > 0) printf(" ");
    printf("|- (esc: %02X) LEAF: val %04X\n",t->escapecode,t->value);
  }
}

static struct smk_huff_t *smk_bigtree_rec(struct smk_huff_big_t *big, struct smk_huff_t *low8, struct smk_huff_t *hi8, struct smk_bit_t *bs)
{
    struct smk_huff_t *ret;

    unsigned short lowval, hival;

    /* check out-of-bits condition */
    if (!smk_bits_left(bs))
    {
        fprintf(stderr,"libsmacker::smk_bigtree_rec - ERROR: out of bits in bs when building a bigtree (node/leaf).\n");
        return NULL;
    }

    if (smk_bs_1(bs))
    {
        smk_malloc(ret,sizeof(struct smk_huff_t));
        ret->b0 = smk_bigtree_rec(big,low8,hi8,bs);
        if (ret->b0 == NULL) { smk_free(ret); return NULL; }
        ret->b1 = smk_bigtree_rec(big,low8,hi8,bs);
        if (ret->b1 == NULL) { smk_del_huffman(ret->b0); smk_free(ret); return NULL; }
        ret->value = 0;
        ret->escapecode = 0xFF;
    } else {
        lowval = smk_tree_lookup(bs,low8);
        /* low8 is an 8-byte tree: if we get something in the top bytes, that's an error */
        if (lowval & 0xFF00)
        {
            fprintf(stderr,"libsmacker::smk_bigtree_rec - ERROR: smk_tree_lookup(bs,low8) returned a bogus value.\n");
            return NULL;
        }

        hival = smk_tree_lookup(bs,hi8);
        /* hi8 is an 8-byte tree: if we get something in the top bytes, that's an error */
        if (hival & 0xFF00)
        {
            fprintf(stderr,"libsmacker::smk_bigtree_rec - ERROR: smk_tree_lookup(bs,hi8) returned a bogus value.\n");
            return NULL;
        }

        /* Looks OK: we got low and hi values.  Return a new LEAF */
        smk_malloc(ret,sizeof(struct smk_huff_t));
        ret->b0 = NULL;
        ret->b1 = NULL;
        ret->value = lowval | (hival << 8);

        /* Check for escape markers */
        if (ret->value == big->s16[0])
        {
            ret->escapecode = 0;
            ret->value = 0;
        } else if (ret->value == big->s16[1]) {
            ret->escapecode = 1;
            ret->value = 0;
        } else if (ret->value == big->s16[2]) {
            ret->escapecode = 2;
            ret->value = 0;
        } else {
            ret->escapecode = 0xFF;
        }
    }
    return ret;
}

struct smk_huff_big_t *smk_build_bigtree(struct smk_bit_t* bs)
{
    struct smk_huff_big_t *big;

    struct smk_huff_t *low8;
    struct smk_huff_t *hi8;

    /* check for out-of-bits */
    if (!smk_bits_left(bs))
    {
        fprintf(stderr,"libsmacker::smk_build_bigtree - ERROR: out of bits in bs when building a bigtree HEAD.\n");
        return NULL;
    }
    if (smk_bs_1(bs))
    {
        low8 = smk_build_tree(bs);
        if (low8 == NULL) return NULL;
        hi8 = smk_build_tree(bs);
        if (hi8 == NULL) { smk_del_huffman(low8); return NULL; }

        smk_malloc(big,sizeof(struct smk_huff_big_t));

/* Retrieve three 16-bit values.  That means, 6 bytes should be available. */
        if (bs->byte_num + 7 >= bs->size) {
            fprintf(stderr,"libsmacker::smk_build_bigtree - ERROR: out of bytes in bs when retrieving s16.\n");
            smk_del_huffman(hi8); smk_del_huffman(low8); smk_free(big); return NULL;
        }
        big->s16[0] = smk_bs_8(bs) | (smk_bs_8(bs) << 8);
        big->s16[1] = smk_bs_8(bs) | (smk_bs_8(bs) << 8);
        big->s16[2] = smk_bs_8(bs) | (smk_bs_8(bs) << 8);

        big->t = smk_bigtree_rec(big,low8,hi8,bs);

        /*  skip bits */
        if ( big->t == NULL || !smk_bits_left(bs) || smk_bs_1(bs) )
        {
            fprintf(stderr,"libsmacker::smk_build_tree - ERROR: subtree empty, out of bits in bs when building a tree tail, OR final get_bit returned 1.\n");
            smk_del_huffman(big->t);
            smk_free(big);
        }

        smk_del_huffman(hi8);
        smk_del_huffman(low8);

    } else {
        fprintf(stderr,"libsmacker::smk_build_bigtree - ERROR: initial get_bit returned 0.\n");
        big = NULL;
    }

    btp(big->t,0);

    return big;
}

void smk_bigtree_reset (struct smk_huff_big_t *big)
{
    big->s16[0] = big->s16[1] = big->s16[2] = 0;
}

static unsigned short smk_bigtree_lookup_rec (struct smk_bit_t *bs, struct smk_huff_big_t *big, struct smk_huff_t *t)
{
    unsigned short val;

    if (t->b0 == NULL || t->b1 == NULL)
    {
printf("LEAF: ");
        if (t->escapecode != 0xFF)
        {
            val = big->s16[t->escapecode];
printf("escapecode: %u",t->escapecode);
        } else {
            val = t->value;
printf("value: %u",t->value);
        }

        if ( t->escapecode != 0 )
        {
printf("ESC!=0: shufflin' ");
            big->s16[2] = big->s16[1];
            big->s16[1] = big->s16[0];
            big->s16[0] = val;
        }
printf("ret\n");

        return val;
    } else {
        if (!smk_bits_left(bs))
        {
            fprintf(stderr,"libsmacker::smk_bigtree_lookup - ERROR: out of bits in bs when reading.  Returning bogus value.\n");
            return -1;
        }

        if (smk_bs_1(bs)) {
        printf("NODE: bit1: turning right");
            return smk_bigtree_lookup_rec(bs,big,t->b1);
        } else {
    printf("NODE: bit0: turning left");
            return smk_bigtree_lookup_rec(bs,big,t->b0);
        }
    }
}

unsigned short smk_bigtree_lookup (struct smk_bit_t *bs, struct smk_huff_big_t *big)
{
    return smk_bigtree_lookup_rec(bs,big,big->t);
}

