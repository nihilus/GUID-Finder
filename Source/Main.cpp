
// ****************************************************************************
// File: Main.cpp
// Desc: GUID Finder plug-in by Sirmabus
//
// ****************************************************************************
#include "stdafx.h"

// Run IDA in plug-in debug mode with -z20

// === Function Prototypes ===
int IDAP_init();
void IDAP_term();
void IDAP_run(int arg);
extern void CORE_Init();
extern void CORE_Process(int iArg);
extern void CORE_Exit();


// === Data ===
char IDAP_comment[] = "GUID-Finder: .";
char IDAP_help[] 	= "GUID-Finder: .";
char IDAP_name[] 	= "GUID-Finder";
char IDAP_hotkey[] 	= "Alt-5"; // Preferred/default hotkey

// Plug-in description block
extern "C" ALIGN(32) plugin_t PLUGIN =
{
	IDP_INTERFACE_VERSION,	// IDA version plug-in is written for
	PLUGIN_UNL,				// Plug-in flags
	IDAP_init,	            // Initialization function
	IDAP_term,	            // Clean-up function
	IDAP_run,	            // Main plug-in body
	IDAP_comment,	        // Comment - unused
	IDAP_help,	            // As above - unused
	IDAP_name,	            // Plug-in name shown in Edit->Plugins menu
	IDAP_hotkey	            // Hot key to run the plug-in
};

// Init
int IDAP_init()
{
    CORE_Init();
    return(PLUGIN_OK);   
}

// Un-init
void IDAP_term()
{
    CORE_Exit();
}

// Run 
void IDAP_run(int iArg)
{	
    CORE_Process(iArg);   
}



