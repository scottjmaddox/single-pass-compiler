# Single-Pass Language (`.spl`) Type System

## Notation and Axioms

$\newcommand{\subtype}{\sqsubseteq}$
$\newcommand{\vals}{\mathord{\operatorname{.vals}}}$
$\newcommand{\ops}{\mathord{\operatorname{.ops}}}$
$\newcommand{\type}{\mathord{\operatorname{.type}}}$

### Types and Sets

- $x: T$, read as "x of type T", denotes that the variable $x$ has type $T$.
- $x = v$, read as "x assigned to v", denotes assignment of value $v$ to variable $x$.
- $x: T = v$, read as "x of type T assigned to v", combines the two notations above.
- $e \in \{ 0, 1 \}$, read as "e element of set $0$, $1$", denotes that the expression $e$ is an element of the set containing $0$ and $1$.
- $\{ 0 \} \subseteq \{ 0 \}$, read as "set $0$ subset set $0$", denotes that the set containing $0$ is a (non-strict) subset of the set containing $0$.
- $\{ 0 \} \supseteq \{ 0 \}$, read as "set $0$ superset set $0$", denotes that the set containing $0$ is a (non-strict) superset of the set containing $0$.
- $\{ 0, 1 \} \not\subseteq \{ 0 \}$, read as "set $0$, $1$ not subset set $0$", denotes that the set containing $0$ and $1$ is *not* a subset of the set containing $0$.

### Integer Types

- $\mathtt{u0} ... \mathtt{u64}$ are unsigned binary integer types with $\mathtt{0} ... \mathtt{64}$ bits, respectively.
- $\mathtt{i1} ... \mathtt{u64}$ are two's complement signed binary integer types with $\mathtt{1} ... \mathtt{64}$ bits, respectively.
- $e \in \mathbb{N}$, read as "e (is an) element of the set of natural numbers". The set of natural numbers is the set of all non-negative integers.
- $e \in \mathbb{N}_+$, read as "e (is an) element of the set of positive natural numbers". Unlike $\mathbb{N}$, $\mathbb{N}_+$ does not include zero.
- $\mathtt{u}x$ is an unsigned binary integer type with an unknown number of bits represented by the variable $x \in \mathbb{N}$.
- $\mathtt{i}x$ is a two's complement signed binary integer type with an unknown number of bits represented by the variable $x \in \mathbb{N}_+$.

### Logical Connectives

