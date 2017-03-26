#define _CRT_SECURE_NO_WARNINGS
#include "Module.h"

#include <Windows.h>
#include "Utils.h"
#include <Psapi.h>
#include <algorithm>
#include <diacreate.h>
#include <ImageHlp.h>

#ifdef _WIN64
#if defined(_VS2015_) || defined(_VS2017_)
static const wchar_t* g_sMsdiaDllName = L"msdia140_64.dll";
#elif defined(_VS2012_) 
static const wchar_t* g_sMsdiaDllName = L"msdia110_64.dll";
#endif // (_VS2015_) || defined(_VS2017_)
#else
#if defined(_VS2015_) || defined(_VS2017_)
static const wchar_t* g_sMsdiaDllName = L"msdia140.dll";
#elif defined(_VS2012_) 
static const wchar_t* g_sMsdiaDllName = L"msdia110.dll";
#endif // (_VS2015_) || defined(_VS2017_)
#endif // __X64


PLOADED_IMAGE ImageLoadHelper( const char* sModuleName, std::vector<std::string> oSearchPaths )
{
	PLOADED_IMAGE pLoadedImage = ImageLoad( sModuleName, NULL );
	if( pLoadedImage != NULL )
		return pLoadedImage;

	{
		int iPathIndex = 0;
		while( pLoadedImage == NULL && iPathIndex < oSearchPaths.size() )
		{
			pLoadedImage = ImageLoad( sModuleName, oSearchPaths[ iPathIndex ].c_str() );
			if( pLoadedImage != NULL )
				return pLoadedImage;
			iPathIndex++;
		}
	}
	return NULL;
}

// Load module's code sections in memory at proper address and get PDB info
bool LoadModuleImage( Module* pModule, const char* sModuleName, std::vector<std::string> oSearchPaths )
{
	PLOADED_IMAGE pLoadedImage = ImageLoadHelper( sModuleName, oSearchPaths );
	if( pLoadedImage == NULL )
		return false;
	int iSizeOfImage = 0;
	if( pLoadedImage->FileHeader->FileHeader.Machine == IMAGE_FILE_MACHINE_I386 )
		iSizeOfImage = ( (LOADED_IMAGE32*)pLoadedImage )->FileHeader->OptionalHeader.SizeOfImage;
	else
		iSizeOfImage = ( (LOADED_IMAGE64*)pLoadedImage )->FileHeader->OptionalHeader.SizeOfImage;

	// Copy sections to their place in memory
	uint8* pBuffer = (uint8*)VirtualAlloc( NULL, iSizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE );
	// memset( pBuffer, 0xCC, iSizeOfImage );
	for( int iSection = 0; iSection < pLoadedImage->NumberOfSections; ++iSection )
	{
		PIMAGE_SECTION_HEADER pSection = pLoadedImage->Sections + iSection;
		// We only need code section
		if( ( pSection->Characteristics & IMAGE_SCN_CNT_CODE ) != 0 && pSection->SizeOfRawData > 0 )
		{
			uint8* pDest = pBuffer + pSection->VirtualAddress;
			uint8* pSrc = pLoadedImage->MappedAddress + pSection->PointerToRawData;
			memcpy( pDest, pSrc, pSection->SizeOfRawData );
		}
	}

	// Look for PDB filename
	ULONG dirEntrySize;
	IMAGE_DEBUG_DIRECTORY* pBaseDebugDirectory = reinterpret_cast<IMAGE_DEBUG_DIRECTORY*>( ImageDirectoryEntryToDataEx(
		reinterpret_cast<PVOID>( pLoadedImage->MappedAddress ),
		FALSE,
		IMAGE_DIRECTORY_ENTRY_DEBUG, &dirEntrySize, NULL ) );
	for( int i = 0; i < dirEntrySize / sizeof( IMAGE_DEBUG_DIRECTORY ); ++i )
	{
		IMAGE_DEBUG_DIRECTORY* pDebugDirectory = pBaseDebugDirectory + i;
		if( pDebugDirectory->Type == IMAGE_DEBUG_TYPE_CODEVIEW )
		{
			struct CV_HEADER
			{
				DWORD Signature;
				DWORD Offset;
			};

			CV_HEADER* pHeader = (CV_HEADER*)( pLoadedImage->MappedAddress + pDebugDirectory->PointerToRawData );
			if( memcmp( &pHeader->Signature, "RSDS", 4 ) == 0 )
			{
				struct CV_INFO_PDB70
				{
					DWORD  CvSignature;
					GUID Signature;
					DWORD Age;
					BYTE PdbFileName[];
				};
				CV_INFO_PDB70* pPDBHeader = (CV_INFO_PDB70*)( pHeader );
				pModule->m_oPDBInfo.sPDBName			= (char*) pPDBHeader->PdbFileName;
				pModule->m_oPDBInfo.oSignature		= pPDBHeader->Signature;
				pModule->m_oPDBInfo.iFileSignature	= pPDBHeader->CvSignature;
				pModule->m_oPDBInfo.iAge				= pPDBHeader->Age;
				pModule->m_oPDBInfo.bValid			= true;
				break;
			}
		}
	}

	pModule->m_pLoadAddress = pBuffer;
	pModule->m_bIsX64 = pLoadedImage->FileHeader->FileHeader.Machine != IMAGE_FILE_MACHINE_I386;

	ImageUnload( pLoadedImage );

	return true;
}

