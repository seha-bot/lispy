(define (eq a b) (eq a b))
(define (* a b) (* a b))
(define (- a b) (- a b))
(define (print x) (print x))

(define (fact n)
  (if (eq n 1)
    1
    (* n (fact (- n 1)))))

(define (fact-iter n)
  (define (iter n res)
    (if (eq n 1)
      res
      (iter (- n 1) (* res n))))
  (iter n 1))

(define (main)
  (print (fact 5))
  (print (fact-iter 5)))
