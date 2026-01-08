#pragma once

#include <stdio.h>
#include <stdbool.h>
#include "bn.h"

/* Functions for shifting number in-place. */
static inline void _lshift_one_bit(struct bn* a);
static inline void _rshift_one_bit(struct bn* a);
static inline void _lshift_word(struct bn* a, int nwords);
static inline void _rshift_word(struct bn* a, int nwords);

/* Public / Exported functions. */
inline void bignum_init(struct bn* n)
{
  require(n, "n is null");

  for (int i = 0; i < BN_ARRAY_SIZE; ++i)
  {
    n->array[i] = 0;
  }
}

inline void bignum_from_int(struct bn* n, DTYPE_TMP i)
{
  require(n, "n is null");

  bignum_init(n);

  /* Endianness issue if machine is not little-endian? */
#ifdef WORD_SIZE
 #if (WORD_SIZE == 1)
  n->array[0] = (i & 0x000000ff);
  n->array[1] = (i & 0x0000ff00) >> 8;
  n->array[2] = (i & 0x00ff0000) >> 16;
  n->array[3] = (i & 0xff000000) >> 24;
 #elif (WORD_SIZE == 2)
  n->array[0] = (i & 0x0000ffff);
  n->array[1] = (i & 0xffff0000) >> 16;
 #elif (WORD_SIZE == 4)
  n->array[0] = (uint32_t)(i & 0xffffffff);
  DTYPE_TMP num_32 = 32;
  DTYPE_TMP tmp = i >> num_32; /* bit-shift with U64 operands to force 64-bit results */
  n->array[1] = (uint32_t)(tmp & 0xffffffff);
 #endif
#endif
}

inline int bignum_to_int(struct bn* n)
{
  require(n, "n is null");

  int ret = 0;

  /* Endianness issue if machine is not little-endian? */
#if (WORD_SIZE == 1)
  ret += n->array[0];
  ret += n->array[1] << 8;
  ret += n->array[2] << 16;
  ret += n->array[3] << 24;
#elif (WORD_SIZE == 2)
  ret += n->array[0];
  ret += n->array[1] << 16;
#elif (WORD_SIZE == 4)
  ret += n->array[0];
#endif

  return ret;
}

inline void bignum_from_string(struct bn* n, char* str, int nbytes)
{
  require(n, "n is null");
  require(str, "str is null");
  require(nbytes > 0, "nbytes must be positive");
  require((nbytes & 1) == 0, "string format must be in hex -> equal number of bytes");
  require((nbytes % (sizeof(DTYPE) * 2)) == 0, "string length must be a multiple of (sizeof(DTYPE) * 2) characters");

  bignum_init(n);

  DTYPE tmp;                        /* DTYPE is defined in bn.h - uint{8,16,32,64}_t */
  int i = nbytes - (2 * WORD_SIZE); /* index into string */
  int j = 0;                        /* index into array */

  /* reading last hex-byte "MSB" from string first -> big endian */
  /* MSB ~= most significant byte / block ? :) */
  while (i >= 0)
  {
    tmp = 0;
    sscanf(&str[i], SSCANF_FORMAT_STR, &tmp);
    n->array[j] = tmp;
    i -= (2 * WORD_SIZE); /* step WORD_SIZE hex-byte(s) back in the string. */
    j += 1;               /* step one element forward in the array. */
  }
}

inline void bignum_to_string(struct bn* n, char* str, int nbytes)
{
  require(n, "n is null");
  require(str, "str is null");
  require(nbytes > 0, "nbytes must be positive");
  require((nbytes & 1) == 0, "string format must be in hex -> equal number of bytes");

  int j = BN_ARRAY_SIZE - 1; /* index into array - reading "MSB" first -> big-endian */
  int i = 0;                 /* index into string representation. */

  /* reading last array-element "MSB" first -> big endian */
  while ((j >= 0) && (nbytes > (i + 1)))
  {
    sprintf(&str[i], SPRINTF_FORMAT_STR, n->array[j]);
    i += (2 * WORD_SIZE); /* step WORD_SIZE hex-byte(s) forward in the string. */
    j -= 1;               /* step one element back in the array. */
  }

  /* Count leading zeros: */
  j = 0;
  while (str[j] == '0')
  {
    j += 1;
  }

  /* Move string j places ahead, effectively skipping leading zeros */
  for (i = 0; i < (nbytes - j); ++i)
  {
    str[i] = str[i + j];
  }

  /* Zero-terminate string */
  str[i] = 0;
}