bool LoadPDB( Module* pModule, const std::vector<std::string>& oSearchPaths )
{
	if( pModule->m_oPDBInfo.bValid )
	{
		Log( E_LOG_INFO_VERBOSE, "Trying to load PDB %s \n", pModule->m_oPDBInfo.sPDBName.c_str() );
		wchar_t wsModName[ _MAX_PATH ];
		mbstowcs( wsModName, pModule->m_oPDBInfo.sPDBName.c_str(), sizeof( wsModName ) / sizeof( wsModName[ 0 ] ) );
		HRESULT hr = pModule->m_pSource->loadAndValidateDataFromPdb( wsModName, &pModule->m_oPDBInfo.oSignature, pModule->m_oPDBInfo.iFileSignature, pModule->m_oPDBInfo.iAge );

		if( SUCCEEDED( hr ) )
			return true;
		else if( hr == E_PDB_NOT_FOUND || hr == E_PDB_FILE_SYSTEM )
			Log( E_LOG_ERROR_VERBOSE, "PDB file %s not found\n", pModule->m_oPDBInfo.sPDBName.c_str() );
		else if( hr == E_PDB_INVALID_SIG || hr == E_PDB_INVALID_AGE )
			Log( E_LOG_ERROR_VERBOSE, "PDB file %s does not match module %s\n", pModule->m_oPDBInfo.sPDBName.c_str(), pModule->m_sModuleName.c_str() );
		else if( hr == E_PDB_FORMAT )
			Log( E_LOG_ERROR_VERBOSE, "PDB file %s is invalid\n", pModule->m_oPDBInfo.sPDBName.c_str() );
		else
			Log( E_LOG_ERROR_VERBOSE, "loadAndValidateDataFromPdb failed with error 0x%08X loading %s\n", hr, pModule->m_oPDBInfo.sPDBName.c_str() );
	}
	
	for( int i = 0; i < oSearchPaths.size(); ++i )
	{
		Log( E_LOG_INFO_VERBOSE, "Trying to load PDB in directory %s\n", oSearchPaths[i].c_str() );

		std::string sModulePath = pModule->m_sModuleName.c_str();
		if( PathIsRelative( sModulePath.c_str() ) )
		{
			if( oSearchPaths[ i ].length() > 0 && ( oSearchPaths[ i ].back() == '/' || oSearchPaths[ i ].back() == '\\' ) )
				sModulePath = oSearchPaths[ i ] + sModulePath;
			else
				sModulePath = oSearchPaths[ i ] + "\\" + sModulePath;
		}
		
		wchar_t wsModName[ _MAX_PATH ];
		mbstowcs( wsModName, sModulePath.c_str(), sizeof( wsModName ) / sizeof( wsModName[ 0 ] ) );
		wchar_t wsModDir[ _MAX_PATH ];
		mbstowcs( wsModDir, oSearchPaths[ i ].c_str(), sizeof( wsModDir ) / sizeof( wsModDir[ 0 ] ) );
		HRESULT hr = pModule->m_pSource->loadDataForExe( wsModName, wsModDir, NULL );
		if( FAILED( hr ) )
		{
			if( hr == E_PDB_NOT_FOUND || hr == E_PDB_FILE_SYSTEM )
			{
				Log( E_LOG_ERROR_VERBOSE, "PDB file not found\n", pModule->m_oPDBInfo.sPDBName.c_str() );
			}
			else if( hr == E_PDB_INVALID_SIG || hr == E_PDB_INVALID_AGE )
			{
				Log( E_LOG_ERROR_VERBOSE, "PDB file %s does not match module %s\n", pModule->m_oPDBInfo.sPDBName.c_str(), pModule->m_sModuleName.c_str() );
			}
			else
				Log( E_LOG_ERROR_VERBOSE, "loadAndValidateDataFromPdb failed with error 0x%08X\n", hr );
		}
		else
			return true;
	}

	return false;
}

