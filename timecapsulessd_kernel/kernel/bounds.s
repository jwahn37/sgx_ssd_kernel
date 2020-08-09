	.file	"bounds.c"
# GNU C89 (Ubuntu 7.5.0-3ubuntu1~18.04) version 7.5.0 (x86_64-linux-gnu)
#	compiled by GNU C version 7.5.0, GMP version 6.1.2, MPFR version 4.0.1, MPC version 1.1.0, isl version isl-0.19-GMP

# GGC heuristics: --param ggc-min-expand=100 --param ggc-min-heapsize=131072
# options passed:  -nostdinc -I ./arch/x86/include
# -I ./arch/x86/include/generated -I ./include -I ./arch/x86/include/uapi
# -I ./arch/x86/include/generated/uapi -I ./include/uapi
# -I ./include/generated/uapi -imultiarch x86_64-linux-gnu -D __KERNEL__
# -D CONFIG_X86_X32_ABI -D CONFIG_AS_CFI=1 -D CONFIG_AS_CFI_SIGNAL_FRAME=1
# -D CONFIG_AS_CFI_SECTIONS=1 -D CONFIG_AS_FXSAVEQ=1 -D CONFIG_AS_SSSE3=1
# -D CONFIG_AS_CRC32=1 -D CONFIG_AS_AVX=1 -D CONFIG_AS_AVX2=1
# -D CONFIG_AS_AVX512=1 -D CONFIG_AS_SHA1_NI=1 -D CONFIG_AS_SHA256_NI=1
# -D RETPOLINE -D CC_HAVE_ASM_GOTO -D CC_USING_FENTRY
# -D KBUILD_BASENAME="bounds" -D KBUILD_MODNAME="bounds"
# -isystem /usr/lib/gcc/x86_64-linux-gnu/7/include
# -include ./include/linux/kconfig.h -MD kernel/.bounds.s.d kernel/bounds.c
# -mno-sse -mno-mmx -mno-sse2 -mno-3dnow -mno-avx -m64 -mno-80387
# -mno-fp-ret-in-387 -mpreferred-stack-boundary=3 -mskip-rax-setup
# -mtune=generic -mno-red-zone -mcmodel=kernel
# -mindirect-branch=thunk-extern -mindirect-branch-register -mfentry
# -march=x86-64 -auxbase-strip kernel/bounds.s -g -gdwarf-4 -O2 -Wall
# -Wundef -Wstrict-prototypes -Wno-trigraphs
# -Werror=implicit-function-declaration -Wno-format-security
# -Wno-sign-compare -Wno-frame-address -Wformat-truncation=0
# -Wformat-overflow=0 -Wno-int-in-bool-context -Wframe-larger-than=1024
# -Wno-unused-but-set-variable -Wunused-const-variable=0
# -Wdeclaration-after-statement -Wno-pointer-sign -Werror=implicit-int
# -Werror=strict-prototypes -Werror=date-time
# -Werror=incompatible-pointer-types -Werror=designated-init -std=gnu90 -p
# -fno-strict-aliasing -fno-common -fshort-wchar -fno-PIE -falign-jumps=1
# -falign-loops=1 -funit-at-a-time -fno-asynchronous-unwind-tables
# -fno-delete-null-pointer-checks -fstack-protector-strong
# -fno-omit-frame-pointer -fno-optimize-sibling-calls
# -fno-var-tracking-assignments -fno-strict-overflow -fstack-check=no
# -fconserve-stack -fverbose-asm --param allow-store-data-races=0
# -fstack-protector-strong
# options enabled:  -faggressive-loop-optimizations -falign-labels
# -fauto-inc-dec -fbranch-count-reg -fcaller-saves
# -fchkp-check-incomplete-type -fchkp-check-read -fchkp-check-write
# -fchkp-instrument-calls -fchkp-narrow-bounds -fchkp-optimize
# -fchkp-store-bounds -fchkp-use-static-bounds
# -fchkp-use-static-const-bounds -fchkp-use-wrappers -fcode-hoisting
# -fcombine-stack-adjustments -fcompare-elim -fcprop-registers
# -fcrossjumping -fcse-follow-jumps -fdefer-pop -fdevirtualize
# -fdevirtualize-speculatively -fdwarf2-cfi-asm -fearly-inlining
# -feliminate-unused-debug-types -fexpensive-optimizations
# -fforward-propagate -ffp-int-builtin-inexact -ffunction-cse -fgcse
# -fgcse-lm -fgnu-runtime -fgnu-unique -fguess-branch-probability
# -fhoist-adjacent-loads -fident -fif-conversion -fif-conversion2
# -findirect-inlining -finline -finline-atomics
# -finline-functions-called-once -finline-small-functions -fipa-bit-cp
# -fipa-cp -fipa-icf -fipa-icf-functions -fipa-icf-variables -fipa-profile
# -fipa-pure-const -fipa-reference -fipa-sra -fipa-vrp -fira-hoist-pressure
# -fira-share-save-slots -fira-share-spill-slots
# -fisolate-erroneous-paths-dereference -fivopts -fkeep-static-consts
# -fleading-underscore -flifetime-dse -flra-remat -flto-odr-type-merging
# -fmath-errno -fmerge-constants -fmerge-debug-strings
# -fmove-loop-invariants -foptimize-strlen -fpartial-inlining -fpeephole
# -fpeephole2 -fplt -fprefetch-loop-arrays -fprofile -free
# -freg-struct-return -freorder-blocks -freorder-functions
# -frerun-cse-after-loop -fsched-critical-path-heuristic
# -fsched-dep-count-heuristic -fsched-group-heuristic -fsched-interblock
# -fsched-last-insn-heuristic -fsched-rank-heuristic -fsched-spec
# -fsched-spec-insn-heuristic -fsched-stalled-insns-dep -fschedule-fusion
# -fschedule-insns2 -fsemantic-interposition -fshow-column -fshrink-wrap
# -fshrink-wrap-separate -fsigned-zeros -fsplit-ivs-in-unroller
# -fsplit-wide-types -fssa-backprop -fssa-phiopt -fstack-protector-strong
# -fstdarg-opt -fstore-merging -fstrict-volatile-bitfields -fsync-libcalls
# -fthread-jumps -ftoplevel-reorder -ftrapping-math -ftree-bit-ccp
# -ftree-builtin-call-dce -ftree-ccp -ftree-ch -ftree-coalesce-vars
# -ftree-copy-prop -ftree-cselim -ftree-dce -ftree-dominator-opts
# -ftree-dse -ftree-forwprop -ftree-fre -ftree-loop-if-convert
# -ftree-loop-im -ftree-loop-ivcanon -ftree-loop-optimize
# -ftree-parallelize-loops= -ftree-phiprop -ftree-pre -ftree-pta
# -ftree-reassoc -ftree-scev-cprop -ftree-sink -ftree-slsr -ftree-sra
# -ftree-switch-conversion -ftree-tail-merge -ftree-ter -ftree-vrp
# -funit-at-a-time -fvar-tracking -fverbose-asm -fzero-initialized-in-bss
# -m128bit-long-double -m64 -malign-stringops -mavx256-split-unaligned-load
# -mavx256-split-unaligned-store -mfentry -mfxsr -mglibc -mieee-fp
# -mindirect-branch-register -mlong-double-80 -mno-fancy-math-387
# -mno-red-zone -mno-sse4 -mpush-args -mskip-rax-setup
# -mtls-direct-seg-refs -mvzeroupper

	.text