inline void bignum_dec(struct bn* n)
{
  require(n, "n is null");

  DTYPE tmp; /* copy of n */
  DTYPE res;

  for (int i = 0; i < BN_ARRAY_SIZE; ++i)
  {
    tmp = n->array[i];
    res = tmp - 1;
    n->array[i] = res;

    if (!(res > tmp))
    {
      break;
    }
  }
}

inline void bignum_inc(struct bn* n)
{
  require(n, "n is null");

  DTYPE res;
  DTYPE_TMP tmp; /* copy of n */

  for (int i = 0; i < BN_ARRAY_SIZE; ++i)
  {
    tmp = n->array[i];
    res = (DTYPE)((tmp + 1) & MAX_VAL);
    n->array[i] = res;

    if (res > tmp)
    {
      break;
    }
  }
}

inline void bignum_add(struct bn* a, struct bn* b, struct bn* c)
{
  require(a, "a is null");
  require(b, "b is null");
  require(c, "c is null");

  DTYPE_TMP tmp;
  int carry = 0;
  for (int i = 0; i < BN_ARRAY_SIZE; ++i)
  {
    tmp = (DTYPE_TMP)a->array[i] + (DTYPE_TMP)b->array[i] + carry;
    carry = (tmp > MAX_VAL);
    c->array[i] = (DTYPE)(tmp & MAX_VAL);
  }
}

inline void bignum_sub(struct bn* a, struct bn* b, struct bn* c)
{
  require(a, "a is null");
  require(b, "b is null");
  require(c, "c is null");

  DTYPE_TMP res;
  DTYPE_TMP tmp1;
  DTYPE_TMP tmp2;
  int borrow = 0;
  for (int i = 0; i < BN_ARRAY_SIZE; ++i)
  {
    tmp1 = (DTYPE_TMP)a->array[i] + (MAX_VAL + 1); /* + number_base */
    tmp2 = (DTYPE_TMP)b->array[i] + (DTYPE_TMP)borrow;
    res = (tmp1 - tmp2);
    c->array[i] = (DTYPE)(res & MAX_VAL); /* "modulo number_base" */
    borrow = (res <= MAX_VAL);
  }
}

inline void bignum_mul(struct bn* a, struct bn* b, struct bn* c)
{
  require(a, "a is null");
  require(b, "b is null");
  require(c, "c is null");

  struct bn row;
  struct bn tmp;

  bignum_init(c);

  for (int i = 0; i < BN_ARRAY_SIZE; ++i)
  {
    bignum_init(&row);

    for (int j = 0; j < BN_ARRAY_SIZE; ++j)
    {
      if (i + j < BN_ARRAY_SIZE)
      {
        bignum_init(&tmp);
        DTYPE_TMP intermediate = ((DTYPE_TMP)a->array[i] * (DTYPE_TMP)b->array[j]);
        bignum_from_int(&tmp, intermediate);
        _lshift_word(&tmp, i + j);
        bignum_add(&tmp, &row, &row);
      }
    }
    bignum_add(c, &row, c);
  }
}

inline void bignum_div(struct bn* a, struct bn* b, struct bn* c)
{
  require(a, "a is null");
  require(b, "b is null");
  require(c, "c is null");

  struct bn current;
  struct bn denom;
  struct bn tmp;

  bignum_from_int(&current, 1);
  bignum_assign(&denom, b);
  bignum_assign(&tmp, a);

  const DTYPE_TMP half_max = 1 + (DTYPE_TMP)(MAX_VAL / 2);
  bool overflow = false;
  while (bignum_cmp(&denom, a) != LARGER)
  {
    if (denom.array[BN_ARRAY_SIZE - 1] >= half_max)
    {
      overflow = true;
      break;
    }
    _lshift_one_bit(&current);
    _lshift_one_bit(&denom);
  }
  if (!overflow)
  {
    _rshift_one_bit(&denom);
    _rshift_one_bit(&current);
  }
  bignum_init(c);

  while (!bignum_is_zero(&current))
  {
    if (bignum_cmp(&tmp, &denom) != SMALLER)
    {
      bignum_sub(&tmp, &denom, &tmp);
      bignum_or(c, &current, c);
    }
    _rshift_one_bit(&current);
    _rshift_one_bit(&denom);
  }
}

