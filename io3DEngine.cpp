// io3DEngine.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"

#include "ioResourceLoader.h"
#include "ioThreadTaskManager.h"
#include <io.h>

CLog g_EngineLog;
BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved )
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
        if (0 == access("info/engine.log", 0))
        {
            if (!SetFileAttributes("info/engine.log", FILE_ATTRIBUTE_NORMAL))
                return FALSE;
        }
        g_EngineLog.OpenLog(0, "info/engine.log");
    }
    if (ul_reason_for_call == DLL_PROCESS_DETACH)
    {
        g_EngineLog.CloseLog();
        ioThreadTaskManager::ReleaseInstance();
        ioResourceLoader::ReleaseInstance();
    }
	

    return TRUE;
}

