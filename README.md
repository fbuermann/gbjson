gbjson - A GenBank to JSON converter
====================================

Introduction
------------

This program interconverts GenBank and JSON formats. The program contains two
executables. gb2json converts GenBank to JSON, and json2gb does the reverse.

Example usage
-------------
```shell
$ gb2json in.gb out.json  
$ json2gb in.json out.gb
```

Building from source
--------------------
Use [CMake](https://cmake.org/) to build from source.

### UNIX-like operating systems
```shell
$ cd gbjson  
$ mkdir build  
$ cd build  
$ cmake -G "Unix Makefiles" ..  
$ make
```

### Windows
Building from source has been tested with 
[Visual Studio/MSVC 2019](https://visualstudio.microsoft.com/) using [CMake](https://cmake.org/).

Dependencies
------------

__gbjson__ depends on the [RapidJSON](http://rapidjson.org/) and
[Lean Mean C++ Option Parser](http://optionparser.sourceforge.net/optionparser_8h_source.html) libraries.
They are included in the source tree.

Use as a library
-------------------------

__gbjson__ can be easily used as a C++ library.
Include gbjson.h in your source code. The functions _gb2json_ and _json2gb_ are the API.

Source code documentation
-------------------------

Source code documentation can be extracted with [Doxygen](http://www.doxygen.org/):  
```shell
$ doxygen Doxyfile
```