---
title: "Resolve lambda init-capture pack grammar (CWG2378)"
document: D2095R0
date: today
audience: CWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: false
---

# Introduction

When [@P0780R2] was adopted in Jacksonville in 2018, the paper contained a grammar
rule that was inconsistent with other uses of packs. As pointed out by Richard
Smith a few hours after plenary [@Smith.Core], the grammar requires a pack of
references to be captured like:

```cpp
[...&x=init]
```

rather than how we write packs of references everywhere else.

```cpp
[&...x=init]
```

This is now [@CWG2378].

As of this writing, clang implements the Core Issue direction (with the `&`
preceeding the `...`) and gcc implements the standard wording. I opened a gcc
bug for this [@gcc.91847], but we should really fix the wording asap.

This paper exists to do that.

# Wording

Change the grammar in [expr.prim.lambda.capture]{.sref}:

::: bq
```diff
  @_capture_@:
-	  @_simple-capture_ [`...`{.x}~opt~]{.diffdel}@
-	  @[`...`{.x}~opt~]{.diffdel} _init-capture_@
+	  @_simple-capture_@
+	  @_init-capture_@

  @_simple-capture_@:
-	  @_identifier_@
-	  & @_identifier_@
+	  @_identifier_ [`...`{.x}~opt~]{.diffins}@
+	  & @_identifier_ [`...`{.x}~opt~]{.diffins}@
	  this
	  * this

  @_init-capture_@:
-	  @_identifier_ _initializer_@
-	  & @_identifier_ _initializer_@
+	  @[`...`{.x}~opt~]{.diffins} _identifier_ _initializer_@
+	  & @[`...`{.x}~opt~]{.diffins} _identifier_ _initializer_@
```
:::

Change [expr.prim.lambda.capture]{.sref}/2:

::: bq
If a _lambda-capture_ includes a _capture-default_ that is `=`, each
_simple-capture_ of that _lambda-capture_ shall be of the form
“`&`{.x} [`...`{.x}~opt~]{.addu} _identifier_”, “`this`”, or “`* this`”.
:::

Change [expr.prim.lambda.capture]{.sref}/6:

::: bq
An _init-capture_ [without ellipsis]{.addu} behaves as if it declares and
explicitly captures a variable of the form “`auto init-capture ;`{.x}”
whose declarative region is the _lambda-expression_'s compound-statement, except that: 
::: 

Change [expr.prim.lambda.capture]{.sref}/17:

::: bq
[17]{.pnum} A _simple-capture_ [followed by]{.rm} [containing]{.addu} an ellipsis is a pack expansion ([temp.variadic]).
An _init-capture_ [preceded by]{.rm} [containing]{.addu} an ellipsis is a pack expansion
that introduces an _init-capture_ pack ([temp.variadic])
whose declarative region is the _lambda-expression_'s _compound-statement_.
:::

Change [temp.variadic]{.sref}/5.10:

::: bq
[5]{.pnum} [...] Pack expansions can occur in the following contexts:

- [5.10]{.pnum} In a _capture-list_ ([expr.prim.lambda]); the pattern is [a _capture_]{.rm} [the _capture_ without the ellipses]{.addu}.
:::

---
references:
  - id: Smith.Core
    citation-label: Smith.Core
    title: "p0780r2 has wrong grammar for reference init-capture packs"
    author:
      - family: Richard Smith
    issued:
      - year: 2018
    URL: https://lists.isocpp.org/core/2018/03/4095.php
  - id: gcc.91847
    citation-label: gcc.91847
    title: "init-capture pack of references requires ... on wrong side"
    author:
      - family: Barry Revzin
    issued:
      - year: 2019
    URL: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=91847  
---
