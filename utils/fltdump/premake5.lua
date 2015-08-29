-- WORK IN PROGRESS NOT USE --
local action = _ACTION or ""

solution "fltdump"
	location ( "build" )
	configurations { "Debug", "Release" }
	platforms {"x64", "x32"}
  
  	project "fltdump"
		kind "ConsoleApp"
		language "C++"
		files { "fltdump.cc", "../../src/flt.h" }
		includedirs { "./", "../../src/", "../../extern/vld/include/"}
	 		
		configuration { "windows" }         
			links { "user32" }
		
	    configuration { "Debug", "x32" }
            defines { "DEBUG", "_DEBUG" }
            flags { "Symbols", "ExtraWarnings"}                
            libdirs {"../../extern/vld/lib/Win32/"}
            objdir "./build/obj/x32/debug"
            targetdir "./build/bin/x32/debug/"
            
        configuration { "Debug", "x64" }
            defines { "DEBUG", "_DEBUG" }
            flags { "Symbols", "ExtraWarnings"}
            libdirs {"../../extern/vld/lib/Win64/"}
            objdir "./build/obj/x64/debug"
            targetdir "./build/bin/x64/debug/"

        configuration {"Release", "x32"}
            defines { "NDEBUG" }            
            flags { "Optimize", "ExtraWarnings"}            
            libdirs {""}
            objdir "./build/obj/x32/release"
            targetdir "./build/bin/x32/release/"
            
        configuration {"Release", "x64"}
            defines { "NDEBUG" }
            flags { "Optimize", "ExtraWarnings"}
            libdirs {""}
            objdir "./build/obj/x64/release"    
            targetdir "./build/bin/x64/release/"            
            
    project "fltdumpu"
		kind "ConsoleApp"
		language "C++"
		files { "fltdump.cc", "../../src/flt.h" }
		includedirs { "./", "../../src/", "../../extern/vld/include/"}
	 		
		configuration { "windows" }   
      defines { "FLT_UNIQUE_FACES" }
			links { "user32" }
		
	    configuration { "Debug", "x32" }
            defines { "DEBUG", "_DEBUG" }
            flags { "Symbols", "ExtraWarnings"}                
            libdirs {"../../extern/vld/lib/Win32/"}
            objdir "./build/obj/x32/debug"
            targetdir "./build/bin/x32/debug/"
            
        configuration { "Debug", "x64" }
            defines { "DEBUG", "_DEBUG" }
            flags { "Symbols", "ExtraWarnings"}
            libdirs {"../../extern/vld/lib/Win64/"}
            objdir "./build/obj/x64/debug"
            targetdir "./build/bin/x64/debug/"

        configuration {"Release", "x32"}
            defines { "NDEBUG" }            
            flags { "Optimize", "ExtraWarnings"}            
            libdirs {""}
            objdir "./build/obj/x32/release"
            targetdir "./build/bin/x32/release/"
            
        configuration {"Release", "x64"}
            defines { "NDEBUG" }
            flags { "Optimize", "ExtraWarnings"}
            libdirs {""}
            objdir "./build/obj/x64/release"    
            targetdir "./build/bin/x64/release/"            