.Ltext0:
	.cfi_sections	.debug_frame
	.p2align 4,,15
	.globl	foo
	.type	foo, @function
foo:
.LFB141:
	.file 1 "kernel/bounds.c"
	.loc 1 17 0
	.cfi_startproc
1:	call	__fentry__
	pushq	%rbp	#
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp	#,
	.cfi_def_cfa_register 6
# kernel/bounds.c:19: 	DEFINE(NR_PAGEFLAGS, __NR_PAGEFLAGS);
	.loc 1 19 0
#APP
# 19 "kernel/bounds.c" 1
	
.ascii "->NR_PAGEFLAGS $25 __NR_PAGEFLAGS"	#
# 0 "" 2
# kernel/bounds.c:20: 	DEFINE(MAX_NR_ZONES, __MAX_NR_ZONES);
	.loc 1 20 0
# 20 "kernel/bounds.c" 1
	
.ascii "->MAX_NR_ZONES $5 __MAX_NR_ZONES"	#
# 0 "" 2
# kernel/bounds.c:22: 	DEFINE(NR_CPUS_BITS, ilog2(CONFIG_NR_CPUS));
	.loc 1 22 0
# 22 "kernel/bounds.c" 1
	
.ascii "->NR_CPUS_BITS $13 ilog2(CONFIG_NR_CPUS)"	#
# 0 "" 2
# kernel/bounds.c:24: 	DEFINE(SPINLOCK_SIZE, sizeof(spinlock_t));
	.loc 1 24 0
# 24 "kernel/bounds.c" 1
	
