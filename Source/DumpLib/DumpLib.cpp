
// ****************************************************************************
// File: DumpLib.cpp
// Desc: Utility for "GUID-Finder" IDA Pro plug-in.
//       Searches through libs and dumps out GUID info
//       Based on Matt Pietrek's LibDump from MSJ April 1998 article
//       http://www.microsoft.com/msj/0498/hood0498.aspx
//
// ****************************************************************************
#define WIN32_LEAN_AND_MEAN
#define WINVER       0x0502 // WinXP++    
#define _WIN32_WINNT 0x0502
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>
#include <string.h>

/*
	To use this:
	1. Put the lib files you want to process into the same directory where DumpLib.exe is located.
	2. Run it.
	3. Creates two files on finding GUIDs, INTERFACE_FILENAME and CLASS_FILENAME.
*/

#define INTERFACE_FILENAME  "NewInterfaces.txt" 
#define CLASS_FILENAME      "NewClasses.txt" 

// Archive file format start
#pragma warning(disable: 4200)
#pragma pack(1)
struct tARCHIVE_FILE
{
	char szMagic[IMAGE_ARCHIVE_START_SIZE];	// "!<arch>\n"
	IMAGE_ARCHIVE_MEMBER_HEADER tHeader;	// Header
	UINT  uSymbolCount;						// Symbols (BigEndian)
	UINT  aMemberOffsets[];					// Array of offsets to archive members
	//...
};
#pragma pack()


// ==== Forward defs ====
void DisplayLibInfoForSymbol(PSTR pszSymbol, PVOID pFileBase, DWORD archiveMemberOffset);
UINT ProcessLib(LPSTR pszFile, UINT uFileSize);
UINT ProcessMember(tARCHIVE_FILE *pFileData, UINT uIndex, UINT uMemberOffset);
LPCSTR GetSectionSymbolName(PIMAGE_FILE_HEADER pImgFileHdr, int iSection);
int  GetGUIDType(LPCSTR pszName);
void AddGUID(LPCSTR pszFile, LPCSTR pszLabel, GUID *pGUID);
void DumpData(const void *pData, int iSize);

// From big endian to little endian number
inline UINT ToLittleEndian(UINT uBig)
{
	_asm
	{
		mov   eax,uBig;
		bswap eax
	};
}


// MakePtr is a macro that allows you to easily add to values (including
// pointers) together without dealing with C's pointer arithmetic.  It
// essentially treats the last two parameters as DWORDs.  The first
// parameter is used to typecast the result to the appropriate pointer type.
#pragma warning(disable: 4311 4312)
#define MakePtr(cast, ptr, addValue) (cast) ((UINT)(ptr) + (UINT) (addValue))


// ==== Main ====
void main()
{
	UINT uFilesProcessed = 0;
	UINT uGUIDTotal      = 0;

	printf("\n\n==== DumLib ====\n");

	// Iterate through all LIB files in current path..
	WIN32_FIND_DATAA tFileData = {0};
	HANDLE hSearch = FindFirstFileA("*.tlb", &tFileData);
	if(hSearch != INVALID_HANDLE_VALUE)
	{
		do
		{ 
			uGUIDTotal += ProcessLib(tFileData.cFileName, tFileData.nFileSizeLow); 
			uFilesProcessed++;

		}while(FindNextFile(hSearch, &tFileData));

		FindClose(hSearch);
	}

	printf("\nFiles processed: %u\n", uFilesProcessed);
	printf("     GUID Found: %u\n", uGUIDTotal);
}