inline void bignum_lshift(struct bn* a, struct bn* b, int nbits)
{
  require(a, "a is null");
  require(b, "b is null");
  require(nbits >= 0, "no negative shifts");

  bignum_assign(b, a);
  const int nbits_pr_word = (WORD_SIZE * 8);
  int nwords = nbits / nbits_pr_word;
  if (nwords != 0)
  {
    _lshift_word(b, nwords);
    nbits -= (nwords * nbits_pr_word);
  }

  if (nbits != 0)
  {
    int i;
    for (i = (BN_ARRAY_SIZE - 1); i > 0; --i)
    {
      b->array[i] = (b->array[i] << nbits) | (b->array[i - 1] >> ((8 * WORD_SIZE) - nbits));
    }
    b->array[i] <<= nbits;
  }
}

inline void bignum_rshift(struct bn* a, struct bn* b, int nbits)
{
  require(a, "a is null");
  require(b, "b is null");
  require(nbits >= 0, "no negative shifts");

  bignum_assign(b, a);
  const int nbits_pr_word = (WORD_SIZE * 8);
  int nwords = nbits / nbits_pr_word;
  if (nwords != 0)
  {
    _rshift_word(b, nwords);
    nbits -= (nwords * nbits_pr_word);
  }

  if (nbits != 0)
  {
    int i;
    for (i = 0; i < (BN_ARRAY_SIZE - 1); ++i)
    {
      b->array[i] = (b->array[i] >> nbits) | (b->array[i + 1] << ((8 * WORD_SIZE) - nbits));
    }
    b->array[i] >>= nbits;
  }
}

inline void bignum_mod(struct bn* a, struct bn* b, struct bn* c)
{
  require(a, "a is null");
  require(b, "b is null");
  require(c, "c is null");

  struct bn tmp;
  bignum_divmod(a, b, &tmp, c);
}

inline void bignum_divmod(struct bn* a, struct bn* b, struct bn* c, struct bn* d)
{
  require(a, "a is null");
  require(b, "b is null");
  require(c, "c is null");

  struct bn tmp;

  bignum_div(a, b, c);
  bignum_mul(c, b, &tmp);
  bignum_sub(a, &tmp, d);
}

inline void bignum_not(struct bn* a, struct bn* b)
{
  require(a, "a is null");
  require(b, "b is null");

  for (int i = 0; i < BN_ARRAY_SIZE; ++i)
  {
    b->array[i] = (DTYPE)~a->array[i];
  }
}

inline void bignum_and(struct bn* a, struct bn* b, struct bn* c)
{
  require(a, "a is null");
  require(b, "b is null");
  require(c, "c is null");

  for (int i = 0; i < BN_ARRAY_SIZE; ++i)
  {
    c->array[i] = (a->array[i] & b->array[i]);
  }
}

inline void bignum_or(struct bn* a, struct bn* b, struct bn* c)
{
  require(a, "a is null");
  require(b, "b is null");
  require(c, "c is null");

  for (int i = 0; i < BN_ARRAY_SIZE; ++i)
  {
    c->array[i] = (a->array[i] | b->array[i]);
  }
}

inline void bignum_xor(struct bn* a, struct bn* b, struct bn* c)
{
  require(a, "a is null");
  require(b, "b is null");
  require(c, "c is null");

  for (int i = 0; i < BN_ARRAY_SIZE; ++i)
  {
    c->array[i] = (a->array[i] ^ b->array[i]);
  }
}

inline int bignum_cmp(struct bn* a, struct bn* b)
{
  require(a, "a is null");
  require(b, "b is null");

  int i = BN_ARRAY_SIZE;
  do
  {
    i -= 1;
    if (a->array[i] > b->array[i])
    {
      return LARGER;
    }
    else if (a->array[i] < b->array[i])
    {
      return SMALLER;
    }
  }
  while (i != 0);

  return EQUAL;
}

