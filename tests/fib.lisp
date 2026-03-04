;; (module Prelude

;; (type Unit
;;   Unit)

;; ;; (type (To A B)
;; ;;   (To A B))

;; ;; (dec panic (To String Never))
;; ;; (defmacro todo '(panic "todo"))

;; (type Bool
;;   True
;;   False)

;; ;; (def type
;; ;;   (lambda A
;; ;;     (To Bool ; cond
;; ;;     (To (To Unit A) ; then
;; ;;     (To (To Unit A) ; else
;; ;;     A))))
;; ;;   if-impl cond then else
;; ;;     (case cond
;; ;;       Bool.True (then Unit.Unit)
;; ;;       Bool.False (else Unit.Unit)))

;; ;; (defmacro if cond then else
;; ;;   '(if-impl ,cond (lambda _ ,then) (lambda _ ,else)))

;; )


(def (type (To Int (To Int Int))) + +)
(def (type (To Int (To Int Int))) - -)
(def (type (To Int (To Int Bool))) < <)

;; (def type (To Int (To Int Bool)) = =)

(def (type (To Int Int))
  fib
    (lambda (type (To Int Int)) n
      (case (< n 2)
        True n
        False (+ (fib (- n 1)) (fib (- n 2))))))

;; (def type (To Int (To Int (To Int Int)))
;;   fib-iter n a b
;;     (if (= n 0)
;;       a
;;       (fib-iter (- n 1) b (+ a b))))
