
// ****************************************************************************
// File: Core.cpp
// Desc: GUID-Finder plug-in
//
// ****************************************************************************
#include "stdafx.h"
#include "ContainersInl.h"

//#define SAVE_FIXED // To help sort out duplicates

// DB info container
struct tDB_ENTRY
{
	LPCSTR pszFileName;  // DB File name
	LPCSTR pszType;    // DB Type prefix
} static const aDBFile[] =
{
	{"Interfaces.txt", "IID"},
	{"Classes.txt",    "CLSID"},
	//{"Libraries.txt",  "LIBID"}, // TODO:
};
static const UINT DB_FILE_COUNT = (sizeof(aDBFile) / sizeof(tDB_ENTRY));


// GUID info container
struct tGUIDNODE : public Container::NodeEx<Container::ListHT, tGUIDNODE>
{
	tid_t StructID;
	char  szGUID[48];
	char  szLabel[48];
	UINT  uHash;
	#ifdef SAVE_FIXED
	char  szGUIDCpy[48];
	#endif

	// Use IDA allocs
	static PVOID operator new(size_t size){	return(qalloc(size)); };
	static void operator delete(PVOID _Ptr){ return(qfree(_Ptr)); }
};


// === Function Prototypes ===
static BOOL LoadDB();
static void RemoveGUIDList();
static BOOL CheckBreak();
static void SafeJumpTo(ea_t ea);


// === Data ===
static ALIGN(16) Container::ListEx<Container::ListHT, tGUIDNODE> s_GUIDList;


// Main dialog
static const char szMainDialog[] =
{	
	"BUTTON YES* Continue\n" // 'Continue' instead of 'okay'
	
	// Help block
	"HELP\n"
	"\"GUID-Finder\"" 
	"Scans current IDB for GUIDs. and creates matching structure w/label and comment.\n"
	"By Sirmabus @ http://www.openrce.org\n"
	"Based on Frank Boldewin's \"ClassAndInterfaceToNames.py\" script.\n"		
	"See \"GUID-Finder.txt\" for more information.\n"
	"ENDHELP\n"	

	// Title
	"<GUID-Finder Plug-in>\n"

	// Message text
	"-Version: %A, %A, by Sirmabus-\n\n" 		

	// checkbox -> wbSkipCodeAndIAT
	"<#Skip code and import segments to make searching faster.\nUsually ok, but some times GUIDs are in code segments too, in particular Delphi executables. #"
	"Skip code segments for speed. :C>>\n"				
	
	"\n\n"
};


// Initialize
void CORE_Init()
{	
	//msg("SIZE: %d\n", sizeof(tGUIDNODE));
}


// Un-initialize
void CORE_Exit()
{
	RemoveGUIDList();
}


