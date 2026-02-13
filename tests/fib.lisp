(define (+ x y) (+ x y))
(define (- x y) (- x y))
(define (< x y) (< x y))
(define (eq x y) (eq x y))
(define (print x) (print x))

(define (fib n)
  (if (< n 2)
    n
    (+ (fib (- n 1)) (fib (- n 2)))))

(define (fib-iter n a b)
  (if (eq n 0)
    a
    (fib-iter (- n 1) b (+ a b))))

(define (main)
  (print (fib 30))
  (print (fib-iter 30 0 1)))

;; :define $fib n
;;   :if (< n 2)
;;     n
;;     (+ $fib (- n 1)
;;        $fib (- n 2))
