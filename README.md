# Sim Utils
Work In Pogress 
List of small utility libraries written in C/C++ for simulation related work.

# Libraries

library    | lastest version | category | description
---------- | --------------- | ---------| ------------
**pip.h** | 0.1a | communications | Foundation socket library based in IP protocol (UDP/TCP)
**cigi.h** | 0.1a | communications | Common Image Generator Interface implementation
**proj.h** | 0.1a | projections | Functions to convert between different projections
**flt.h** | 0.1a | openflight | Load geometry and other metadata from Openflight files
**sgi.h** | 0.1a | openflight | Decode SGI RGB/RGBA format from file. RLE and Verbatim modes supported
**vis.h** | 0.1a | rendering | Rendering functions, specific implementations vis_dx11, vis_dx12, vis_gl

# Utils
Set of applications and small tools making use of these libraries.
* flt2dds : Converts images into Direct Draw Surface (DDS) format.
* flt2elev: Generate elevation maps out of flt files.
* flt2xml : Dumps openflight information into xml.
* fltextent: Dumps extent information of flt files.
* fltfind : Searchs for openflight files with specific opcodes
* fltheader: Dumps header information of flt files.
* fltmlod : Makes files with LOD structure out of xml specification and external references.
* fltview : Visualizes an openflight file.
* cigitest: Test of cigi lib.
* dx12test: Test of vis_dx12.

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
