// SPDX-License-Identifier: MIT
//
// Copyright (c) 2011 Google, Inc.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

/*
 * Copyright (c) 2017 by Delphix. All rights reserved.
 */

#if 0
#include <cityhash.h>

#define	HASH_K1 0xb492b66fbe98f273ULL
#define	HASH_K2 0x9ae16a3b2f90404fULL

/*
 * Bitwise right rotate.  Normally this will compile to a single
 * instruction.
 */
static inline uint64_t
rotate(uint64_t val, int shift)
{
	// Avoid shifting by 64: doing so yields an undefined result.
	return (shift == 0 ? val : (val >> shift) | (val << (64 - shift)));
}

static inline uint64_t
cityhash_helper(uint64_t u, uint64_t v, uint64_t mul)
{
	uint64_t a = (u ^ v) * mul;
	a ^= (a >> 47);
	uint64_t b = (v ^ a) * mul;
	b ^= (b >> 47);
	b *= mul;
	return (b);
}

static inline uint64_t
cityhash_impl(uint64_t w1, uint64_t w2, uint64_t w3, uint64_t w4)
{
	uint64_t mul = HASH_K2 + 64;
	uint64_t a = w1 * HASH_K1;
	uint64_t b = w2;
	uint64_t c = w4 * mul;
	uint64_t d = w3 * HASH_K2;
	return (cityhash_helper(rotate(a + b, 43) + rotate(c, 30) + d,
	    a + rotate(b + HASH_K2, 18) + c, mul));
}

/*
 * Passing w as the 2nd argument could save one 64-bit multiplication.
 */
uint64_t
cityhash1(uint64_t w)
{
	return (cityhash_impl(0, w, 0, 0));
}

uint64_t
cityhash2(uint64_t w1, uint64_t w2)
{
	return (cityhash_impl(w1, w2, 0, 0));
}

uint64_t
cityhash3(uint64_t w1, uint64_t w2, uint64_t w3)
{
	return (cityhash_impl(w1, w2, w3, 0));
}

uint64_t
cityhash4(uint64_t w1, uint64_t w2, uint64_t w3, uint64_t w4)
{
	return (cityhash_impl(w1, w2, w3, w4));
}
#endif

/* XXX */
typedef uint64_t XXH64_hash_t;
typedef struct {
	XXH64_hash_t low64;
	XXH64_hash_t high64;
} XXH128_hash_t;

#define XXH3_SECRET_SIZE_MIN 136
#define XXH_SECRET_DEFAULT_SIZE 192

XXH64_hash_t XXH3_64bits(void const *const input, size_t const length);

#define PRIME32_1	0x9E3779B1U
#define PRIME32_2	0x85EBCA77U
#define PRIME32_3	0xC2B2AE3DU

#define PRIME64_1	0x9E3779B185EBCA87ULL
#define PRIME64_2	0xC2B2AE3D27D4EB4FULL
#define PRIME64_3	0x165667B19E3779F9ULL
#define PRIME64_4	0x85EBCA77C2B2AE63ULL
#define PRIME64_5	0x27D4EB2F165667C5ULL

/* Portably reads a 32-bit little endian integer from p. */
static uint32_t XXH_read32(uint8_t const *const p)
{
	uint32_t ret;
	memcpy(&ret, p, sizeof(uint32_t));
	return ret;
}

/* Portably reads a 64-bit little endian integer from p. */
static uint64_t XXH_read64(uint8_t const *const p)
{
	uint64_t ret;
	memcpy(&ret, p, sizeof(uint64_t));
	return ret;
}

/* Portably writes a 64-bit little endian integer to p. */
static void XXH_write64(uint8_t *const p, uint64_t val)
{
	memcpy(p, &val, sizeof(uint64_t));
}

#define XXH_swap32 __builtin_bswap32
#define XXH_swap64 __builtin_bswap64

#define	XXH_rotl32(x, n)	(((x) << (n)) | ((x) >> (32 - (n))))
#define	XXH_rotl64(x, n)	(((x) << (n)) | ((x) >> (64 - (n))))