// Plug-in process
void CORE_Process(int iArg)
{
	msg("\n== GUID-Finder plug-in: v: %s - %s, By Sirmabus ==\n", MY_VERSION, __DATE__);
	//while(_kbhit()) getchar();

	if(autoIsOk())
	{
		WORD wbSkipCodeAndIAT = TRUE;
		int iUIResult = AskUsingForm_c(szMainDialog, MY_VERSION, __DATE__, &wbSkipCodeAndIAT);
		if(!iUIResult)
		{			
			msg(" - Canceled -\n");				
			return;
		}

		// Load in GUID database
		if(LoadDB())
		{			
			msg("\nScanning, <Press Pause/Break key to abort>...\n");
			// TODO: Add UI handler for "cancel"
			show_wait_box("Working..\nTake a smoke, drink some coffee, this could be a while..  \n\n<Press Pause/Break key to abort>"); 

			//TIMESTAMP StartTime = GetTimeStamp();
			
			// Walk through segments
			int iSegCount = get_segm_qty();              
			for(int i = 0; i < iSegCount; i++)
			{                            
				if(segment_t *pSegInfo = getnseg(i))
				{
					ea_t startEA = pSegInfo->startEA;
					ea_t endEA   = pSegInfo->endEA;
					char szName[128];
					get_segm_name(pSegInfo, szName, (sizeof(szName) - 1));
					if(szName[0] == '_') szName[0] = '.';
					char szClass[128];
					get_segm_class(pSegInfo, szClass, (sizeof(szClass) - 1));

					// Skip code and import/export segs?
					if(wbSkipCodeAndIAT && ((pSegInfo->type == SEG_CODE) || (pSegInfo->type == SEG_XTRN)))		
						msg("Seg: %6s, %s, (%08X - %08X) SKIPPED\n", szName, szClass, startEA, endEA);
					else
					{
						msg("Seg: %6s, %s, (%08X - %08X) ..\n", szName, szClass, startEA, endEA);
									
						tGUIDNODE *pNode = s_GUIDList.GetHead();
						while(pNode)
						{	
							//msg("Looking for: \"%s\", \"%s\".\n", pNode->szLabel, pNode->szGUID);				
							//while(!_kbhit()) Sleep(100);

							ea_t ea = startEA;
							while((ea = find_binary(ea, endEA, pNode->szGUID, 0, (SEARCH_DOWN | SEARCH_NEXT | SEARCH_NOSHOW))) != BADADDR)
							{
								msg("%08X %s\n", ea, pNode->szLabel);

								jumpto(ea, 0);
								autoWait();								
								do_unknown(ea, FALSE);
								auto_mark_range(ea, (ea + sizeof(GUID)), AU_UNK);

								// Place GUID struct here                             
								if(pNode->StructID != BADADDR)
								{
									if(!doStruct(ea, sizeof(GUID), pNode->StructID))
										msg("  %08X *** Set struct failed! ***\n", ea);
								}
								
								// Label it
								#define NAME_FLAGS (SN_AUTO | SN_NOCHECK | SN_NOWARN)                             
								if(!set_name(ea, pNode->szLabel, NAME_FLAGS))
								{	
									// Can't name it if it's a tail byte (fixes hang-up bug)
									if(isTail(getFlags(ea)))									
										msg("  %08X *** \"Tail\" byte here, failed to set name! ***\n", ea);									
									else
									{
										// Must already exist, append w/reference count suffix
										for(UINT i = 0; i < 0x7FFFFFFF; i++)
										{
											char szName[256] = {0};
											qsnprintf(szName, (sizeof(szName) - 1), "%s_%02u", pNode->szLabel, i);
											//msg("    TRY[%u]: \"%s\" F: %d.\n", i, szName, isTail(getFlags(ea)));
											if(set_name(ea, szName, NAME_FLAGS))
												break;                                     
										}
									}
								}
								#undef NAME_FLAGS

								// Anterior comment separator
								//describe(ea, TRUE, ";");

								// Add comment																							
								char szComment[512];
								qsnprintf(szComment, (sizeof(szComment) - 1), "GUID %s", pNode->szLabel);
								set_cmt(ea, szComment, TRUE);
							};
							
							// User abort?
							if(CheckBreak())
								goto BailOut;

							pNode = pNode->GetNext();
						};
					}
				}
			}

			BailOut:;
			//msg("TIME: %.4f Seconds.\n", (GetTimeStamp() - StartTime));

			// Just scanning:
			// Everything direct: 113.0443 Seconds.
			// Everything seg walker: 112.6742 Seconds. (why faster?)
			// Test .data and .rdata only: 27.5386 Seconds.			
			// Walking segments .data and .rdata: 27.7137 Seconds.

			// Clean up
			RemoveGUIDList();
			hide_wait_box();
			msg("\nFinsihed.\n-------------------------------------------------------------\n");
		}		
	}
	else
	{
		warning("Autoanalysis must finish first before you run the plug-in!");
		msg("\n*** Aborted ***\n");
	}
}


