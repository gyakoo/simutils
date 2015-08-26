# Sim Utils
List of small utility libraries written in C/C++ for simulation related work.

# Libraries

library    | lastest version | category | description
---------- | --------------- | ---------| ------------
**com_ip.h** | 0.1a | communications | Foundation socket library based in IP protocol (UDP/TCP)
**com_cigi33.h** | 0.1a | communications | Common Image Generator Interface 3.3 implementation
**proj.h** | 0.1a | projections | Functions to convert between different projections
**flt.h** | 0.1a | openflight | Load geometry and other metadata from Openflight files
**sgi.h** | 0.1a | openflight | Decode SGI RGB/RGBA format from file. RLE and Verbatim modes supported
**rend.h** | 0.1a | rendering | Rendering functions, supporting DX11 and OpenGL
**fltr.h** | 0.1a | openfligh | Specific function for Openflight Rendering

# Utils
Set of applications and small tools making use of these libraries.
### flt2dds
A single-cpp file C++11 tool to convert all imagery data from Openflight graph into DDS. <br/>
See <a href="https://github.com/gyakoo/simutils/tree/master/utils/flt2dds">flt2dds readme</a> for more information.

### fltviewer
Small OpenGL FLT viewer to showcase the use of flt.h.

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