.ascii "->SPINLOCK_SIZE $4 sizeof(spinlock_t)"	#
# 0 "" 2
# kernel/bounds.c:26: }
	.loc 1 26 0
#NO_APP
	popq	%rbp	#
	.cfi_restore 6
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE141:
	.size	foo, .-foo
.Letext0:
	.file 2 "./include/linux/types.h"
	.file 3 "./arch/x86/include/asm/alternative.h"
	.file 4 "./arch/x86/include/asm/nops.h"
	.file 5 "./include/linux/init.h"
	.file 6 "./include/linux/printk.h"
	.file 7 "./include/linux/jump_label.h"
	.file 8 "./arch/x86/include/asm/jump_label.h"
	.file 9 "./include/linux/kernel.h"
	.file 10 "./include/linux/page-flags.h"
	.file 11 "./include/linux/mmzone.h"
	.file 12 "./include/linux/lockdep.h"
	.file 13 "./include/asm-generic/int-ll64.h"
	.file 14 "./arch/x86/include/asm/asm.h"
	.section	.debug_info,"",@progbits
.Ldebug_info0:
	.long	0x54b
	.value	0x4
	.long	.Ldebug_abbrev0
	.byte	0x8
	.uleb128 0x1
	.long	.LASF119
	.byte	0x1
	.long	.LASF120
	.long	.LASF121
	.quad	.Ltext0
	.quad	.Letext0-.Ltext0
	.long	.Ldebug_line0
	.uleb128 0x2
	.byte	0x1
	.byte	0x6
	.long	.LASF0
	.uleb128 0x2
	.byte	0x1
	.byte	0x8
	.long	.LASF1
	.uleb128 0x3
	.long	0x34
	.uleb128 0x2
	.byte	0x2
	.byte	0x5
	.long	.LASF2
	.uleb128 0x2
	.byte	0x2
	.byte	0x7
	.long	.LASF3
	.uleb128 0x4
	.byte	0x4
	.byte	0x5
	.string	"int"
	.uleb128 0x2
	.byte	0x4
	.byte	0x7
	.long	.LASF4
	.uleb128 0x2
	.byte	0x8
	.byte	0x5
	.long	.LASF5
	.uleb128 0x2
	.byte	0x8
	.byte	0x7
	.long	.LASF6
	.uleb128 0x5
	.string	"u64"
	.byte	0xd
	.byte	0x1a
	.long	0x63
	.uleb128 0x2
	.byte	0x8
	.byte	0x7
	.long	.LASF7
	.uleb128 0x2
	.byte	0x1
	.byte	0x6
	.long	.LASF8
	.uleb128 0x3
	.long	0x7c
	.uleb128 0x2
	.byte	0x8
	.byte	0x5
	.long	.LASF9
	.uleb128 0x6
	.byte	0x8
	.long	0x7c
	.uleb128 0x7
	.long	.LASF10
	.byte	0x2
	.byte	0x1e
	.long	0xa0
	.uleb128 0x2
	.byte	0x1
	.byte	0x2
	.long	.LASF11
	.uleb128 0x8
	.byte	0x4
	.byte	0x2
	.byte	0xb0
	.long	0xbc
	.uleb128 0x9
	.long	.LASF35
	.byte	0x2
	.byte	0xb1
	.long	0x4e
	.byte	0
	.byte	0
	.uleb128 0x7
	.long	.LASF12
	.byte	0x2
	.byte	0xb2
	.long	0xa7
	.uleb128 0xa
	.long	.LASF122
	.byte	0xe
	.byte	0x92
	.long	0x75
	.uleb128 0xb
	.long	.LASF13
	.byte	0x3
	.byte	0x3d
	.long	0x4e
	.uleb128 0xb
	.long	.LASF14
	.byte	0x3
	.byte	0x4b
	.long	0x95
	.uleb128 0xb
	.long	.LASF15
	.byte	0x4
	.byte	0x8f
	.long	0xf3
	.uleb128 0x6
	.byte	0x8
	.long	0xff
	.uleb128 0x6
	.byte	0x8
	.long	0x3b
	.uleb128 0x3
	.long	0xf9
	.uleb128 0x7
	.long	.LASF16
	.byte	0x5
	.byte	0x6d
	.long	0x10f
	.uleb128 0x6
	.byte	0x8
	.long	0x115
	.uleb128 0xc
	.long	0x4e
	.uleb128 0x6
	.byte	0x8
	.long	0x120
	.uleb128 0xd
	.uleb128 0xe
	.long	0x104
	.long	0x12c
	.uleb128 0xf
	.byte	0
	.uleb128 0xb
	.long	.LASF17
	.byte	0x5
	.byte	0x70
	.long	0x121
	.uleb128 0xb
	.long	.LASF18
	.byte	0x5
	.byte	0x70
	.long	0x121
	.uleb128 0xb
	.long	.LASF19
	.byte	0x5
	.byte	0x71
	.long	0x121
	.uleb128 0xb
	.long	.LASF20
	.byte	0x5
	.byte	0x71
	.long	0x121
	.uleb128 0xe
	.long	0x7c
	.long	0x163
	.uleb128 0xf
	.byte	0
	.uleb128 0xb
	.long	.LASF21
	.byte	0x5
	.byte	0x78
	.long	0x158
	.uleb128 0xb
	.long	.LASF22
	.byte	0x5
	.byte	0x79
	.long	0x8f
	.uleb128 0xb
	.long	.LASF23
	.byte	0x5
	.byte	0x7a
	.long	0x55
	.uleb128 0xb
	.long	.LASF24
	.byte	0x5
	.byte	0x83
	.long	0x95
	.uleb128 0xb
	.long	.LASF25
	.byte	0x5
	.byte	0x89
	.long	0x11a
	.uleb128 0xb
	.long	.LASF26
	.byte	0x5
	.byte	0x8b
	.long	0x95
	.uleb128 0xe
	.long	0x83
	.long	0x1b0
	.uleb128 0xf
	.byte	0
	.uleb128 0x3
	.long	0x1a5
	.uleb128 0xb
	.long	.LASF27
	.byte	0x6
	.byte	0xb
	.long	0x1b0
	.uleb128 0xb
	.long	.LASF28
	.byte	0x6
	.byte	0xc
	.long	0x1b0
	.uleb128 0xe
	.long	0x4e
	.long	0x1d6
	.uleb128 0xf
	.byte	0
	.uleb128 0xb
	.long	.LASF29
	.byte	0x6
	.byte	0x3f
	.long	0x1cb
	.uleb128 0xb
	.long	.LASF30
	.byte	0x6
	.byte	0x53
	.long	0x158
	.uleb128 0xb
	.long	.LASF31
	.byte	0x6
	.byte	0xbc
	.long	0x4e
	.uleb128 0xb
	.long	.LASF32
	.byte	0x6
	.byte	0xbd
	.long	0x4e
	.uleb128 0x10
	.long	.LASF33
	.byte	0x6
	.value	0x118
	.long	0x4e
	.uleb128 0xb
	.long	.LASF34
	.byte	0x7
	.byte	0x53
	.long	0x95
	.uleb128 0x11
	.long	.LASF65
	.byte	0x18
	.byte	0x8
	.byte	0x47
	.long	0x24a
	.uleb128 0x9
	.long	.LASF36
	.byte	0x8
	.byte	0x48
	.long	0x24a
	.byte	0
	.uleb128 0x9
	.long	.LASF37
	.byte	0x8
	.byte	0x49
	.long	0x24a
	.byte	0x8
	.uleb128 0x12
	.string	"key"
	.byte	0x8
	.byte	0x4a
	.long	0x24a
	.byte	0x10
	.byte	0
	.uleb128 0x7
	.long	.LASF38
	.byte	0x8
	.byte	0x42
	.long	0x6a
	.uleb128 0xe
	.long	0x219
	.long	0x260
	.uleb128 0xf
	.byte	0
	.uleb128 0xb
	.long	.LASF39
	.byte	0x7
	.byte	0x96
	.long	0x255
	.uleb128 0xb
	.long	.LASF40
	.byte	0x7
	.byte	0x97
	.long	0x255
	.uleb128 0x13
	.long	.LASF42
	.uleb128 0x3
	.long	0x276
	.uleb128 0x10
	.long	.LASF41
	.byte	0x6
	.value	0x1da
	.long	0x27b
	.uleb128 0x13
	.long	.LASF43
	.uleb128 0x10
	.long	.LASF44
	.byte	0x9
	.value	0x122
	.long	0x28c
	.uleb128 0x14
	.long	0x88
	.long	0x2ac
	.uleb128 0x15
	.long	0x4e
	.byte	0
	.uleb128 0x10
	.long	.LASF45
	.byte	0x9
	.value	0x123
	.long	0x2b8
	.uleb128 0x6
	.byte	0x8
	.long	0x29d
	.uleb128 0x10
	.long	.LASF46
	.byte	0x9
	.value	0x1e3
	.long	0x4e
	.uleb128 0x10
	.long	.LASF47
	.byte	0x9
	.value	0x1e4
	.long	0x4e
	.uleb128 0x10
	.long	.LASF48
	.byte	0x9
	.value	0x1e5
	.long	0x4e
	.uleb128 0x10
	.long	.LASF49
	.byte	0x9
	.value	0x1e6
	.long	0x4e
	.uleb128 0x10
	.long	.LASF50
	.byte	0x9
	.value	0x1e7
	.long	0x4e
	.uleb128 0x10
	.long	.LASF51
	.byte	0x9
	.value	0x1e8
	.long	0x4e
	.uleb128 0x10
	.long	.LASF52
	.byte	0x9
	.value	0x1e9
	.long	0x4e
	.uleb128 0x10
	.long	.LASF53
	.byte	0x9
	.value	0x1ea
	.long	0x4e
	.uleb128 0x10
	.long	.LASF54
	.byte	0x9
	.value	0x1ec
	.long	0x95
	.uleb128 0x10
	.long	.LASF55
	.byte	0x9
	.value	0x1f3
	.long	0xbc
	.uleb128 0x10
	.long	.LASF56
	.byte	0x9
	.value	0x207
	.long	0x4e
	.uleb128 0x10
	.long	.LASF57
	.byte	0x9
	.value	0x209
	.long	0x95
	.uleb128 0x16
	.long	.LASF73
	.byte	0x7
	.byte	0x4
	.long	0x55
	.byte	0x9
	.value	0x20f
	.long	0x385
	.uleb128 0x17
	.long	.LASF58
	.byte	0
	.uleb128 0x17
	.long	.LASF59
	.byte	0x1
	.uleb128 0x17
	.long	.LASF60
	.byte	0x2
	.uleb128 0x17
	.long	.LASF61
	.byte	0x3
	.uleb128 0x17
	.long	.LASF62
	.byte	0x4
	.uleb128 0x17
	.long	.LASF63
	.byte	0x5
	.byte	0
	.uleb128 0x10
	.long	.LASF64
	.byte	0x9
	.value	0x216
	.long	0x34e
	.uleb128 0x18
	.long	.LASF66
	.byte	0x3
	.byte	0x9
	.value	0x22b
	.long	0x3c6
	.uleb128 0x19
	.long	.LASF67
	.byte	0x9
	.value	0x22c
	.long	0x7c
	.byte	0
	.uleb128 0x19
	.long	.LASF68
	.byte	0x9
	.value	0x22d
	.long	0x7c
	.byte	0x1
	.uleb128 0x19
	.long	.LASF69
	.byte	0x9
	.value	0x22e
	.long	0x95
	.byte	0x2
	.byte	0
	.uleb128 0x3
	.long	0x391
	.uleb128 0xe
	.long	0x3c6
	.long	0x3db
	.uleb128 0x1a
	.long	0x75
	.byte	0x10
	.byte	0
	.uleb128 0x3
	.long	0x3cb
	.uleb128 0x10
	.long	.LASF70
	.byte	0x9
	.value	0x231
	.long	0x3db
	.uleb128 0x10
	.long	.LASF71
	.byte	0x9
	.value	0x233
	.long	0x1b0
	.uleb128 0x10
	.long	.LASF72
	.byte	0x9
	.value	0x23e
	.long	0x1b0
	.uleb128 0x1b
	.long	.LASF74
	.byte	0x7
	.byte	0x4
	.long	0x55
	.byte	0xa
	.byte	0x4b
	.long	0x4e8
	.uleb128 0x17
	.long	.LASF75
	.byte	0
	.uleb128 0x17
	.long	.LASF76
	.byte	0x1
	.uleb128 0x17
	.long	.LASF77
	.byte	0x2
	.uleb128 0x17
	.long	.LASF78
	.byte	0x3
	.uleb128 0x17
	.long	.LASF79
	.byte	0x4
	.uleb128 0x17
	.long	.LASF80
	.byte	0x5
	.uleb128 0x17
	.long	.LASF81
	.byte	0x6
	.uleb128 0x17
	.long	.LASF82
	.byte	0x7
	.uleb128 0x17
	.long	.LASF83
	.byte	0x8
	.uleb128 0x17
	.long	.LASF84
	.byte	0x9
	.uleb128 0x17
	.long	.LASF85
	.byte	0xa
	.uleb128 0x17
	.long	.LASF86
	.byte	0xb
	.uleb128 0x17
	.long	.LASF87
	.byte	0xc
	.uleb128 0x17
	.long	.LASF88
	.byte	0xd
	.uleb128 0x17
	.long	.LASF89
	.byte	0xe
	.uleb128 0x17
	.long	.LASF90
	.byte	0xf
	.uleb128 0x17
	.long	.LASF91
	.byte	0x10
	.uleb128 0x17
	.long	.LASF92
	.byte	0x11
	.uleb128 0x17
	.long	.LASF93
	.byte	0x12
	.uleb128 0x17
	.long	.LASF94
	.byte	0x13
	.uleb128 0x17
	.long	.LASF95
	.byte	0x14
	.uleb128 0x17
	.long	.LASF96
	.byte	0x15
	.uleb128 0x17
	.long	.LASF97
	.byte	0x16
	.uleb128 0x17
	.long	.LASF98
	.byte	0x17
	.uleb128 0x17
	.long	.LASF99
	.byte	0x18
	.uleb128 0x17
	.long	.LASF100
	.byte	0x19
	.uleb128 0x17
	.long	.LASF101
	.byte	0x9
	.uleb128 0x17
	.long	.LASF102
	.byte	0x9
	.uleb128 0x17
	.long	.LASF103
	.byte	0xd
	.uleb128 0x17
	.long	.LASF104
	.byte	0x9
	.uleb128 0x17
	.long	.LASF105
	.byte	0x4
	.uleb128 0x17
	.long	.LASF106
	.byte	0x9
	.uleb128 0x17
	.long	.LASF107
	.byte	0xc
	.uleb128 0x17
	.long	.LASF108
	.byte	0xd
	.uleb128 0x17
	.long	.LASF109
	.byte	0x11
	.byte	0
	.uleb128 0x16
	.long	.LASF110
	.byte	0x7
	.byte	0x4
	.long	0x55
	.byte	0xb
	.value	0x12d
	.long	0x51f
	.uleb128 0x17
	.long	.LASF111
	.byte	0
	.uleb128 0x17
	.long	.LASF112
	.byte	0x1
	.uleb128 0x17
	.long	.LASF113
	.byte	0x2
	.uleb128 0x17
	.long	.LASF114
	.byte	0x3
	.uleb128 0x17
	.long	.LASF115
	.byte	0x4
	.uleb128 0x17
	.long	.LASF116
	.byte	0x5
	.byte	0
	.uleb128 0xb
	.long	.LASF117
	.byte	0xc
	.byte	0x11
	.long	0x4e
	.uleb128 0xb
	.long	.LASF118
	.byte	0xc
	.byte	0x12
	.long	0x4e
	.uleb128 0x1c
	.string	"foo"
	.byte	0x1
	.byte	0x10
	.quad	.LFB141
	.quad	.LFE141-.LFB141
	.uleb128 0x1
	.byte	0x9c
	.byte	0
	.section	.debug_abbrev,"",@progbits
