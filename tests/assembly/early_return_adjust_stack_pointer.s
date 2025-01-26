	.section	__TEXT,__text,regular,pure_instructions
	; fn prologue
	.globl	_main
	.p2align	2
_main:
	.cfi_startproc
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
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
	ldp	x29, x30, [sp], #16
	ret
	; end return
	; end block
	; end block
	; pop fn return
	mov	x0, #0	; clear
	; fn epilogue
	ldp	x29, x30, [sp], #16
	ret
	.cfi_endproc

.subsections_via_symbols
