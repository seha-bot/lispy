;; (module Prelude

  (form Never (variant))
  (form Unit (variant :unit))
  (form Int (variant :zero))
  (form Bool (variant :true :false))

  (form Nat
    (variant
      :zero
      (:succ Nat)))

  ;; (def add
  ;;   (lambda (type (to Nat (to Nat Nat))) n
  ;;     (lambda (type (to Nat Nat)) m
  ;;       (case m
  ;;         :zero n
  ;;         (:succ p) (add (Nat (:succ n)) p)))))

  (form List
    (lambda (kind (to * *)) A
      (variant
        (:nil Unit)
        (:cons (tuple A (List A))))))

  (def empty
    (forall (kind *) A
      ((List A) (:nil (Unit :unit)))))

  (def prepend
    (forall (kind *) A
      (lambda (type (to A (to (List A) (List A)))) x
        (lambda (type (to (List A) (List A))) xs
          ((List A) (:cons x xs))))))

  (form Functor
    (lambda (kind (to (to * *) *)) F
      (variant
        (:fmap
          (lambda (kind (to * (to * *))) A
            (lambda (kind (to * *)) B
              (to (to A B) (to (F A) (F B)))))))))
;; )

(dec + (to Int (to Int Int)))
(dec - (to Int (to Int Int)))
(dec < (to Int (to Int Bool)))
(dec = (to Int (to Int Bool)))

;; (dec (to Int Int) fib)
(def fib
  (lambda (type (to Int Int)) n
    (case (< n 2)
      :true n
      :false (+ (fib (- n 1)) (fib (- n 2))))))

(def (type (to Int (to Int (to Int Int))))
  fib-iter
    (lambda n
    (lambda a
    (lambda b
      (case (= n 0)
        :true a
        :false (fib-iter (- n 1) b (+ a b)))))))