.Ldebug_abbrev0:
	.uleb128 0x1
	.uleb128 0x11
	.byte	0x1
	.uleb128 0x25
	.uleb128 0xe
	.uleb128 0x13
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x1b
	.uleb128 0xe
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x7
	.uleb128 0x10
	.uleb128 0x17
	.byte	0
	.byte	0
	.uleb128 0x2
	.uleb128 0x24
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3e
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0xe
	.byte	0
	.byte	0
	.uleb128 0x3
	.uleb128 0x26
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x4
	.uleb128 0x24
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3e
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0x8
	.byte	0
	.byte	0
	.uleb128 0x5
	.uleb128 0x16
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x6
	.uleb128 0xf
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x7
	.uleb128 0x16
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x8
	.uleb128 0x13
	.byte	0x1
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x9
	.uleb128 0xd
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x38
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0xa
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x3f
	.uleb128 0x19
	.byte	0
	.byte	0
	.uleb128 0xb
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x3f
	.uleb128 0x19
	.uleb128 0x3c
	.uleb128 0x19
	.byte	0
	.byte	0
	.uleb128 0xc
	.uleb128 0x15
	.byte	0
	.uleb128 0x27
	.uleb128 0x19
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xd
	.uleb128 0x15
	.byte	0
	.uleb128 0x27
	.uleb128 0x19
	.byte	0
	.byte	0
	.uleb128 0xe
	.uleb128 0x1
	.byte	0x1
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xf
	.uleb128 0x21
	.byte	0
	.byte	0
	.byte	0
	.uleb128 0x10
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0x5
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x3f
	.uleb128 0x19
	.uleb128 0x3c
	.uleb128 0x19
	.byte	0
	.byte	0
	.uleb128 0x11
	.uleb128 0x13
	.byte	0x1
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x12
	.uleb128 0xd
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x38
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0x13
	.uleb128 0x13
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3c
	.uleb128 0x19
	.byte	0
	.byte	0
	.uleb128 0x14
	.uleb128 0x15
	.byte	0x1
	.uleb128 0x27
	.uleb128 0x19
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x15
	.uleb128 0x5
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x16
	.uleb128 0x4
	.byte	0x1
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3e
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0x5
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x17
	.uleb128 0x28
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x1c
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0x18
	.uleb128 0x13
	.byte	0x1
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0x5
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x19
	.uleb128 0xd
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0x5
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x38
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0x1a
	.uleb128 0x21
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2f
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0x1b
	.uleb128 0x4
	.byte	0x1
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3e
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x1c
	.uleb128 0x2e
	.byte	0
	.uleb128 0x3f
	.uleb128 0x19
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0x19
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x7
	.uleb128 0x40
	.uleb128 0x18
	.uleb128 0x2117
	.uleb128 0x19
	.byte	0
	.byte	0
	.byte	0
	.section	.debug_aranges,"",@progbits
	.long	0x2c
	.value	0x2
	.long	.Ldebug_info0
	.byte	0x8
	.byte	0
	.value	0
	.value	0
	.quad	.Ltext0
	.quad	.Letext0-.Ltext0
	.quad	0
	.quad	0
	.section	.debug_line,"",@progbits
