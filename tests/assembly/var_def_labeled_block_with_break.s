	.section	__TEXT,__text,regular,pure_instructions
	; begin fn
	.globl	_main
	.p2align	2
_main:
	.cfi_startproc
	stp	fp, lr, [sp, #-16]!
	mov	fp, sp
	; begin block
	; begin let
	; begin labeled block
	; begin block
	; begin break
	ldr	x11, =0x0
	str	x11, [sp, #-16]!	; push
	b	1f
	; end break
	; end block
1:
	; end labeled block
	; end let
	ldr	x11, [fp, #-16]	; load
	str	x11, [sp, #-16]!	; push
	; end block
	; pop fn return
	ldr	x0, [sp], #16	; pop
	add	sp, sp, #16	; adjust stack pointer
	ldp	fp, lr, [sp], #16
	ret
	.cfi_endproc
	; end fn

.subsections_via_symbols