bool Module::InitModule( const char* sModuleName, const std::vector<std::string>& oSearchPaths )
{
	m_pLoadAddress			= NULL;
	m_pSession				= NULL;
	m_pSource				= NULL;
	m_sModuleName			= sModuleName;
	m_oPDBInfo.bValid		= false;

	if( LoadModuleImage( this, sModuleName, oSearchPaths ) == false )
	{
		Log( E_LOG_ERROR_VERBOSE, "LoadModuleImage failed\n" );
		return false;
	}

	HRESULT hr = NoRegCoCreate( g_sMsdiaDllName, CLSID_DiaSource, __uuidof( IDiaDataSource ), (void **)&m_pSource );
	if( FAILED( hr ) )
	{
		Log( E_LOG_ERROR_VERBOSE, "CoCreateInstance failed with error 0x%08X\n", hr );
		return false;
	}

	if( LoadPDB( this, oSearchPaths ) == false )
		return false;

	hr = m_pSource->openSession( &m_pSession );
	if( FAILED( hr ) )
	{
		Log( E_LOG_ERROR_VERBOSE, "openSession failed with error 0x%08X\n", hr );
		return false;
	}

	hr = m_pSession->put_loadAddress( (uint64)m_pLoadAddress );
	if( FAILED( hr ) )
	{
		Log( E_LOG_ERROR_VERBOSE, "put_loadAddress failed with error 0x%08X\n", hr );
		return false;
	}

	return true;
}

void Module::DestroyModule()
{
	if( m_pSession != NULL )
		m_pSession.Release();
	if( m_pSource != NULL )
		m_pSource.Release();

	if( m_pLoadAddress != NULL )
	{
		VirtualFree( m_pLoadAddress, 0, MEM_RELEASE );
		m_pLoadAddress = NULL;
	}
}

struct Instruction
{
	uint64					iAddress;
	const xed_inst_t*		pInstruction;
};

