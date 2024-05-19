---
title: "Code Injection with Token Sequences"
document: P3294R0
date: today
audience: SG7, EWG
author:
    - name: Andrei Alexandrescu, NVIDIA
      email: <andrei@nvidia.com>
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
    - name: Daveed Vandevoorde
      email: <daveed@edg.com>
toc: true
---

# Introduction

This paper is proposing amending [@P2996R3] to add code injection in the form of token sequences.

We consider the motivation for this feature to some degree pretty obvious, so we will not repeat it here, since there are plenty of other things to cover here. Instead we encourage readers to read some other papers on the topic ([@P0707R4], [@P0712R0], [@P1717R0], [@P2237R0]).

# A Comparison of Injection Models

There are a lot of things that make code injection in C++ difficult, and the most important problem to solve first is: what will the actual injection mechanism look like? Not its syntax specifically, but what is the shape of the API that we want to expose to users? We hope in this section to do a thorough job of comparing the various semantic models we're aware of to help explain why we came to the conclusion that we came to.

If you're not interested in this journey, you can simply skip to the [next section](#token-sequences).

Here, we will look at a few interesting examples for injection and how different models can implement them. The examples aren't necessarily intended to be the most compelling examples that exist in the wild. Instead, they're hopefully representative enough to cover a wide class of problems. They are:

1. Implementing the storage for `std::tuple<Ts...>`
2. Implementing `std::enable_if` without resorting to class template specialization
3. Implementing properties (i.e. given a name like `"author"` and a type like `std::string`, emit a member `std::string m_author`, a getter `get_author()` which returns a `std::string const&` to that member, and a setter `set_author()` which takes a new value of type `std::string const&` and assigns the member).
4. Implement postfix increment in terms of prefix increment.

In order to focus on the API model rather than the syntax, we will try to use uniform syntax throughout. Specifically, we're going to use the following:

* `consteval` blocks ([@P3289R0])
* a metafunction `std::meta::inject`, which takes a thing to inject (whatever that thing is) and, optionally, a context to inject it into. By default, we inject into the point of the `consteval` block.

We do this in an attempt to minimize the overall API surface, since there's going to be a lot to add regardless. Let's dive in.

## The Spec API

In P2996, the injection API is based on a function `define_class()` which takes a range of `spec` objects. But `define_class()` is a really clunky API, because invoking it is an expression - but we want to do it in contexts that want a declaration. So a simple example of injecting a single member `int` named `x` is:

::: std
```cpp
struct C;
static_assert(is_type(define_class(^C,
    {data_member_spec{.name="x", .type=^int}})));
```
:::

We are already separately proposing `consteval` blocks and we would like to inject each spec more directly, without having to complete `C` in one ago. As in:

::: std
```cpp
struct C {
    consteval {
        inject(data_member_spec{.name="x", .type=^int});
    }
};
```
:::

We already think of this as an improvement. But let's go through several use-cases to expand the API and see how well it holds up.

### `std::tuple`

The tuple use-case was already supported by P2996 directly with `define_class()` (even as we think itd be better as a member pack), but it's worth just showing what it looks like with a direct injection API instead:

::: std
```cpp
template <class... Ts>
struct Tuple {
    consteval {
        std::array types{^Ts...};
        for (size_t i = 0; i != types.size() ;++i) {
            inject(data_member_spec{.name=std::format("_{}", i),
                                    .type=types[i]});
        }
    }
};
```
:::

### `std::enable_if`

Now, `std::enable_if` has already been obsolete technology since C++20. So implementing it, specifically, is not entirely a motivating example. However, the general idea of `std::enable_if` as *conditionally* having a member type is a problem that has no good solution in C++ today.

The spec API along with injection does allow for a clean solution here. We would just need to add an `alias_spec` construct to get the job done:

::: std
```cpp
template <bool B, class T=void>
struct enable_if {
    consteval {
        if (B) {
            inject(alias_spec{.name="type", .type=^T});
        }
    }
};
```
:::

So far so good.

### Properties

Now is when the spec API really goes off the rails. We've shown data members and extended it to member aliases. But how do we support member functions?

We want to be able to add a `property` with a given `name` and `type` that adds a member of that type and a getter and setter for it. For instance, we want this code:

::: std
```cpp
struct Book {
    consteval {
        property("author", ^std::string);
        property("title", ^std::string);
    }
};
```
:::

to emit a class with two members (`m_author` and `m_title`), two getters that each return a `std::string const&` (`get_author()` and `get_title()`) and two setters that each take a `std::string const&` (`set_author()` and `set_title()`). Fairly basic property.

We start by injecting the member:

::: std
```cpp
consteval auto property(string_view name, meta::info type)
    -> void
{
    inject(data_member_spec{.name=std::format("m_{}", name),
                            .type=type});

    // ...
}
```
:::

Now, we need to inject two functions. We'll need a new kind of `spec` for that case, and then we can use a lambda for the function body. Let's start with the getter:


