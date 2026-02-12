(define (+ x y) (+ x y))
(define (- x y) (- x y))
(define (< x y) (< x y))
(define (print x) (print x))

(define (fib n)
  (if (< n 2)
    n
    (+ (fib (- n 1)) (fib (- n 2)))))

(define (main)
  (print (fib 30)))

;; :define $fib n
;;   :if (< n 2)
;;     n
;;     (+ $fib (- n 1)
;;        $fib (- n 2))
