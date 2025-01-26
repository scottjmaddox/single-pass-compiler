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
	; begin if
	ldr	x11, [fp, #-16]	; load
	str	x11, [sp, #-16]!	; push
	ldr	x11, [sp], #16	; pop
	cbz	x11, 2f
	; then
	; begin block
	; begin break
	; pop block result
	add	sp, sp, #16	; adjust stack pointer
	; push block result
	b	1f
	; end break
	; end block
2:
3:
	; end if
	; begin let
	ldr	x11, =0x1
	str	x11, [sp, #-16]!	; push
	; end let
	; begin if
	ldr	x11, [fp, #-32]	; load
	str	x11, [sp, #-16]!	; push
	ldr	x11, [sp], #16	; pop
	cbz	x11, 4f
	; then
	; begin block
	; begin break
	; pop block result
	add	sp, sp, #32	; adjust stack pointer
	; push block result
	b	1f
	; end break
	; end block
4:
5:
	; end if
	; begin let
	ldr	x11, =0x1
	str	x11, [sp, #-16]!	; push
	; end let
	; end block
	; pop block result
	add	sp, sp, #48	; adjust stack pointer
	; push block result
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
