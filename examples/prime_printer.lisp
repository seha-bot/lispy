(define (+ x y) (+ x y))
(define (- x y) (- x y))
(define (< x y) (< x y))
(define (mod x y) (mod x y))
(define (print x) (print x))
(define (eq x y) (eq x y))
(define (not x) (if x 0 1))

(define (has_divisor x i)
  (if (eq (mod x i) 0)
    (not (eq i 1))
    (has_divisor x (- i 1))))

(define (is_prime x)
  (if (< x 2)
    0
    (not (has_divisor x (- x 1)))))

(define (loop n)
    (if (is_prime n)
        (print n)
        ())
    (loop (+ n 1)))

(define (main)
    (loop 2))
