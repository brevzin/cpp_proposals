Title: `constexpr asm`
Document-Number: DxxxxR0
Authors: Barry Revzin, barry dot revzin at gmail dot com
Audience: EWG

# Motivation

The crux of this proposal is to make it marginally easier to implement the kind of conditionally constexpr functions that are allowed by [P0595R2](https://wg21.link/P0595R2). The sole purpose of this proposal is to let you write the code on the right instead of having to write the code on the left (example borrowed from David Stone):

<table style="width:100%">
<tr>
<th style="width:50%">
Working Draft
</th>
<th style="width:50%">
Proposed
</th>
</tr>
<tr>
<td>
    :::cpp hl_lines="9,11"
    constexpr std::size_t strlen(char const * s) { 
        if (std::is_constant_evaluated()) { 
            for (const char *p = s; ; ++p) { 
                if (*p == '\0') { 
                    return static_cast<std::size_t>(p - s); 
                } 
            } 
        } else { 
            [&]{ 
            asm("SSE 4.2 insanity"); 
            }(); 
        } 
    }     
</td>
<td>
    :::cpp
    constexpr std::size_t strlen(char const * s) { 
        if (std::is_constant_evaluated()) { 
            for (const char *p = s; ; ++p) { 
                if (*p == '\0') { 
                    return static_cast<std::size_t>(p - s); 
                } 
            } 
        } else { 
        
            asm("SSE 4.2 insanity"); 
            
        } 
    }     
</td>
</tr>
</table>

The only difference between the two examples is highlighted: having to wrap the `asm` in an immediately invoked lambda expression to get around the restriction from \[dcl.constexpr\]/3 that function bodies are not allowed to contain an _asm-definition_.

The workaround is silly. Let's just remove the need for it. 

There are currently four things you cannot put in a function body. This paper is **NOT** proposing allowing any of those things to be used in a constant expression - this paper is simply proposing allowing them to appear in `constexpr` functions so that they can be used in situations like the above. It is clearly possible to use these things in `constexpr` functions so the restrictions at this point seem totally arbitrary.

# Wording

Add three more sub-bullets into 7.7 [expr.const] paragraph 4:

> An expression `e` is a _core constant expression_ unless the evaluation of e, following the rules of the abstract machine, would evaluate one of the following expressions:
> 
> - [...]
> - <ins>an _asm-definition_,</ins>
> - <ins>a `goto` statement,</ins>
> - <ins>a definition of a variable of non-literal type or of static or thread storage duration or for which no initialization is performed.</ins>

Note that while those aren't expressions, neither is a checked contract. So perhaps "one of the following expressions" needs to be worded slightly differently.

Remove paragraph 3.3 from 9.1.5 [dcl.constexpr]:

> The definition of a `constexpr` function shall satisfy the following requirements:
> 
> - its return type shall be a literal type;
> - each of its parameter types shall be a literal type;
> - <del>its function-body shall not contain</del>
> 
>     - <del>an _asm-definition_,</del>
>     - <del>a `goto` statement,</del>
>     - <del>an identifier label ([stmt.label]),</del>
>     - <del>a definition of a variable of non-literal type or of static or thread storage duration or for which no initialization is performed.</del>
> 
> <del>[*Note:* A _function-body_ that is `= delete` or `= default` contains none of the above. *-end note*]</del>
