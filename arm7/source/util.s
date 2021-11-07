.arm

.global biosRead16
.global hijackFunc

biosRead16:
	mov r1, r0
	mov r2, #2
	swi #0xE0000
	mov r0, r3
	bx lr
	
hijackFunc:
	@ r0 = old function  r1 = new function
	tst r0, #1
	bicne r0, r0, #1
	bne _hijack_thumb
	
	adr r12, _hijack_template
	ldmia r12, {r2-r3}
	mov r12, r1
	stmia r0, {r2-r3, r12}
	bx lr
_hijack_template:
	ldr r3, [pc]
	bx r3
	.word 0xACAB
	
_hijack_thumb:
	tst r0, #2
	ldrne r12, =0x46C0
	strneh r12, [r0], #2
	adr r12, _hijack_thumb_template
	ldr r2, [r12]
	mov r12, r1
	stmia r0, {r2, r12}
	bx lr
_hijack_thumb_template:
	.thumb
	ldr r3, [pc]
	bx r3
	.word 0xACAB
	.arm
	
.pool
