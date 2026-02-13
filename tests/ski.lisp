(define (s x) (lambda (y) (lambda (z) ((x z) (y z)))))
(define (k x) (lambda (y) x))
(define (i x) x)

; EXPECT:
; s:
; 	closure lam0
; 	capture [1]
; 	setret 1
; lam0:
; 	closure lam1
; 	capture [1]
; 	capture [2]
; 	setret 2
; lam1:
; 	push [2]
; 	indcall [2]
; 	push [3]
; 	indcall [2]
; 	indcall [1]
; 	setret 4
; k:
; 	closure lam2
; 	capture [1]
; 	setret 1
; lam2:
; 	push [0]
; 	setret 2
; i:
; 	ret 0
