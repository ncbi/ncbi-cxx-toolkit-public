; -*- asm -*-

; $Id: ncbicntr_workshop.il,v 1.1 2002/05/23 22:24:22 ucko Exp $
	
;; for SPARC v9s
; in:  intended new value (%o0), address (%o1), required old value (%o2)
; out: actual old value (%o0)
.inline NCBICORE_asm_cas, 16
	cas [%o1], %o2, %o0
.end

;; for older SPARCs
; in:  new value (%o0), address (%o1)
; out: original value at address
.inline NCBICORE_asm_swap, 8
	swap [%o1], %o0
.end

;; for Intel x86s
; in:  address ((%esp)), delta (4(%esp))
; out: old value (%eax)
.inline NCBICORE_asm_lock_xaddl, 8
	movl   (%esp), %edx
	movl  4(%esp), %eax
	lock
	xaddl    %eax, (%edx)
.end