bool GetInstructions( const xed_state_t* pState, void* pVa, int iFunctionSize, const std::vector<bool>& oExtensionsToCheck, std::vector<Instruction>& oOutVA, uint64 iIsaVa, bool& bCheckForIsa )
{
	AN_ASSERT( oExtensionsToCheck.size() == XED_EXTENSION_LAST );

	int iOffset = 0;
	xed_uint8_t* pLoc = (xed_uint8_t*)( pVa );

	const xed_inst_t*		pLastInstruction = NULL;

	while( iOffset < iFunctionSize )
	{
		int iInstructionSize = iFunctionSize - iOffset;
		xed_decoded_inst_t oInstruction;
		xed_decoded_inst_zero_set_mode( &oInstruction, pState );
		xed_error_enum_t eError = xed_decode( &oInstruction, pLoc, iInstructionSize );

		if( eError != XED_ERROR_NONE )
		{
			return false;
		}
		iInstructionSize = xed_decoded_inst_get_length( &oInstruction );

		if( xed_inst_iclass( oInstruction._inst ) == XED_ICLASS_CMP )
		{
			const xed_operand_values_t* pOperands = xed_decoded_inst_operands_const( &oInstruction );
			xed_int64_t iDisplacement = xed_decoded_inst_get_memory_displacement( &oInstruction, 0 );
			if( ( (uint64)pLoc + iDisplacement + iInstructionSize ) == iIsaVa )
				bCheckForIsa = true;
		}

		pLastInstruction = oInstruction._inst;
		xed_extension_enum_t eExtension = xed_decoded_inst_get_extension( &oInstruction );

		if( oExtensionsToCheck[ eExtension ] == true )
		{
			Instruction oInst;
			oInst.iAddress		= (uint64)pLoc;
			oInst.pInstruction	= oInstruction._inst;

			oOutVA.push_back( oInst );
		}
		
		iOffset += iInstructionSize;
		pLoc += iInstructionSize;
	}

	return true;
}

#define MAX_BUFFER_SIZE ( 4 * 1024 )

inline int Min( int a, int b )
{
	return ( a < b ) ? a : b;
}

