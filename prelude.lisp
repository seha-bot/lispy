(defun null (x)
  (cond ((atom x) (eq x 'NIL))
        ('T 'F)))

(defmacro list (x)
  (CONS (CAR x) (list (CDR x)))
)

(DEFINE test (LAMBDA (x)
    (list x 'o)
))

(PRINT (test 'f))

;; (DEFINE defun (MACRO x
;;   (EVAL
;;     (list
;;       'DEFINE
;;       (CAR x)
;;       (list 'LAMBDA (CAR (CDR x)) (CAR (CDR (CDR x))))
;;     )
;;   )
;; ))

;; (DEFINE defun_impl (LAMBDA (name params body)
;;   (EVAL (list 'DEFINE name
;;     (list 'LAMBDA params body)))))

;; (DEFINE defun_impl (LAMBDA (name x body)
;;   (EVAL (list 'DEFINE name
;;     (list 'LAMBDA x body)))))

;; (DEFINE test (LAMBDA (x)
;;     (list_impl '(x 'o))
;; ))
;; (test '(k) 'k)

;; (defun id (k) k)
;; (defun_impl 'id '(k) 'k)
