	.section	__TEXT,__text,regular,pure_instructions
	; begin fn
	.globl	_sum
	.p2align	2
_sum:
	.cfi_startproc
	stp	fp, lr, [sp, #-16]!
	mov	fp, sp
	str	x0, [sp, #-16]!	; push
	str	x1, [sp, #-16]!	; push
	str	x2, [sp, #-16]!	; push
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
	; end block
	; pop fn return
	ldr	x0, [sp], #16	; pop
	add	sp, sp, #48	; adjust stack pointer
	ldp	fp, lr, [sp], #16
	ret
	.cfi_endproc
	; end fn
	; begin fn
	.globl	_main
	.p2align	2
_main:
	.cfi_startproc
	stp	fp, lr, [sp, #-16]!
	mov	fp, sp
	; begin block
	; begin fn call
	ldr	x11, =0x1
	str	x11, [sp, #-16]!	; push
	ldr	x0, [sp], #16	; pop
	brk	#0	; __builtin_trap()
	; end fn call
	; end block
	.cfi_endproc
	; end fn

.subsections_via_symbols
