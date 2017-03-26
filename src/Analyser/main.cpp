#include "Module.h"
#include <Windows.h>
#include <Psapi.h>
#include <cstdio>
#include <ImageHlp.h>
#include <vector>
#include <string>
#include "Utils.h"

struct CommandLineOptions
{
	char*	sWorkingDir;
	bool	bNoDependencies;
	bool	bVerbose;
};

CommandLineOptions ParseCommanLineOptions( int* argc, char* argv[] )
{
	char** pTempArgs = (char**)alloca( sizeof( char* ) * *argc );
	int iStartPos = 0;
	int iEndPos = *argc - 1;


	CommandLineOptions oOptions;
	oOptions.sWorkingDir				= NULL;
	oOptions.bNoDependencies			= false;
	oOptions.bVerbose					= false;

	for( int i = 0; i < *argc; ++i )
	{
		bool bIsOption = true;
		if( _stricmp( argv[ i ], "-wd" ) == 0 )
		{
			if( ( i + 1 ) < *argc )
			{
				oOptions.sWorkingDir = argv[ i + 1 ];
				int iLength = strlen( oOptions.sWorkingDir );
				for( int c = 0; c < iLength; ++c )
				{
					// One of debug function doesn't work with "/"
					if( oOptions.sWorkingDir[ c ] == '/' )
					{
						fprintf( stderr, "Warning : Workingdir contains '/' instead of '\\'" );
						break;
					}
				}

				pTempArgs[ iEndPos ] = argv[ i ];
				iEndPos--;
				i++;
			}
		}
		else if( _stricmp( argv[ i ], "-nodep" ) == 0 )
		{
			oOptions.bNoDependencies = true;
		}
		else if( _stricmp( argv[ i ], "-verbose" ) == 0 )
		{
			oOptions.bVerbose = true;
		}
		else
			bIsOption = false;

		if( bIsOption )
		{
			pTempArgs[ iEndPos ] = argv[ i ];
			iEndPos--;
		}
		else
		{
			pTempArgs[ iStartPos ] = argv[ i ];
			iStartPos++;
		}
	}

	for( int i = 0; i < *argc; ++i )
		argv[ i ] = pTempArgs[ i ];

	*argc = iStartPos;

	return oOptions;
}

void PrintUsage()
{
	Log( E_LOG_INFO, "Usage : Analyser.exe <DLL/Exe> <Extension1>...<ExtensionN> \n" );
	Log( E_LOG_INFO, "    Option : -wd <Workingdir>     Set the path for DLL dependecies\n" );
	Log( E_LOG_INFO, "             -verbose             Log extra informations\n" );
	Log( E_LOG_INFO, "             -nodep               Only check specified module\n" );
	Log( E_LOG_INFO, "    Extension :\n" );
	for( int i = XED_EXTENSION_INVALID; i < XED_EXTENSION_LAST; ++i )
	{
		Log( E_LOG_INFO, "       %s\n", xed_extension_enum_t2str( (xed_extension_enum_t)i ) );
	}
}

