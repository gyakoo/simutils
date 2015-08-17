# flt2dds
A single-cpp file C++11 tool to convert all imagery data from Openflight graph into DDS. <br/>
 
# building
* Double click in the .bat file to generate for VS2012, or premake for another target.<br/>
* Be sure the <a href="https://github.com/gyakoo/flt/blob/master/flt_sgirgb.h">flt_sgirgb.h</a> file is under parent directory "../flt/" or copy it here and change premake4.lua
* In order to run/distribute you need to copy the corresponding nvtt.dll binary for your configuration in the same place where the flt2dds.exe executable is.

(premake does not support build-step so far.)

# features
* Multithreaded
* Supported image formats: 
 * sgi, rgb, rgba with a custom loader. <a href="https://github.com/gyakoo/flt/blob/master/flt_sgirgb.h">flt_sgirgb.h</a>
 * jpg, png, tga, bmp, psd, gif, hdr, pic, pnm. <a href="https://github.com/nothings/stb/blob/master/stb_image.h">stb_image.h</a>
* Optional best-fit block compression format for textures with alpha. (-a 0 option)
* Converts recursively with the -r option.

# todo
* Add CUDA version (nvtt) for performance improvement

# some tests
* 21GB rgb/rgba imagery data:
 * 3GB dds files
 * 7 minutes (8 threads/cores) (-q 0)

# dependencies
* For SGI RGB decoding it uses <a href="../../flt/flt_sgirgb.h">flt_sgirgb.h</a> library.
* For block compression uses <a href="https://github.com/castano/nvidia-texture-tools">nvtt</a>. Includes no-CUDA vs2012 binaries for win32 and x64 for both debug and release configurations.
* For other image formats uses <a href="https://github.com/nothings/stb/blob/master/stb_image.h">stb_image.h</a>. Included.

# help screen
```
flt2dds: Converts imagery data from Openflight to DirectX DDS files

Usage: $ flt2dds.exe <options> <file0> [<file1>...]
Options:
         -?   : This help screen
         -v   : Enable verbose mode. default=disabled
         -f   : Force DDS write even though the target DDS file already exists. default=disabled
         -m   : Generate mip maps. default=no
         -q N : DDS out quality (0:low 1:medium 2:high). default=0
         -a N : Alpha format (0:Auto 1:BC1 2:BC2 3:BC3). Auto uses variance of image, slower. default=2:BC2
         -t N : Force N threads. Values 0..512 (0:Max HW threads). default=0:Max HW threads
         -r w : Converts recursively all files matching wildcard 'w' using the input file paths.

         <file0>...: Files can be either any image file (rgb, png...) or FLT files where to look at
         Supported image formats: rgb, rgba, sgi, jpg, jpeg, png, tga, bmp, psd, gif, hdr, pic, pnm

Examples:
        Converts all images pointed recursively by master.flt with quality medium, forcing overwrite dds
          $ flt2dds.exe -q 1 -f -v master.flt

        Converts these three images
          $ flt2dds.exe image1.png image2.jpg image3.rgba

        Converts all rgb files recursively from directory c:\flight\ (note to add \ at end)
          $ flt2dds.exe -r *.rgb c:\flight\

        Converts images from these flt files
          $ flt2dds.exe models/tank.flt models/plane.flt
```


