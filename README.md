# Sim Utils
List of small utility libraries written in C/C++ for simulation related work.

# Libraries
## /com

Communication libraries.
library    | lastest version | description
---------- | --------------- | --------------------------------
**com_ip.h** | 0.1 | Foundation socket library based in IP protocol (UDP/TCP)
**com_cigi.h** | 0.1 | Common Image Generator Interface functions

## /proj

Map projections functions.
library    | lastest version | description
---------- | --------------- | --------------------------------
**proj.h** | 0.1 | Functions to convert between different projections

## /flt
Openflight related utilities.

library    | lastest version | description
---------- | --------------- | --------------------------------
**flt.h** | 0.1 | Load geometry and other metadata from Openflight files
**flt_sgirgb.h** | 0.1 | Decode SGI RGB/RGBA format from file. RLE and Verbatim modes supported.


# Utils
Set of applications and small tools making use of these libraries.
## flt2dds
A single-cpp file C++11 tool to convert all imagery data from Openflight graph into DDS. <br/>
See <a href="utils/flt2dds/readme.md">flt2dds readme</a> for more information.


# Why single file headers?

Citing <a href="https://github.com/nothings/stb/blob/master/README.md">nothings/stb</a>:
Windows doesn't have standard directories where libraries
live. That makes deploying libraries in Windows a lot more
painful than open source developers on Unix-derivates generally
realize. (It also makes library dependencies a lot worse in Windows.)

There's also a common problem in Windows where a library was built
against a different version of the runtime library, which causes
link conflicts and confusion. Shipping the libs as headers means
you normally just compile them straight into your project without
making libraries, thus sidestepping that problem.

Making them a single file makes it very easy to just
drop them into a project that needs them. (Of course you can
still put them in a proper shared library tree if you want.)

Why not two files, one a header and one an implementation?
The difference between 10 files and 9 files is not a big deal,
but the difference between 2 files and 1 file is a big deal.
You don't need to zip or tar the files up, you don't have to
remember to attach *two* files, etc.