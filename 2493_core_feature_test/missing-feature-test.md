---
title: "Missing feature test macros for C++20 core papers"
document: P2493R0
date: today
audience: CWG, SG10
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
---

# Introduction

As Jonathan Wakely pointed out on the [SG10 mailing list](https://lists.isocpp.org/sg10/2021/10/0810.php), neither [@P0848R3] (Conditionally Trivial Special Member Functions) nor [@P1330R0] ( Changing the active member of a union inside `constexpr`) provided a feature-test macro.

In order to implement [@P2231R1] (Missing constexpr in `std::optional` and `std::variant`), the standard library needs to know whether these features are available to know when it can use them in the library. And currently, there is no way of doing so.

Even though some compilers have implemented these features for a while, others haven't, so it's better to provide the feature-test macro now (when it will have some false negatives) than to not provide it at all (in which case libstdc++ needs to choose between never providing the library feature or causing unsupported compilers to fail even if users weren't trying to use the new library feature).

As far as choice of value goes, P0848R3 was adopted in Cologne (201907) and P1330R0  in San Diego (201811), and there is already currently a `201907L` value in the history of `__cpp_constexpr` [@SD6] (a value which includes [@P1331R2] and [@P1668R1]) and `__cpp_concepts` (including [@P1452R2]). Richard Smith [suggests](https://lists.isocpp.org/sg10/2021/10/0815.php) that we should either bump both macros to `201908L` (as just _some_ larger value) or both to `202002L` (to mean the complete set of C++20 proposals).

# Proposal

This paper proposes Richard's second suggestion: bump `__cpp_concepts` and `__cpp_constexpr` to `202002L` to include P0848R3 and P1330R0, respectively. For `__cpp_concepts`, this is a new value that requires a wording change. For` __cpp_constexpr`, there is already a later value as of `202110L` a result of [@P2242R3] (gcc already implements this change and reports this new macro version, but gcc also already implements P1330R0 so this is okay. clang and msvc do not yet report this higher value).

## Wording

Change [cpp.predefined]{.sref}, table 21:

::: bq
```diff
__cpp_­concepts     @[201907L]{.diffdel} [202002L]{.diffins}@
```
:::

Note that no change to `__cpp_constexpr` is visible in the working draft, but it will appear in SD-FeatureTest [@SD6].