.Ldebug_line0:
	.section	.debug_str,"MS",@progbits,1
.LASF101:
	.string	"PG_checked"
.LASF26:
	.string	"initcall_debug"
.LASF110:
	.string	"zone_type"
.LASF41:
	.string	"kmsg_fops"
.LASF72:
	.string	"hex_asc_upper"
.LASF96:
	.string	"PG_uncached"
.LASF27:
	.string	"linux_banner"
.LASF49:
	.string	"panic_on_unrecovered_nmi"
.LASF6:
	.string	"long long unsigned int"
.LASF63:
	.string	"SYSTEM_RESTART"
.LASF45:
	.string	"panic_blink"
.LASF61:
	.string	"SYSTEM_HALT"
.LASF99:
	.string	"PG_idle"
.LASF40:
	.string	"__stop___jump_table"
.LASF100:
	.string	"__NR_PAGEFLAGS"
.LASF5:
	.string	"long long int"
.LASF0:
	.string	"signed char"
.LASF20:
	.string	"__security_initcall_end"
.LASF22:
	.string	"saved_command_line"
.LASF52:
	.string	"sysctl_panic_on_rcu_stall"
.LASF121:
	.string	"/home/jinu/sgx_ssd_kernel/timecapsulessd_kernel"
.LASF88:
	.string	"PG_private_2"
