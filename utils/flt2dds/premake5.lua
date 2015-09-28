
local action = _ACTION or ""
local examples = { "flt2dds" }

solution "flt2dds"
	location ( "build" )
	configurations { "Debug", "Release" }
	platforms {"x64", "x32"}
  startproject "flt2dds"  
  
  for k,v in ipairs(examples) do
    project(tostring(v))
      kind "ConsoleApp"
      language "C++"
      files { "**.h", "**.cpp", "**.c", "**.cc", "../../extern/nvtt/include/*.*", "../../extern/stb_image.h", "../../src/sgi.h" }
      includedirs { "./", "../../extern/", "../../extern/nvtt/include/", "../../src/" }
      links {"nvtt.lib"}      
      --[[
      -- NOTE: ADD THIS POST-BUILD COMMAND STEP TO THE SOLUTION ******
      buildrule {
        description="Copying nvtt.dll",
        dependencies={},
        commands = { "copy /Y $(SolutionDir)$(LocalDebuggerWorkingDirectory)\nvtt.dll $(SolutionDir)" },
        outputs = { "$(SolutionDir)nvtt.dll" }
        }
      --]]
      
      configuration { "linux" }
         links { "rt", "pthread" }

      configuration { "windows" }
         links { "user32" }

      configuration { "macosx" }        

      
      configuration { "Debug", "x32" }
        defines { "DEBUG", "_DEBUG" }
        flags { "Symbols", "ExtraWarnings"}                
        libdirs {"../../extern/nvtt/"..action.."/Debug.Win32/"}        
        objdir "./build/obj/x32/debug"
        targetdir "./build/bin/x32/debug/"
        
      configuration { "Debug", "x64" }
        defines { "DEBUG", "_DEBUG" }
        flags { "Symbols", "ExtraWarnings"}
        libdirs {"../../extern/nvtt/"..action.."/Debug.x64/"}        
        objdir "./build/obj/x64/debug"
        targetdir "./build/bin/x64/debug/"

      configuration {"Release", "x32"}
        defines { "NDEBUG" }
        flags { "Optimize", "ExtraWarnings"}    
        libdirs {"../../extern/nvtt/"..action.."/Release.Win32/"}        
        objdir "./build/obj/x32/release"
        targetdir "./build/bin/x32/release/"
        
      configuration {"Release", "x64"}
        defines { "NDEBUG" }
        flags { "Optimize", "ExtraWarnings"}    
        libdirs {"../../extern/nvtt/"..action.."/Release.x64/"}  
        objdir "./build/obj/x64/release"    
        targetdir "./build/bin/x64/release/"
  end