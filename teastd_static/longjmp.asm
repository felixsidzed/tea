.data

.const

.code
	PUBLIC __setjmp
	PUBLIC __longjmp

	__setjmp PROC
		mov     [rcx+00h], rbp
		mov     [rcx+08h], rbx
		lea     rax, [rsp+08h]
		mov     [rcx+10h], rax
		mov     [rcx+18h], rbp
		mov     [rcx+20h], rsi
		mov     [rcx+28h], rdi
		mov     [rcx+30h], r12
		mov     [rcx+38h], r13
		mov     [rcx+40h], r14
		mov     [rcx+48h], r15

		mov     rax, [rsp]
		mov     [rcx+50h], rax

		stmxcsr [rcx+58h]
		fnstcw  [rcx+5Ch]

		movdqa  [rcx+60h], xmm6
		movdqa  [rcx+70h], xmm7
		movdqa  [rcx+80h], xmm8
		movdqa  [rcx+90h], xmm9
		movdqa  [rcx+0A0h], xmm10
		movdqa  [rcx+0B0h], xmm11
		movdqa  [rcx+0C0h], xmm12
		movdqa  [rcx+0D0h], xmm13
		movdqa  [rcx+0E0h], xmm14
		movdqa  [rcx+0F0h], xmm15
	
		xor     eax, eax
		ret
	__setjmp ENDP

	__longjmp PROC
		mov     rbp, [rcx+00h]
		mov     rbx, [rcx+08h]
		mov     rsp, [rcx+10h]
		mov     rbp, [rcx+18h]
		mov     rsi, [rcx+20h]
		mov     rdi, [rcx+28h]
		mov     r12, [rcx+30h]
		mov     r13, [rcx+38h]
		mov     r14, [rcx+40h]
		mov     r15, [rcx+48h]

		ldmxcsr [rcx+58h]
		fldcw   [rcx+5Ch]

		movdqa  xmm6,  [rcx+60h]
		movdqa  xmm7,  [rcx+70h]
		movdqa  xmm8,  [rcx+80h]
		movdqa  xmm9,  [rcx+90h]
		movdqa  xmm10, [rcx+0A0h]
		movdqa  xmm11, [rcx+0B0h]
		movdqa  xmm12, [rcx+0C0h]
		movdqa  xmm13, [rcx+0D0h]
		movdqa  xmm14, [rcx+0E0h]
		movdqa  xmm15, [rcx+0F0h]

		mov     eax, edx
		test    eax, eax
		jne     @f
		mov     eax, 1
	@@:
		jmp     qword ptr [rcx+50h]
	__longjmp ENDP
END