::: std
```cpp
consteval auto property(string_view name, meta::info type)
    -> void
{
    inject(data_member_spec{.name=std::format("m_{}", name),
                            .type=type});


    inject(function_member_spec{
        .name=std::format("get_{}", name),
        .body=^[](auto const& self) -> auto const& {
            return self./* ????? */;
        }
    });

    // ...
}
```
:::

Okay. Uh. What do we return? For the title property, this needs to be `return self.m_title;`, but how do we spell that? We just... can't. We have our member right there (the `data_member_spec` we're injecting), so you might think we could try to capture it:

::: std
```cpp
consteval auto property(string_view name, meta::info type)
    -> void
{
    auto member = inject(data_member_spec{
        .name=std::format("m_{}", name),
        .type=type
    });

    inject(function_member_spec{
        .name=std::format("get_{}", name),
        .body=^[member](auto const& self) -> auto const& {
            return self.[:member:];
        }
    });

    // ...
}
```
:::

But that doesn't work - in order to splice `member`, it needs to be a constant expression - and it's not in this context.

Now, the body of the lambda isn't going to be evaluted in this constant evaluation, so it's possible to maybe some up with some mechanism to pass a context through - such that from the body we _can_ simply splice `member`. We basically need to come up with a way to defer this instantiation.

For now, let's try a spelling like this:

::: std
```cpp
consteval auto property(string_view name, meta::info type)
    -> void
{
    auto member = inject(data_member_spec{
        .name=std::format("m_{}", name),
        .type=type
    });

    inject(function_member_spec{
        .name=std::format("get_{}", name),
        .body=defer(member, ^[]<std::meta::info M>(auto const& self) -> auto const& {
            return self.[:M:];
        })
    });

    // ...
}
```
:::

and we can do something similar with the setter:

::: std
```cpp
consteval auto property(string_view name, meta::info type)
    -> void
{
    auto member = inject(data_member_spec{
        .name=std::format("m_{}", name),
        .type=type
    });

    inject(function_member_spec{
        .name=std::format("get_{}", name),
        .body=defer(member, ^[]<std::meta::info M>(auto const& self) -> auto const& {
            return self.[:M:];
        })
    });

    inject(function_member_spec{
        .name=std::format("set_{}", name),
        .body=defer(member, ^[]<std::meta::info M>(auto& self, typename [:type_of(M):] const& x) -> void {
            self.[:M:] = x;
        })
    });
}
```
:::

Now we run into the next problem:  what actual signature is the compiler going to inject for `get_author()` and `set_author()`?  First, we're introducing this extra non-type template parameter which we have to know to strip off somehow. Secondly, we're always taking the object parameter as a deduced parameter. How does the API know what we mean by that?

::: std
```cpp
struct Book {
    // do we mean this
    auto get_author(this Book const& self) -> auto const& { return self.m_author; }
    auto set_author(this Book& self, string const& x) -> void { self.m_author = x; }

    // or this
    auto get_author(this auto const& self) -> auto const& { return self.m_author; }
    auto set_author(this auto& self, string const& x) -> void { self.m_author = x; }
};
```
:::

That is: how does the compiler know whether we're injecting a member function or a member function template? Our lambda has to be generic either way. Moreover, even if we actually wanted to inject a function template, it's possible that we might want some parameter to be dependent but *not* the object parameter.

Well, we could provide another piece of information to `function_member_spec`: the signature directly:

::: std
```cpp
template <class T> using getter_type = auto() const -> T const&;
template <class T> using setter_type = auto(T const&) -> void;

consteval auto property(string_view name, meta::info type)
    -> void
{
    auto member = inject(data_member_spec{
        .name=std::format("m_{}", name),
        .type=type
    });

    inject(function_member_spec{
        .name=std::format("get_{}", name),
        .signature=substitute(^getter_type, {^type}),
        .body=defer(member, ^[]<std::meta::info M>(auto const& self) -> auto const& {
            return self.[:M:];
        })
    });

    inject(function_member_spec{
        .name=std::format("set_{}", name),
        .signature=substitute(^setter_type, {^type}),
        .body=defer(member, ^[]<std::meta::info M>(auto& self, typename [:type_of(M):] const& x) -> void {
            self.[:M:] = x;
        })
    });
}
```
:::

Which then maybe feels like the correct spelling is actually more like this, so that we can actually properly infer all the informatino:

::: std
```cpp
consteval auto property(string_view name, meta::info type)
    -> void
{
    auto member = inject(data_member_spec{
        .name=std::format("m_{}", name),
        .type=type
    });

    // note that this type is structural
    struct Context {
        std::meta::info type;
        std::meta::info member;
    };
    auto pctx = Context{
      // get the type of the current context that we're injecting into
      .type=type_of(std::meta::current()),
      .member=member
    };

    inject(function_member_spec{
        .name=std::format("get_{}", name),
        .body=defer(context, ^[]<Context C>(){
            return [](typename [:C.type:] const& self) -> auto const& {
                return self.[:C.member:];
            };
        })
    });

    inject(function_member_spec{
        .name=std::format("set_{}", name),
        .body=defer(context, ^[]<Context C>(){
            return [](typename [:C.type:]& self, typename [:type_of(C.member):] const& x) -> void {
                self.[:C.member:] = x;
            };
        })
    });
}
```
:::