// Process a lib file for GUID data, return count found
UINT ProcessLib(LPSTR pszFile, UINT uFileSize)
{
	UINT uFound = 0;

	printf("File: \"%s\", %u Bytes.\n", pszFile, uFileSize);

	// Load the whole in at once to be processed
	if(FILE *fp = fopen(pszFile, "rb"))
	{
		if(tARCHIVE_FILE *pFileData = (tARCHIVE_FILE *) _aligned_malloc(uFileSize, 32))
		{
			if(fread(pFileData, uFileSize, 1, fp) == 1)
			{
				// Valid LIB file?
				if(strncmp(pFileData->szMagic, IMAGE_ARCHIVE_START,	IMAGE_ARCHIVE_START_SIZE) == 0)
				{	
					try
					{										
						if(UINT uSymbolCount = ToLittleEndian(pFileData->uSymbolCount))
						{								
							// Iterate through members looking for GUIDs
							UINT uLastOffset = -1;
							for(UINT i = 0; i < uSymbolCount; i++)
							{
								UINT uOffset = ToLittleEndian(pFileData->aMemberOffsets[i]);
								if(uOffset != uLastOffset)
								{
									uFound += ProcessMember(pFileData, i, uOffset);
									uLastOffset = uOffset;
								}
							}
						}
					}
					catch(LPSTR pszException)
					{
						printf("\n*** Exception: \"%s\" while processing file \"%s\"! ***\n", pszException, pszFile);
					}
				}			
			}

			_aligned_free(pFileData);
		}

		fclose(fp);
	}

	printf("\n");

	return(uFound);
}


// Process member, return number of GUIDs found
UINT ProcessMember(tARCHIVE_FILE *pFileData, UINT uIndex, UINT uMemberOffset)
{
	UINT uFound = 0;	
				
	// Member header
	PIMAGE_ARCHIVE_MEMBER_HEADER pMbrHdr = MakePtr(PIMAGE_ARCHIVE_MEMBER_HEADER, pFileData, uMemberOffset);	
	PIMAGE_FILE_HEADER pImgFileHdr = (PIMAGE_FILE_HEADER) (pMbrHdr + 1);

	if(pImgFileHdr->NumberOfSections != 0xFFFF)
	{
		// File header must have a symbol table	
		if(pImgFileHdr->PointerToSymbolTable)
		{		
			printf(" [%04u] Offset: %08X ==================================================\n", uIndex, uMemberOffset);
			
			// Iterate through sections..
			PIMAGE_SECTION_HEADER pSectHdr = (PIMAGE_SECTION_HEADER) (pImgFileHdr + 1);
			for(UINT i = 0; i < pImgFileHdr->NumberOfSections; i++, pSectHdr++)
			{	
				// flags only 40301040?
				// IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_LNK_COMDAT | IMAGE_SCN_ALIGN_4BYTES | IMAGE_SCN_MEM_READ
				// IMAGE_SCN_LNK_COMDAT	0x00001000

				//printf("    [%04u] \"%s\".\n", i, pSectHdr->Name);

				// Size of GUID?
				if(pSectHdr->SizeOfRawData == sizeof(GUID))
				{		
					// .rdata section?
					//if(strcmp((LPCSTR) pSectHdr->Name, ".rdata") == 0)
					{					
						if(LPCSTR pszSymbolName = GetSectionSymbolName(pImgFileHdr, (i + 1)))
						{
							//printf("    [%04u] \"%s\".\n", i, pszSymbolName);

							// Is pszName a GUID type we want?
							switch(GetGUIDType(pszSymbolName))
							{
								// Interface
								case 0:
								{
									printf("    [%04u] I: \"%s\".\n", i, pszSymbolName+1);									
									AddGUID(INTERFACE_FILENAME, (pszSymbolName + (sizeof("_IID_") - 1)), (GUID *) ((UINT) pImgFileHdr + pSectHdr->PointerToRawData));
									//DumpData((PVOID) ((UINT) pImgFileHdr + pSectHdr->PointerToRawData), sizeof(GUID));	
									uFound++;
								}
								break;

								// Class
								case 1:
								{
									printf("    [%04u] C: \"%s\".\n", i, pszSymbolName+1);										
									AddGUID(CLASS_FILENAME, (pszSymbolName + (sizeof("_CLSID_") - 1)), (GUID *) ((UINT) pImgFileHdr + pSectHdr->PointerToRawData));
									//DumpData((PVOID) ((UINT) pImgFileHdr + pSectHdr->PointerToRawData), sizeof(GUID));
									uFound++;
								}
								break;
							};																																	
						}
					}
				}
			}
		}
	}

	printf("\n");

	return(uFound);
}

// Returns 0 if is a interface, 1 if a class, or -1
int GetGUIDType(LPCSTR pszName)
{
	// MS symbol names prefixed with a '_'
	if(strncmp(pszName, "_IID_", (sizeof("_IID_") - 1)) == 0)
		return(0);
	else
	if(strncmp(pszName, "_CLSID_", (sizeof("_CLSID_") - 1)) == 0)
		return(1);	

	return(-1);
}


