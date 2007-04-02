! -*- asm -*-

! $Id$
	
!! for SPARC v9s
! in:  intended new value (%o0), address (%o1), required old value (%o2)
! out: actual old value (%o0)
.inline NCBICORE_asm_cas, 16
	cas [%o1], %o2, %o0
.end

.inline NCBICORE_asm_casx, 24
	casx [%o1], %o2, %o0
.end


!! for older SPARCs
! in:  new value (%o0), address (%o1)
! out: original value at address
.inline NCBICORE_asm_swap, 8
	swap [%o1], %o0
.end


!! for Intel x86s
! in:  address ((%esp)), delta (4(%esp))
! out: old value (%eax)
.inline NCBICORE_asm_lock_xaddl, 8
	movl   (%esp), %edx
	movl  4(%esp), %eax
	lock
	xaddl    %eax, (%edx)
.end

! in:  new value ((%esp)), location (4(%esp))
! out: old value (%eax)
.inline NCBICORE_asm_xchg, 8
	movl   (%esp), %eax
	movl  4(%esp), %edx
	xchg     %eax, (%edx)
.end

!! for x86-64
! in:  address (%rdi), delta (%esi)
! out: old value (%eax)
.inline NCBICORE_asm_lock_xaddl_64, 12
	movl  %esi, %eax
	lock
	xaddl %eax, (%rdi)
.end

! in:  new value (%rdi), location (%rsi)
! out: old value (%rax)
.inline NCBICORE_asm_xchgq, 16
	movq   %rdi, %rax
	xchgq  %rax, (%rsi)
.end