.LASF58:
	.string	"SYSTEM_BOOTING"
.LASF74:
	.string	"pageflags"
.LASF120:
	.string	"kernel/bounds.c"
.LASF59:
	.string	"SYSTEM_SCHEDULING"
.LASF13:
	.string	"alternatives_patched"
.LASF9:
	.string	"long int"
.LASF102:
	.string	"PG_swapcache"
.LASF51:
	.string	"panic_on_warn"
.LASF66:
	.string	"taint_flag"
.LASF85:
	.string	"PG_arch_1"
.LASF55:
	.string	"panic_cpu"
.LASF28:
	.string	"linux_proc_banner"
.LASF54:
	.string	"crash_kexec_post_notifiers"
.LASF75:
	.string	"PG_locked"
.LASF80:
	.string	"PG_lru"
.LASF77:
	.string	"PG_referenced"
.LASF16:
	.string	"initcall_t"
.LASF42:
	.string	"file_operations"
.LASF112:
	.string	"ZONE_DMA32"
.LASF109:
	.string	"PG_isolated"
.LASF4:
	.string	"unsigned int"
.LASF87:
	.string	"PG_private"
.LASF56:
	.string	"root_mountflags"
.LASF70:
	.string	"taint_flags"
.LASF7:
	.string	"long unsigned int"