That is, we create a custom context type that we pass in as a non-type template parameter into a lambda, so that it _it_ can return a new lambda with all the types and names properly substituted in when that can actually be made to work.

This solution... might be workable. But it's already pretty complicated and the problem we're trying to solve really isn't. As a result, we believe that the spec API is somewhat of a dead end when it comes to extending injection support.

## The CodeReckons API

The [CodeReckons](https://lists.isocpp.org/sg7/2024/04/0507.php) approach provides a very different injection mechanism than what is in P2996 or what has been described in any of the metaprogramming papers. We can run through these three examples and see what they look like. Here, we will use the actual syntax as implemented in that compiler.

### `std::tuple`

The initial CodeReckons article provides [an implementation](https://cppmeta.codereckons.com/tools/z/j74nc4) for adding the data members of a tuple like so:

::: std
```cpp
template <class... Ts>
struct tuple {
  % [](class_builder& b){
    int k = 0;
    for (type T : std::meta::type_list{^Ts...}) {
        append_field(b, cat("m", k++), T);
    }
  }();
};
```
:::

This isn't too different from what we showed in the earlier section with `data_member_spec`. Different spelling and API, but it's the same model (`append_field` is equivalent to injecting a `data_member_spec`).

### `std::enable_if`

Likewise, we have just a [difference of spelling](https://cppmeta.codereckons.com/tools/z/rh8zef):

::: std
```cpp
template <bool B, typename T=void>
struct enable_if {
    % [](class_builder& b){
        if (B) {
            append_alias(b, identifier{"type"}, ^T);
        }
    }();
};
```
:::

### Properties

Here is where the CodeReckons approach differs greatly from the potential spec API, and it's worth examining how they [got it working](https://cppmeta.codereckons.com/tools/z/znjs3f):

::: std
```cpp
consteval auto property(class_builder& b, type type, std::string name) -> void {
    auto member_name = identifier{("m_" + name).c_str()};
    append_field(b, member_name, type);

    // getter
    {
        method_prototype mp;
        object_type(mp, make_const(decl_of(b)));
        return_type(mp, make_lvalue_reference(make_const(type)));

        append_method(b, identifier{("get_" + name).c_str()}, mp,
            [member_name](method_builder& b){
                append_return(b,
                    make_field_expr(
                        make_deref_expr(make_this_expr(b)),
                        member_name));
            });
    }

    // setter
    {
        method_prototype mp;
        append_parameter(mp, "x", make_lvalue_reference(make_const(type)));
        object_type(mp, decl_of(b));
        return_type(mp, ^void);

        append_method(b, identifier{("set_" + name).c_str()}, mp,
            [member_name](method_builder& b){
                append_expr(b,
                    make_operator_expr(
                        operator_kind::assign,
                        make_field_expr(make_deref_expr(make_this_expr(b)), member_name),
                        make_decl_ref_expr(parameters(decl_of(b))[1])
                        ));
            });
    }
}

struct Book {
    % property(^std::string, "author");
    % property(^std::string, "title");
};
```
:::

In this model, we have to provide the signature of the two member functions (via `method_prototype`), and the bodies of the two member functions are provided as lambdas. But the lambda bodies here are not the C++ code that will be evaluated at runtime - it's still part of the AST building process. We have to define, at the AST level, what these member functions do.

In the spec API, we struggled how to write a function that takes a `string const&` and whose body is `self.{member name} = x;`. Here, because we don't need to access any of our reflections as constant expressions, we can make use of them directly.

But the result is... extremely verbose. This is a lot of code, that doesn't seem like it would scale very well. The setter alone (which is just trying to do something like `self.m_author = x;`) is already 14 lines of code and is fairly complicated. We think it's important that code injection still look like writing C++ code, not live at the AST level.

Nevertheless, this API does actually work. Whereas the spec API is still, at best, just a guess.

## String Injection

If we go back all the way to the beginning - we're trying to inject code. Perhaps the simplest possible model for how to inject code would be: just inject strings.

The advantage of strings is clear: everyone already knows how to build up strings. This makes implementing the three use-cases presented thus far is pretty straightforward.

### `std::tuple`

We could just do tuple this way:

::: std
```cpp
template <class... Ts>
struct Tuple {
    consteval {
        std::array types{^Ts...};
        for (size_t i = 0; i != types.size(); ++i) {
            inject(std::format(
                "[[no_unique_address]] {} _{};",
                qualified_name_of(types[i]),
                i));
        }
    }
};
```
:::

Note that here we even added support for `[[no_unique_address]]`, which we haven't done in either of the previous models. Although we could come up with a way to add it to either of the two previous APIs, the fact that with string injection we don't even have to come up with a way to do this is a pretty significant upside. Everything just works.

Now, this would work - we'd have to be careful to use `qualified_name_of` here to avoid any question of name lookup. But it would be better to simply avoid these questions altogether by actually being able to splice in the type rather than referring to it by name.

We can do that by very slightly extending the API to take, as its first argument, an environment. And then we can reduce it again by having the API itself be a `format` API:

::: std
```cpp
template <class... Ts>
struct Tuple {
    consteval {
        std::array types{^Ts...};
        for (size_t i = 0; i != types.size(); ++i) {
            inject(
                {{"type", types[i]}},
                "[[no_unique_address]] [:type:] _{};",
                i);
        }
    }
};
```
:::

### `std::enable_if`

This one is even simpler, since we don't even need to bother with name lookup questions or splicing:

::: std
```cpp
template <bool B, class T=void>
struct enable_if {
    consteval {
        if (B) {
            inject("using type = T;");
        }
    };
};
```
:::

### Properties

Unlike with the spec API, implementing a property by way of code is straightforward. And unlike the CodeReckons API, we can write what looks like C++ code:

::: std
```cpp
consteval auto property(info type, string_view name) -> void {
    inject(meta::format_with_environment(
        {{"T", type}},
        R"(
        private:
            [:T:] m_{0};

        public:
            auto get_{0}() const -> [:T:] const& {{
                return m_{0};
            }}

            auto set_{0}(typename [:T:] const& x) -> void {{
                m_{0} = x;
            }}
        )",
        name));
}

struct Book {
    consteval {
        property(^string, "author");
        property(^string, "title");
    }
}
```
:::

Can pretty much guarantee that strings have the lowest possible barrier to entry of any code injection API. Which is a benefit that is not to be taken lightly! It is not surprising that D and Jai both have string-based injection mechanisms.

But string injection is hardly perfect, and several of the issues with it might be clear already:

1. String injection does let you write what looks like C++ code, but it wouldn't let you use any macros - as those don't affect the contents of string literals and we can't run another preprocessing step later.
2. Our main string formatting mechanism, `format`, uses `{}` for replacement fields, which means actual braces - which show up in C++ a lot - have to be escaped. It also likely isn't the most compile-time efficient API, so driving reflection off of it might be suboptimal.
3. You don't get syntax highlighting for injected code strings. They're just strings. Perhaps we could introduce a new kind of string literal that syntax highlighters could recognize, but that seems like pre-emptively admitting defeat.
4. Errors happen at the point of *injection*, not at the point where you're writing the code. And the injection could happen very far away from the code.

But string injection offers an extremely significant advantage that's not to be underestimated: everyone can deal with strings and strings already just support everything, for all future evolution, without the need for a large API.

Can we do better?

## Fragments

[@P1717R0] introduced the concept of fragments. It introduced many different kinds of fragments, under syntax that changed a bit in [@P2050R0] and [@P2237R0]. For the purposes of this section we'll just use `@fragment` as the introducer.

### `std::tuple`

The initial fragments paper itself led off with an implementation of `std::tuple` storage and the concept of a `consteval` block (now also [@P3289R0]). An updated version of that approach using updated syntax for expansion statements and reflection is:

::: std
```cpp
template<class... Ts>
struct Tuple {
    consteval {
        std::array types{^Ts...};
        for (size_t i = 0; i != types.size(); ++i) {
            inject(@fragment {
                [[no_unique_address]] [: $(types[i]) :] $("_", i);
            });
        }
    }
};
```
:::

Now, the big advantage of fragments is that it's just C++ code in the middle there. That doesn't show up clearly in this particular example, but it will more shortly. The problem that fragments need to solve is how to get context information into it. For instance, how do get the type `types[i]` and how do we produce the names `_0`, `_1`, ..., for all of these members? Some revisions of the paper introduced an operator spelled `unqualid` (to create an unqualified id). The latest used `|# #|`.

Instead, we are going to introduce a new solution spelled, for now, `$(e...)`. We'll go into [more detail on it later](#quoting-into-a-token-sequence), but for now, suffice to say that the above works - the first "quote" of `$(types[i])` will capture that reflection so that it can be spliced when the fragment is injected, and the second concatenates the string `"_"` with successive numbers `i` to produce the member names.

### `std::enable_if`

It is very hard to compete [with this](https://godbolt.org/z/nf8nPnnh4):

::: std
```cpp
template <bool B, class T=void>
struct enable_if {
    consteval {
        if (B) {
            inject(@fragment { using type = T; });
        }
    };
};
```
:::

Sure, you might want to simplify this just having a class scope `if` directly and then putting the contents of the `@fragment` in there.

### Properties

The [implementation here](https://godbolt.org/z/ddbsYWsvr) (again adjusting the syntax for something we think will be better, and more in line with the other examples presented in other models) isn't too different from the [string implementation](#properties-2):

::: std
```cpp
consteval auto property(std::meta::info type, std::string name) -> void {
    std::string member_name = "m_" + name;

    inject(@fragment {
        $(type) $(member_name);

        auto $("get_", name)() -> $(type) const& {
            return $(member_name);
        }

        auto $("set_", name)(typename $(type) const& x) -> void {
            $(member_name) = x;
        }
    });
}

struct Book {
    consteval {
        property(^std::string, "author");
        property(^std::string, "title");
    }
};
```
:::

It's a bit busy because nearly everything in properties involves quoting outside context, so seemingly everything here is quoted. It's even busier in the linked implementation by way of the quoting syntax being heavier.

Now, there's one very important property of fragments (as designed in these papers) hold: every fragment must be parsable in its context. A fragment does not leak its declarations out of its context; only out of the context where it is injected. Not only that, we get full name lookup and everything.

On the one hand, this seems like a big advantage: the fragment is checked at the point of its declaration, not at the point of its use. With the string model above, that was not the case - you can write whatever garbage string you want and it's still a perfectly valid string, it only becomes invalid C++ code when it's injected.

On the other, it has some consequences for how we can code using fragments. In the above implementation, we inject the whole property in one go. But let's say we wanted to split it up for whatever reason. We can't.

::: cmptable
### Valid
```cpp
consteval auto property(meta::info type, std::string name)
    -> void
{
    std::string member_name = "m_" + name;

    inject(@fragment {
        $(type) $(member_name);

        auto $("get_", name)() -> $(type) const& {
            return $(member_name);
        }

        auto $("set_", name)(typename $(type) const& x)
            -> void {
            $(member_name) = x;
        }
    });
}
```

### Invalid
```cpp
consteval auto property(meta::info type, std::string name)
    -> void
{
    std::string member_name = "m_" + name;

    inject(@fragment {
        $(type) $(member_name);
    });

    inject(@fragment {
        auto $("get_", name)() -> $(type) const& {
            return $(member_name);
        }

        auto $("set_", name)(typename $(type) const& x)
            -> void {
            $(member_name) = x;
        }
    });
}
```
:::

On the right, we're injecting the member in one fragment and the getter/setter in another. The problem is, in the second fragment, name lookup for `m_author` fails. We can't do that.

We have to teach the fragment how to find the name, which requires writing this (note the added `requires` statement in the fragment on the right):

::: cmptable
### Single Fragment
```cpp
consteval auto property(meta::info type, std::string name)
    -> void
{
    std::string member_name = "m_" + name;

    inject(@fragment {
        $(type) $(member_name);

        auto $("get_", name)() -> $(type) const& {
            return $(member_name);
        }

        auto $("set_", name)(typename $(type) const& x)
            -> void {
            $(member_name) = x;
        }
    });
}
```

### Split Fragments
```cpp
consteval auto property(meta::info type, std::string name)
    -> void
{
    std::string member_name = "m_" + name;

    inject(@fragment {
        $(type) $(member_name);
    });

    inject(@fragment {
        // this requirement right here
        requires $(type) $(member_name);

        auto $("get_", name)() -> $(type) const& {
            return $(member_name);
        }

        auto $("set_", name)(typename $(type) const& x)
            -> void {
            $(member_name) = x;
        }
    });
}
```
:::

### Postfix increment

One boilerplate annoyance is implementing `x++` in terms of `++x`. Can code injection help us out? Here is how you have to implement it using the CodeReckons approach and using fragments:

::: cmptable
### [CodeReckons](https://cppmeta.codereckons.com/tools/z/71To7o)
```cpp
consteval auto postfix_increment(class_builder& b) -> void {
    method_prototype mp;
    append_parameter(mp, "x", ^int);
    object_type(mp, decl_of(b));
    return_type(mp, decl_of(b));

    append_method(b, operator_kind::post_inc, mp,
        [](method_builder& b){
            // auto tmp = *this;
            auto tmp = append_var(b, "tmp", auto_ty,
                make_deref_expr(make_this_expr(b)));
            // ++*this;
            append_expr(b,
                make_operator_expr(
                    operator_kind::pre_inc,
                    make_deref_expr(make_this_expr(b))));
            // return tmp;
            append_return(b, make_decl_ref_expr(tmp));
        });
}

struct C {
    int i;

    auto operator++() -> C& {
        ++i;
        return *this;
    }

    % postfix_increment();
};
```

### [Fragments](https://godbolt.org/z/r1v3e43sd)
```cpp
consteval auto postfix_increment() {
    -> fragment struct T {
        requires T& operator++();

        auto operator++(int) -> T {
            auto tmp = *this;
            ++*this;
            return tmp;
        }
    };
}










struct C {
    int i;

    auto operator++() -> C& {
        ++i;
        return *this;
    }

    consteval { postfix_increment(); }
};
```
:::

Here we're using the actual fragments syntax as in the linked implementation. In this case, we don't have to quite anything - there is just a very nice way of getting the type of the class that we are injecting to. In this case the name `T` binds to `C`.

Now, the rule in the fragments implementation is that the fragments themselves are checked. This includes name lookup. So any name used in the body of the fragment has to be found and pre-declared, which is what we're doing in the `requires` clause there. The implementation right now appears to have a bug with respect to operators (if you change the body to calling `inc(*this)`, it does get flagged), which is why it's commented out in the link.

In any case, the fragment model seems substantially easier to program in than the CodeReckons model. We're actually writing C++ code. But we think the fragment model still isn't quite right. By nobly trying to diagnose errors at the point of fragment declaration, it adds a complexity to the fragment model in a way that we don't think carries its weight. The fragment papers ([@P1717R0] and [@P2237R0]) each go into some detail of different approaches of how to do name checking at the point of fragment declaration. They are all complicated.

# Token Sequences

Generation of code from low-level syntactic elements such as strings or token sequences may be considered quite unsophisticated. Indeed, previous proposals for code synthesis in C++ have studiously minimized using strings or tokens as input but resorting to AST-based APIs, expansion statements, or code fragments, as shown above. As noted by Andrew Sutton in [@P2237R0]:

::: quote
synthesizing new code from strings is straightforward, especially when the language/library has robust tools for compile-time string manipulation […] the strings or tokens are syntactically and semantically unanalyzed until they are injected
:::

whereas the central premise—and purported advantage—of a code fragment is it

::: quote
should be fully syntactically and semantically validated prior to its injection
:::

Due to the lack of consensus for a code synthesis mechanism, some C++ reflection proposals shifted focus to the query side of reflection and left room for scant code synthesis capabilities.

After extensive study and experimentation (as seen above), we concluded that some crucially important forms of token synthesis are necessary for practical code generation, and that insisting upon early syntactic and semantic validation of generated code is a net liability. The very nature of code synthesis involves assembling meaningful constructs out of pieces that have little or no meaning in separation. Using concatenation and deferring syntax/semantics analysis to offer said concatenation is by far the simplest, most direct approach to code synthesis.

Generally, we think that imposing early checking on generated code is likely to complicate and restrict the ways in which users can use the facility and also be difficult for implementers, thus hurting everyone involved.

We therefore acknowledge the notion of token sequence as a core building block for generating code. Using token sequences allows flexibility to code that generates other code, while deferring name lookup and semantic analysis to well-defined points in the compilation process. Thus we reach the notion of a `@tokens` literal dedicated to representing unprocessed sequences of tokens.

## `@tokens` literal

We propose the introduction of a new literal with the following syntax (the specific introducer can be decided later):

::: std
```cpp
@tokens { $balanced-brace-tokens$ }
```
:::

where `$balanced-brace-tokens$` is an arbitrary sequence of C++ tokens with the sole requirement that the `{` and `}` pairs are balanced. Parentheses and square brackets may be unbalanced. The opening and closing `{`/`}` are not part of the token sequence. The type of a `@tokens` literal is `std::meta::info`.

For example:

::: std
```cpp
constexpr auto t1 = @tokens { a + b };      // three tokens
static_assert(std::is_same_v<decltype(t1), const std::meta::info>);
constexpr auto t2 = @tokens { a += ( };     // code does not have to be meaningful
constexpr auto t3 = @tokens { abc { def };  // Error, unpaired brace
```
:::

Token sequences can be concatenated with the `+` operator. The result is a token sequence consisting of the concatenation of the operands.

::: std
```cpp
constexpr auto t1 = @tokens { c =  };
constexpr auto t2 = @tokens { a + b; };
constexpr auto t3 = t1 + t2;
static_assert(t3 == @tokens { c = a + b; });
```
:::

The concatenation is not textual - two tokens concatenated together preserve their identity, they are not pasted together into a single token. For example:

::: std
```cpp
constexpr auto t1 = @tokens { abc };
constexpr auto t2 = @tokens { def };
constexpr auto t3 = t1 + t2;
static_assert(t3 != @tokens { abcdef });
static_assert(t3 == @tokens { abc def });
```
:::

Whitespace and comments are treated just like in regular code - they are not significant beyond their role as token separator. For example:

::: std
```cpp
constexpr auto t1 = @tokens { hello  = /* world */   "world" };
constexpr auto t2 = @tokens { /* again */ hello="world" };
static_assert(t1 == t2);
```
:::

Because tokens are handled after the the initial phase of preprocessing, macros and string concatenation can apply - but only within a single token sequence:

::: std
```cpp
static_assert(@tokens { "abc" "def" } == @tokens { "abcdef" });

// this concatenation produces the token sequence "abc" "def", not "abcdef"
// when this token sequence will be injected, that will be ill-formed
static_assert(@tokens { "abc" } + @tokens { "def" } != @tokens { "abcdef" });

#define PLUS_ONE(x) ((x) + 1)
static_assert(@tokens { PLUS_ONE(x) } == @tokens { ((x) + 1) });

// the macro PLUS_ONE is not invoked because
// token string concatenation happens after the preprocessor
static_assert(@tokens { PLUS_ONE(x } + @tokens{ ) } == @tokens { PLUS_ONE(x) });
```
:::

A token sequence has no meaning by itself, until injected. But because (hopefully) users will write valid C++ code, the resulting injection actually does look like C++ code.

## Quoting into a token sequence

There's still the issue that you need to access outside context from within a token sequence. For that we introduce dedicated capture syntax: `$(e...)`.

The implementation model for this is that we collect the tokens within a `@token { ... }` literal, but every time we run into a capture, we parse and evaluate the expression within and replace it with the value as described below:

* `$(e)` for `e` of type `meta::info` is replaced by a pseudo-literal token holding the `info` value. If `e` is itself a token sequence, the contents of that token sequence are concatenated in place.
* Otherwise `$(e)` for `e` being string-like or integral is replaced with that value. `$(e...)` can concatenate multiple string-like or integral values into a single identifier.

With that in mind, we can start going through our examples.

## Examples

Now, the `std::tuple` and `std::enable_if` cases would look identical to their corresponding implementations with [fragments](#fragments). In both cases, we are injecting complete code fragments that require no other name lookup, so there is not really any difference between a token sequence and a proper fragment:

::: cmptable
### `std::tuple`
```cpp
template<class... Ts>
struct Tuple {
    consteval {
        std::array types{^Ts...};
        for (size_t i = 0; i != types.size(); ++i) {
            inject(@tokens {
                [[no_unique_address]]
                [: $(types[i]) :] $("_", i);
            });
        }
    }
};
```

### `std::enable_if`
```cpp
template <bool B, class T=void>
struct enable_if {
    consteval {
        if (B) {
            inject(@tokens { using type = T; });
        }
    };
};
```
:::

The property example likewise could be identical, but we do not run into any name lookup issues, so we can write it any way we want - either as injecting one token sequence or even injecting three. Both work fine without needing any additional declarations:

::: cmptable
### Single Token Sequence
```cpp
consteval auto property(meta::info type, std::string name)
    -> void
{
    std::string member_name = "m_" + name;

    inject(@tokens {
        $(type) $(member_name);

        auto $("get_", name)() -> $(type) const& {
            return $(member_name);
        }

        auto $("set_", name)(typename $(type) const& x)
            -> void {
            $(member_name) = x;
        }
    });
}
```

### Three Token Sequences
```cpp
consteval auto property(meta::info type, std::string name)
    -> void
{
    std::string member_name = "m_" + name;

    inject(@tokens {
        $(type) $(member_name);
    });

    inject(@tokens {
        auto $("get_", name)() -> $(type) const& {
            return $(member_name);
        }
    });

    inject(@tokens {
        auto $("set_", name)(typename $(type) const& x)
            -> void {
            $(member_name) = x;
        }
    });
}
```
:::

With the postfix increment example, we see some more interesting difference. We are not proposing any special-case syntax for getting at the type that we are injecting into, so it would have to be pulled out from the context (we'll name it `T` in both places for consistency):

::: cmptable
### Fragment
```cpp
consteval auto postfix_increment() -> void {
    -> fragment struct T {
        requires T& operator++();

        auto operator++(int) -> T {
            auto tmp = *this;
            ++*this;
            return tmp;
        }
    };
}
```

### Token Sequence
```cpp
consteval auto postfix_increment() -> void {
    auto T = type_of(std::meta::current());
    inject(@tokens {

        auto operator++(int) -> $(T) {
            auto tmp = *this;
            ++*this;
            return tmp;
        }
    });
}
```
:::

The syntax here is, unsurprisingly, largely the same. We're mostly writing C++ code. The difference is that we no longer need to pre-declare the functions we're using and the feature set is smaller. While declaring `T` as part of the fragment is certainly convenient, we're shooting for a smaller feature.

## Logging Vector: Cloning a Type

The goal here is we want to implement a type `LoggingVector<T>` which behaves like `std::vector<T>` in all respects except that it prints the function being called.

We start with this:

::: std
```cpp
template <typename T>
class LoggingVector {
    std::vector<T> impl;

public:
    LoggingVector(std::vector<T> v) : impl(std::move(v)) { }

    consteval {
        for (std::meta::info fun : /* public, non-special member functions */) {
            inject(@tokens {
                declare [: $(decl_of(fun)) ] {
                    // ...
                }
            });
        }
    }
};
```
:::

We want to clone every member function, which requires copying the declaration. We don't want to actually have to spell out the declaration in the token sequence that we inject - that would be a tremendous amount of work given the complexity of C++ declarations. So instead we introduce a new kind of splice: a declaration splice. We already have `typename [: e :]` and `template [: e :]` in other contexts, so `declare [: e :]` at least fits within the family of splicers.

Now, we have two problems to solve in the body (as well as a few more problems we'll get to later).

First, we need to print the name of the function we're calling. This is easy, since we have the function and can just ask for its name.

Second, we need to actually forward the parameters of the function into our member `impl`. This is not easy:

::: std
```cpp
consteval {
    for (std::meta::info fun : /* public, non-special member functions */) {
        inject(@tokens {
            declare [: $(decl_of(fun)) ] {
                std::println("Calling {}", $(name_of(fun)));
                return impl.[: $(fun) :](/* ???? */);
            }
        });
    }
}
```
:::

This is where the ability of token sequences to be concatenated from purely sequences of tokens really gives us a lot of value. How do we forward the parameters along? We don't even have the parameter names here - the declaration that we're cloning might not even _have_ parameter names. But with the ability to just ask for the parameters themselves (which [@P3096R0] should provide), we can get reflections to those parameters, and we can splice those reflections:

::: std
```cpp
consteval {
    for (std::meta::info fun : /* public, non-special member functions */) {
        auto argument_list = @tokens { };
        bool first = true;
        for (auto param : parameters_of(fun)) {
            if (not first) {
                argument_list += @tokens { , };
            }
            first = false;
            argument_list += @tokens {
                static_cast<$(type_of(param))&&>([: $(param) :])
            };
        }

        inject(@tokens {
            declare [: $(decl_of(fun)) ] {
                std::println("Calling {}", $(name_of(fun)));
                return impl.[: $(fun) :]( $(argument_list) );
            }
        });
    }
}
```
:::

The `argument_list` is simply building up the token sequence `[: p0 :], [: p1 :], [: p2 :], ..., [: pN :]` for each parameter (except forwarded). There is no name lookup going on, no checking of fragment correctness. Just building up the right tokens.

Once we have those tokens, we can concatenate this token sequence using the same `$()` quoting operator that we've used for other problems and we're done.

Note that we didn't actually have to implement it this way - we could've concatenated the entire token sequence piecewise. But this structure allows factoring out parameter-forwarding into its own function:

::: std
```cpp
consteval auto forward_parameters(std::meta::info fun) -> std::meta::info {
    auto argument_list = @tokens { };
    bool first = true;
    for (auto param : parameters_of(fun)) {
        if (not first) {
            argument_list += @tokens { , };
        }
        first = false;
        argument_list += @tokens {
            static_cast<$(type_of(param))&&>([: $(param) :])
        };
    }
    return argument_list;
}
```
:::

And then:

::: std
```cpp
consteval {
    for (std::meta::info fun : /* public, non-special member functions */) {
        inject(@tokens {
            declare [: $(decl_of(fun)) ] {
                std::println("Calling {}", $(name_of(fun)));
                return impl.[: $(fun) :]( $(forward_parameters(fun)) );
            }
        });
    }
}
```
:::

However, we've still got some work to do.

## Logging Vector II: Cloning with Modifications

The above implementation already gets us a great deal of functionality, and should create code that looks something like this:

::: std
```cpp
template <typename T>
class LoggingVector {
    std::vector<T> impl;

public:
    LoggingVector(std::vector<T> v) : impl(std::move(v)) { }

    auto clear() -> void {
        std::println("Calling {}", "clear");
        return impl.clear();
    }

    auto push_back(T const& value) -> void {
        std::println("Calling {}", "push_back");
        return impl.push_back(static_cast<T const&>(value));
    }

    auto push_back(T&& value) -> void {
        std::println("Calling {}", "push_back");
        return impl.push_back(static_cast<T&&>(value));
    }

    // ...
};
```
:::

For a lot of `std::vector'`s member functions, we're done. But some need some more work. One of the functions we're emitting is member `swap`:

::: std
```cpp
template <typename T>
class LoggingVector {
    std::vector<T> impl;

public:
    // ...

    auto swap(std::vector<T>& other) noexcept(/* ... */) -> void {
        std::println("Calling {}", "swap");
        return impl.swap(other); // <== omitting the cast here for readability
    }

    // ...
};
```
:::

But this... isn't right. Or rather, it could potentially be right in some design, but it's not what we want to do. We don't want `LoggingVector<int>` to be swappable with `std::vector<int>`... we want it to be swappable with itself. What we actually want to do is emit this:

::: std
```cpp
    auto swap(LoggingVector<T>& other) noexcept(/* ... */) -> void {
        std::println("Calling {}", "swap");
        return impl.swap(other.impl);
    }
```
:::

Two changes here: the parameter needs to change from `std::vector<T>&` to `LoggingVector<T>&`, and then in the call-forwarding we need to forward not `other` (which is now the wrong type) but rather `other.impl`. How can we do that?

---
references:
  - id: P2996R3
    citation-label: P2996R3
    title: "Reflection for C++26"
    author:
      - family: Barry Revzin
      - family: Wyatt Childers
      - family: Peter Dimov
      - family: Andrew Sutton
      - family: Faisal Vali
      - family: Daveed Vandevoorde
      - family: Dan Katz
    issued:
      - year: 2024
        month: 05
        day: 16
    URL: https://wg21.link/p2996r3
  - id: P3289R0
    citation-label: P3289R0
    title: "`consteval` blocks"
    author:
      - family: Wyatt Childers
      - family: Barry Revzin
      - family: Daveed Vandevoorde
    issued:
      - year: 2024
        month: 05
        day: 18
    URL: https://wg21.link/p3289r0
---
