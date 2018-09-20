Title: When do you actually use `<=>`?
Document-Number: DxxxxRx
Authors: Barry Revzin, barry dot revzin at gmail dot com
Audience: EWG

# Motivation

[P0515](https://wg21.link/p0515r3) introduced `operator<=>` as a way of generating all six comparison operators from a single function. But the spaceship operator has some usability issues that still need to be addressed.

## Doesn't quite work in generic code

Following from first principles, it would appear that the way you would implement `<=>` for a type like `optional<T>` would look like (writing as a non-member function for clarity):

    :::cpp
    template <typename T>
    auto operator<=>(optional<T> const& lhs, optional<T> const& rhs)
        -> decltype(*lhs <=> *rhs)
    {
        if (lhs.has_value() && rhs.has_value()) {
            return *lhs <=> *rhs;
        } else {
            return lhs.has_value() <=> rhs.has_value();
        }
    }

This is a clean and elegant way of implementing this functionality.
    
But it is wrong.

## Doesn't quite work in non-generic code

One of the big selling features of `<=>` was the ability to simply default it for types that just want to do normal member-by-member lexicographical comparison. 

So it seems like this should work:

    :::cpp
    // some perfectly functional C++17 type that implements a total order
    struct Ordered {
        bool operator==(Ordered const&) const { ... }
        bool operator!=(Ordered const&) const { ... }
        bool operator<(Ordered const&) const { ... }
        bool operator<=(Ordered const&) const { ... }
        bool operator>(Ordered const&) const { ... }
        bool operator>=(Ordered const&) const { ... }
    };
    
    struct Y {
        int i;
        char c;
        Ordered o;
        
        auto operator<=>(Y const&) const = default;
    };

But this doesn't even compile.

## ... Why not?

The problem here is, not all types implement `<=>`. Indeed, at this moment, only the fundamental types do. So writing any sort of code that relies on the existence of `<=>` is highly limited in its functionality. 

The provided implementation of `<=>` for `optional` relies on the existence of `<=>` for `T`. As a result, while it works great for `optional<int>`, it would not be a viable candidate for `optional<Ordered>`. Likewise, in order to default the implementation of `<=>` for `Y`, we need each to perform <code>x<sub>i</sub> <span class="token operator"><=></span> y<sub>i</sub></code> for each member (see [\[class.spaceship\]][class.spaceship]), but as above, `Ordered` does not implement `<=>`, so this is ill-formed.

What we have to do instead is to use a library function which was also introduced in P0515 but adopted by way of [P0768R1](https://wg21.link/p0768r1): [`std::compare_3way()`][alg.3way]. What this function does is add more fallback implementations, in a way best illustrated by this skeleton:

    :::cpp
    template<class T, class U>
    auto compare_3way(const T& a, const U& b) {
        if constexpr (/* can invoke a <=> b */)
            return a <=> b;
        else if constexpr (/* can invoke a<b and a==b */)
            return a==b ? strong_ordering::equal : a<b ? strong_ordering::less : strong_ordering::greater;
        else if constexpr (/* can invoke a==b */)
            return a == b ? strong_equality::equal : strong_equality::unequal;
        else
            /* ill-formed, defined as deleted */
    }

That is, this is an extra library function that lets us use three-way comparisons with types that haven't yet opted into this new language feature. Since the typical case is that types won't have `<=>` implemented, we basically want to make sure that we use `compare_3way()` for the functionality that we need. In other words, the way we want to implement `<=>` for `optional<T>` and for `Y` is:

    :::cpp hl_lines="8,18,19"
    template <typename T>
    auto operator<=>(optional<T> const& lhs, optional<T> const& rhs)
        -> decltype(compare_3way(*lhs, *rhs))
    {
        if (lhs.has_value() && rhs.has_value()) {
            return compare_3way(*lhs, *rhs);
        } else {
            return compare_3way(lhs.has_value(), rhs.has_value());
        }
    }
    
    struct Y {
        int i;
        char c;
        Ordered o;
        
        strong_ordering operator<=>(Y const& rhs) const {
            if (auto cmp = compare_3way(i, rhs.i); cmp != 0) return cmp;
            if (auto cmp = compare_3way(c, rhs.c); cmp != 0) return cmp;
            return compare_3way(o, rhs.o);
        }
    };    

Now, there are places in the above code where I _could_ have used `<=>`. I know for a fact that it works for `bool`, `int`, and `char` - so the highlighted lines could have used `<=>` instead of `compare_3way`. But that seems to add so much more cognitive load on the programmer - you have to keep track of what happens to use `<=>` and what doesn't. Just writing `compare_3way` will call `<=>` for you anyway, so why not just always use it?

This is a nuisance for the `optional` implementation, but is arguably worse than that for types like `Y` above - which merely want to opt-in to default semantics. Not only do I have to manually list the implementation, but note that I also have to manually specify the return type! I cannot rely on `auto` to just determine the correct comparison category for me!

Which of course begs the question...

## When do you actually write `<=>`?

P0515 states:

> `<=>` is for type implementers: User code (including generic code) outside the implementation of an `operator<=>` should almost never invoke an `<=>` directly (as already discovered as a good practice in other languages); for example, code that wants to test `a<b` should just write that, not `a<=>b < 0`.

I absolutely agree with the intent of this statement - user code should use binary operators. But as illustrated above, type implementers can't use `<=>` either! At least, not until every type the implements an ordering does so by way of `<=>`. Which is to say, never.

Effectively, there are exactly two places that can correctly use `<=>`:

1. The compiler, which would transform expressions like `a<b` into `a<=>b < 0`.
2. The implementation of `std::compare_3way()`.

That seems a waste of a perfectly good token that we have specially reserved for this occasion. `<=>` just seems like a much better spelling than `compare_3way`, especially given its ability to be used as an infix operator.

# Proposal

This paper proposes to move the entirety of the logic of `compare_3way()` into the specification of `operator<=>()`. In other words, ignoring parameter inversion, `a <=> b` shall mean:

1. Lookup `a.operator<=>(b)` and `operator<=>(a, b)` as usual for binary operators. If a viable candidate is found, we are done.
2. Otherwise, if `a == b` and `a < b` are each well-formed and convertible to `bool`, then the expression has type `strong_ordering` with value `(a == b) ? strong_ordering::equal : ((a < b) ? strong_ordering::less : strong_ordering::greater)`
3. Otherwise, if `a == b` is well-formed and convertible to `bool`, then the expression has type `strong_equality` with value `(a == b) ? strong_equality::equal : strong_equality::nonequal`.
4. Otherwise, the expression is ill-formed

This paper additionally proposes to turn `compare_3way` into a function object class that simply invokes `<=>` on its two arguments, along the lines of `less`.

With this change, we would actually be able to use `<=>` in all the places where we want to use `<=>` today but cannot. We would also be able to default the implementation of `<=>` instead of having to manually implement what should be the defaulted implementation.

## Examples

<table style="width:100%">
<tr>
<th style="width:50%">
Today (P0515/P0768/C++2a)
</th>
<th style="width:50%">
Proposed
</th>
</tr>
<tr>
<td>
    :::cpp
    template <typename T>
    auto operator<=>(optional<T> const& lhs,
            optional<T> const& rhs)
        -> decltype(compare_3way(*lhs, *rhs))
    {
        if (lhs.has_value() && rhs.has_value()) {
            return compare_3way(*lhs, *rhs);
        } else {
            return compare_3way(lhs.has_value(),
                rhs.has_value());
        }
    }
</td>
<td>
    :::cpp
    template <typename T>
    auto operator<=>(optional<T> const& lhs,
            optional<T> const& rhs)
        -> decltype(*lhs <=> *rhs)
    {
        if (lhs.has_value() && rhs.has_value()) {
            return *lhs <=> *rhs;
        } else {
            return lhs.has_value() <=>
                rhs.has_value();
        }
    }
</td>
</tr>
<tr>
<td>
    :::cpp
    struct Y {
        int i;
        char c;
        Ordered o;
        
        strong_ordering operator<=>(Y const& rhs) const {
            if (auto cmp = compare_3way(i, rhs.i); cmp != 0) return cmp;
            if (auto cmp = compare_3way(c, rhs.c); cmp != 0) return cmp;
            return compare_3way(o, rhs.o);
        }
    };
</td>
<td>
    :::cpp
    struct Y {
        int i;
        char c;
        Ordered o;
        
        auto operator<=>(Y const&) const = default;
    };
</td>
</tr>    
</table>

# Counter-arguments

There are two counter-arguments I'm aware of to this proposal. What follows is an accounting of those arguments and my responses to them. 

## The initial premise is false: `optional<T>` shouldn't always have `<=>`

> `optional<T>` only has `<` if `T` has `<`, so `optional<T>` should only have `<=>` if `T` has `<=>`. In other words, the initial provided implementation is the correct implementation and the suggested one using `compare_3way()` is incorrect.

There isn't anything particularly specific to `optional` in this argument, so we can extend this to: Any compound type should have `<=>` only if all of its constituents have `<=>`.

This argument sounds seductive, and offers consistency with the way we write other operators. But it has some serious problems. 

To start with, the implication here is that `optional<T>` (and by extension every compound type, and really by extension every type) would need now to conditionally implement _seven_ operators (all six preexisting comparisons, plus `<=>`) instead of the advertised _one_ operator. As a result, we lose a big advantage from `<=>`: the ability to write less code.

The consequence of writing all seven operators is that it makes `<=>` completely useless. User code is not supposed to directly invoke `<=>` - user code is supposed to just use the comparison it needs. The idea of `<=>` is we simplify code by having `a < b` translate to `a <=> b < 0`. But if we always have all seven operators, this translation will never happen, since `operator<` exists and would be preferred. The only code that would actually invoke `<=>` is the code that directly uses `<=>` - and that code only exists in other implementations of `<=>`. At this point, it may as well not exist at all. 

As if that wasn't enough, we would also lose any ability to improve performance with three-way comparisons. For many types, `<=>` can be better than consecutive calls to `==` and `<`. Now, consider `pair<T, T>`. If we're only providing `<=>` for such a type if `T` has `<=>`, that means we must still be providing `<` if `T` has `<`. If `T` has `<=>`, `T` has `<`, which means that our `pair<T,T>` provides everything. 

Now suppose `T` has an efficient `<=>`, `pair<T, T>`'s `operator<` can't use it! Which means that the expression `p1 < p2`, instead of doing two calls to `T`'s efficient `<=>` has to instead do a call to `==` and then `<` twice. That could be a serious pessimization!

Thus, `optional<T>` needs to provide `<=>` if `T` is comparable at all - not simply if `T` implements `<=>`. 


## Unintentional comparison category strengthening

> When a class author implements `<=>` for their type, they have to decide what comparison category to use as the return type. Other code could use that choice to make important decisions. But if we had `<=>` fallback to `compare_3way()`, we effectively are guessing what the intended comparison category was. `decltype(x <=> y)` might be a deliberate choice of the class author - or it might be compiler inference, and we can't tell. 

> Moreover, we might get this wrong in a very confusing way that inadvertently strengthens the comparison category. For example, the following code is well-formed:
>
    :::cpp
    bool foo(error_code const& a, error_condition const& b) {
        return a == b;
    }

> because there exists an equality comparison between these [two types][syserr.compare]. These two types mean rather different things and obviously are not substitutible, but the proposed changed would nevertheless give `decltype(a <=> b)` the type `strong_equality` and that is a very misleading and strongly undesired result. 

I have two responses to this argument. 

First, practically speaking there is no difference between using `a <=> b` in this context and getting `strong_equality` and using `compare_3way(a, b)` in this context and getting `strong_equality`. The end result is the same - equally misleading, and the added weight on the meaning of `<=>` here is a distinction without a difference. In today's world, people will write `<=>` when they need a three-way comparison, find that it doesn't work, and then switch to `compare_3way()` - because `compare_3way()` solves a problem. I'm unconvinced that _specifically_ `<=>` yielding a misleading response is a poblem.

Second, the fact that `compare_3way(a, b)` today and `a <=> b` with this proposal yields `strong_equality` is a problem - but it's not `<=>`'s problem, it's `error_code`'s problem. The decision to use `==` in this context is arguably a bad design decision. With the direction the standard library is going and the new concepts in Ranges in [P0898](https://wg21.link/p0898r3) (both terminology concepts and language `concept`s), we are adding semantic requirements on top of syntactic ones - and the lack of substitutibility between `error_code` and `error_condition` means that we're meeting the syntactic but not the semantic requirements of `EqualityComparableWith`. This is bad.

Jonathan Müller recently wrote an in-depth series of blog posts on the [mathematics behind comparisons][muller.compare] which makes the compelling argument that the existence of `==` should be `strong_equality` only, and the existence of `==` and `<` should be `strong_ordering` only. Any other required comparison category should be a named function instead. This argument is very much in line with the principles behind Ranges. 

The conclusion of this is that yes, there is a problem. But the problem lies with `error_code` and `error_condition` for violating expectations with the decision to improperly use `==` when it doesn't mean equality and substitutibility. Having `<=>` fall back to guessing at the comparison category and yielding `strong_ordering` or `strong_equality` works for types that follow good design guidance in their choice of using operators. 

Note that this problem could easily be fixed by replacing the currently existing `==` and `!=` for these types with a `<=>` returning `weak_equality`. This is still a questionable choice, but at least you would observe the correct comparison category without otherwise breaking user code. Ideally, `==` and `!=` get replaced with a named function that itself returns `weak_equality`.

This leaves the question of whether or not `<=>` was explicitly provided or inferred. In practice, I think this is about as relevant as whether or not the copy constructor was explicit or compiler generated. As long as it has sane semantics. However, if people feel strongly about wanting this particular piece of information, if we're already adding language magic to have `<=>` perform multiple possible operations, it is surely possible to add language magic to directly retrieve the type of the actual binary `<=>` operation if and only if it exists. 
    
[class.spaceship]: http://eel.is/c++draft/class.spaceship "[class.spaceship]"
[alg.3way]: http://eel.is/c++draft/alg.3way "[alg.3way]"
[syserr.compare]: http://eel.is/c++draft/syserr.compare "[syserr.compare]"
[muller.compare]: https://foonathan.net/blog/2018/09/07/three-way-comparison.html "foonathan::blog() - Mathematics behind Mathematics behind Comparison #4: Three-Way Comparison||Jonathan Müller||2018-09-07"