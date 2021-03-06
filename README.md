# ExtensionAnalyser

Check instructions using specific extension in DLL/EXE files.

### Usage examples
    Check SSE4 instructions in Mydll.dll and dll dependencies :
        Analyser.exe Mydll.dll SSE4
    Check SSE4 and AVX instructions in Mydll.dll and dll dependencies :
      Analyser.exe Mydll.dll SSE4 AVX
    Check SSE4 instructions in Mydll.dll and dll dependencies, look for dependencies in "C:\DEPENDENCIES_PATH\" :
        Analyser.exe Mydll.dll SSE4 -wd "C:\DEPENDENCIES_PATH\"
    Check SSE4 instructions in Mydll.dll only :
        Analyser.exe Mydll.dll SSE4 -nodep
    Check SSE4 instructions in Mydll.dll and dll dependencies, log extra info :
        Analyser.exe Mydll.dll SSE4 -verbose
    Log available extensions :
        Analyser.exe
        
### How to build

Execute Project/GenerateSolution.bat to generate Visual Studio 2017 solution.
For other versions of Visual Studio you will need to recompile Intel Xed lib.

### Third Party Software

ExtensionAnalyser uses Intel Xed to decode binary files.
https://github.com/intelxed/xed
