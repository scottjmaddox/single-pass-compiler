	.section	__TEXT,__text,regular,pure_instructions
	; fn prologue
	.globl	_main
	.p2align	2
_main:
	.cfi_startproc
	stp	fp, lr, [sp, #-16]!
	mov	fp, sp
	; begin block
	; end block
	ldr	x11, =0x1
	str	x11, [sp, #-16]!	; push
	; pop fn return
	ldr	x0, [sp], #16	; pop
	; fn epilogue
	ldp	fp, lr, [sp], #16
	ret
	.cfi_endproc

.subsections_via_symbols
