This is a minimal documentation of the:

- `slugsin` input format that can be used for direct input to `slugs`

- structured slugs input format

- the translation of structured slugs to `slugsin` using the function `slugs.convert_to_slugsin` from the `python` package `slugs` that is under the directory `tools`.


# Input syntax

## structuredslugs

The description here is based on the Python module `slugs.compiler` and the file `slugs/grammar.wi` under `tools/StructuredSlugsParser/`.

- Both integer and Boolean variables can be used in this format.

- the `INPUT` section defines the variables controlled by the environment

- the `OUTPUT` section defines the variables controlled by the system

- `ENV_INIT` and `SYS_INIT` define the initial conditions. If in multiples lines, then their conjunction is taken.

- `ENV_TRANS` and `SYS_TRANS` define the GR(1) transition relation. If in multiple lines `l0, l1, ...`, then the result is `[](l0 & l1 & ...)`.

- `ENV_LIVENESS` and `SYS_LIVENESS` define the GR(1)

- the syntax of Boolean expressions is the same as that of the `gr1c` synthesizer described [here](http://slivingston.github.io/gr1c/md_spc_format.html), with the difference that variables can appear in expressions on both sides of a comparator.

- other section types available: `OBSERVABLE_INPUT`, `UNOBSERVABLE_INPUT`, `CONTROLLABLE_INPUT`.

- [infix notation](https://en.wikipedia.org/wiki/Infix_notation) is used for logic formulas, except for the next operator `'` that is postfix.

- `slugs` can synthesize for a fragment of LTL that is larger than GR(1).
  In this fragment the next operator can appear inside liveness conditions.

- one-line comments are prefixed by `#`.

### Operators

`Z` denotes the set of integers `{..., -1, 0, 1, ...}`.
`N` denotes the set of natural numbers `{0, 1, ...}`.

Logic:

- constants: `FALSE`, `TRUE`
- negation: `!`, `~`
- conjunction: `&`, `&&`, `/\`
- disjunction: `|`, `||`, `\/`
- exclusive disjunction: `^`
- implication: `->`, `-->`
- bi-implication: `<->`, `<-->`

Arithmetic:

- operators (2-ary functions): `+, -, *, /`
  (domain: `Z x Z`, codomain: `Z`)
- comparators (2nd order predicates): `=, !=, <, <=, >=, >`
  (domain: `Z x Z`, codomain: `B`)

Temporal:

- next: `()`, `next`, `X`, and prime symbol for referring to next values of variables
- always: `[]`, `G`
- eventually: `<>`, `F`
- until: `U`
- weak until: `W`


### Operator precedence

High
- unary operators: `~`, `X`, `F`, `G`
- temporal binary operators: `U`, `W`
- `&`
- `|`
- `^`
- `->`
- `<->`
Low


### EBNF

```
expr ::= lexpr
       | aexpr

# logic expression
lexpr ::= '(' lexpr ')'
        | l_unary lexpr
        | lexpr l_binary lexpr
        | aexpr comparator aexpr
        | BOOLVARNAME "'"
        | BOOLVARNAME
        | FALSE
        | TRUE

# arithmetic expression
aexpr ::= '(' aexpr ')'
        | aexpr a_binary aexpr
        | INTVARNAME
        | INT

# logic unary operator
l_unary ::= negation
          | next
          | always
          | eventually

# logic binary operator
l_binary_op ::= conjunction
              | disjunction
              | exclusive_disjunction
              | implication
              | bi-implication
              | until
              | weak_until

# arithmetic binary operator
a_binary ::= '+'
           | '-'  # not supported
           | '*'  # not supported
           | '/'  # not supported

comparator ::= '='
             | '<'
             | '<='
             | '>='
             | '>'
             | '!='
```


### Example

```
# variable definitions

[INPUT]
a
b:0...10

[OUTPUT]
c:2...8
d


# initial conditions

[ENV_INIT]
! a
b = 1

[SYS_INIT]
d
c = 4


# safety formula [](...)

[ENV_TRANS]
a -> (a' <-> ! a)
b' = b + 1

[SYS_TRANS]
d -> (c' = 3)


# conjunction of liveness formulas []<>(...)

[ENV_LIVENESS]
! a | (b = 3)

[SYS_LIVENESS]
d
c = 2

```


## slugsin

The description here is based on the file `src/synthesisContextBasics.cpp`:

- available sections: `INPUT, OUTPUT` and `(ENV | SYS)_(INIT | TRANS | LIVENESS)`.

- operators:

  - negation `!`
  - conjunction `&`
  - disjunction `|`
  - exclusive disjunction `^`

- Boolean constants: `0, 1`.

- operators for buffers:

  - `$ N` create a buffer with `N` elements, where `N` is a positive integer.
    The operator `$` starts a memory buffer and together with its formulas, they can appear as an expression.
   	For how memory buffers can be used, see the relevant section below.

  - `? i` refer to element `i` of a memory buffer.
    Must appear inside a memory buffer, i.e., in the scope of `$`.

- only Boolean variables can be used in this format.

- formulas must appear in [prefix notation](https://en.wikipedia.org/wiki/Polish_notation), except for the postfix next operator `'`.


### Memory buffers

Any expression can be a memory buffer.
For example, starting the line with `$ 5` means that the line comprises of 5 formulas indexed as follows:

```
$ 5 f0 f1 f2 f3 f4
```

Any of these formulas can refer to another formula with `? i` where `i` is the index of the referrent formula. For example, inside formula `f3` we can refer to the value of formula `f0` by writing the expression `? 0`.

For example, the constraint:

```
x + 1 <= 2
```

is translated to the expression:

```
$ 5 ^ 1 x@0.0.3 & 1 x@0.0.3 ^ x@1 ? 1 & x@1 ? 1 & ! ? 3 | & ! ? 2 1 & | 1 ! ? 2 | & ! ? 0 0 & | 0 ! ? 0 1
```

which is a memory buffer that contains the following 5 formulas:

- f0: `^ 1 x@0.0.3 = 1 ^ x@0.0.3`

- f1: `& 1 x@0.0.3 = 1 & x@0.0.3`

- f2: `^ x@1 ? 1 = x@1 ^ (? 1) = x@1 ^ f1`

- f3: `& x@1 ? 1 = x@1 & (? 1) = x@1 & f1`

- f4: `& ! ? 3 | & ! ? 2 1 & | 1 ! ? 2 | & ! ? 0 0 & | 0 ! ? 0 1 =`
  `!(? 3) & ((!(? 2) & 1) | ((1 | !(? 2)) & ((!(? 0) & 0) | ((0 | !(? 0)) & 1)))) =`
  `!f3 & ((!f2 & 1) | ((1 | !f2) & ((!f0 & 0) | ((0 | !f0) & 1))))`

The value of a memory buffer expression like `$ 5 f0 f1 f2 f3 f4` is equal to the value of the last formula in the buffer, here `f4`.

Memory buffers allow using the values of formulas at arbitrary places without the need to copy the formulas there, nor introduce auxiliary binary variables. This is handy to define [full-adders](https://en.wikipedia.org/wiki/Adder_%28electronics%29) for ripple-carry addition, and more general bitvector arithmetic. The [ternary conditional operator](https://en.wikipedia.org/wiki/%3F:) can also be efficiently represented by using a memory buffer.

This avoids duplicating the corresponding subformula, thus maintaining more its structure.
It can be regarded as a way of defining edges in the underlying binary decision diagram (BDD).


### EBNF

```
exprs ::= exprs expr
        | expr

# start symbol
expr ::= unary expr
       | binary expr expr
       | memory
       | recall
       | BOOLVARNAME "'"
       | BOOLVARNAME
       | '0'
       | '1'

unary ::= '!'

binary ::= '&' | '|' | '^'

# memory buffer: number of exprs must match INT constant
memory ::= '$' INT exprs

recall ::= '?' INT
```


### Example

The function `slugs.convert_to_slugsin` from the `python` package `slugs` translates the structured slugs example to:

```
[INPUT]
a
b@0.0.10
b@1
b@2
b@3

[OUTPUT]
c@0.2.8
c@1
c@2
d
# initial conditions
# initial conditions

[ENV_TRANS]
| ! a | & ! a' ! ! a & a' ! a
$ 9 ^ 1 b@0.0.10 & 1 b@0.0.10 ^ b@1 ? 1 & b@1 ? 1 ^ b@2 ? 3 & b@2 ? 3 ^ b@3 ? 5 & b@3 ? 5 & ! ? 7 & ! ^ b@3' ? 6 & ! ^ b@2' ? 4 & ! ^ b@1' ? 2 & ! ^ b@0.0.10' ? 0 1
## Variable limits: 0<=b<=10
| ! b@3 & ! b@2 | ! b@1 | ! b@0.0.10 0
## Variable limits: 0<=b'<=10
| ! b@3' & ! b@2' | ! b@1' | ! b@0.0.10' 0

[ENV_INIT]
! a
$ 1 & ! b@3 & ! b@2 & ! b@1 & ! ^ b@0.0.10 1 1
## Variable limits: 0<=b<=10
| ! b@3 & ! b@2 | ! b@1 | ! b@0.0.10 0

[SYS_TRANS]
| ! d $ 7 ^ 0 c@0.2.8' & 0 c@0.2.8' ^ ^ 1 c@1' ? 1 | | & 1 c@1' & ? 1 1 & ? 1 c@1' ^ c@2' ? 3 & c@2' ? 3 & ! ? 5 & ! ? 4 & ! ^ ? 2 1 & ! ^ ? 0 1 1
# conjunction of liveness formulas []<>(...)
## Variable limits: 2<=c<=8
| ! c@2 | ! c@1 | ! c@0.2.8 0
## Variable limits: 2<=c'<=8
| ! c@2' | ! c@1' | ! c@0.2.8' 0

[SYS_INIT]
d
$ 7 ^ 0 c@0.2.8 & 0 c@0.2.8 ^ ^ 1 c@1 ? 1 | | & 1 c@1 & ? 1 1 & ? 1 c@1 ^ c@2 ? 3 & c@2 ? 3 & ! ? 5 & ! ^ ? 4 1 & ! ^ ? 2 0 & ! ^ ? 0 0 1
# safety formula [](...)
## Variable limits: 2<=c<=8
| ! c@2 | ! c@1 | ! c@0.2.8 0

[ENV_LIVENESS]
| ! a $ 1 & ! b@3 & ! b@2 & ! ^ b@1 1 & ! ^ b@0.0.10 1 1

[SYS_LIVENESS]
d
$ 7 ^ 0 c@0.2.8 & 0 c@0.2.8 ^ ^ 1 c@1 ? 1 | | & 1 c@1 & ? 1 1 & ? 1 c@1 ^ c@2 ? 3 & c@2 ? 3 & ! ? 5 & ! ? 4 & ! ^ ? 2 1 & ! ^ ? 0 0 1
```

Some notes about this translation are given in the next section.


# Translation from structured `slugs` to `slugsin`

This section describes some of the conventions used by the function `slugs.convert_to_slugsin` under `tools/StructuredSlugsParser/`.

Each integer variable can be represented in binary format using one Boolean variable for each bit in the binary representation.
The number of Boolean variables needed is logarithmic in the range of the integer variable, rounded to the smallest following integer.

To aid with the inversion of this mapping, the function `slugs.convert_to_slugsin` names the auxiliary Boolean variables according to the following convention.
The integer variable `b: 0...10` can take the values: `0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10`.
The smallest power of 2 that is larger than `max(b) = 10` is `2 ** 4 = 16`.
So variable `b` is stored in a bitfield of size 4.
Each bit of the bitfield is represented with a Boolean variable.
The names of the 4 Boolean variables needed are:

```
b@0.0.10
b@1
b@2
b@3
```

The convention is [little endian](https://en.wikipedia.org/wiki/Endianness#Little-endian):

  - the [least significant bit](https://en.wikipedia.org/wiki/Least_significant_bit) is `"varname"'@0.'u'.'M`.
    It is numbered with 0 because of the little-endian convention, so this is the power of 2 it corresponds to.
    `u` is the maximal and `M` the minimal value in the integer variable's domain.
    In this example it is `b@0.0.10`

  - the other bits are simply numbered by their position (equiv. corresponding power of 2): `"varname"'@'N`.
    In this example they are `b@1, b@2, b@3`.

So the assignment:

```
b@0.0.10 = 1
b@1 = 1
b@2 = 0
b@3 = 0
```

is the number `3`.
