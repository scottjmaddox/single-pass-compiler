	.section	__TEXT,__text,regular,pure_instructions
	; fn prologue
	.globl	_sum
	.p2align	2
_sum:
	.cfi_startproc
	stp	fp, lr, [sp, #-16]!
	mov	fp, sp
	str	x0, [sp, #-16]!	; push
	str	x1, [sp, #-16]!	; push
	str	x2, [sp, #-16]!	; push
	str	x3, [sp, #-16]!	; push
	str	x4, [sp, #-16]!	; push
	str	x5, [sp, #-16]!	; push
	str	x6, [sp, #-16]!	; push
	str	x7, [sp, #-16]!	; push
	; begin block
	ldr	x11, [fp, #-16]	; load
	str	x11, [sp, #-16]!	; push
	ldr	x11, [fp, #-32]	; load
	str	x11, [sp, #-16]!	; push
	; binary op
	ldr	x12, [sp], #16	; pop
	ldr	x11, [sp], #16	; pop
	add	x11, x11, x12
	str	x11, [sp, #-16]!	; push
	ldr	x11, [fp, #-48]	; load
	str	x11, [sp, #-16]!	; push
	; binary op
	ldr	x12, [sp], #16	; pop
	ldr	x11, [sp], #16	; pop
	add	x11, x11, x12
	str	x11, [sp, #-16]!	; push
	ldr	x11, [fp, #-64]	; load
	str	x11, [sp, #-16]!	; push
	; binary op
	ldr	x12, [sp], #16	; pop
	ldr	x11, [sp], #16	; pop
	add	x11, x11, x12
	str	x11, [sp, #-16]!	; push
	ldr	x11, [fp, #-80]	; load
	str	x11, [sp, #-16]!	; push
	; binary op
	ldr	x12, [sp], #16	; pop
	ldr	x11, [sp], #16	; pop
	add	x11, x11, x12
	str	x11, [sp, #-16]!	; push
	ldr	x11, [fp, #-96]	; load
	str	x11, [sp, #-16]!	; push
	; binary op
	ldr	x12, [sp], #16	; pop
	ldr	x11, [sp], #16	; pop
	add	x11, x11, x12
	str	x11, [sp, #-16]!	; push
	ldr	x11, [fp, #-112]	; load
	str	x11, [sp, #-16]!	; push
	; binary op
	ldr	x12, [sp], #16	; pop
	ldr	x11, [sp], #16	; pop
	add	x11, x11, x12
	str	x11, [sp, #-16]!	; push
	ldr	x11, [fp, #-128]	; load
	str	x11, [sp, #-16]!	; push
	; binary op
	ldr	x12, [sp], #16	; pop
	ldr	x11, [sp], #16	; pop
	add	x11, x11, x12
	str	x11, [sp, #-16]!	; push
	ldr	x11, [fp, #16]	; load
	str	x11, [sp, #-16]!	; push
	; binary op
	ldr	x12, [sp], #16	; pop
	ldr	x11, [sp], #16	; pop
	add	x11, x11, x12
	str	x11, [sp, #-16]!	; push
	; end block
	; pop fn return
	ldr	x0, [sp], #16	; pop
	add	sp, sp, #128	; adjust stack pointer
	; fn epilogue
	ldp	fp, lr, [sp], #16
	ret
	.cfi_endproc
	; fn prologue
	.globl	_main
	.p2align	2
_main:
	.cfi_startproc
	stp	fp, lr, [sp, #-16]!
	mov	fp, sp
	; begin block
	; begin fn call
	add	sp, sp, #-16	; adjust stack pointer
	ldr	x11, =0x0
	str	x11, [sp, #-16]!	; push
	ldr	x0, [sp], #16	; pop
	ldr	x11, =0x1
	str	x11, [sp, #-16]!	; push
	ldr	x1, [sp], #16	; pop
	ldr	x11, =0xffffffffffffffff
	str	x11, [sp, #-16]!	; push
	ldr	x2, [sp], #16	; pop
	ldr	x11, =0x2
	str	x11, [sp, #-16]!	; push
	ldr	x3, [sp], #16	; pop
	ldr	x11, =0xfffffffffffffffe
	str	x11, [sp, #-16]!	; push
	ldr	x4, [sp], #16	; pop
	ldr	x11, =0x3
	str	x11, [sp, #-16]!	; push
	ldr	x5, [sp], #16	; pop
	ldr	x11, =0xfffffffffffffffd
	str	x11, [sp, #-16]!	; push
	ldr	x6, [sp], #16	; pop
	ldr	x11, =0x4
	str	x11, [sp, #-16]!	; push
	ldr	x7, [sp], #16	; pop
	ldr	x11, =0xfffffffffffffffc
	str	x11, [sp, #-16]!	; push
	ldr	x9, [sp], #16	; pop
	str	x9, [sp, #0]	; store
	bl	_sum	; fn call
	add	sp, sp, #16	; adjust stack pointer
	; push fn return
	str	x0, [sp, #-16]!	; push
	; end fn call
	; end block
	; pop fn return
	ldr	x0, [sp], #16	; pop
	; fn epilogue
	ldp	fp, lr, [sp], #16
	ret
	.cfi_endproc

.subsections_via_symbols