inline int bignum_is_zero(struct bn* n)
{
  require(n, "n is null");

  for (int i = 0; i < BN_ARRAY_SIZE; ++i)
  {
    if (n->array[i])
    {
      return 0;
    }
  }

  return 1;
}

inline void bignum_pow(struct bn* a, struct bn* b, struct bn* c, uint32_t maxIterations)
{
  require(a, "a is null");
  require(b, "b is null");
  require(c, "c is null");

  struct bn tmp;

  bignum_init(c);

  if (bignum_cmp(b, c) == EQUAL)
  {
    bignum_inc(c);
  }
  else
  {
    struct bn d;
    bignum_from_int(&d, maxIterations);
    int cmp = bignum_cmp(b, &d);
    require(cmp == EQUAL || cmp == SMALLER, "max iterations violated");
    struct bn bcopy;
    bignum_assign(&bcopy, b);

    bignum_assign(&tmp, a);

    bignum_dec(&bcopy);

#ifdef _DEBUG
    uint32_t iterations = 0;
#endif
    while (!bignum_is_zero(&bcopy))
    {
#ifdef _DEBUG
      if (iterations++ > maxIterations)
      {
        require(false, "pow out of range");
        break;
      }
#endif
      bignum_mul(&tmp, a, c);
      bignum_dec(&bcopy);

      bignum_assign(&tmp, c);
    }

    bignum_assign(c, &tmp);
  }
}

inline void bignum_isqrt(struct bn* a, struct bn* b)
{
  require(a, "a is null");
  require(b, "b is null");

  struct bn low, high, mid, tmp;

  bignum_init(&low);
  bignum_assign(&high, a);
  bignum_rshift(&high, &mid, 1);
  bignum_inc(&mid);

  while (bignum_cmp(&high, &low) > 0)
  {
    bignum_mul(&mid, &mid, &tmp);
    if (bignum_cmp(&tmp, a) > 0)
    {
      bignum_assign(&high, &mid);
      bignum_dec(&high);
    }
    else
    {
      bignum_assign(&low, &mid);
    }
    bignum_sub(&high, &low, &mid);
    _rshift_one_bit(&mid);
    bignum_add(&low, &mid, &mid);
    bignum_inc(&mid);
  }
  bignum_assign(b, &low);
}

inline void bignum_assign(struct bn* dst, struct bn* src)
{
  require(dst, "dst is null");
  require(src, "src is null");

  for (int i = 0; i < BN_ARRAY_SIZE; ++i)
  {
    dst->array[i] = src->array[i];
  }
}

/* Private / Static functions. */
static inline void _rshift_word(struct bn* a, int nwords)
{
  require(a, "a is null");
  require(nwords >= 0, "no negative shifts");

  int i;
  if (nwords >= BN_ARRAY_SIZE)
  {
    for (i = 0; i < BN_ARRAY_SIZE; ++i)
    {
      a->array[i] = 0;
    }
    return;
  }

  for (i = 0; i < BN_ARRAY_SIZE - nwords; ++i)
  {
    a->array[i] = a->array[i + nwords];
  }
  for (; i < BN_ARRAY_SIZE; ++i)
  {
    a->array[i] = 0;
  }
}

static inline void _lshift_word(struct bn* a, int nwords)
{
  require(a, "a is null");
  require(nwords >= 0, "no negative shifts");

  int i;
  for (i = (BN_ARRAY_SIZE - 1); i >= nwords; --i)
  {
    a->array[i] = a->array[i - nwords];
  }
  for (; i >= 0; --i)
  {
    a->array[i] = 0;
  }
}

static inline void _lshift_one_bit(struct bn* a)
{
  require(a, "a is null");

  for (int i = (BN_ARRAY_SIZE - 1); i > 0; --i)
  {
    a->array[i] = (a->array[i] << 1) | (a->array[i - 1] >> ((8 * WORD_SIZE) - 1));
  }
  a->array[0] <<= 1;
}

static inline void _rshift_one_bit(struct bn* a)
{
  require(a, "a is null");

  for (int i = 0; i < (BN_ARRAY_SIZE - 1); ++i)
  {
    a->array[i] = (a->array[i] >> 1) | (a->array[i + 1] << ((8 * WORD_SIZE) - 1));
  }
  a->array[BN_ARRAY_SIZE - 1] >>= 1;
}