/* Pseudorandom data taken directly from FARSH */
static uint8_t const kSecret[XXH_SECRET_DEFAULT_SIZE] = {
	0xb8, 0xfe, 0x6c, 0x39, 0x23, 0xa4, 0x4b, 0xbe, 0x7c, 0x01, 0x81,
	0x2c, 0xf7, 0x21, 0xad, 0x1c, 0xde, 0xd4, 0x6d, 0xe9, 0x83, 0x90,
	0x97, 0xdb, 0x72, 0x40, 0xa4, 0xa4, 0xb7, 0xb3, 0x67, 0x1f, 0xcb,
	0x79, 0xe6, 0x4e, 0xcc, 0xc0, 0xe5, 0x78, 0x82, 0x5a, 0xd0, 0x7d,
	0xcc, 0xff, 0x72, 0x21, 0xb8, 0x08, 0x46, 0x74, 0xf7, 0x43, 0x24,
	0x8e, 0xe0, 0x35, 0x90, 0xe6, 0x81, 0x3a, 0x26, 0x4c, 0x3c, 0x28,
	0x52, 0xbb, 0x91, 0xc3, 0x00, 0xcb, 0x88, 0xd0, 0x65, 0x8b, 0x1b,
	0x53, 0x2e, 0xa3, 0x71, 0x64, 0x48, 0x97, 0xa2, 0x0d, 0xf9, 0x4e,
	0x38, 0x19, 0xef, 0x46, 0xa9, 0xde, 0xac, 0xd8, 0xa8, 0xfa, 0x76,
	0x3f, 0xe3, 0x9c, 0x34, 0x3f, 0xf9, 0xdc, 0xbb, 0xc7, 0xc7, 0x0b,
	0x4f, 0x1d, 0x8a, 0x51, 0xe0, 0x4b, 0xcd, 0xb4, 0x59, 0x31, 0xc8,
	0x9f, 0x7e, 0xc9, 0xd9, 0x78, 0x73, 0x64, 0xea, 0xc5, 0xac, 0x83,
	0x34, 0xd3, 0xeb, 0xc3, 0xc5, 0x81, 0xa0, 0xff, 0xfa, 0x13, 0x63,
	0xeb, 0x17, 0x0d, 0xdd, 0x51, 0xb7, 0xf0, 0xda, 0x49, 0xd3, 0x16,
	0x55, 0x26, 0x29, 0xd4, 0x68, 0x9e, 0x2b, 0x16, 0xbe, 0x58, 0x7d,
	0x47, 0xa1, 0xfc, 0x8f, 0xf8, 0xb8, 0xd1, 0x7a, 0xd0, 0x31, 0xce,
	0x45, 0xcb, 0x3a, 0x8f, 0x95, 0x16, 0x04, 0x28, 0xaf, 0xd7, 0xfb,
	0xca, 0xbb, 0x4b, 0x40, 0x7e
};

/* Calculates a 64-bit to 128-bit unsigned multiply, then xor's the low bits of the product with
 * the high bits for a 64-bit result. */
static uint64_t XXH3_mul128_fold64(uint64_t const lhs, uint64_t const rhs)
{
#if defined(__SIZEOF_INT128__) || (defined(_INTEGRAL_MAX_BITS) && _INTEGRAL_MAX_BITS >= 128)
	__uint128_t product = (__uint128_t) lhs * (__uint128_t) rhs;
	return (uint64_t) (product & 0xFFFFFFFFFFFFFFFFULL) ^
	    (uint64_t) (product >> 64);

	/* There are other platform-specific versions in the official repo.
	 * They would all be left out in favor of the code above, but it is not
	 * portable, so we keep the generic version. */

#else				/* Portable scalar version */
	/* First calculate all of the cross products. */
	uint64_t const lo_lo = (lhs & 0xFFFFFFFF) * (rhs & 0xFFFFFFFF);
	uint64_t const hi_lo = (lhs >> 32) * (rhs & 0xFFFFFFFF);
	uint64_t const lo_hi = (lhs & 0xFFFFFFFF) * (rhs >> 32);
	uint64_t const hi_hi = (lhs >> 32) * (rhs >> 32);

	/* Now add the products together. These will never overflow. */
	uint64_t const cross = (lo_lo >> 32) + (hi_lo & 0xFFFFFFFF) + lo_hi;
	uint64_t const upper = (hi_lo >> 32) + (cross >> 32) + hi_hi;
	uint64_t const lower = (cross << 32) | (lo_lo & 0xFFFFFFFF);

	return upper ^ lower;
#endif
}

