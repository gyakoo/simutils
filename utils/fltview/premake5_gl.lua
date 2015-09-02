-- WORK IN PROGRESS NOT USE --
local action = _ACTION or ""
local build="build"..action
solution "fltview_gl"
	location ( build )
	configurations { "Debug", "Release" }
	platforms {"x64", "x32"}
  
  	project "fltview_gl"
		kind "ConsoleApp"
		language "C++"
		files { "**.cc", "**.h", "**.cpp", "../../src/flt.h", "../../src/vis.h", "../../extern/GLFW/**.h",
        "../../extern/imgui/stb_truetype.h", 
        "../../extern/imgui/stb_textedit.h",
        "../../extern/imgui/stb_rect_pack.h",
        "../../extern/imgui/imgui_internal.h",
        "../../extern/imgui/imgui_impl_glfw.h",
        "../../extern/imgui/imgui.h",
        "../../extern/imgui/imconfig.h",
        "../../extern/imgui/imgui_impl_glfw.cpp",
        "../../extern/imgui/imgui_draw.cpp",
        "../../extern/imgui/imgui_demo.cpp",
        "../../extern/imgui/imgui.cpp" }
		includedirs { "./", "../../src/", "../../extern/vld/include/", "../../extern/GLFW/", "../../extern/imgui/"}
	 		
		configuration { "windows" }   
            defines { "VIS_GL" }
			links { "gdi32", "winmm", "user32", "glfw3", "opengl32" }
		
	    configuration { "Debug", "x32" }
            defines { "DEBUG", "_DEBUG" }
            flags { "Symbols", "ExtraWarnings"}                
            libdirs {"../../extern/vld/lib/Win32/", "../../extern/GLFW/lib/release/x86/"}
            objdir (build.."/obj/x32/debug")
            targetdir (build.."/bin/x32/debug/")
            
        configuration { "Debug", "x64" }
            defines { "DEBUG", "_DEBUG" }
            flags { "Symbols", "ExtraWarnings"}
            libdirs {"../../extern/vld/lib/Win64/", "../../extern/GLFW/lib/release/x86/"}
            objdir (build.."/obj/x64/debug")
            targetdir (build.."/bin/x64/debug/")

        configuration {"Release", "x32"}
            defines { "NDEBUG" }            
            flags { "Optimize", "ExtraWarnings"}            
            libdirs {"../../extern/GLFW/lib/release/x86/"}
            objdir (build.."/obj/x32/release")
            targetdir (build.."/bin/x32/release/")
            
        configuration {"Release", "x64"}
            defines { "NDEBUG" }
            flags { "Optimize", "ExtraWarnings"}
            libdirs {"../../extern/GLFW/lib/release/x86/"}
            objdir (build.."/obj/x64/release")
            targetdir (build.."/bin/x64/release/")