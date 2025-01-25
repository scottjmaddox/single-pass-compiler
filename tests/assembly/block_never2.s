	.section	__TEXT,__text,regular,pure_instructions
	; fn prologue
	.globl	_main
	.p2align	2
_main:
	.cfi_startproc
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	; begin block
	ldr	x11, =0x1
	str	x11, [sp, #-16]!	; push
	add	sp, sp, #16	; drop
	brk	#0	; __builtin_trap()
	; end block
	; pop fn return
	ldr	x0, [sp], #16	; pop
	; fn epilogue
	ldp	x29, x30, [sp], #16
	ret
	.cfi_endproc

.subsections_via_symbols