#define STRIPE_LEN 64
#define XXH_SECRET_CONSUME_RATE 8	/* nb of secret bytes consumed at each accumulation */
#define ACC_NB (STRIPE_LEN / sizeof(uint64_t))

/* Mixes up the hash to finalize */
static XXH64_hash_t XXH3_avalanche(uint64_t hash)
{
	hash ^= hash >> 37;
	hash *= 0x165667919E3779F9ULL;
	hash ^= hash >> 32;
	return hash;
}

/* ==========================================
 * Short keys
 * ========================================== */

/* Hashes zero-length keys */
static XXH64_hash_t XXH3_len_0_64b(uint8_t const *const secret,
				   XXH64_hash_t const seed)
{
	uint64_t acc = seed;
	acc += PRIME64_1;
	acc ^= XXH_read64(secret + 56);
	acc ^= XXH_read64(secret + 64);
	return XXH3_avalanche(acc);
}

/* Hashes short keys from 1 to 3 bytes. */
static XXH64_hash_t XXH3_len_1to3_64b(uint8_t const *const input,
				      size_t const length,
				      uint8_t const *const secret,
				      XXH64_hash_t const seed)
{
	uint8_t const byte1 = input[0];
	uint8_t const byte2 = (length > 1) ? input[1] : input[0];
	uint8_t const byte3 = input[length - 1];

	uint32_t const combined = ((uint32_t) byte1 << 16)
	    | ((uint32_t) byte2 << 24)
	    | ((uint32_t) byte3 << 0)
	    | ((uint32_t) length << 8);
	uint64_t acc = (uint64_t) (XXH_read32(secret) ^ XXH_read32(secret + 4));
	acc += seed;
	acc ^= (uint64_t) combined;
	acc *= PRIME64_1;
	return XXH3_avalanche(acc);
}

/* Hashes short keys from 4 to 8 bytes. */
static XXH64_hash_t XXH3_len_4to8_64b(uint8_t const *const input,
				      size_t const length,
				      uint8_t const *const secret,
				      XXH64_hash_t seed)
{
	uint32_t const input_hi = XXH_read32(input);
	uint32_t const input_lo = XXH_read32(input + length - 4);
	uint64_t const input_64 =
	    (uint64_t) input_lo | ((uint64_t) input_hi << 32);
	uint64_t acc = XXH_read64(secret + 8) ^ XXH_read64(secret + 16);
	seed ^= (uint64_t) XXH_swap32(seed & 0xFFFFFFFF) << 32;
	acc -= seed;
	acc ^= input_64;
	/* rrmxmx mix, skips XXH3_avalanche */
	acc ^= XXH_rotl64(acc, 49) ^ XXH_rotl64(acc, 24);
	acc *= 0x9FB21C651E98DF25ULL;
	acc ^= (acc >> 35) + (uint64_t) length;
	acc *= 0x9FB21C651E98DF25ULL;
	acc ^= (acc >> 28);
	return acc;
}

/* Hashes short keys from 9 to 16 bytes. */
static XXH64_hash_t XXH3_len_9to16_64b(uint8_t const *const input,
				       size_t const length,
				       uint8_t const *const secret,
				       XXH64_hash_t const seed)
{
	uint64_t input_lo = XXH_read64(secret + 24) ^ XXH_read64(secret + 32);
	uint64_t input_hi = XXH_read64(secret + 40) ^ XXH_read64(secret + 48);
	uint64_t acc = (uint64_t) length;
	input_lo += seed;
	input_hi -= seed;
	input_lo ^= XXH_read64(input);
	input_hi ^= XXH_read64(input + length - 8);
	acc += XXH_swap64(input_lo);
	acc += input_hi;
	acc += XXH3_mul128_fold64(input_lo, input_hi);
	return XXH3_avalanche(acc);
}

