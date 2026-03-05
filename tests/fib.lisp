(module Prelude
  (type Unit
    Unit)

  (type (To A B)
    (To A B))

  (type Bool
    True
    False)

  ;; (defmacro if cond then else
  ;;   '(case ,cond
  ;;     True ,then
  ;;     False ,else))
)

(def (type (To Int (To Int Int))) + +)
(def (type (To Int (To Int Int))) - -)
(def (type (To Int (To Int Bool))) < <)
(def (type (To Int (To Int Bool))) = =)

(def (type (To Int Int))
  fib
    (lambda (type (To Int Int)) n
      (case (< n 2)
        True n
        False (+ (fib (- n 1)) (fib (- n 2))))))

(def (type (To Int (To Int (To Int Int))))
  fib-iter
    (lambda n
    (lambda a
    (lambda b
      (case (= n 0)
        True a
        False (fib-iter (- n 1) b (+ a b)))))))
