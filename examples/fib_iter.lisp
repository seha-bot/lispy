(define (+ x y) (+ x y))
(define (eq x y) (eq x y))
(define (< x y) (< x y))
(define (print x) (print x))

(define (fib n a b)
  (if (eq n 30)
    a
    (fib (+ n 1) b (+ a b))))

(define (main)
    (print (fib 1 1 1)))
