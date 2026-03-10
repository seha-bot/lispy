#import "@preview/curryst:0.6.0": prooftree, rule, rule-set

#set heading(numbering: "1.")

#show raw.where(lang: "dsl"): it => {
  let matches = it.text.matches(regex("\(([^\n\r\s\)]*)"))

  for match in matches {
    if match.captures.at(0).len() != 0 {
      it = {
        show match.captures.at(0): txt => text(txt, fill: blue.darken(20%))
        it
      }
    }
  }

  show regex("Bool|Unit"): txt => text(txt, fill: red.darken(20%))
  show regex("true|false|unit"): txt => text(txt, fill: green.darken(35%))
  it
}

#let lisp(body) = box(raw(body, lang: "dsl"))
#let example(body) = figure(
  block(width: 100%)[
    #align(left)[
      #context [
        \[Example #counter(figure.where(kind: "example")).display():

        #body

        --- end example\]
      ]
    ]
  ],
  kind: "example",
  supplement: [Example],
)
#let todo() = rect(
  [
    #set text(fill: red.darken(35%))
    *TODO: section under construction*
  ],
  stroke: (dash: "dashed", paint: red.darken(35%), thickness: 2pt),
  inset: 10pt,
)
#let pair(x, y) = $chevron.l #x, #y chevron.r$

= Introduction
A _symbol_ is a non-empty sequence of letters and digits.
Two symbols are equal if they consist of the exact same letters and digits in the same order.

A symbol followed by a quote, like $alpha', X', "Truck"', x', n'$, represents an arbitrary symbol.
Whenever arbitrary symbols appear multiple times within a single sentence, only the instances with the same initial symbol refer to the same symbol.
For example, $X'$ and $X'$ refer to the same symbol, but $X'$ and $Y'$ do not.

A _type_ is formed with a symbol $X'$ by writing $X' : *$ and saying $X'$ is a type.
Two types are equal if their symbols are equal.
Types $X' : *$ and $Y' : *$ are equal if $X'$ and $X'$ are equal.
If types $X' : *$ and $Y' : *$ are equal, we write $X' = Y' : *$.

A _value_ is formed with a symbol $x'$ and a type $X' : *$ by writing $x' in X'$ and saying $x'$ is a value with the type $X'$.
Values $x' in T'$ and $y' in T'$ are equal if $x'$ and $y'$ are equal.
If values $x' in T'$ and $y' in T'$ are equal, we write $x' = y' in T'$.

== General rules
_General rules_ are a shortcut to forming sentences.
We write a horizontal line and write premises separated by spaces above it and write a conclusion under it.
General rules may be stacked.

$
  prooftree(
    rule(
      P_1,
      P_2,
      ...,
      P_n,
      C,
    ),
  )
$

The rule above translates to the sentence: Given $P_1$ and $P_2$ and ... and $P_n$, we also have $C$.

== Function axioms
// We can write $(X -> (Y -> ...))$ as $(X -> Y -> ...)$.

$
  prooftree(
    rule(
      name: "function type formation axiom",
      Gamma tack X' : *,
      Gamma tack Y' : *,
      Gamma tack (X' -> Y') : *,
    ),
  )
$

$
  prooftree(
    rule(
      name: "abstraction axiom",
      Gamma\; x' in X' tack y' in Y',
      Gamma tack lambda x'. y' in (X' -> Y'),
    ),
  )
$

$
  prooftree(
    rule(
      name: "application axiom",
      Gamma tack f' in (X' -> Y'),
      Gamma tack x' in X',
      Gamma tack f'[x'] in Y',
    ),
  )
$

== Bottom
$
  prooftree(
    rule(
      name: "bottom type formation axiom",
      Gamma tack bot : *,
    ),
  )
$

$
  prooftree(
    rule(
      name: "bottom type contradiction axiom",
      Gamma tack X' : *,
      Gamma tack b' in (X' -> bot),
    ),
  )
$

// == Pair axioms
// $
//   prooftree(
//     rule(
//       name: "pair type formation axiom",
//       Gamma tack X' : *,
//       Gamma tack Y' : *,
//       Gamma tack (X' times Y') : *,
//     ),
//   )
// $

// $
//   prooftree(
//     rule(
//       name: "pairing axiom",
//       Gamma tack x' in X',
//       Gamma tack y' in Y',
//       Gamma tack pair(x', y') in (X' times Y'),
//     ),
//   )
// $

// $
//   prooftree(
//     rule(
//       name: "fst axiom",
//       Gamma tack pair(x', y') in (X' times Y'),
//       Gamma tack "fst"[pair(x', y')] = x' in X',
//     ),
//   )
// $

// $
//   prooftree(
//     rule(
//       name: "snd axiom",
//       Gamma tack pair(x', y') in (X' times Y'),
//       Gamma tack "snd"[pair(x', y')] = y' in Y',
//     ),
//   )
// $

