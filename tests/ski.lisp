(define (s x) (lambda (y) (lambda (z) ((x z) (y z)))))
(define (k x) (lambda (y) x))
(define (i x) x)

; EXPECT:
