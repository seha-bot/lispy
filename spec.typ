#set heading(numbering: "1.")
#let lisp(body) = box(raw(body, lang: "lisp"))
#let example(body) = figure(
  block(width: 100%)[
    #align(left)[
      #context [
        \[Example #counter(figure.where(kind: "example")).display(): #body
        --- end example\]
      ]
    ]
  ],
  kind: "example",
  supplement: [Example],
)
#let todo() = rect(
  [
    #set text(fill: red)
    *TODO: unfinished section*
  ],
  stroke: (dash: "dashed", paint: red, thickness: 2pt),
  inset: 10pt,
)

= Type system
Expressions evaluate to a _value_.
Each value is an element of some _type_.
If a value $v$ is of type $T$, we write $v in T$.
Types may be mutually recursive.

== Simple types
A type definition of the form #lisp("(simple S s1 s2 ... sn)") defines a new type $S$ with values $s_1,s_2,...,s_n$.

*Equality*:
For $s_i, s_j in S$ where $i, j in [1..n]$:
- $s_i = s_j <=> i = j$.

== Tuple types
A type definition of the form #lisp("(tuple T X1 X2 ... Xn)") defines a new type $T$.
Values of $T$ are all tuples $chevron.l x_1, x_2, ..., x_n chevron.r in T$ where $and.big_(i = 1)^n (x_i in X_i)$.
Expressions of the form #lisp("(T x1 x2 ... xn)") evaluate to $chevron.l x_1, x_2, ..., x_n chevron.r in T$.

*Equality*:
For $x, y in T$ where
$x := chevron.l x_1, x_2, ..., x_n chevron.r$ and
$y := chevron.l y_1, y_2, ..., y_n chevron.r$:
- $x = y <=> and.big_(i=1)^n (x_i = y_i)$.

== Variant types
A type definition of the form #lisp("(variant V (d1 X1) (d2 X2) ... (dn Xn))") defines a new type $V$ and values of some type $D$ as if #lisp("(simple D d1 d2 ... dn)") were written.
Values of $V$ are all tuples $chevron.l d_i, x chevron.r in V$ where $x in X_i and i in [1..n]$.
Expressions of the form #lisp("(di x)") evaluate to $chevron.l d_i, x chevron.r in V$.

*Equality*:
For $a, b in V$ where
$a := chevron.l d_i, x chevron.r$ and
$b := chevron.l d_j, y chevron.r$:
- $a = b <=> i = j and x = y$.

#example[
  ```lisp
  (simple Bool true false)
  (simple Unit unit)

  (tuple Cons Bool List)
  (variant List
    (nil Unit)
    (cons Cons))
  ```
] <bool-list>

If the type $"Unit"$ is defined as #lisp("(simple Unit unit)"), then in any variant definition, #lisp("(d Unit)") may be written as #lisp("d") where $d in D$.

If the type $"Unit"$ is defined as #lisp("(simple Unit unit)"), then for any expression $d$ which evaluates to a value $d in D$, the expression #lisp("(d unit)") may be written as #lisp("d").

#example[

  The type $"List"$ from @bool-list can be defined as:
  ```lisp
  (variant List
    nil
    (cons Cons))
  ```
]

#pagebreak()

== Type functions
#todo()

A definition of the form #lisp("(lambda A d)") defines a type function. If the definition $d$ would have defined a type $T$, then #lisp("(T X)") is that type with $A := X$. If the definition $d$ would have defined a type function $F$, then #lisp("(F X)") is that type function with $A := X$.

#example(
  ```lisp
  (lambda A
    (tuple Cons A (List A)))

  (lambda A
    (variant List
      nil
      (cons (Cons A))))
  ```,
)

== Function types
#todo()

#example(
  ```lisp
  (lambda A (lambda B (tuple To A B)))

  (dec (To Bool Bool) not)
  (dec (To Bool (To Bool Bool)) and)
  (dec (To Bool Unit) forget)
  ```,
)
