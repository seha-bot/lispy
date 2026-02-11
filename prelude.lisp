(define (print x) (print x))
(define (cons x y) (cons x y))
(define (eq x y) (eq x y))
(define (car x) (car x))
(define (cdr x) (cdr x))

(define (null x)
  (eq x 'NIL))

(define (foldl f init xs)
  (if (null xs)
    init
    (foldl f (f init (car xs)) (cdr xs))))

(define (reverse xs)
  (foldl (lambda (acc x) (cons x acc)) 'NIL xs))

(define (main)
  (print (reverse '(1 2 3))))
