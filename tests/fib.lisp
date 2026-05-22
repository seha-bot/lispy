;; ;; (module Prelude

;;   (form Never (variant))
;;   (form Unit (variant :unit))
  (form Bool (variant :true :false))

  (form Nat
    (variant
      :zero
      (:succ Nat)))

;;   (def add
;;     (lambda (n Nat)
;;       (lambda (m Nat)
;;         (case m
;;           :zero n
;;           (:succ p) (add (Nat (:succ n)) p)))))

;;   (form List
;;     (tt-lambda (A *)
;;       (variant
;;         (:nil Unit)
;;         (:cons (tuple A (List A))))))

;;   (form ListN
;;     (variant
;;       :nil
;;       (:cons (tuple N (ListN N)))))

;; (def prazna
;;   (tv-lambda (A *)
;;     (lambda (l (Lista A))
;;       (case l
;;         :prazna :true
;;         (:spoj _ _) :false))))

;;   ;; (dec empty (forall (A *) (List A)))
;;   (def empty
;;     (tv-lambda (A *)
;;       ((List A) (:nil (Unit :unit)))))

;;   ;; (dec prepend (forall (A *) (to A (to (List A) (List A)))))
;;   (def prepend
;;     (tv-lambda (A *)
;;       (lambda (x A)
;;         (lambda (xs (List A))
;;           ((List A) (:cons x xs))))))

;; (dec append (forall (A *) (to (Lista A) (to (Lista A) (Lista A)))))
;; (def append
;;   (tv-lambda (A *)
;;     (lambda xs
;;       (lambda ys
;;         (case ys
;;           :prazna xs
;;           (:spoj z zs)
;;             ((Lista A) (:spoj z (append xs zs))))))))

;;   (form Functor
;;     (lambda (kind (to (to * *) *)) F
;;       (variant
;;         (:fmap
;;           (lambda (kind (to * (to * *))) A
;;             (lambda (kind (to * *)) B
;;               (to (to A B) (to (F A) (F B)))))))))
;; ;; )

(dec + (to Nat (to Nat Nat)))
(dec - (to Nat (to Nat Nat)))
(dec < (to Nat (to Nat Bool)))
(dec = (to Nat (to Nat Bool)))

(def zero (:zero Nat))
(def one (:succ Nat zero))
(def two (:succ Nat one))

(def fib
  (lambda n
    (case (< n two)
      :true n
      :false (+ (fib (- n one)) (fib (- n two))))))

(dec fib-iter (to Nat (to Nat (to Nat Nat))))
(def fib-iter
  (lambda (n Nat)
    (lambda (a Nat)
      (lambda (b Nat)
        (case (= n zero)
          :true a
          :false (fib-iter (- n one) b (+ a b)))))))
