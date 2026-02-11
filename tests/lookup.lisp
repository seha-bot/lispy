(define (f) id)
(define (id x) x)

; EXPECT:
; f:
; 	push id
; 	ret 0
; id:
; 	push [0]
; 	set 1
; 	ret 0