- $A \lor B$, read as "A or B", denotes [logical disjunction](https://en.wikipedia.org/wiki/Logical_disjunction) of the propositions $A$ and $B$.
- $A \land B$, read as "A and B", denotes [logical conjuction](https://en.wikipedia.org/wiki/Logical_conjunction) of the propositions $A$ and $B$.
- $A \implies B$, read as "A implies B", denotes [logical consequence](https://en.wikipedia.org/wiki/Logical_consequence) from the proposition $A$ to the proposition $B$.
- $A \iff B$, read as "$A$ if and only if $B$", denotes a [logical biconditional](https://en.wikipedia.org/wiki/Logical_biconditional) between the propositions $A$ and $B$.

### Meta-properties

- $\alpha\vals$, read as "alpha's vals", is the set of values that the expression or type $\alpha$ can take or represent.
- $\alpha\ops$, read as "alpha's ops", is the set of operations that the expression or type $\alpha$ supports.
- $x\type$, read as "x's type", is the type of the variable $x$.

### Subtyping

- $\alpha \subtype T$, read as "alpha subtype T", denotes that the expression or type $\alpha$ is a subtype of the type $T$, which is true if and only if every value that $\alpha$ can take or represent is also representable by $T$ and $\alpha$ supports all the operations that $T$ supports. Note that this extends the operation-only definition of subtyping with an additional value subset requirement, which is reversed from the operation superset requirement. In notational form:
  - $\alpha \subtype T \iff \alpha\vals \subseteq T\vals \land \alpha\ops \supseteq T\ops \quad (\text{Axiom 1})$
    - read as "alpha subtype T if and only if alpha's vals are a subset of T's vals and alpha's ops are a superset of T's ops"
- $\alpha \not\subtype T$, read as "alpha *not* subtype T", denotes that the proposition $\alpha \subtype T$ is false.

### Well Typedness

- $\Gamma \vdash \tau$, read as "Gamma proves tau is well typed", denotes that the context $\Gamma$ proves that the type-annotated expression $\tau$ is well typed.
- $x\type \subtype T \vdash y: T = x \quad (\text{Axiom 2})$
  - read as "x's type subtype T proves y of type T assigned to x is well typed"

### Proofs

- $\blacksquare$ denotes the end of a proof (Q.E.D.).

## Integer Subtyping

- $\mathtt{u}x\ops \supseteq \mathtt{u}y\ops \iff x \leq y \quad \text{(Theorem 1)}$
  - Proof:
  - All unsigned binary integer types support all arithemetic, comparison, bitwise, and logical operators and `if` conditioning.
  - $\mathtt{u}x \subtype \mathtt{u}y \iff x \leq y \vdash a: \mathtt{u}y = (b: \mathtt{u}x) \iff x \leq y \quad \text{by Theorem 2 and Axiom 2}$
  - $\implies \mathtt{u}x$ supports all remaining operations (function/method calls) that $\mathtt{u}y$ supports
  - $\implies \mathtt{u}x\ops \supseteq \mathtt{u}y\ops \iff x \leq y$
  - $\blacksquare$

- $\mathtt{u}x \subtype \mathtt{u}y \iff x \leq y \quad \text{(Theorem 2)}$
  - Proof:
  - $\mathtt{u}x\vals \equiv \{ 0 ... (2^x - 1) \}$
  - $\mathtt{u}y\vals \equiv \{ 0 ... (2^y - 1) \}$
  - $\mathtt{u}x\vals \subseteq \mathtt{u}y\vals \iff \{ 0 ... (2^x - 1) \} \subseteq \{ 0 ... (2^y - 1) \} \iff x \leq y$
  - $\mathtt{u}x\ops \supseteq \mathtt{u}y\ops \iff x \leq y \quad \text{by Theorem 1}$
  - $\implies \mathtt{u}x \subtype \mathtt{u}y \iff x \leq y \quad \text{by Axiom 1}$
  - $\blacksquare$

- $\mathtt{i}x\ops \supseteq \mathtt{i}y\ops \iff x \leq y \quad \text{(Theorem 3)}$
  - Proof:
  - All two's complement signed binary integer types support all arithemetic, comparison, bitwise, and logical operators and `if` conditioning.
  - $\mathtt{i}x \subtype \mathtt{i}y \iff x \leq y \vdash a: \mathtt{i}y = (b: \mathtt{i}x) \iff x \leq y \quad \text{by Theorem 4 and Axiom 2}$
  - $\implies \mathtt{i}x$ supports all remaining operations (function/method calls) that $\mathtt{i}y$ supports
  - $\implies \mathtt{i}x\ops \supseteq \mathtt{i}y\ops \iff x \leq y$
  - $\blacksquare$

- $\mathtt{i}x \subtype \mathtt{i}y \iff x \leq y \quad \text{(Theorem 4)}$
  - Proof:
  - $\mathtt{i}x\vals \equiv \{ -2^{x-1} ... (2^{x-1} - 1) \}$
  - $\mathtt{i}y\vals \equiv \{ -2^{y-1} ... (2^{y-1} - 1) \}$
  - $\mathtt{i}x\vals \subseteq \mathtt{i}y\vals \iff \{ -2^{x-1} ... (2^{x-1} - 1) \} \subseteq \{ -2^{y-1} ... (2^{y-1} - 1) \} \iff x \leq y$
  - $\mathtt{i}x\ops \supseteq \mathtt{i}y\ops \iff x \leq y \quad \text{by Theorem 3}$
  - $\implies \mathtt{i}x \subtype \mathtt{i}y \iff x \leq y \quad \text{by Axiom 1}$
  - $\blacksquare$

- $\mathtt{u}x\ops \supseteq \mathtt{i}y\ops \iff x \lt y \quad \text{(Theorem 5)}$
  - Proof:
  - All unsigned and two's complement signed binary integer types support all arithemetic, comparison, bitwise, and logical operators and `if` conditioning.
  - $\mathtt{u}x \subtype \mathtt{i}y \iff x \lt y \vdash a: \mathtt{i}y = (b: \mathtt{u}x) \iff x \lt y \quad \text{by Theorem 6 and Axiom 2}$
  - $\implies \mathtt{u}x$ supports all remaining operations (function/method calls) that $\mathtt{i}y$ supports
  - $\implies \mathtt{u}x\ops \supseteq \mathtt{i}y\ops \iff x \leq y$
  - $\blacksquare$

- $\mathtt{u}x \subtype \mathtt{i}y \iff x \lt y \quad \text{(Theorem 6)}$
  - Proof:
  - $\mathtt{u}x\vals \equiv \{ 0 ... (2^x - 1) \}$
  - $\mathtt{i}y\vals \equiv \{ -2^{y-1} ... (2^{y-1} - 1) \}$
  - $\mathtt{u}x\vals \subseteq \mathtt{i}y\vals \iff \{ 0 ... (2^x - 1) \} \subseteq \{ -2^{y-1} ... (2^{y-1} - 1) \}$
  - $\iff 2^x - 1 \leq 2^{y-1} - 1 \iff x \leq y-1 \iff x \lt y$
  - $\mathtt{u}x\ops \supseteq \mathtt{i}y\ops \iff x \lt y \quad \text{by Theorem 5}$
  - $\implies \mathtt{u}x \subtype \mathtt{i}y \iff x \lt y \quad \text{by Axiom 1}$
  - $\blacksquare$

- $\mathtt{i}x \not\subtype \mathtt{u}y$
  - Proof:
  - $\mathtt{i}x\vals \equiv \{ -2^{x-1} ... (2^{x-1} - 1) \}$
  - $\mathtt{u}y\vals \equiv \{ 0 ... (2^y - 1) \}$
  - $\{ -2^{x-1} ... (2^{x-1} - 1) \} \not\subseteq \{ 0 ... (2^y - 1) \}$
  - $\implies \mathtt{i}x \not\subtype \mathtt{u}y$
  - $\blacksquare$

### Examples

- $\mathtt{u0} \subtype \mathtt{u1}$
  - $\mathtt{u0}\vals \equiv \{ 0 \}$
  - $\mathtt{u1}\vals \equiv \{ 0, 1 \}$
  - $\{ 0 \} \subseteq \{ 0, 1 \}$
  - $\implies \mathtt{u0} \subtype \mathtt{u1}$

- $\mathtt{u1} \subtype \mathtt{u1}$
  - $\mathtt{u1}\vals \equiv \{ 0, 1 \}$
  - $\{ 0, 1 \} \subseteq \{ 0, 1 \}$
  - $\implies \mathtt{u1} \subtype \mathtt{u1}$

- $\mathtt{i0} \subtype \mathtt{i1}$
  - $\mathtt{i0}\vals \equiv \{ 0 \}$
  - $\mathtt{i1}\vals \equiv \{ -1, 0 \}$
  - $\{ 0 \} \subseteq \{ -1, 0 \}$
  - $\implies \mathtt{i0} \subtype \mathtt{i1}$

- $\mathtt{i1} \subtype \mathtt{i1}$
  - $\mathtt{i1}\vals \equiv \{ -1, 0 \}$
  - $\{ -1, 0 \} \subseteq \{ -1, 0 \}$
  - $\implies \mathtt{i1} \subtype \mathtt{i1}$

- $\mathtt{u0} \subtype \mathtt{i1}$
  - $\mathtt{u0}\vals \equiv \{ 0 \}$
  - $\mathtt{i1}\vals \equiv \{ -1, 0 \}$
  - $\{ 0 \} \subseteq \{ -1, 0 \}$
  - $\implies \mathtt{u0} \subtype \mathtt{i1}$

- $\mathtt{u1} \subtype \mathtt{i2}$
  - $\mathtt{u1}\vals \equiv \{ 0, 1 \}$
  - $\mathtt{i2}\vals \equiv \{ -2, -1, 0, 1 \}$
  - $\{ 0, 1 \} \subseteq \{ -2, -1, 0, 1 \}$
  - $\implies \mathtt{u1} \subtype \mathtt{i2}$

---

<div style="text-align: center; font-size: 0.9em; color: #555;">
Copyright © 2025 by <a href="https://github.com/scottjmaddox/single-pass-compiler">Scott J Maddox</a>. All rights reserved.
</div>
