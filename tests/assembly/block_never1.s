	.section	__TEXT,__text,regular,pure_instructions
	; begin fn
	.globl	_main
	.p2align	2
_main:
	.cfi_startproc
	stp	fp, lr, [sp, #-16]!
	mov	fp, sp
	; begin block
	brk	#0	; __builtin_trap()
	; end block
	.cfi_endproc
	; end fn

.subsections_via_symbols