bool Module::CheckAllFunctions( const std::vector<bool>& oExtensionsToCheck )
{
	Log( E_LOG_INFO, "Checking module %s : ", m_sModuleName.c_str() );

	CComPtr<IDiaSymbol> pGlobal;
	HRESULT hr = m_pSession->get_globalScope( &pGlobal );
	if( FAILED( hr ) )
	{
		Log( E_LOG_ERROR_VERBOSE, "get_globalScope 0x%08X\n", hr );
		return false;
	}

	CComPtr<IDiaEnumSymbols> pFunctionEnumerator;
	hr = pGlobal->findChildren( SymTagEnum::SymTagFunction, NULL, nsfCaseInsensitive | nsfUndecoratedName, &pFunctionEnumerator );
	if( FAILED( hr ) )
	{
		Log( E_LOG_ERROR_VERBOSE, "pGlobal->findChildren 0x%08X\n", hr );
		return false;
	}

	uint64 iIsaVa = 0;
	{ // Look for "instruction set available" variable's virtual address
		CComPtr<IDiaEnumSymbols> pIsaEnum;
		hr = pGlobal->findChildren( SymTagEnum::SymTagData, L"__isa_available", nsfCaseInsensitive | nsfUndecoratedName, &pIsaEnum );
		ULONG celt2;
		CComPtr< IDiaSymbol > pIsaSymbol;
		if( SUCCEEDED( hr = pIsaEnum->Next( 1, &pIsaSymbol, &celt2 ) ) && celt2 == 1 )
		{
			pIsaSymbol->get_virtualAddress( &iIsaVa );
			pIsaSymbol = NULL;
		}
	}

	std::vector<Instruction> oInstructions;

	xed_state_t oXedState;
	xed_tables_init();
	xed_state_zero( &oXedState );
	if( m_bIsX64 )
	{
		xed_state_init( &oXedState,
						XED_MACHINE_MODE_LONG_64,
						XED_ADDRESS_WIDTH_64b,
						XED_ADDRESS_WIDTH_64b );
	}
	else
	{
		xed_state_init( &oXedState,
						XED_MACHINE_MODE_LONG_COMPAT_32,
						XED_ADDRESS_WIDTH_32b,
						XED_ADDRESS_WIDTH_32b );
	}

	bool bFoundInstructions = false;

	char* pBuffer = (char*)malloc( MAX_BUFFER_SIZE );

	ULONG celt;
	CComPtr< IDiaSymbol > pFunction;
	while( SUCCEEDED( hr = pFunctionEnumerator->Next( 1, &pFunction, &celt ) ) && celt == 1 )
	{
		uint64 iVa = 0;
		hr = pFunction->get_virtualAddress( &iVa );
		ULONGLONG iLength = 0;
		hr = pFunction->get_length( &iLength ); // Sometime returns length bigger than real code length ( bug? )

		oInstructions.clear();
		bool bCheckIsa = false;
		bool bSuccess = GetInstructions( &oXedState, (void*)iVa, iLength, oExtensionsToCheck, oInstructions, iIsaVa, bCheckIsa );
		if( oInstructions.size() > 0 )
		{
			if( bFoundInstructions == false )
			{
				Log( E_LOG_INFO, "\n" );
				bFoundInstructions = true;
			}
			// Print funtion name
			{
				BSTR sFuncName;
				pFunction->get_name( &sFuncName );
				wcstombs( pBuffer, sFuncName, Min( SysStringLen( sFuncName ) + 1, MAX_BUFFER_SIZE ) );
				::SysFreeString( sFuncName );
				Log( E_LOG_INFO, "    Function %s : ", pBuffer );
				if( bCheckIsa )
					Log( E_LOG_INFO, "( Check for instruction set )\n" );
				else
					Log( E_LOG_INFO, "\n" );

			}
			// Print every instructions found
			for( int i = 0; i < oInstructions.size(); ++i )
			{
				IDiaEnumLineNumbers* pLineEnum = NULL;
				m_pSession->findLinesByVA( oInstructions[i].iAddress, 1, &pLineEnum );

				CComPtr<IDiaLineNumber> pLineNumber;
				ULONG celt2;
				while( SUCCEEDED( hr = pLineEnum->Next( 1, &pLineNumber, &celt2 ) ) && celt2 == 1 )
				{
					CComPtr<IDiaSourceFile> pSourceFile = NULL;
					hr = pLineNumber->get_sourceFile( &pSourceFile );
					BSTR sFileName;
					hr = pSourceFile->get_fileName( &sFileName );
					wcstombs( pBuffer, sFileName, Min( SysStringLen( sFileName ) + 1, MAX_BUFFER_SIZE ) );
					::SysFreeString( sFileName );
					DWORD iLine = 0;
					hr = pLineNumber->get_lineNumber( &iLine );

					Log( E_LOG_INFO, "        %s, L.%d : %s instruction %s\n", 
						 pBuffer,
						 iLine, 
						 xed_extension_enum_t2str( xed_inst_extension( oInstructions[i].pInstruction ) ), 
						 xed_iclass_enum_t2str( xed_inst_iclass( oInstructions[ i ].pInstruction ) ) );
					pLineNumber = NULL;
				}
			}
		}
		if( bSuccess == false && GetLogLevel() == E_VERBOSE )
		{
			if( bFoundInstructions == false )
			{
				Log( E_LOG_ERROR_VERBOSE, "\n" );
				bFoundInstructions = true;
			}
			BSTR sFuncName;
			pFunction->get_name( &sFuncName );
			wcstombs( pBuffer, sFuncName, Min( SysStringLen( sFuncName ) + 1, MAX_BUFFER_SIZE ) );
			::SysFreeString( sFuncName );
			Log( E_LOG_ERROR_VERBOSE, "    Unexpected ending checking function %s\n", pBuffer );
		}

		pFunction = NULL;
	}

	free( pBuffer );

	if( bFoundInstructions == false )
	{
		Log( E_LOG_INFO, "Nothing found\n" );
	}

	return true;
}
