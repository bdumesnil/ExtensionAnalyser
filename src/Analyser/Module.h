#pragma once

#include <dia2.h>
#include <atlbase.h>
#include <vector>
#include <string>
#include "Config.h"
#include <ImageHlp.h>

extern "C"
{
#include <xed-interface.h.>
}

struct PDBInfo
{
	std::string				sPDBName;
	GUID					oSignature;
	DWORD					iAge;
	DWORD					iFileSignature;
	bool					bValid;
};

struct Module
{
	bool InitModule( const char* sModuleName, const std::vector<std::string>& oSearchPaths );
	bool CheckAllFunctions( const std::vector<bool>& oExtensionsToCheck );
	void DestroyModule();

	PDBInfo					m_oPDBInfo;

	CComPtr<IDiaDataSource>	m_pSource;
	CComPtr<IDiaSession>	m_pSession;

	std::string				m_sModuleName;
	void*					m_pLoadAddress;
	bool					m_bIsX64;
};

/////////////////////////////////////////////////////////////////////////////////
// Helper functions
/////////////////////////////////////////////////////////////////////////////////

// Have to redefine LOADED_IMAGE32 and LOADED_IMAGE64 because they are defined at compile time in ImageHlp header
struct LOADED_IMAGE32
{
	PSTR					ModuleName;
	HANDLE					hFile;
	PUCHAR					MappedAddress;
	PIMAGE_NT_HEADERS32		FileHeader;
	PIMAGE_SECTION_HEADER	LastRvaSection;
	ULONG					NumberOfSections;
	PIMAGE_SECTION_HEADER	Sections;
	ULONG					Characteristics;
	BOOLEAN					fSystemImage;
	BOOLEAN					fDOSImage;
	BOOLEAN					fReadOnly;
	UCHAR					Version;
	LIST_ENTRY				Links;
	ULONG					SizeOfImage;
};

struct LOADED_IMAGE64
{
	PSTR					ModuleName;
	HANDLE					hFile;
	PUCHAR					MappedAddress;
	PIMAGE_NT_HEADERS64		FileHeader;
	PIMAGE_SECTION_HEADER	LastRvaSection;
	ULONG					NumberOfSections;
	PIMAGE_SECTION_HEADER	Sections;
	ULONG					Characteristics;
	BOOLEAN					fSystemImage;
	BOOLEAN					fDOSImage;
	BOOLEAN					fReadOnly;
	UCHAR					Version;
	LIST_ENTRY				Links;
	ULONG					SizeOfImage;
};

// Do ImageLoad with multiple search paths
PLOADED_IMAGE ImageLoadHelper( const char* sModuleName, std::vector<std::string> oSearchPaths );

// Need template to handle both 32/64bits versions
template <class T> PIMAGE_SECTION_HEADER GetEnclosingSectionHeader( DWORD rva, T* pNTHeader ) // 'T' == PIMAGE_NT_HEADERS 
{
	PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION( pNTHeader );
	unsigned i;

	for( i = 0; i < pNTHeader->FileHeader.NumberOfSections; i++, section++ )
	{
		// This 3 line idiocy is because Watcom's linker actually sets the
		// Misc.VirtualSize field to 0.  (!!! - Retards....!!!)
		DWORD size = section->Misc.VirtualSize;
		if( 0 == size )
			size = section->SizeOfRawData;

		// Is the RVA within this section?
		if( ( rva >= section->VirtualAddress ) &&
			( rva < ( section->VirtualAddress + size ) ) )
			return section;
	}

	return 0;
}

template <class T> LPVOID GetPtrFromRVA( DWORD rva, T* pNTHeader, PBYTE imageBase ) // 'T' = PIMAGE_NT_HEADERS 
{
	PIMAGE_SECTION_HEADER pSectionHdr;
	INT delta;

	pSectionHdr = GetEnclosingSectionHeader( rva, pNTHeader );
	if( !pSectionHdr )
		return 0;

	delta = (INT)( pSectionHdr->VirtualAddress - pSectionHdr->PointerToRawData );
	return (PVOID)( imageBase + rva - delta );
}

inline bool Contains( const std::vector<std::string>& oDependencies, const std::string& sDependency )
{
	for( int i = 0; i < oDependencies.size(); ++i )
	{
		if( oDependencies[ i ] == sDependency )
			return true;
	}
	return false;
}

template <class T> void ListDllDependeciesFromPath( T* pImage, const std::vector<std::string>& oSearchPaths, std::vector<std::string>& oDependencies )
{
	if( pImage->FileHeader->OptionalHeader.NumberOfRvaAndSizes >= 2 )
	{
		PIMAGE_IMPORT_DESCRIPTOR importDesc =
			(PIMAGE_IMPORT_DESCRIPTOR)GetPtrFromRVA(
				pImage->FileHeader->OptionalHeader.DataDirectory[ 1 ].VirtualAddress,
				pImage->FileHeader, pImage->MappedAddress );
		while( 1 )
		{
			if( importDesc == NULL || ( importDesc->TimeDateStamp == 0 ) && ( importDesc->Name == 0 ) )
				break;

			const char* sPath = (char*)GetPtrFromRVA( importDesc->Name,
													  pImage->FileHeader,
													  pImage->MappedAddress );

			if( Contains( oDependencies, sPath ) == false )
			{
				oDependencies.push_back( sPath );
				PLOADED_IMAGE pDepImage = ImageLoadHelper( sPath, oSearchPaths );
				if( pDepImage != NULL )
				{
					if( pDepImage->FileHeader->FileHeader.Machine == IMAGE_FILE_MACHINE_I386 )
						ListDllDependeciesFromPath( (LOADED_IMAGE32*)pDepImage, oSearchPaths, oDependencies );
					else
						ListDllDependeciesFromPath( (LOADED_IMAGE64*)pDepImage, oSearchPaths, oDependencies );
				}
			}

			importDesc++;
		}
	}
}