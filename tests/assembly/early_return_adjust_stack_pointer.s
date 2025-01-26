	.section	__TEXT,__text,regular,pure_instructions
	; begin fn
	.globl	_main
	.p2align	2
_main:
	.cfi_startproc
	stp	fp, lr, [sp, #-16]!
	mov	fp, sp
	; begin block
	; begin block
	; begin let
	ldr	x11, =0x0
	str	x11, [sp, #-16]!	; push
	; end let
	; begin return
	; begin block
	; end block
	; pop fn return
	mov	x0, #0	; clear
	add	sp, sp, #16	; adjust stack pointer
	ldp	fp, lr, [sp], #16
	ret
	; end return
	; end block
	; end block
	.cfi_endproc
	; end fn

.subsections_via_symbols