/* Hashes short keys that are less than or equal to 16 bytes. */
static XXH64_hash_t XXH3_len_0to16_64b(uint8_t const *const input,
				       size_t const length,
				       uint8_t const *const secret,
				       XXH64_hash_t const seed)
{
	if (length > 8)
		return XXH3_len_9to16_64b(input, length, secret, seed);
	else if (length >= 4)
		return XXH3_len_4to8_64b(input, length, secret, seed);
	else if (length != 0)
		return XXH3_len_1to3_64b(input, length, secret, seed);
	return XXH3_len_0_64b(secret, seed);
}

/* The primary mixer for the midsize hashes */
static uint64_t XXH3_mix16B(uint8_t const *const input,
			    uint8_t const *const secret, XXH64_hash_t seed)
{
	uint64_t lhs = seed;
	uint64_t rhs = 0U - seed;
	lhs += XXH_read64(secret);
	rhs += XXH_read64(secret + 8);
	lhs ^= XXH_read64(input);
	rhs ^= XXH_read64(input + 8);
	return XXH3_mul128_fold64(lhs, rhs);
}

/* Hashes midsize keys from 17 to 128 bytes */
static XXH64_hash_t XXH3_len_17to128_64b(uint8_t const *const input,
					 size_t const length,
					 uint8_t const *const secret,
					 XXH64_hash_t const seed)
{
	int i = (int)((length - 1) / 32);

	uint64_t acc = length * PRIME64_1;

	for (; i >= 0; i--) {
		acc += XXH3_mix16B(input + (16 * i), secret + (32 * i), seed);
		acc +=
		    XXH3_mix16B(input + length - (16 * (i + 1)),
				secret + (32 * i) + 16, seed);
	}
	return XXH3_avalanche(acc);
}

#define XXH3_MIDSIZE_MAX 240

/* Hashes midsize keys from 129 to 240 bytes */
static XXH64_hash_t XXH3_len_129to240_64b(uint8_t const *const input,
					  size_t const length,
					  uint8_t const *const secret,
					  XXH64_hash_t const seed)
{

#define XXH3_MIDSIZE_STARTOFFSET 3
#define XXH3_MIDSIZE_LASTOFFSET  17

	uint64_t acc = (uint64_t) length * PRIME64_1;
	int const nbRounds = (int)length / 16;
	int i;
	for (i = 0; i < 8; i++) {
		acc += XXH3_mix16B(input + (16 * i), secret + (16 * i), seed);
	}

	acc = XXH3_avalanche(acc);

	for (i = 8; i < nbRounds; i++) {
		acc += XXH3_mix16B(input + (16 * i), secret + (16 * (i - 8))
				   + XXH3_MIDSIZE_STARTOFFSET, seed);
	}
	/* last bytes */
	acc += XXH3_mix16B(input + length - 16,
			   secret + XXH3_SECRET_SIZE_MIN
			   - XXH3_MIDSIZE_LASTOFFSET, seed);
	return XXH3_avalanche(acc);
}

/* Hashes a short input, < 240 bytes */
static XXH64_hash_t XXH3_hashShort_64b(uint8_t const *const input,
				       size_t const length,
				       uint8_t const *const secret,
				       XXH64_hash_t const seed)
{
	if (length <= 16)
		return XXH3_len_0to16_64b(input, length, secret, seed);
	if (length <= 128)
		return XXH3_len_17to128_64b(input, length, secret, seed);
	return XXH3_len_129to240_64b(input, length, secret, seed);
}

