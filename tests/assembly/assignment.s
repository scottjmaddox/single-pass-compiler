	.section	__TEXT,__text,regular,pure_instructions
	; fn prologue
	.globl	_main
	.p2align	2
_main:
	.cfi_startproc
	stp	fp, lr, [sp, #-16]!
	mov	fp, sp
	; begin block
	; begin let
	ldr	x11, =0x1
	str	x11, [sp, #-16]!	; push
	; end let
	; begin assignment
	ldr	x11, =0x0
	str	x11, [sp, #-16]!	; push
	ldr	x11, [sp], #16	; pop
	str	x11, [fp, #-16]	; store
	; end assignment
	ldr	x11, [fp, #-16]	; load
	str	x11, [sp, #-16]!	; push
	; end block
	; pop fn return
	ldr	x0, [sp], #16	; pop
	add	sp, sp, #16	; adjust stack pointer
	; fn epilogue
	ldp	fp, lr, [sp], #16
	ret
	.cfi_endproc

.subsections_via_symbols