int main( int argc, char* argv[] )
{
	CommandLineOptions oOptions = ParseCommanLineOptions( &argc, argv );
	if( argc < 3 )
	{
		Log( E_LOG_ERROR, "Error : You need to give the process DLL and extensions to check\n" );
		PrintUsage();
		return -1;
	}

	// Set working dir to a temp directory to avoid poluting user's working dir ( loadDataForExe create dummy pdb when it cannot find one )
	char sTempPath[ MAX_PATH + 1 ];
	char sTempDirectory[ MAX_PATH + 1 ];
	GetTempPath( MAX_PATH, sTempPath );
	GetTempFileName( sTempPath, "abc", 1, sTempDirectory );
	CreateDirectory( sTempDirectory, NULL );
	SetCurrentDirectory( sTempDirectory );

	const char* sMainModule = argv[ 1 ];

	if( oOptions.bVerbose )
	{
		SetLogLevel( E_VERBOSE );
	}

	std::vector<bool> oExtensionsToCheck( XED_EXTENSION_LAST, false );
	bool bHasValidExtension = false;
	for( int i = 2; i < argc; ++i )
	{
		xed_extension_enum_t eExtension = str2xed_extension_enum_t( argv[ i ] );
		if( eExtension == XED_EXTENSION_INVALID )
			Log( E_LOG_ERROR, "%s is not a valid extension\n", argv[ i ] );
		else
		{
			bHasValidExtension = true;
			oExtensionsToCheck[ eExtension ] = true;
		}
	}
	if( bHasValidExtension == false )
	{
		Log( E_LOG_ERROR, "No valid extension to check\n" );
		PrintUsage();
		return -1;
	}

	HRESULT hr = CoInitialize( NULL );
	if( FAILED( hr ) )
	{
		Log( E_LOG_ERROR_VERBOSE, "CoInitialize failed with error 0x%08X\n", hr );
		return -1;
	}

	std::vector<std::string> oSearchPaths;
	PLOADED_IMAGE pImage = ImageLoad( sMainModule, 0 );
	if( pImage == NULL )
	{
		Log( E_LOG_ERROR, "Could not load module %s\n", sMainModule );
		return -1;
	}
	bool bMainModuleIsX64 = pImage->FileHeader->FileHeader.Machine != IMAGE_FILE_MACHINE_I386;

	if( oOptions.sWorkingDir != NULL )
	{
		SetDllDirectory( oOptions.sWorkingDir );
		oSearchPaths.push_back( oOptions.sWorkingDir );
	}

	{ // Add system path to search directory
		char sSystemPath[ MAX_PATH ];
		int iPathLength = 0;
#ifdef _WIN64
		if( bMainModuleIsX64 )
			iPathLength = GetSystemDirectory( sSystemPath, MAX_PATH );
		else
			iPathLength = GetSystemWow64Directory( sSystemPath, MAX_PATH );
#else
		BOOL bIsWoW64 = FALSE;
		IsWow64Process( GetCurrentProcess(), &bIsWoW64 );
		if( bMainModuleIsX64 )
		{
			if( bIsWoW64 )
				iPathLength = GetSystemDirectory( sSystemPath, MAX_PATH );
		}
		else
		{
			if( bIsWoW64 )
				iPathLength = GetSystemWow64Directory( sSystemPath, MAX_PATH );
			else
				iPathLength = GetSystemDirectory( sSystemPath, MAX_PATH );
		}
#endif // _WIN64

		if( iPathLength > 0 )
		{
			oSearchPaths.push_back( sSystemPath );
		}
	}

	AN_ASSERT( oExtensionsToCheck.size() == XED_EXTENSION_LAST );

	if( oOptions.bNoDependencies == false )
	{
		std::vector<std::string> oDependencies;
		if( bMainModuleIsX64 == false )
			ListDllDependeciesFromPath<LOADED_IMAGE32>( (LOADED_IMAGE32*)pImage, oSearchPaths, oDependencies );
		else
			ListDllDependeciesFromPath<LOADED_IMAGE64>( (LOADED_IMAGE64*)pImage, oSearchPaths, oDependencies );
		for( int i = 0; i < oDependencies.size(); ++i )
		{
			Log( E_LOG_INFO_VERBOSE, "Loading module %s\n", oDependencies[ i ].c_str() );
			Module oModule;
			if( oModule.InitModule( oDependencies[ i ].c_str(), oSearchPaths ) == true )
			{
				oModule.CheckAllFunctions( oExtensionsToCheck );
				Log( E_LOG_INFO, "\n" );
			}
			else
			{
				//Log( E_LOG_INFO, "Could not load module %s\n", oDependencies[ i ].c_str() );
			}
			oModule.DestroyModule();

			Log( E_LOG_INFO_VERBOSE, "----------------------------------------\n" );
		}
	}
	{
		Module oModule;
		if( oModule.InitModule( sMainModule, oSearchPaths ) == true )
		{
			oModule.CheckAllFunctions( oExtensionsToCheck );
		}
		else
		{
			Log( E_LOG_ERROR, "Could not load module %s\n", sMainModule );
		}
	}

	if( IsDebuggerPresent() )
	{
		system("PAUSE");
	}

	return 0;
}