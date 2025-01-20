	.section	__TEXT,__text,regular,pure_instructions
	; fn prologue
	.globl	_v
	.p2align	2
_v:
	.cfi_startproc
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	ldr	x11, =0x0
	str	x11, [sp, #-16]!	; push
	; pop return value
	ldr	x0, [sp], #16	; pop
	; fn epilogue
	ldp	x29, x30, [sp], #16
	ret
	.cfi_endproc
	; fn prologue
	.globl	_main
	.p2align	2
_main:
	.cfi_startproc
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	; fn call
	bl	_v
	str	x0, [sp, #-16]!	; push
	ldr	x11, [sp], #16	; pop
	cbz	x11, 0f
	ldr	x11, =0x1
	str	x11, [sp, #-16]!	; push
	b	1f
0:
	ldr	x11, =0x0
	str	x11, [sp, #-16]!	; push
1:
	; pop return value
	ldr	x0, [sp], #16	; pop
	; fn epilogue
	ldp	x29, x30, [sp], #16
	ret
	.cfi_endproc

.subsections_via_symbols
