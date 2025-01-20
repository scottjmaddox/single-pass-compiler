	.section	__TEXT,__text,regular,pure_instructions
	; fn prologue
	.globl	_main
	.p2align	2
_main:
	.cfi_startproc
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	ldr	x8, =0x1
	str	x8, [sp, #-16]!	; push
	add	sp, sp, #16	; drop
	brk	#0
	; pop return value
	mov	x0, #0	; clear
	; fn epilogue
	ldp	x29, x30, [sp], #16
	ret
	.cfi_endproc

.subsections_via_symbols
