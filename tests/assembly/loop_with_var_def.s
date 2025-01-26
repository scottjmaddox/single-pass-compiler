	.section	__TEXT,__text,regular,pure_instructions
	; fn prologue
	.globl	_main
	.p2align	2
_main:
	.cfi_startproc
	stp	fp, lr, [sp, #-16]!
	mov	fp, sp
	; begin block
	; begin loop
1:
	; begin block
	; begin let
	ldr	x11, =0x1
	str	x11, [sp, #-16]!	; push
	; end let
	; end block
	add	sp, sp, #16	; adjust stack pointer
	b	1b
2:
	; end loop
	; end block
	; pop fn return
	mov	x0, #0	; clear
	; fn epilogue
	ldp	fp, lr, [sp], #16
	ret
	.cfi_endproc

.subsections_via_symbols
