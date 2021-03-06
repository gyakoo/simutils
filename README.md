# Sim Utils
**Work In Pogress**

List of small utility libraries written in C/C++ for simulation related work.

Most of the work is about Openflight parsing and storing.

# Libraries

library    | category | description
---------- | ---------| ------------
**flt.h** | openflight | Load geometry and other metadata from Openflight files
**sgi.h** | openflight | Decode SGI RGB/RGBA format from file. RLE and Verbatim modes supported
**vis.h** | rendering | Rendering functions, specific implementations vis_dx11, vis_dx12, vis_gl
<strike>**cigi.h** | <strike>communications | <strike>Common Image Generator Interface implementation (planned)
<strike>**pip.h** | <strike>communications | <strike>Foundation socket library based in IP protocol (UDP/TCP) (planned)
<strike>**proj.h** | <strike>projections | <strike>Functions to convert between different projections (planned)


# Utils
Set of tools making use of these libraries, c++11.
* flt2dds : Converts concurrently images into Direct Draw Surface (DDS) format using nvtt.
* flt2elev: Generate elevation maps out of flt files. Mosaic mode which generates every tile concurrently.
* flt2xml : Dumps openflight information into xml.
* fltextent: Dumps information about extension and bounding volumes of flt files.
* fltfind : Searchs for openflight files with specific opcodes.
* fltheader: Dumps header information of flt files.
* fltmlod : Makes files with LOD structure out of xml specification and external references.
* fltview : Visualizes an openflight file (wip).
* cigitest: Test of cigi lib (wip).
* dx12test: Test of vis_dx12 (wip).

# Building
You'll need premake (extern/premake5.exe) tool for building. Just click the .bat file to generate corresponding VS solution or generate your own target batch file.

# Why single file headers?

Citing <a href="https://github.com/nothings/stb/blob/master/README.md">nothings/stb</a>:

Making them a single file makes it very easy to just
drop them into a project that needs them. (Of course you can
still put them in a proper shared library tree if you want.)

Why not two files, one a header and one an implementation?
The difference between 10 files and 9 files is not a big deal,
but the difference between 2 files and 1 file is a big deal.
You don't need to zip or tar the files up, you don't have to
remember to attach *two* files, etc.
