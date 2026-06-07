(form Bool (variant :true :false))

(def true (:true Bool))
(def false (:false Bool))

(form Nat
  (variant
    :zero
    (:succ Nat)))

(def succ (lambda (n Nat) (:succ Nat n)))
(def zero (:zero Nat))
(def one (succ zero))
(def two (succ one))

(dec + (to Nat (to Nat Nat)))
(def +
  (lambda (n Nat)
    (lambda (m Nat)
      (case m
        :zero n
        (:succ p) (+ (succ n) p)))))

(dec - (to Nat (to Nat Nat)))
(def -
  (lambda (n Nat)
    (lambda (m Nat)
      (case m
        :zero n
        (:succ mp)
          (case n
            :zero zero
            (:succ np) (- np mp))))))

(dec < (to Nat (to Nat Bool)))
(def <
  (lambda (n Nat)
    (lambda (m Nat)
      (case m
        :zero false
        (:succ mp)
          (case n
            :zero true
            (:succ np) (< np mp))))))

(dec = (to Nat (to Nat Bool)))
(def =
  (lambda (n Nat)
    (lambda (m Nat)
      (case m
        :zero
          (case n
            :zero true
            (:succ np) false)
        (:succ mp)
          (case n
            :zero false
            (:succ np) (= mp np))))))

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
