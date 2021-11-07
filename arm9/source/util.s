.arm

.global getCP15Ctrl
.global disableMPU
.global enableMPU

_getCP15Ctrl:
	mrc p15, 0, r0, c1, c0, 0
	bx lr
	
_disableMPU:
	mrc p15, 0, r3, c1, c0, 0
	bic r3, r3, #0xD
	mcr p15, 0, r3, c1, c0, 0
	bx lr
	
_enableMPU:
	mrc p15, 0, r3, c1, c0, 0
	orr r3, r3, #0xD
	mcr p15, 0, r3, c1, c0, 0
	bx lr
	
.thumb

getCP15Ctrl:
	ldr r3, =_getCP15Ctrl
	bx r3
	
disableMPU:
	ldr r3, =_disableMPU
	bx r3
	
enableMPU:
	ldr r3, =_enableMPU
	bx r3
