if _ACTION == "vs2015" then
	visualSuffix = "_2015"
	visualVersionDefine = "_VS2015_"
	visualPath = "C:/Program Files (x86)/Microsoft Visual Studio 14.0/"
elseif _ACTION == "vs2017" then
	visualSuffix = "_2017"
	visualVersionDefine = "_VS2017_"
	visualPath = "C:/Program Files (x86)/Microsoft Visual Studio/2017/Community/"
elseif _ACTION == "vs2012" then
	visualSuffix = "_2012"
	visualVersionDefine = "_VS2012_"
	visualPath = "C:/Program Files (x86)/Microsoft Visual Studio 11.0/"
end

sln = solution	"Analyser"
	location (_OPTIONS["to"])
	configurations 	{ "Debug", "Release" }

	platforms 		{ "x32", "x64" }
	
	objdir			( "../Temp/".._ACTION )
	
	defines 			{ "_HAS_EXCEPTIONS=0" }
	if _ACTION ~= "vs2012" then
		linkoptions			{ "/DEBUG:FULL"  }
	end
	
	configuration{ "x32" }
		libdirs				{ "../Lib/Win32" }
	configuration{ "x64" }
		libdirs				{ "../Lib/x64" }
	
	configuration{ "Debug" }
		defines 			{ "_DEBUG", "__AN_DEBUG__", "_ITERATOR_DEBUG_LEVEL=0", visualVersionDefine}
	configuration{ "Release" }
		defines 			{ "NDEBUG=1", "__AN_RELEASE__", "_ITERATOR_DEBUG_LEVEL=0", visualVersionDefine }

-------------------------------------------------------------
name = "Analyser"
project( name, "" )
	location 			( "../Project" )
	
	local analyserCode = "../src/Analyser/"
	uuid				( os.uuid() )
	
	kind					"consoleApp"
	language 				"C++"
	
	configuration{ "x32" }
		libdirs				{ visualPath.."DIA SDK/lib", "../Lib/".._ACTION.."/Win32" }
	configuration{ "x64" }
		libdirs				{ visualPath.."DIA SDK/lib/amd64", "../Lib/".._ACTION.."/x64" }
	configuration{ }
		
	includedirs				{ "../Include/xed", visualPath.."DIA SDK/include" }
	
	libdirs					{ disasmCode }
	links					{ "xed", "diaguids", "ImageHlp" }
	
	files 					{ analyserCode.."**.*" }
	vpaths 					{ ["*"] = analyserCode.."/**" }
	excludes				( {	analyserCode.."**.bak", 	analyserCode.."**.aps",	analyserCode.."**.gch",	analyserCode.."**.d",
								analyserCode.."**.cpp.*", analyserCode.."**.h.*" }  )

	flags				{ "StaticRuntime" }

	configuration{ "x32" }
		targetdir			( "../Bin" )
	configuration{ "x64" }
		targetdir			( "../Bin" )
	
	configuration{ "Debug" }
		targetname			( name.."_d" )
		flags 				{ "ExtraWarnings", "No64BitChecks","NoEditAndContinue", "NoExceptions", "NoRTTI", "Symbols" }
	configuration{ "Release" }
		targetname			( name.."_r" )
		flags 			{ "Optimize", "ExtraWarnings", "No64BitChecks", "NoEditAndContinue", "NoExceptions", "NoRTTI", "Symbols" }
		
	configuration{ "x64", "Debug" }
		targetname			( name.."64_d" )
	configuration{ "x64", "Release" }
		targetname			( name.."64_r" )

	configuration{ "x32 or x64", "Debug" }
		buildoptions 		{ "/Ob1" }

	-- Reset the configuration flag
	configuration{}
	
-------------------------------------------------------------
