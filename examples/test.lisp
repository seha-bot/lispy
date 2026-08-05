(form Bool (variant :true :false))

; (dec not (to Bool Bool))
;
; (def id (tv-lambda A (lambda (x A) x)))
; (def const (tv-lambda A (tv-lambda B (lambda (x A) (lambda (y B) x)))))
;
; (def f
;   (lambda x
;     (not x)))

; (form List
;   (variant
;     :nil
;     (:cons
;       (struct
;         (:head Bool)
;         (:tail List)))))
;
; (dec prepend (to Bool (to List List)))
; (def prepend
;   (lambda x
;     (lambda xs
;       (:cons
;         (pack
;           (:head x)
;           (:tail xs))))))

; (def prepend
;   (lambda (x Bool)
;     (lambda (xs List)
;       (:cons
;         (pack
;           (:head x)
;           (:tail xs))))))

(def with-false
  (lambda (f (to Bool Bool))
    (f :false)))

(dec id (forall X (to X X)))
(def id (tv-lambda _ (lambda x x)))
; (def id (lambda x x))

; (def val
;   (pack
;     (:a :true)
;     (:b :false)))

(def do-stuff
  (with-false id))
