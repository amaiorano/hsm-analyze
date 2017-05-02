# hsm-analyze

hsm-analyze is a tool based on [Clang LibTooling](https://clang.llvm.org/docs/LibTooling.html) that analyzes C++ code that makes use of [the HSM library](https://github.com/amaiorano/hsm).

It's presently a work-in-progress, but the goals are:
- Provide useful information about potential problems with a given state machine
- Output graphs of state machines (e.g. GraphViz dot files)


## Usage

Like all Clang tools based on LibTooling, hsm-analyze can use either a compilation database or you can pass compilation flags directly via command line right after passing "--". Read more about how to use a compilation database (here)[https://clang.llvm.org/docs/HowToSetupToolingForLLVM.html].

Here the basic form when passing compilation flags directly via command line:

```
hsm-analyze path/to/source_file_with_hsm.cpp -- -Ipath/to/hsm.h
```

For example, on Windows:
```
hsm-analyze c:\code\hsm\samples\hsm_book_samples\source\ch2\process_state_transitions.cpp  -- -Ic:\code\hsm\include
```


## How to build

On Windows:

* Download and install the latest binary distribution of [ClangOnWin from here](https://sourceforge.net/projects/clangonwin/files/MsvcBuild/)

* Assuming you've installed it to the default location, C:\Program Files\LLVM:

```
cd path/to/hsm-analyze
mkdir build && cd build
cmake -G "Visual Studio 14 2015 Win64" -DCMAKE_PREFIX_PATH="C:\Program Files\LLVM\lib\cmake\clang" -DUSE_RELEASE_LIBS_IN_DEBUG=On ..
```

* Open the generated sln and build