// Get a section Symbol name
LPCSTR GetSectionSymbolName(PIMAGE_FILE_HEADER pImgFileHdr, int iSection)
{
	LPCSTR pResult = NULL;

	PIMAGE_SYMBOL pSymbol = (PIMAGE_SYMBOL) ((UINT) pImgFileHdr + pImgFileHdr->PointerToSymbolTable);
	LPSTR pszStringTable  = (LPSTR) &pSymbol[pImgFileHdr->NumberOfSymbols];

	// Iterate through symbol table
	int iIndex = -1;
	for(UINT i = 0; i < pImgFileHdr->NumberOfSymbols; i++)
	{
		/*
		printf("  [%04u] ", i);

		if(pSymbol->N.Name.Short != 0)
		printf("%-20.8s, S: %d\n", pSymbol->N.ShortName, pSymbol->SectionNumber);
		else
		printf("%-20s, S: %d\n", (pszStringTable + pSymbol->N.Name.Long), pSymbol->SectionNumber);
		*/

		if(pSymbol->SectionNumber == iSection)
		{
			// Since the format of the ".rdata" (and others?) can have two names for a section, w need 
			// the last one.  Can't just bail out on the first match.
			pResult	= ((pSymbol->N.Name.Short == 0) ? (LPCSTR) (pszStringTable + pSymbol->N.Name.Long) : (LPCSTR) pSymbol->N.ShortName);
		}

		i += pSymbol->NumberOfAuxSymbols;
		pSymbol += pSymbol->NumberOfAuxSymbols;
		pSymbol++;			
	}	

	return(pResult);
}

// Append GUID to specified list file
void AddGUID(LPCSTR pszFile, LPCSTR pszLabel, GUID *pGUID)
{
	if(FILE *fp = fopen(pszFile, "ab"))
	{
		fprintf(fp, "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X %s\n", pGUID->Data1, pGUID->Data2, pGUID->Data3, pGUID->Data4[0],pGUID->Data4[1], pGUID->Data4[2],pGUID->Data4[3],pGUID->Data4[4],pGUID->Data4[5],pGUID->Data4[6],pGUID->Data4[7], pszLabel);
		fclose(fp);
	}
}


/*
void DumpData(const void *pData, int iSize)
{
	#define RUN 16

	if(pData && (iSize > 0))
	{
		BYTE *pSrc = (BYTE *) pData;
		char szBuff[256];
		szBuff[0] = 0;
		char szString[(RUN + 1) * 3];
		int  iOffset = 0;

		// Do runs of 16
		while(iSize >= RUN)
		{
			sprintf(szBuff, "[%04X]: ", iOffset);
	
			// Hex
			BYTE *pLine = pSrc;			
			for(int i = 0; i < RUN; i++)
			{
				sprintf(szString, "%02X ", *pLine);
				strcat(szBuff, szString);
				++pLine;
			}

			strcat(szBuff, "  ");

			// ASCII
			pLine = pSrc;
			for(int i = 0; i < RUN; i++)
			{
				sprintf(szString, "%c", (*pLine >= ' ') ? *pLine : '.');
				strcat(szBuff, szString);
				++pLine;
			}

			printf("%s\n", szBuff);
			iOffset += RUN, pSrc += RUN, iSize -= RUN;
		};

		if(iSize > 0)
		{
			sprintf(szBuff, "[%04X]: ", iOffset);
	
			// Hex
			BYTE *pLine = pSrc;			
			for(int i = 0; i < iSize; i++)
			{
				sprintf(szString, "%02X ", *pLine);
				strcat(szBuff, szString);
				++pLine;
			}

			// Pad out line
			for(int i = 0; i < (RUN - iSize); i++) strcat(szBuff, "   ");
			strcat(szBuff, "  ");

			// ASCII
			pLine = pSrc;
			for(int i = 0; i < iSize; i++)
			{
				sprintf(szString, "%c", (*pLine >= ' ') ? *pLine : '.');
				strcat(szBuff, szString);
				++pLine;
			}

			printf("%s\n", szBuff);
		}
	}

	#undef RUN
}
*/