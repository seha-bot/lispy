(LABEL null (LAMBDA (x)
    (EQ x '())
))

;; (DEFMACRO defun (name args body)
;;   (list 'LABEL name
;;     (list 'LAMBDA args body)))

;; (defun null (x)
;;   (EQ x '()))

;; (defun and (x y)
;;   (COND (x (COND (y 'T) ('T 'F)))
;;         ('T 'F)))

;; (defun not (x)
;;     (COND (x 'F)
;;           ('T 'T)))
