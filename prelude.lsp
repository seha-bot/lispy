(LABEL sequence (LAMBDA (x) '()))

(LABEL out (LAMBDA (x) (
    COND
        ((ATOM x) x)
        ('T (sequence
            (PRINT (CAR x))
            (out (CDR x))
        ))
)))
