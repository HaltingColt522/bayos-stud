extern schedule
extern get_current_process

global timer_int_handler
timer_int_handler:
	; acknowledge the interrupt
	push rax
	mov al, 0x20
	out 0x20, al
	pop rax

	; store all registers on stack
	; TODO: also store xmm registers on the user stack
	; otherwise user space programs which use these registers
	; wont work properly
	; -> for now compile user space with -mgeneral-regs-only too
	push rax
	push rbx
	push rcx
	push rdx
	push rbp
	push rsi
	push rdi
	push r8
	push r9
	push r10
	push r11
	push r12
	push r13
	push r14
	push r15

	; move the stack pointer as first argument
	mov rdi, rsp
	call schedule
	; returns a point to stack, store it
	mov r15, rax

	call get_current_process
	; returns a pointer to the current process
	; first member is cr3 -> pointer points to process cr3
	mov rcx, cr3
	cmp rax, rcx
	je __cr3_equal

	mov rcx, [rax]
	mov cr3, rcx

__cr3_equal:
	; switch to the new stack
	mov rsp, r15

	; restore the registers
	pop r15
	pop r14
	pop r13
	pop r12
	pop r11
	pop r10
	pop r9
	pop r8
	pop rdi
	pop rsi
	pop rbp
	pop rdx
	pop rcx
	pop rbx
	pop rax

	; return to the context we just selected
	iretq
