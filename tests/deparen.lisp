define $foldl f init xs:
  if (null xs):
    init
    $foldl f (f init (car xs)) (cdr xs)

define $foldl f init xs:
  cases xs:
    init
    #$foldl f (f init #1) #2