/* This is the main loop. This is usually written in SIMD code. */
static void XXH3_accumulate_512_64b(uint64_t *const acc,
				    uint8_t const *const input,
				    uint8_t const *const secret)
{
	size_t i;
	for (i = 0; i < ACC_NB; i++) {
		uint64_t input_val = XXH_read64(input + (8 * i));
		acc[i] += input_val;
		input_val ^= XXH_read64(secret + (8 * i));
		acc[i] += (uint32_t) input_val *(input_val >> 32);
	}
}

/* Scrambles input. This is usually written in SIMD code, as it is usually part of the main loop. */
static void XXH3_scrambleAcc(uint64_t *const acc, uint8_t const *const secret)
{
	size_t i;
	for (i = 0; i < ACC_NB; i++) {
		acc[i] ^= acc[i] >> 47;
		acc[i] ^= XXH_read64(secret + (8 * i));
		acc[i] *= PRIME32_1;
	}
}

/* Processes a full block. */
static void XXH3_accumulate_64b(uint64_t *const acc,
				uint8_t const *const input,
				uint8_t const *const secret,
				size_t const nb_stripes)
{
	size_t n;
	for (n = 0; n < nb_stripes; n++) {
		XXH3_accumulate_512_64b(acc, input + n * STRIPE_LEN,
					secret + (8 * n));
	}
}

/* Combines two accumulators with two keys */
static uint64_t XXH3_mix2Accs(uint64_t const *const acc,
			      uint8_t const *const secret)
{
	return XXH3_mul128_fold64(acc[0] ^ XXH_read64(secret),
				  acc[1] ^ XXH_read64(secret + 8));
}

/* Combines 8 accumulators with keys into 1 finalized 64-bit hash. */
static XXH64_hash_t XXH3_mergeAccs(uint64_t const *const acc,
				   uint8_t const *const key,
				   uint64_t const start)
{
	uint64_t result64 = start;
	size_t i = 0;
	for (i = 0; i < 4; i++)
		result64 += XXH3_mix2Accs(acc + 2 * i, key + 16 * i);

	return XXH3_avalanche(result64);
}

/* Controls the long hash function. This is used for both XXH3_64 and XXH3_128. */
static XXH64_hash_t XXH3_hashLong_64b(uint8_t const *const input,
				      size_t const length,
				      uint8_t const *const secret,
				      size_t const secret_size)
{
	size_t const nb_rounds =
	    (secret_size - STRIPE_LEN) / XXH_SECRET_CONSUME_RATE;
	size_t const block_len = STRIPE_LEN * nb_rounds;
	size_t const nb_blocks = length / block_len;
	size_t const nb_stripes =
	    (length - (block_len * nb_blocks)) / STRIPE_LEN;
	size_t n;
	uint64_t acc[ACC_NB];

	acc[0] = PRIME32_3;
	acc[1] = PRIME64_1;
	acc[2] = PRIME64_2;
	acc[3] = PRIME64_3;
	acc[4] = PRIME64_4;
	acc[5] = PRIME32_2;
	acc[6] = PRIME64_5;
	acc[7] = PRIME32_1;

	for (n = 0; n < nb_blocks; n++) {
		XXH3_accumulate_64b(acc, input + n * block_len, secret,
				    nb_rounds);
		XXH3_scrambleAcc(acc, secret + secret_size - STRIPE_LEN);
	}

	/* last partial block */
	XXH3_accumulate_64b(acc, input + nb_blocks * block_len, secret,
			    nb_stripes);

	/* last stripe */
	if (length % STRIPE_LEN != 0) {
		uint8_t const *const p = input + length - STRIPE_LEN;
		/* Do not align on 8, so that the secret is different from the scrambler */
#define XXH_SECRET_LASTACC_START 7
		XXH3_accumulate_512_64b(acc, p,
					secret + secret_size - STRIPE_LEN -
					XXH_SECRET_LASTACC_START);
	}

#define XXH_SECRET_MERGEACCS_START 11

	/* converge into final hash */
	return XXH3_mergeAccs(acc, secret + XXH_SECRET_MERGEACCS_START,
			      (uint64_t) length * PRIME64_1);
}

