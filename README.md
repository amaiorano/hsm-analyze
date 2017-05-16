# hsm-analyze

hsm-analyze is a tool based on [Clang LibTooling](https://clang.llvm.org/docs/LibTooling.html) that analyzes C++ code that makes use of [the HSM library](https://github.com/amaiorano/hsm). It can be used to:

- Provide useful information about potential problems with a given state machine
- Output graphs of state machines (e.g. GraphViz dot files)


## Usage

- Install the latest distribution of LLVM (Windows: [here](http://releases.llvm.org/download.html) or [here](https://sourceforge.net/projects/clangonwin/files/MsvcBuild/) if you plan to compile hsm-analyze).

- [Download](https://link/to/releases/here) (or build) the latest version of hsm-analyze, and copy the single executable to your LLVM/bin directory (e.g. ```C:\Program Files\LLVM\bin``` on Windows). This is required for most LibTooling-based tools to access the Clang core headers.

- Install [GraphViz](http://www.graphviz.org/Download.php) to render state machines graphs.

Like all Clang tools based on LibTooling, hsm-analyze can either use a compilation database, or you can pass compilation arguments via command line following a double-dash ("--"). Read more about how to use a compilation database [here](https://clang.llvm.org/docs/HowToSetupToolingForLLVM.html). 

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

* Download and install the latest binary distribution of [ClangOnWin from here](https://sourceforge.net/projects/clangonwin/files/MsvcBuild/)

* Assuming you've installed it to the default location, C:\Program Files\LLVM:

```
cd path/to/hsm-analyze
mkdir build && cd build
cmake -G "Visual Studio 14 2015 Win64" -DCMAKE_PREFIX_PATH="C:\Program Files\LLVM\lib\cmake\clang" -DUSE_RELEASE_LIBS_IN_DEBUG=On ..
```

* Open the generated sln and build
