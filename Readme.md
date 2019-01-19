# Build the Doppelkopf Server / AI Client

To build the server and ai client you will need a C++ compiler and CMake. This application also needs the [boost](https://www.boost.org) and [fmtlib](http://fmtlib.net) libraries installed on your system. On a MacOs you can install these via the brew package manager using the following instructions.

```
$ brew install cmake boost cppformat
```

To build this project use cmake

```
$ mkdir build
$ cd build
$ cmake ${PATH_TO_DOPPELKOPF_REPOSITORY}
$ make
```