.LASF33:
	.string	"kptr_restrict"
.LASF43:
	.string	"atomic_notifier_head"
.LASF29:
	.string	"console_printk"
.LASF60:
	.string	"SYSTEM_RUNNING"
.LASF3:
	.string	"short unsigned int"
.LASF119:
	.ascii	"GNU C89 7.5.0 -mno-sse -mno-mmx -mno-sse2 -mno-3dnow -mno-av"
	.ascii	"x -m64 -mno-80387 -mno-fp-ret-in-387 -mpreferred-stack-bound"
	.ascii	"ary=3 -mskip-rax-setup -mtune=generic -mno-red-zone -mcmodel"
	.ascii	"=kernel -mindirect-branch=thunk-extern -mindirect-branch-reg"
	.ascii	"ister -mfentry -march=x86-64 -g -gdwarf-4 -O2 -std=gnu90 -p "
	.ascii	"-fno-strict-aliasing -fno-common -fshort-wchar -fno-PIE -fal"
	.ascii	"ign-jumps=1 -falign-loops=1 -funit-at-a-time -fno-asynchrono"
	.ascii	"us-unwind-ta"
	.string	"bles -fno-delete-null-pointer-checks -fstack-protector-strong -fno-omit-frame-pointer -fno-optimize-sibling-calls -fno-var-tracking-assignments -fno-strict-overflow -fstack-check=no -fconserve-stack --param allow-store-data-races=0 -fstack-protector-strong"
