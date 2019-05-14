# MxCompiler
A Mx-language compiler focused on backend optimization and code generation for x86-64.

This is the codebase for compiler project of ACM class @ SJTU

Useful links:
- [wiki](https://acm.sjtu.edu.cn/wiki/Compiler_2017)
- [language reference](https://acm.sjtu.edu.cn/w/images/3/30/M_language_manual.pdf?20170401)

## Main Optimizations
- Function Inlining
- Register Allocation based on SSA
- Loop-invariant Code Motion
- Dead Code Elimination
- Global Value Numbering