// = $NN$
// $
//   prooftree(
//     rule(
//       name: "form axiom",
//       Gamma tack NN : *,
//     ),
//   )
// $

// $
//   prooftree(
//     rule(
//       name: "zero axiom",
//       Gamma tack 0 in NN,
//     ),
//   )
// $

// $
//   prooftree(
//     rule(
//       name: "succ axiom",
//       Gamma tack "succ" in (NN -> NN),
//     ),
//   )
// $

// *succ[n] theorem*:
// $
//   prooftree(
//     rule(
//       name: "succ[n] theorem",
//       Gamma tack n in NN,
//       Gamma tack "succ"[n] in NN,
//     ),
//   )
// $
// *succ[n] theorem proof*:
// $
//   prooftree(
//     rule(
//       name: "by application axiom",
//       rule(
//         name: "by succ axiom",
//         Gamma tack "succ" in (NN -> NN),
//       ),
//       Gamma tack n in NN,
//       Gamma tack "succ"[n] in NN,
//     ),
//   )
// $

// *one theorem*:
// $
//   prooftree(
//     rule(
//       name: "one theorem",
//       Gamma tack "succ"[0] in NN,
//     ),
//   )
// $

// *one theorem proof*:
// $
//   prooftree(
//     rule(
//       name: "by succ[n] theorem",
//       rule(
//         name: "by zero axiom",
//         rule(
//           name: "by form axiom",
//           Gamma tack NN : *,
//         ),
//         Gamma tack 0 in NN,
//       ),
//       Gamma tack "succ"[0] in NN,
//     ),
//   )
// $

// *two theorem proof*:
// $
//   prooftree(
//     rule(
//       name: "by succ[n] theorem",
//       rule(
//         name: "by one theorem",
//         Gamma tack "succ"[0] in NN,
//       ),
//       Gamma tack "succ"["succ"[0]] in NN,
//     ),
//   )
// $

// == List
// $
//   prooftree(
//     rule(
//       name: "type application axiom",
//       Gamma tack alpha' : * -> *,
//       Gamma tack X' : *,
//       Gamma tack alpha'[X'] : *,
//     ),
//   )
// $

// $
//   prooftree(
//     rule(
//       name: "list type constructor formation axiom",
//       Gamma tack "List" : * -> *,
//     ),
//   )
// $

// $
//   prooftree(
//     rule(
//       name: "nil axiom",
//       Gamma tack "List"[X'] : *,
//       Gamma tack "nil" in "List"[X'],
//     ),
//   )
// $

// $
//   prooftree(
//     rule(
//       name: "cons axiom",
//       Gamma tack "List"[X'] : *,
//       Gamma tack "cons" in (X' -> "List"[X'] -> "List"[X']),
//     ),
//   )
// $

// $
//   prooftree(
//     rule(
//       name: "head axiom",
//       Gamma tack "cons"[h'][t'] in "List"[X'],
//       Gamma tack "head"["cons"[h'][t']] = h' in X',
//     ),
//   )
// $

// $
//   prooftree(
//     rule(
//       name: "tail axiom",
//       Gamma tack "cons"[h'][t'] in "List"[X'],
//       Gamma tack "tail"["cons"[h'][t']] = t' in "List"[X'],
//     ),
//   )
// $

== Lisp syntax
There are types $"Atom"$, $"Cons"$, and $"Sexp"$ defined by:
$
  prooftree(
    rule(
      name: "atom type formation axiom",
      Gamma tack "Atom" : *,
    ),
  )
$
$
  prooftree(
    rule(
      name: "symbols are atoms axiom",
      Gamma tack a',
      Gamma tack a' in "Atom",
    ),
  )
$
$
  prooftree(
    rule(
      name: "cons type formation axiom",
      Gamma tack "Cons" : *,
    ),
  )
$
$
  prooftree(
    rule(
      name: "cons axiom",
      Gamma tack x' in "Sexp",
      Gamma tack y' in "Sexp",
      Gamma tack "cons"[x'][y'] in "Cons",
    ),
  )
$
$
  prooftree(
    rule(
      name: "sexp type formation axiom",
      Gamma tack "Sexp" : *,
    ),
  )
