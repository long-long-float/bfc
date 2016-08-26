BrainFu*k Compiler
====

It needs LLVM.

```sh
$ make
$ ./bfc hello.bf # it generates a.bc (LLVM bitcode)
$ llc a.bc
$ clang a.s
```

License: MIT

Copyright (c) 2016 long_long_float
