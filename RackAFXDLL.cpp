#include "RackAFXDLL.h"
//#define RAFX_DLLMAIN

// WARNING: DO NOT UNDER ANY CIRCUMSTANCES EDIT THIS FILE! **********************

///////////////////////////////////////////////////////////
//
// Global variables
//
void* g_hModule;   // DLL module handle
#define stringCompanyName "RackAFX User"
static float g_fMUResult;
unsigned long uPCID1 = 4294943855;
unsigned long uPCID2 = 4294967189;
unsigned long uPCID3 = 4294967198;
unsigned long uPCID4 = 2354318337;
unsigned long uCCID1 = 4294946412;
unsigned long uCCID2 = 4289611069;
unsigned long uCCID3 = 2336425584;
unsigned long uCCID4 = 4294967291;
// --- VSTCompatibility: ENABLED --- //


///////////////////////////////////////////////////////////
//
// DLL module information
//

extern "C"
{
	// entry point DllMain
	BOOL APIENTRY DllMain(HINSTANCE hInst, DWORD dwReason, LPVOID /*lpvReserved*/)
	{
		if(dwReason == DLL_PROCESS_ATTACH)
		{
			g_hModule = hInst;

			// --- VST2 and VST3 -- DO NOT EDIT
			CRAFXPlugInContainer::createRAFXPlugInContainer(g_hModule, createObject, uPCID1, uPCID2, uPCID3, uPCID4, uCCID1, uCCID2, uCCID3, uCCID4, &g_pGUIXML[0], g_nGUIXMLSize, stringCompanyName);
		}
		else if(dwReason == DLL_PROCESS_DETACH)
		{
			// --- VST2 and VST3 -- DO NOT EDIT
			CRAFXPlugInContainer::destroyRAFXPlugInContainer();
		}

		return TRUE ;
	}
	// end entry point DllMain*/

	// --- VST3 ONLY: not used now but I may use later
	/*bool DllExportVST3 InitDll()
	{
		return true;
	}
	bool DllExportVST3 ExitDll()
	{
		return true;
	}*/
}

//RackAFX Creation Function
DllExport CPlugIn* createObject()
{
	CPlugIn* pOb = new CSimpleHPF(); // ***
	return pOb;
}

DllExport UINT getAPIVersion()
{
	return CURRENT_PLUGIN_API;
}


