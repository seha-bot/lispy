(define (null x)
  (eq x 'NIL))

(define (foldl f init xs)
  (if (null xs)
      init
      (foldl f (f init (car xs)) (cdr xs))
  )
)
