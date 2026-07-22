; (form List
;   (tt-lambda (A *)
;     (variant
;       :nil
;       (:cons
;         (struct
;           (:head A)
;           (:tail (List A)))))))

; (def id (lambda (x Src) x))
; (id '(form Bool (variant :true :false)))

(form Bool (variant :true :false))

(form List
  (variant
    :nil
    (:cons
      (struct
        (:head Bool)
        (:tail List)))))

(def with-false
  (lambda (f (to Bool Bool))
    (f (:false Bool))))

(dec id (forall X (to X X)))
(def id (tv-lambda A (lambda (x A) x)))
; (def id (lambda x x))

(def do-stuff
  (with-false id))

; (def k (tv-lambda A (tv-lambda B (lambda (x A) (lambda (y B) x)))))
