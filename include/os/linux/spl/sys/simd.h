/*
 *  Copyright (C) 2024 Tino Reichardt <milky-zfs@mcmilk.de>
 *
 *  This file is part of the SPL, Solaris Porting Layer.
 *
 *  The SPL is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  The SPL is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with the SPL.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef	_SPL_SIMD_H
#define	_SPL_SIMD_H

extern void simd_stat_init(void);
extern void simd_stat_fini(void);

extern int spl_kfpu_allowed(void);
extern int spl_kfpu_cpu_has(int feature);
extern void spl_kfpu_begin(void);
extern void spl_kfpu_end(void);

/*
 * Define functions for CPUID features testing
 */
#define	DF(name, id)	\
static inline int			\
name(void)				\
{					\
	return (spl_kfpu_cpu_has(id));	\
}

/* X86 */
#define	ZFS_X86_INIT_DONE	(1 << 1)
#define	ZFS_X86_SSE		(1 << 2)
#define	ZFS_X86_SSE2		(1 << 3)
#define	ZFS_X86_SSSE3		(1 << 4)
#define	ZFS_X86_SSE4_1		(1 << 5)
#define	ZFS_X86_SSE4_2		(1 << 6)
#define	ZFS_X86_AVX		(1 << 7)
#define	ZFS_X86_AVX2		(1 << 8)
#define	ZFS_X86_AVX512F		(1 << 9)
#define	ZFS_X86_AVX512BW	(1 << 10)
#define	ZFS_X86_AVX512VL	(1 << 11)
#define	ZFS_X86_AES		(1 << 12)
#define	ZFS_X86_MOVBE		(1 << 13)
#define	ZFS_X86_PCLMULQDQ	(1 << 14)
#define	ZFS_X86_SHA_NI		(1 << 15)

DF(zfs_sse_available, ZFS_X86_SSE);
DF(zfs_sse2_available, ZFS_X86_SSE2);
DF(zfs_ssse3_available, ZFS_X86_SSSE3);
DF(zfs_sse4_1_available, ZFS_X86_SSE4_1);
DF(zfs_sse4_2_available, ZFS_X86_SSE4_2);
DF(zfs_avx_available, ZFS_X86_AVX);
DF(zfs_avx2_available, ZFS_X86_AVX2);
DF(zfs_avx512f_available, ZFS_X86_AVX512F);
DF(zfs_avx512bw_available, ZFS_X86_AVX512BW);
DF(zfs_avx512vl_available, ZFS_X86_AVX512VL);
DF(zfs_aes_available, ZFS_X86_AES);
DF(zfs_movbe_available, ZFS_X86_MOVBE);
DF(zfs_pclmulqdq_available, ZFS_X86_PCLMULQDQ);
DF(zfs_shani_available, ZFS_X86_SHA_NI);

/* ARM */
#define	ZFS_ARM_INIT_DONE	(1 << 1)
#define	ZFS_ARM_NEON		(1 << 2)
#define	ZFS_ARM_SHA256		(1 << 3)
#define	ZFS_ARM_SHA512		(1 << 4)

DF(zfs_neon_available, ZFS_ARM_NEON);
DF(zfs_sha256_available, ZFS_ARM_SHA256);
DF(zfs_sha512_available, ZFS_ARM_SHA512);

/* PPC */
#define	ZFS_PPC_INIT_DONE	(1 << 1)
#define	ZFS_PPC_ALTIVEC		(1 << 2)
#define	ZFS_PPC_VSX		(1 << 3)
#define	ZFS_PPC_ISA207		(1 << 4)

DF(zfs_altivec_available, ZFS_PPC_ALTIVEC);
DF(zfs_vsx_available, ZFS_PPC_VSX);
DF(zfs_isa207_available, ZFS_PPC_ISA207);
#undef DF

#define kfpu_allowed	spl_kfpu_allowed
#define kfpu_cpu_has	spl_kfpu_cpu_has
#define kfpu_begin	spl_kfpu_begin
#define kfpu_end	spl_kfpu_end

#endif /* _SPL_SIMD_H */