// Delete GUID list
static void RemoveGUIDList()
{
	while(tGUIDNODE *pHeadNode = s_GUIDList.GetHead())
	{		
		s_GUIDList.RemoveHead();
		delete pHeadNode;
	};
}


// Checks and handles if break key pressed; returns TRUE on break.
static BOOL CheckBreak()
{
	if(GetAsyncKeyState(VK_PAUSE) & 0x8000)
	{			
		msg("\n*** Aborted ***\n\n");	
		return(TRUE);
	}

	return(FALSE);
}


// Load in GUID database
static BOOL LoadDB()
{
	// Load it dynamically to allow DB edits between invocations
	RemoveGUIDList();

	// Iterate through DB list loading all the GUIDs	
	for(int i = 0; i < DB_FILE_COUNT; i++)
	{	
		// Load next DB file
		msg("Loading \"%s\"..\n", aDBFile[i].pszFileName);
		UINT uGUIDCount = 0;
		int iFileLine = 0;		
		char szPath[MAX_PATH] = {0};
		getsysfile(szPath, (MAX_PATH - 1), aDBFile[i].pszFileName, "plugins\\GUID-Finder");	
		if(FILE *fp = qfopen(szPath, "rb"))
		{
			// Create GUID struct for this type
			tid_t StructID = get_struc_id(aDBFile[i].pszType);
			if(StructID == BADADDR)
			{
				// Create it
				if((StructID = add_struc(BADADDR, aDBFile[i].pszType)) != BADADDR)
				{									
					if(struc_t *ptStuctInfo = get_struc(StructID))
					{						
						add_struc_member(ptStuctInfo, "Data1", 0x0, dwrdflag(), NULL, 4);
						add_struc_member(ptStuctInfo, "Data2", 0x4, wordflag(), NULL, 2);
						add_struc_member(ptStuctInfo, "Data3", 0x6, wordflag(), NULL, 2);
						add_struc_member(ptStuctInfo, "Data4", 0x8, byteflag(), NULL, 8);
					}
				}
			}		
			if(StructID == BADADDR)
				msg("*** Failed to build structure for GUID type \"%s\"! ***\n", aDBFile[i].pszType);

			// Iterate through text lines..
			char szLine[512]; 
			szLine[sizeof(szLine) - 1] = 0;
			while(qfgets(szLine, (sizeof(szLine) - 1), fp))
			{ 
				++iFileLine;

				// Skip comment and blank lines
				if((szLine[0] && szLine[1] && szLine[2]) && (szLine[0] != '#'))
				{
					// Parse fields
					char szGUID[256]; 
					char szLabel[256]; 
					szGUID[sizeof(szGUID) - 1] = 0;
					szLabel[sizeof(szLabel) - 1] = 0;

					if(_snscanf(szLine, (sizeof(szLine) - 1), "%255s %255s", szGUID, szLabel) == 2)
					{
						_strupr(szGUID);
						
						// New GUID container
						if(tGUIDNODE *pNode = new tGUIDNODE())
						{			
							// GUID search string quick'n dirty

							// Data1
							pNode->szGUID[ 0] = szGUID[ 6];
							pNode->szGUID[ 1] = szGUID[ 7];
							pNode->szGUID[ 2] = ' ';
							pNode->szGUID[ 3] = szGUID[ 4];
							pNode->szGUID[ 4] = szGUID[ 5];
							pNode->szGUID[ 5] = ' ';
							pNode->szGUID[ 6] = szGUID[ 2];
							pNode->szGUID[ 7] = szGUID[ 3];
							pNode->szGUID[ 8] = ' ';
							pNode->szGUID[ 9] = szGUID[ 0];
							pNode->szGUID[10] = szGUID[ 1];
							pNode->szGUID[11] = ' ';

							// Data 2
							pNode->szGUID[12] = szGUID[11];
							pNode->szGUID[13] = szGUID[12];
							pNode->szGUID[14] = ' ';
							pNode->szGUID[15] = szGUID[9];
							pNode->szGUID[16] = szGUID[10];
							pNode->szGUID[17] = ' ';

							// Data3
							pNode->szGUID[18] = szGUID[16];
							pNode->szGUID[19] = szGUID[17];
							pNode->szGUID[20] = ' ';
							pNode->szGUID[21] = szGUID[14];
							pNode->szGUID[22] = szGUID[15];
							pNode->szGUID[23] = ' ';

							// Data4-1
							pNode->szGUID[24] = szGUID[19];
							pNode->szGUID[25] = szGUID[20];
							pNode->szGUID[26] = ' ';
							pNode->szGUID[27] = szGUID[21];
							pNode->szGUID[28] = szGUID[22];
							pNode->szGUID[29] = ' ';

							// Data4-2
							pNode->szGUID[30] = szGUID[24];
							pNode->szGUID[31] = szGUID[25];
							pNode->szGUID[32] = ' ';
							pNode->szGUID[33] = szGUID[26];
							pNode->szGUID[34] = szGUID[27];
							pNode->szGUID[35] = ' ';
							pNode->szGUID[36] = szGUID[28];
							pNode->szGUID[37] = szGUID[29];
							pNode->szGUID[38] = ' ';
							pNode->szGUID[39] = szGUID[30];
							pNode->szGUID[40] = szGUID[31];
							pNode->szGUID[41] = ' ';
							pNode->szGUID[42] = szGUID[32];
							pNode->szGUID[43] = szGUID[33];
							pNode->szGUID[44] = ' ';
							pNode->szGUID[45] = szGUID[34];
							pNode->szGUID[46] = szGUID[35];

							pNode->szGUID[47] = 0;


							// Hash it for fast dupe compare
							pNode->uHash = DJBHash((PBYTE) pNode->szGUID, 47);
							#ifdef SAVE_FIXED
							qstrncpy(pNode->szGUIDCpy, szGUID, 48);
							#endif						

							// Skip GUID duplicates
							if(uGUIDCount)
							{
								BOOL bFoundDupe = FALSE;
								tGUIDNODE *pTestNode = s_GUIDList.GetHead();
								while(pTestNode)
								{
									if(pNode->uHash == pTestNode->uHash)
									{
										msg("** Duplicate GUID %s @ line %d **\n", pNode->szGUID, iFileLine);
										bFoundDupe = TRUE;
										break;
									}

									pTestNode = pTestNode->GetNext();
								};
	
								// Skip dupes
								if(bFoundDupe)									
								{
									delete pNode;
									continue;
								}								
							}							
							
							// Label
							qsnprintf(pNode->szLabel, (sizeof(pNode->szLabel) - 1), "%s_%s", aDBFile[i].pszType, szLabel);							
							pNode->StructID = StructID;

							if(!uGUIDCount)
								s_GUIDList.InsertHead(*pNode);
							else
								s_GUIDList.InsertTail(*pNode);

							uGUIDCount++;							
						}
					}
					else
						msg("\n*** GUID format parse error on line %d! ***\n", iFileLine);					
				}
			};			

			qfclose(fp);
			msg("%u GUIDs loaded.\n", uGUIDCount);

			if(s_GUIDList.IsEmpty())
				msg("\n*** No GUIDs loaded! ***\n");
		}
		else
			msg("\n*** Error loading DB file \"%s\"! ***\n", szPath);		
	}

	#ifdef SAVE_FIXED
	if(FILE *fp = qfopen("C:\\FixedList.txt", "wb"))
	{
		tGUIDNODE *pNode = s_GUIDList.GetHead();
		while(pNode)
		{
			qfprintf(fp, "%s %s\n", pNode->szGUIDCpy, pNode->szLabel);					
			pNode = pNode->GetNext();
		};
		
		qfclose(fp);
	}
	#endif
	
	return(!s_GUIDList.IsEmpty());
}