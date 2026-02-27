(type (List A)
  Nil
  (Cons A (List A)))

(namespace List
  (def map f xs
    (case xs
      Nil Nil
      (Cons y ys) (Cons (f y) (map f ys))))

  (def append xs ys
    (case xs
      Nil ys
      (Cons z zs) (Cons z (append zs ys))))

  (def flatten xs
    (case xs
      Nil Nil
      (Cons y ys) (append y (flatten ys)))))

(type (Functor (F (arr Type -> Type)))
  (Functor
    (lambda A B
      (arr (A -> B) -> (F A) -> (F B)))))

(dec (lambda F A B (arr (Functor F) -> (A -> B) -> (F A) -> (F B))) fmap)
(def fmap functor
  (case functor
    (Functor.Functor fn) fn))

(type (Monad (M (arr Type -> Type)))
  (Monad
    (lambda A
      (arr A -> (M A)))
    (lambda A B
      (arr (M A) -> (A -> (M B)) -> (M B)))))

(def functor-list
  (Functor.Functor List.map))

(def monad-list
  (Monad.Monad
    (lambda x (List.Cons x List.Nil))
    (lambda xs f (List.flatten (List.map f xs)))))

(def add-one
  (fmap list-functor (lambda x (Integer.add x 1))))
