Title: An Annex C entry for ==
Subtitle: Or how I learned to stop worrying and embrace symmetry
Document-Number: D1630R0
Authors: Barry Revzin, barry dot revzin at gmail dot com
Audience: CWG, EWG

# Introduction

The introduction of `operator<=>` into the language ([P0515R3](https://wg21.link/p0515R3) with relevant extension [P0905R1](https://wg21.link/p0905r1)) added a novel aspect to name lookup: candidate functions can now include both candidates with different names and a reversed order of arguments. The expression `a < b` used to always only find candidates like `operator<(a, b)` and `a.operator<(b)` now also finds `(a <=> b) < 0` and `0 < (b <=> a)`. This change makes it much easier to write comparisons - since you only need to write the one `operator<=>`.

However, that ended up being insufficient due to the problems pointed out in [P1190](https://wg21.link/p1190R0), and in response [P1185R2](https://wg21.link/p1185R2) was adopted in Kona which made the following changes:

1. Changing candidate sets for equality and inequality  
  a. `<=>` is no longer a candidate for either equality or inequality  
  b. `==` gains `<=>`'s ability for both reversed and rewritten candidates  
2. Defaulted `==` does memberwise equality, defaulted `!=` invokes `==` instead of `<=>`.  
3. Strong structural equality is defined in terms of `==` instead of `<=>`  
4. Defaulted `<=>` can also implicitly declare defaulted `==`

This paper concerns itself specifically with 1b. While the new rewritten and reversed candidate rules for `operator<=>` cannot change or break existing code (none of which can possibly have an `operator<=>`, except for the builtin types which all have the other comparison operators anyway), that is not the case for `operator==`, which is in plentiful use today.

Consider the following example, courtesy of Tomasz Kamiński in [CWG 2407][CWG2407] (note that the use of `int` is not important, simply that we have two types, one of which is implicitly convertible to the other):

    :::cpp
    struct A {
      operator int() const;
    };

    bool operator==(A, int);              // #1
    // builtin bool operator==(int, int); // #2
    // builtin bool operator!=(int, int); // #3

    int check(A x, A y) {
      return (x == y) +  // In C++17, calls #1; in C++20, ambiguous between #1 and reversed #1
        (10 == x) +      // In C++17, calls #2; in C++20, calls #1
        (10 != x);       // In C++17, calls #3; in C++20, calls #1
    }    

There are two kinds of issues this example brings up: code that changes which function gets called, and code that becomes ambiguous. I'll cover both separately.

## Changing the result of overload resolution

The expression `10 == x` in C++17 had only one viable candidate: `operator==(int, int)`, converting the `A` to an `int`. But in C++20, due to P1185, equality and inequality get reversed candidates as well. Since equality is symmetric, `10 == x` is an equivalent expression to `x == 10`, and we consider both forms. This gives us two candidates:

    :::cpp
    bool operator==(A, int);   // #1
    bool operator==(int, int); // #2 (builtin)
    
The first is an Exact Match (once the arguments are reversed), whereas the second requires a Conversion, so the first is the best viable candidate. 

Silently changing which function gets executed is facially the worst thing we can do, but in this particular situation doesn't seem that bad. We're already in a situation where, in C++17, `x == 10` and `10 == x` invoke different kinds of functions (the former invokes a user-defined function, the latter a builtin) and if those two give different answers, that seems like an inherently questionable program. 

The inequality expression behaves the same way. In C++17, `10 != x` had only one viable candidate: the `operator!=(int, int)` builtin, but in C++20 also acquires the reversed and rewritten candidate `(x == 10) ? false : true`, which would be an Exact Match. Here, the status quo was that `x != 10` and `10 != x` both invoke the same function - but again, if that function gave a different answer from `!(x == 10)` or `!(10 == x)`, that seems suspect. 

## Code that becomes ambiguous

The homogeneous comparison is more interesting. `x == y` in C++17 had only one candidate: `operator==(A, int)`, converting `y` to an `int`. But in C++20, it now has two:

    :::cpp
    bool operator==(A, int); // #1
    bool operator==(int, A); // #1 reversed

The first candidate has an Exact Match in the 1st argument and a Conversion in the 2nd, the second candidate has a Conversion in the 1st argument and an Exact Match in the 2nd. While we do have a tiebreaker to choose the non-reversed candidate over the reversed candidate ([\[over.match.best\]/2.9](http://eel.is/c++draft/over.match.best#2.9)), that only happens when each argument's conversion sequence _is not worse than_ the other candidates' ([\[over.match.best\]/2](http://eel.is/c++draft/over.match.best#2))... and that's just not the case here. We have one better sequence and one worse sequence, each way.

As a result, this becomes ambiguous.

Note that the same thing can happen with `<=>` in a similar situation:

    :::cpp
    struct C {
        operator int() const;
        strong_ordering operator<=>(int) const;
    };
    
    C{} <=> C{}; // error: ambiguous
    
But in this case, it's completely new code which is ambiguous - rather than existing, functional code. 

# What do we do about it?

From my personal perspective, I don't consider the changing of overload resolution in these cases to be a problem. Just something that should be noted in Annex C.

The real issue is the homogeneous comparison case. What do we do about `x == y` breaking? We have a few options.

## Nothing

We could do nothing. Arguably, the model around comparisons is better in the working draft than it was in C++17. It does make sense for `x == y` to be ambiguous. We're also now in a position where it's simply much easier to write comparisons for types - we no longer have to live in this world where everybody only declares `operator<` for their types and then everybody writes algorithms that pretend that only `<` exists. Or, more relevantly, where everybody only declares `operator==` for their types and nobody uses `!=`. This is a Good Thing. 

From the perspective of viewing equality as symmetric, having `x == y` become ambiguous makes perfect sense. There are two perfectly valid ways of interpreting the expression `x == y`: `x == static_cast<int>(y)` and `y == static_cast<int>(x)`. Which was intended?

But this perspective has only been part of the Working Draft for a few weeks and such code could have existed for years.

## Carve out an exception to overload resolution for this case

We could add some trailing rule such that if a first round of overload resolution ends up being ambiguous, we try again removing the reversed (but not rewritten) candidates. It's only the reversed candidates that give this criss-cross ambiguity.

Such a language change would mean that the first round of overload resolution for `x == y` fails (with the same ambiguity as before) but the second one succeeds because the only candidate would be forward candidate `operator==(A, int)`. It would not change the behavior of either of the other two expressions in the original example: `10 == x` and `10 != x` still invoke `#1` instead of `#2` or `#3`.

This has the advantage of not breaking any code, but has the disadvantage of adding additional complexity to overload resolution in a way that doesn't seem particularly sound. Changing overload resolution is scary. See: this paper. 

## Back out the reversed and rewritten equality candidates

We could back out part 1b of P1185R2 entirely, which would end up changing none of the behavior of the original example. I think this would be an unfortunate choice, since we would lose a lot of convenience of not having to write purely boilerplate equality operators just to avoid a small, loud, and easy-to-fix breakage.

For heterogeneous comparison, being able to write one function - just `operator==(A, B)` - instead of four to cover each of `a == b`, `b == a`, `a != b`, and `b != a` is really nice. And a significant amount of diff for the wording paper to add `operator<=>` to library is really just removing a bunch of now-unnecessary `operator==`s and `operator!=`s ([P1614R0](https://wg21.link/p1614r0)). 

## Proposal

My proposal is that the status quo is the very best of the quos. Some code will fail to compile, that code can be easily fixed by adding either a homogeneous comparison operator or, if not that, doing an explicit conversion at the call sites. This lets us have the best language rules for the long future this language still has ahead of it.
    
# Wording

Add a new entry to [diff.cpp17.over]:

<blockquote><p><b>Affected subclause</b>: [over.match.oper]<br />
<b>Change:</b> Equality and inequality expressions can now find reversed and rewritten candidates.<br />
<b>Rationale:</b> Improve consistency of equality with spaceship and make it easier to write the full complement of equality operations.<br />
<b>Effect on original feature:</b> Equality and inequality expressions between two objects of different types, where one is convertible to the other, could change which operator is invoked. Equality and inequality expressions between two objects of the same type could become ambiguous.
<pre><code class="language-cpp">struct A {
  operator int() const;
};

bool operator==(A, int);              // #1
// builtin bool operator==(int, int); // #2
// builtin bool operator!=(int, int); // #3

int check(A x, A y) {
  return (x == y) +  // ill-formed; previously well-formed
    (10 == x) +      // calls #1, previously called #2
    (10 != x);       // calls #1, previously called #3
}</code></pre></blockquote>

[CWG2407]: http://wiki.edg.com/pub/Wg21cologne2019/CoreIssuesProcessingTeleconference2019-03-25/cwg_active.html#2407 "CWG 2407: Missing entry in Annex C for defaulted comparison operators||Tomasz Kamiński||Feb 26, 2019"