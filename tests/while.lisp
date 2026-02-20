(define (+ a b) (+ a b))
(define (< a b) (< a b))
(define (print x) (print x))

(define (while state cond body)
  (if (cond state)
    (while (body state) cond body)
    ()))

(define (loop i n)
  (while i (lambda (state) (< state n))
    (lambda (state) (print state) (+ state 1))))

(define (main)
  (loop 0 10))
