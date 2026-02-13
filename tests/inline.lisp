(define (+ a b) (+ a b))

(define (f x)
    (+ 1 ((lambda () (+ x 2)))))

; EXPECT:
; f:
; 	push 1
; 	push [1]
; 	push 2
; 	iadd
; 	iadd
; 	setret 1
