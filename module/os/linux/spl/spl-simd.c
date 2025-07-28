// SPDX-License-Identifier: GPL-2.0-or-later
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
 *
 *  Solaris Porting Layer (SPL) Kernel FPU management.
 */

#include <sys/isa_defs.h>
#include <sys/types.h>
#include <sys/simd.h>

static uint32_t cpu_features = 0;

/* little helper */
#define isset(v,x) (((v)&(x)) == (x))
#define	FSET(l,z) do { if (boot_cpu_has(l)) { cpu_features |= z; }} while (0)

#if defined(__x86_64) || defined(__i386)
#include <asm/fpu/api.h>

int
spl_kfpu_cpu_has(int feature_set)
{
	if (unlikely(cpu_features == 0)) {
		cpu_features = ZFS_X86_INIT_DONE;
		FSET(X86_FEATURE_XMM, ZFS_X86_SSE);
		FSET(X86_FEATURE_XMM2, ZFS_X86_SSE2);
		FSET(X86_FEATURE_SSSE3, ZFS_X86_SSSE3);
		FSET(X86_FEATURE_XMM4_1, ZFS_X86_SSE4_1);
		FSET(X86_FEATURE_XMM4_2, ZFS_X86_SSE4_2);
		FSET(X86_FEATURE_AVX, ZFS_X86_AVX);
		FSET(X86_FEATURE_AVX2, ZFS_X86_AVX2);
		FSET(X86_FEATURE_AVX512F, ZFS_X86_AVX512F);
		FSET(X86_FEATURE_AVX512BW, ZFS_X86_AVX512BW);
		FSET(X86_FEATURE_AVX512VL, ZFS_X86_AVX512VL);
		FSET(X86_FEATURE_AES, ZFS_X86_AES);
		FSET(X86_FEATURE_MOVBE, ZFS_X86_MOVBE);
		FSET(X86_FEATURE_PCLMULQDQ, ZFS_X86_PCLMULQDQ);
		FSET(X86_FEATURE_SHA_NI, ZFS_X86_SHA_NI);
	}
	return isset(cpu_features, feature_set);
}

int
spl_kfpu_allowed(void)
{
	return 1;
}

void
spl_kfpu_begin(void)
{
	kernel_fpu_begin();
}

void
spl_kfpu_end(void)
{
	kernel_fpu_end();
}

#elif defined(__arm) && defined(CONFIG_KERNEL_MODE_NEON)

int
spl_kfpu_cpu_has(int feature_set)
{
	if (unlikely(cpu_features == 0)) {
		cpu_features = ZFS_X86_INIT_DONE;
		FSET(X86_FEATURE_NEON, ZFS_X86_NEON);
		FSET(X86_FEATURE_SHA256 ZFS_X86_SHA256);
		FSET(X86_FEATURE_SHA512 ZFS_X86_SHA512);
	}
	return isset(cpu_features, feature_set);
}


int
spl_kfpu_allowed(void)
{
	return 1;
}

void
spl_kfpu_begin(void)
{
	kernel_neon_begin();
}

void
spl_kfpu_end(void)
{
	kernel_neon_end();
}

#elif defined(__aarch64__) && defined(CONFIG_KERNEL_MODE_NEON)

#include <asm/neon.h>
#include <asm/elf.h>
#include <asm/hwcap.h>

int
spl_kfpu_cpu_has(int feature_set)
{
	if (unlikely(cpu_features == 0)) {
		cpu_features = ZFS_X86_INIT_DONE;
		FSET(X86_FEATURE_NEON, ZFS_X86_NEON);
		FSET(X86_FEATURE_SHA256 ZFS_X86_SHA256);
		FSET(X86_FEATURE_SHA512 ZFS_X86_SHA512);
	}
	return isset(cpu_features, feature_set);
}

int
spl_kfpu_allowed(void)
{
	return 0;
}

void
spl_kfpu_begin(void)
{
	return kernel_neon_begin();
}

void
spl_kfpu_end(void)
{
	return kernel_neon_end();
}

#elif defined(__powerpc)

/* ppc */
extern boolean_t zfs_altivec_available(void);
extern boolean_t zfs_vsx_available(void);
extern boolean_t zfs_isa207_available(void);

static void spe_begin(void)
{
	/* disable preemption and save users SPE registers if required */
	preempt_disable();
}

static void spe_end(void)
{
	disable_kernel_spe();
	/* reenable preemption */
	preempt_enable();
}

int
spl_kfpu_allowed(void)
{
	return 0;
}

void
spl_kfpu_begin(void)
{
	preempt_disable();
	enable_kernel_vsx();
	enable_kernel_spe();
}

void
spl_kfpu_end(void)
{
	disable_kernel_spe();
	disable_kernel_vsx();
	preempt_enable();
}
#elif defined(__loongarch_lp64)
extern void kernel_fpu_begin(void);
extern void kernel_fpu_end(void);

int
spl_kfpu_allowed(void)
{
	return 0;
}

int
spl_kfpu_cpu_has(int feature_set)
{
}

void
spl_kfpu_begin(void)
{
	kernel_fpu_begin();
}

void
spl_kfpu_end(void)
{
	kernel_fpu_end();
}

#else

#define	kfpu_allowed()		0
#define	kfpu_cpu_has()		0
#define	kfpu_begin()		do {} while (0)
#define	kfpu_end()		do {} while (0)

#endif

EXPORT_SYMBOL(spl_kfpu_allowed);
EXPORT_SYMBOL(spl_kfpu_cpu_has);
EXPORT_SYMBOL(spl_kfpu_begin);
EXPORT_SYMBOL(spl_kfpu_end);