.LASF24:
	.string	"rodata_enabled"
.LASF10:
	.string	"bool"
.LASF105:
	.string	"PG_savepinned"
.LASF32:
	.string	"dmesg_restrict"
.LASF18:
	.string	"__con_initcall_end"
.LASF117:
	.string	"prove_locking"
.LASF67:
	.string	"c_true"
.LASF92:
	.string	"PG_reclaim"
.LASF68:
	.string	"c_false"
.LASF76:
	.string	"PG_error"
.LASF104:
	.string	"PG_pinned"
.LASF53:
	.string	"sysctl_panic_on_stackoverflow"
.LASF23:
	.string	"reset_devices"
.LASF64:
	.string	"system_state"
.LASF89:
	.string	"PG_writeback"
.LASF118:
	.string	"lock_stat"
.LASF95:
	.string	"PG_mlocked"
.LASF47:
	.string	"panic_timeout"
.LASF108:
	.string	"PG_double_map"
.LASF114:
	.string	"ZONE_MOVABLE"
.LASF19:
	.string	"__security_initcall_start"
.LASF44:
	.string	"panic_notifier_list"
.LASF91:
	.string	"PG_mappedtodisk"
.LASF113:
	.string	"ZONE_NORMAL"
.LASF34:
	.string	"static_key_initialized"
.LASF78:
	.string	"PG_uptodate"
.LASF11:
	.string	"_Bool"
.LASF1:
	.string	"unsigned char"
.LASF73:
	.string	"system_states"
.LASF65:
	.string	"jump_entry"
.LASF122:
	.string	"current_stack_pointer"
.LASF2:
	.string	"short int"
.LASF35:
	.string	"counter"
.LASF69:
	.string	"module"
.LASF37:
	.string	"target"
.LASF83:
	.string	"PG_slab"
.LASF116:
	.string	"__MAX_NR_ZONES"
.LASF115:
	.string	"ZONE_DEVICE"
.LASF79:
	.string	"PG_dirty"
.LASF62:
	.string	"SYSTEM_POWER_OFF"
.LASF46:
	.string	"oops_in_progress"
.LASF8:
	.string	"char"
.LASF12:
	.string	"atomic_t"
.LASF90:
	.string	"PG_head"
.LASF98:
	.string	"PG_young"
.LASF107:
	.string	"PG_slob_free"
.LASF94:
	.string	"PG_unevictable"
.LASF48:
	.string	"panic_on_oops"
.LASF21:
	.string	"boot_command_line"
.LASF84:
	.string	"PG_owner_priv_1"
.LASF31:
	.string	"printk_delay_msec"
.LASF57:
	.string	"early_boot_irqs_disabled"
.LASF50:
	.string	"panic_on_io_nmi"
.LASF86:
	.string	"PG_reserved"
.LASF15:
	.string	"ideal_nops"
.LASF36:
	.string	"code"
.LASF39:
	.string	"__start___jump_table"
.LASF71:
	.string	"hex_asc"
.LASF103:
	.string	"PG_fscache"
.LASF38:
	.string	"jump_label_t"
.LASF17:
	.string	"__con_initcall_start"
.LASF82:
	.string	"PG_waiters"
.LASF25:
	.string	"late_time_init"
.LASF97:
	.string	"PG_hwpoison"
.LASF14:
	.string	"skip_smp_alternatives"
.LASF93:
	.string	"PG_swapbacked"
.LASF81:
	.string	"PG_active"
.LASF111:
	.string	"ZONE_DMA"
.LASF30:
	.string	"devkmsg_log_str"
.LASF106:
	.string	"PG_foreign"
	.ident	"GCC: (Ubuntu 7.5.0-3ubuntu1~18.04) 7.5.0"
	.section	.note.GNU-stack,"",@progbits
