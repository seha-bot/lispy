#import "@preview/curryst:0.6.0": prooftree, rule, rule-set

#set heading(numbering: "1.")

// #show raw.where(lang: "dsl"): it => {
//   let matches = it.text.matches(regex("\(([^\n\r\s\)]*)"))

//   for match in matches {
//     if match.captures.at(0).len() != 0 {
//       it = {
//         show match.captures.at(0): txt => text(txt, fill: blue.darken(20%))
//         it
//       }
//     }
//   }

//   // show regex("Bool|Unit"): txt => text(txt, fill: red.darken(20%))
//   // show regex("true|false|unit"): txt => text(txt, fill: green.darken(35%))
//   it
// }

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

In this document, a symbol followed by a quote, like $alpha', X', "Truck"'$, represents an arbitrary symbol.
Whenever multiple arbitrary symbols appear within a single sentence, any instances with equal symbols before the quotes are equal.
For example, $X'$ and $X'$ are equal, but $X'$ and $Y'$ are strictly not.

A _type function_ is something.

// == Lisp syntax
// There are types $"Atom"$, $"Cons"$, and $"Sexp"$ defined by:
//
// An _expression_ is a value of type $"Sexp"$.
//
// A symbol $e'$ prefixed by "$:$" is an arbitrary expression.
// A symbol $e'$ prefixed by "$::$" is a non-empty sequence of arbitrary expressions separated by whitespace.
// You may refer to the first expression in #lisp("::e") by writing #lisp("e").
// You may refer to all the expressions except the first in #lisp("::e") by writing #lisp(":::e").
//
// The following rules are called _lisp syntax_ and are used to write expressions more easily:
// + A symbol $e'$ is an expression $"sexpatom"[e'] in "Sexp"$,
// + #lisp("()") is an expression $"sexpatom"["nil"] in "Sexp"$,
// + #lisp("(::e)") is an expression $"sexpcons"["cons"[#lisp("e")][#lisp("(:::e)")]] in "Sexp"$.
//
// = Program
// An _atom expression_ is an expression #lisp(":e") such that \
// there exists an expression #lisp(":a") where $"atom"[#lisp(":a")] = #lisp(":e") in "Sexpr"$.
// // A _program_ is a module $p$ such that:
// // + $m$ has a function definition which defines a function $"main"$ where $"main" in ("Unit" -> "Sexp")$
// // + $m$ does not have a parent scope.
//
// == Modules
// A _module_ is an expression of the form #lisp("(module :name ::body)") where #lisp(":name") is an atom expression.
// Expressions in #lisp("::body") are called _definitions_.
// Each definition represents a general rule axiom.
//
// === Type definition
// A definition of the form #lisp("(type :T)") represents the following rule:
//
// $
//   prooftree(rule(Gamma tack T' : *,),)
// $
//
// where $T'$ is ???.
//
// A definition of the form #lisp("(value :T :v)") represents the following rule:
// *Note to self: this is what you want "dec" to be.*
//
// $
//   prooftree(
//     rule(
//       Gamma tack T' : *,
//       Gamma tack v' in T',
//     ),
//   )
// $
//
// where $N'$ is ??? and $v'$ is ???.
//
// #example(
//   ```dsl
//   (variant N
//     (zero Unit)
//     (succ N))
//
//   (def add n m
//     (case m
//       zero n
//       (succ p) (add (succ n) p)))
//
//   ```,
// )
