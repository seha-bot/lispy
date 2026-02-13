(define (f) id)
(define (id x) x)

; EXPECT:
; f:
; 	push id
; 	ret 0
; id:
; 	ret 0
