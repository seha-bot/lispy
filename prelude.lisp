(define (null x)
  (eq x 'NIL))

(define (foldl f init xs)
  (if (null xs)
      init
      (foldl f (f init (car xs)) (cdr xs))))

(define (main)
  (print (foldl
    (lambda (acc x) (cons x acc))
    'NIL
    '(1 2 3))))
