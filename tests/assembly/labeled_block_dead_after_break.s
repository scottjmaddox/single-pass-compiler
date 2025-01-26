	.section	__TEXT,__text,regular,pure_instructions
	; fn prologue
	.globl	_main
	.p2align	2
_main:
	.cfi_startproc
	stp	fp, lr, [sp, #-16]!
	mov	fp, sp
	; begin block
	; begin labeled block
	; begin block
	; begin let
	ldr	x11, =0x1
	str	x11, [sp, #-16]!	; push
	; end let
	; begin break
	; pop block result
	add	sp, sp, #16	; adjust stack pointer
	; push block result
	b	1f
	; end break
	; end block
1:
	; end labeled block
	; end block
	; pop fn return
	mov	x0, #0	; clear
	; fn epilogue
	ldp	fp, lr, [sp], #16
	ret
	.cfi_endproc

.subsections_via_symbols
