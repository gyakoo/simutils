-- WORK IN PROGRESS NOT USE --
local action = _ACTION or ""

solution "fltview"
	location ( "build" )
	configurations { "Debug", "Release" }
	platforms {"native", "x64", "x32"}
  
  	project "fltview"
		kind "ConsoleApp"
		language "C++"
		files { "fltview.c", "../../flt/flt.h" }
		includedirs { "./", "../../flt/" }		
	 		
		configuration { "windows" }            
			 links { "gdi32", "winmm", "user32" }
		
		configuration "Debug"
      targetdir "./build/bin/debug/"
      objdir "./build/bin/debug/"
			defines { "DEBUG", "_DEBUG" }
			flags { "Symbols", "ExtraWarnings"}

		configuration "Release"
      targetdir "./build/bin/release/"
      objdir "./build/bin/release/"
			defines { "NDEBUG" }
			flags { "Optimize", "ExtraWarnings"}    