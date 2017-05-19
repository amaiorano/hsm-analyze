# hsm-analyze

hsm-analyze is a tool based on [Clang LibTooling](https://clang.llvm.org/docs/LibTooling.html) that analyzes C++ code that makes use of [the HSM library](https://github.com/amaiorano/hsm). It can be used to:

- Provide useful information about potential problems with a given state machine
- Output graphs of state machines (e.g. GraphViz dot files)


## Usage

These instructions are for Windows. For Linux or Mac, follow instructions for how to build hsm-analyze (below) and adapt as necessary.

- [Download](https://link/to/releases/here) the latest version of hsm-analyze.

- Install a distribution of LLVM that matches the LLVM version that was used to build hsm-analyze from [here (recommended)](https://sourceforge.net/projects/clangonwin/files/MsvcBuild/) or [here](http://releases.llvm.org/download.html).

- Copy the single hsm-analyze executable to your LLVM/bin directory (e.g. ```C:\Program Files\LLVM\bin```). This is required for most LibTooling-based tools to access Clang's C++ standard headers.

- Install [GraphViz](http://www.graphviz.org/Download.php) to render state machines graphs.

Like all Clang tools based on LibTooling, you can pass compilation arguments to hsm-analyze via command line following a double-dash ("--"), or by using a compilation database (read about how to generate and use a compilation database [here](https://clang.llvm.org/docs/HowToSetupToolingForLLVM.html)).

Here's an example of how to output a GraphViz dot file from a cpp file:

```
hsm-analyze -dot c:\code\hsm\samples\hsm_book_samples\source\ch4\reusable_states.cpp -- -Ic:\code\hsm\include > reusable_states.dot
```

which produces the following dot file contents:

```
strict digraph G {
  fontname=Helvetica;
  nodesep=0.6;
  //rankdir=LR
  CharacterStates__Alive -> CharacterStates__Stand [style="bold", weight="1", color="black"]
  CharacterStates__Attack -> CharacterStates__Stand [style="dotted", weight="50", color="black", dir="both"]
  CharacterStates__Attack -> CharacterStates__PlayAnim [style="bold", weight="1", color="black"]
  CharacterStates__PlayAnim -> CharacterStates__PlayAnim_Done [style="dotted", weight="50", color="black"]
  CharacterStates__PlayAnim -> CharacterStates__PlayAnim [style="dotted", weight="50", color="black", dir="both"]

  subgraph cluster_CharacterStates { label = "CharacterStates"; labeljust=left;
    subgraph {
      rank=same // depth=0
      CharacterStates__Alive [label="Alive", fontcolor=white, style=filled, color="0.033617 0.500000 0.250000"]
    }
  }

  subgraph cluster_CharacterStates { label = "CharacterStates"; labeljust=left;
    subgraph {
      rank=same // depth=1
      CharacterStates__Attack [label="Attack", fontcolor=white, style=filled, color="0.033617 0.500000 0.475000"]
      CharacterStates__Stand [label="Stand", fontcolor=white, style=filled, color="0.033617 0.500000 0.475000"]
    }
  }

  subgraph cluster_CharacterStates { label = "CharacterStates"; labeljust=left;
    subgraph {
      rank=same // depth=2
      CharacterStates__PlayAnim [label="PlayAnim", fontcolor=white, style=filled, color="0.033617 0.500000 0.700000"]
      CharacterStates__PlayAnim_Done [label="PlayAnim_Done", fontcolor=white, style=filled, color="0.033617 0.500000 0.700000"]
    }
  }

}
```

To render it with GraphViz:

```bash
dot -Tpng -o reusable_states.png reusable_states.dot
```

which produces the following png:

![reusable_states.png](https://github.com/amaiorano/hsm-analyze/wiki/images/reusable_states.png)


## How to build

On Windows:

* Download and install the a binary distribution of [ClangOnWin](https://sourceforge.net/projects/clangonwin/files/MsvcBuild/). **NOTE:** At the time of this writing, hsm-analyze successfully builds with VS2015 and VS2017 against [LLVM-4.0.0svn-r277264-require-python35dll-win64](https://freefr.dl.sourceforge.net/project/clangonwin/MsvcBuild/4.0/LLVM-4.0.0svn-r277264-require-python35dll-win64.exe) and [LLVM-5.0.0svn-r302983-win64](https://ayera.dl.sourceforge.net/project/clangonwin/MsvcBuild/5.0/LLVM-5.0.0svn-r302983-win64.exe).

* Assuming you've installed it to the default location, C:\Program Files\LLVM, here's how you'd build hsm-analyze with Visual Studio 2015:

```
git clone https://github.com/amaiorano/hsm-analyze.git
cd hsm-analyze
mkdir build && cd build
cmake -G "Visual Studio 14 2015 Win64" -DCMAKE_PREFIX_PATH="C:\Program Files\LLVM\lib\cmake" -DUSE_RELEASE_LIBS_IN_DEBUG=On ..
```

**NOTE:** The CMake variable ```USE_RELEASE_LIBS_IN_DEBUG``` is needed to build hsm-analyze in the Debug configuration against the ClangOnWin libraries since ClangOnWin does not include Debug libs.

**NOTE:** If the LLVM/Clang libs you want to link against use the MSVC CRT static libraries (rather than DLLs), you can enable the CMake variable ```USE_STATIC_CRT```.

* Open the generated sln and build