/* Hashes a long input, > 240 bytes */
static XXH64_hash_t XXH3_hashLong_64b_withSeed(uint8_t const *const input,
					       size_t const length,
					       XXH64_hash_t const seed)
{
	uint8_t secret[XXH_SECRET_DEFAULT_SIZE];
	size_t i;

	for (i = 0; i < XXH_SECRET_DEFAULT_SIZE / 16; i++) {
		XXH_write64(secret + (16 * i),
			    XXH_read64(kSecret + (16 * i)) + seed);
		XXH_write64(secret + (16 * i) + 8,
			    XXH_read64(kSecret + (16 * i) + 8) - seed);
	}
	return XXH3_hashLong_64b(input, length, secret, sizeof(secret));
}

/* The XXH3_64 seeded hash function.
 * input: The data to hash.
 * length:  The length of input. It is undefined behavior to have length larger than the
 *          capacity of input.
 * seed:    A 64-bit value to seed the hash with.
 * returns: The 64-bit calculated hash value. */
XXH64_hash_t XXH3_64bits_withSeed(void const *const input, size_t const length,
				  XXH64_hash_t const seed)
{
	if (length <= XXH3_MIDSIZE_MAX)
		return XXH3_hashShort_64b((uint8_t const *)input, length,
					  kSecret, seed);
	return XXH3_hashLong_64b_withSeed((uint8_t const *)input, length, seed);
}

/* The XXH3_64 non-seeded hash function.
 * input: The data to hash.
 * length:  The length of input. It is undefined behavior to have length larger than the
 *          capacity of input.
 * returns: The 64-bit calculated hash value. */
XXH64_hash_t XXH3_64bits(void const *const input, size_t const length)
{
	return XXH3_64bits_withSeed(input, length, 0);
}

/* The XXH3_64 hash function with custom secrets
 * input: The data to hash.
 * length:  The length of input. It is undefined behavior to have length larger than the
 *          capacity of input.
 * secret:  The custom secret.
 * secret_size: The size of the custom secret. Must be >= XXH_SECRET_SIZE_MIN.
 * returns: The 64-bit calculated hash value. */
XXH64_hash_t XXH3_64bits_withSecret(void const *const input,
				    size_t const length,
				    void const *const secret,
				    size_t const secret_size)
{
	if (length <= XXH3_MIDSIZE_MAX)
		return XXH3_hashShort_64b((uint8_t const *)input, length,
					  (uint8_t const *)secret, 0);
	return XXH3_hashLong_64b((uint8_t const *)input, length,
				 (uint8_t const *)secret, secret_size);
}


/*
 * Passing w as the 2nd argument could save one 64-bit multiplication.
 */
uint64_t
cityhash1(uint64_t w)
{
	uint64_t in[1];
	const void *input = in;
	in[0] = w;
	return (XXH3_64bits(input, 8));
}

uint64_t
cityhash2(uint64_t w1, uint64_t w2)
{
	uint64_t in[2];
	const void *input = in;
	in[0] = w1;
	in[1] = w2;
	return (XXH3_64bits(input, 8*2));
}

uint64_t
cityhash3(uint64_t w1, uint64_t w2, uint64_t w3)
{
	uint64_t in[3];
	const void *input = in;
	in[0] = w1;
	in[1] = w2;
	in[2] = w3;
	return (XXH3_64bits(input, 8*3));
}

uint64_t
cityhash4(uint64_t w1, uint64_t w2, uint64_t w3, uint64_t w4)
{
	uint64_t in[4];
	const void *input = in;
	in[0] = w1;
	in[1] = w2;
	in[2] = w3;
	in[3] = w4;
	return (XXH3_64bits(input, 8*4));
}

#if defined(_KERNEL)
EXPORT_SYMBOL(cityhash1);
EXPORT_SYMBOL(cityhash2);
EXPORT_SYMBOL(cityhash3);
EXPORT_SYMBOL(cityhash4);
#endif