$
$
  prooftree(
    rule(
      name: "sexp atom constructor axiom",
      Gamma tack x' in "Atom",
      Gamma tack "sexpatom"[x'] in "Sexp",
    ),
  )
$
$
  prooftree(
    rule(
      name: "sexp cons constructor axiom",
      Gamma tack x' in "Cons",
      Gamma tack "sexpcons"[x'] in "Sexp",
    ),
  )
$

An _expression_ is a value of type $"Sexp"$.

A symbol $e'$ prefixed by "$:$" is an arbitrary expression.
A symbol $e'$ prefixed by "$::$" is a non-empty sequence of arbitrary expressions separated by whitespace.
You may refer to the first expression in #lisp("::e") by writing #lisp("e").
You may refer to all the expressions except the first in #lisp("::e") by writing #lisp(":::e").

The following rules are called _lisp syntax_ and are used to write expressions more easily:
+ A symbol $e'$ is an expression $"sexpatom"[e'] in "Sexp"$,
+ #lisp("()") is an expression $"sexpatom"["nil"] in "Sexp"$,
+ #lisp("(::e)") is an expression $"sexpcons"["cons"[#lisp("e")][#lisp("(:::e)")]] in "Sexp"$.

= Program
An _atom expression_ is an expression #lisp(":e") such that \
there exists an expression #lisp(":a") where $"atom"[#lisp(":a")] = #lisp(":e") in "Sexpr"$.
// A _program_ is a module $p$ such that:
// + $m$ has a function definition which defines a function $"main"$ where $"main" in ("Unit" -> "Sexp")$
// + $m$ does not have a parent scope.

== Modules
A _module_ is an expression of the form #lisp("(module :name ::body)") where #lisp(":name") is an atom expression.
Expressions in #lisp("::body") are called _definitions_.

// === Simple type definition
// A definition of the form #lisp("(simple :N ::v)") defines a type $T$ where
// + #lisp("::N") is an atom expression,
// + $T$ is the identifier of $"name"[N]$,
// + #lisp(":::v") consists only of atom expressions,
// + if #lisp(":::v") consists of expressions $v_1, v_2, ..., v_n$, then $T$ contains values which are identifiers of $"name"[v_1], "name"[v_2], ..., "name"[v_n]$.

// #example[
//   ```dsl
//   (simple Unit unit)
//   (simple Bool true false)
//   (simple Color red green blue)
//   ```
// ]

// === Tuple type definition
// A definition of the form #lisp("(tuple ::N :::X)") defines a type $(T <- (T_1 times T_2 times ... times T_n))$ where
// + #lisp("::N") is an atom expression,
// + $T$ is the identifier of $"name"[N]$,
// + #lisp(":::X") consists only of atom expressions,
// + if #lisp(":::X") consists of expressions $X_1, X_2, ..., X_n$, then $T_1, T_2, ... T_n$ are identifiers of $"name"[X_1], "name"[X_2], ..., "name"[X_n]$.

// // Expressions of the form #lisp("(T x1 x2 ... xn)") evaluate to $chevron.l x_1, x_2, ..., x_n chevron.r in T$.

// #example[
//   ```dsl
//   (tuple Light Bool Color)
//   ```
// ]

// === Variant type definition
// A definition of the form #lisp("(variant ::N :::X)") defines a new type $(T <- (T_1 + T_2 + ... + T_n))$ where
// + #lisp("::N") is an atom expression,
// + $T$ is the identifier of $"name"[N]$,
// + #lisp(":::X") consists only of atom expressions,
// + if #lisp(":::X") consists of expressions $X_1, X_2, ..., X_n$, then $T_1, T_2, ... T_n$ are identifiers of $"name"[X_1], "name"[X_2], ..., "name"[X_n]$.
// // + #lisp(":::X") consists of expressions $X_1, X_2, ..., X_n$ such that for each $i in [1..n]$, $X_i$ is of the form #lisp("(::D ::V)") where
// //   + #lisp("::D") is an atom expression,
// //   + #lisp("::V") is an atom expression,
// //   + $T_i$ is the identifier of $"name"[V]$,

// // Expressions of the form #lisp("(di x)") evaluate to $chevron.l d_i, x chevron.r in V$.

// #example[
//   ```dsl
//   (tuple Cons Bool List)
//   (variant List Unit Cons)
//   ```
// ]

// // === Generic type definition
// // What about (lambda A (simple C a))? How to namespace/infer "a"?

// // If a definition $d$ defines a type or a type function $F$, then #lisp("(lambda A d)") defines a type function $F$.

// // For all types $T$, a definition of the form #lisp("(lambda ::X ::d)") defines a type $U$. If the definition $d$ would have defined a type $T$, then #lisp("(T X)") is that type with $A := X$. If the definition $d$ would have defined a type function $F$, then #lisp("(F X)") is that type function with $A := X$.

// // #example(
// //   ```dsl
// //   (lambda A
// //     (tuple Cons A (List A)))

// //   (lambda A
// //     (variant List
// //       nil
// //       (cons (Cons A))))
// //   ```,
// // )

// // == Function types
// // #todo()

// // #example(
// //   ```dsl
// //   (lambda A (lambda B (tuple To A B)))

// //   (dec (To Bool Bool) not)
// //   (dec (To Bool (To Bool Bool)) and)
// //   (dec (To Bool Unit) forget)
// //   ```,
// // )

// // #example(
// //   ```dsl
// //   (def iota
// //     (lambda n
// //       (if (= n 0)
// //         nil
// //         (cons (Cons n (iota (- n 1)))))))
// //   ```,
// // )
