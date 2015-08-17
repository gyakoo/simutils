-- WORK IN PROGRESS NOT USE --
local action = _ACTION or ""

solution "fltview"
	location ( "build" )
	configurations { "Debug", "Release" }
	platforms {"native", "x64", "x32"}
  
  	project "fltview"
		kind "ConsoleApp"
		language "C++"
		files { "fltview.cpp", "../../flt/flt.h" }
		includedirs { "./", "../../flt/" }		
	 		
		configuration { "windows" }            
			 links { "gdi32", "winmm", "user32" }
		
		configuration "Debug"
            targetdir "./bin/debug/"
            objdir "./build/debug/"
			defines { "DEBUG", "_DEBUG" }
			flags { "Symbols", "ExtraWarnings"}

		configuration "Release"
            targetdir "./bin/release/"
            objdir "./bin/release/"
			defines { "NDEBUG" }
			flags { "Optimize", "ExtraWarnings"}    