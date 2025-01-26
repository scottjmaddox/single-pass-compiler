	.section	__TEXT,__text,regular,pure_instructions
	; begin fn
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
	; begin loop
3:
	; begin block
	; begin let
	ldr	x11, =0x1
	str	x11, [sp, #-16]!	; push
	; end let
	; begin continue
	add	sp, sp, #32	; adjust stack pointer
	b	1b
	; end continue
	; end block
4:
	; end loop
	; end block
2:
	; end loop
	; end block
	.cfi_endproc
	; end fn

.subsections_via_symbols
