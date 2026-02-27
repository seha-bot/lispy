(dec while-impl
  (lambda (M (arr Type -> Type)) A
    (arr! (Monad M) -> A -> (A -> Bool) -> (A -> (M A)) -> (M A))))

(def while-impl monad init cond body
  (if (cond init)
    (do monad
      (val <- (body init))
      (while-impl monad val cond body))
    (pure monad init)))

(defmacro while monad decl cond body...
  '(while-impl
    ,monad
    ,(cdr decl)
    (lambda ,(car decl) ,cond)
    (lambda ,(car decl) (do ,monad ,body...))))

(def main
  (while io-monad (i 0) (< i 10)
    (print i)
    (pure (+ i 1))))
