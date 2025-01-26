	.section	__TEXT,__text,regular,pure_instructions
	; fn prologue
	.globl	_main
	.p2align	2
_main:
	.cfi_startproc
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	; begin block
	; begin loop
1:
	; begin block
	; begin let
	ldr	x11, =0x1
	str	x11, [sp, #-16]!	; push
	; end let
	; begin continue
	add	sp, sp, #16	; adjust stack pointer
	b	1b
	; end continue
	; end block
2:
	; end loop
	; end block
	; pop fn return
	mov	x0, #0	; clear
	; fn epilogue
	ldp	x29, x30, [sp], #16
	ret
	.cfi_endproc

.subsections_via_symbols
