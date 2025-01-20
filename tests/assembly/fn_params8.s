	.section	__TEXT,__text,regular,pure_instructions
	; fn prologue
	.globl	_sum
	.p2align	2
_sum:
	.cfi_startproc
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp
	str	x0, [sp, #-16]!	; push
	str	x1, [sp, #-16]!	; push
	str	x2, [sp, #-16]!	; push
	str	x3, [sp, #-16]!	; push
	str	x4, [sp, #-16]!	; push
	str	x5, [sp, #-16]!	; push
	str	x6, [sp, #-16]!	; push
	str	x7, [sp, #-16]!	; push
	; load var
	ldr	x8, [x29, #-16]
	str	x8, [sp, #-16]!	; push
	; load var
	ldr	x8, [x29, #-32]
	str	x8, [sp, #-16]!	; push
	; binary op
	ldr	x9, [sp], #16	; pop
	ldr	x8, [sp], #16	; pop
	add	x8, x8, x9
	str	x8, [sp, #-16]!	; push
	; load var
	ldr	x8, [x29, #-48]
	str	x8, [sp, #-16]!	; push
	; binary op
	ldr	x9, [sp], #16	; pop
	ldr	x8, [sp], #16	; pop
	add	x8, x8, x9
	str	x8, [sp, #-16]!	; push
	; load var
	ldr	x8, [x29, #-64]
	str	x8, [sp, #-16]!	; push
	; binary op
	ldr	x9, [sp], #16	; pop
	ldr	x8, [sp], #16	; pop
	add	x8, x8, x9
	str	x8, [sp, #-16]!	; push
	; load var
	ldr	x8, [x29, #-80]
	str	x8, [sp, #-16]!	; push
	; binary op
	ldr	x9, [sp], #16	; pop
	ldr	x8, [sp], #16	; pop
	add	x8, x8, x9
	str	x8, [sp, #-16]!	; push
	; load var
	ldr	x8, [x29, #-96]
	str	x8, [sp, #-16]!	; push
	; binary op
	ldr	x9, [sp], #16	; pop
	ldr	x8, [sp], #16	; pop
	add	x8, x8, x9
	str	x8, [sp, #-16]!	; push
	; load var
	ldr	x8, [x29, #-112]
	str	x8, [sp, #-16]!	; push
	; binary op
	ldr	x9, [sp], #16	; pop
	ldr	x8, [sp], #16	; pop
	add	x8, x8, x9
	str	x8, [sp, #-16]!	; push
	; load var
	ldr	x8, [x29, #-128]
	str	x8, [sp, #-16]!	; push
	; binary op
	ldr	x9, [sp], #16	; pop
	ldr	x8, [sp], #16	; pop
	add	x8, x8, x9
	str	x8, [sp, #-16]!	; push
	; pop return value
	ldr	x0, [sp], #16	; pop
	; fn epilogue
	sub	sp, sp, #-128
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
	ldr	x8, =0x1
	str	x8, [sp, #-16]!	; push
	ldr	x0, [sp], #16	; pop
	ldr	x8, =0xffffffffffffffff
	str	x8, [sp, #-16]!	; push
	ldr	x1, [sp], #16	; pop
	ldr	x8, =0x2
	str	x8, [sp, #-16]!	; push
	ldr	x2, [sp], #16	; pop
	ldr	x8, =0xfffffffffffffffe
	str	x8, [sp, #-16]!	; push
	ldr	x3, [sp], #16	; pop
	ldr	x8, =0x3
	str	x8, [sp, #-16]!	; push
	ldr	x4, [sp], #16	; pop
	ldr	x8, =0xfffffffffffffffd
	str	x8, [sp, #-16]!	; push
	ldr	x5, [sp], #16	; pop
	ldr	x8, =0x4
	str	x8, [sp, #-16]!	; push
	ldr	x6, [sp], #16	; pop
	ldr	x8, =0xfffffffffffffffc
	str	x8, [sp, #-16]!	; push
	ldr	x7, [sp], #16	; pop
	; fn call
	bl	_sum
	str	x0, [sp, #-16]!	; push
	; pop return value
	ldr	x0, [sp], #16	; pop
	; fn epilogue
	ldp	x29, x30, [sp], #16
	ret
	.cfi_endproc

.subsections_via_symbols
