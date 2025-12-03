#include "precompiled.h"

void ManagerFrame(double time);
void ManagerInitialize(double time);

template <typename T>
void GetProcAddress(T &pVar, HMODULE hModule, LPCSTR lpProcName)
{
	pVar = (T)GetProcAddress(hModule, lpProcName);
}

void CreateCoFDataStructure()
{
	gCoFData.version = 2;

	gCoFData.pcl_enginefuncs = pEngine;
	gCoFData.pcl_funcs = pClient;
	gCoFData.psv_enginefuncs = pServer;

	gCoFData.p_entityInterface = pgEntityInterface;

	gCoFData.pcl_move = gclmove;
	gCoFData.psv_player = gsv_player;

	gCoFData.gGlobals = gGlobals;

	gCoFData.pcls = gpcls;
	gCoFData.psvc = gpsvc;
}

void InitMainStructs()
{
	pEngine = (cl_enginefunc_t *)Transpose(gHLBase, 0x001A9A90);
	pServer = (enginefuncs_t *)Transpose(gHLBase, 0x001CED18);
	pClient = (cldll_func_t *)Transpose(gHLBase, 0x012B9BE0);
	pgEntityInterface = (DLL_FUNCTIONS *)Transpose(gHLBase, 0x0080D8C0);

	gclmove = (playermove_t *)Transpose(gHLBase, 0x0107F7A0);
	gsv_player = (edict_t **)Transpose(gHLBase, 0x0080E07C);
	gpcls = (client_static_t *)Transpose(gHLBase, 0x010CF280);
	gpsvc = (server_static_t*)Transpose(gHLBase, 0x0081F320);

	memcpy(&Client, pClient, sizeof(cldll_func_t));

	pClient->pHudFrame = ManagerInitialize;
}

void InitAdditionalStructs()
{
	gGlobals = (globalvars_t **)Transpose(gModBase, 0x002196B0);
}

void InitPlugins()
{
	for (auto &&m : gCofPlugins)
	{
		if (!m->pInit)
		{
			pEngine->Con_Printf("WARNING! Module %s does not contain initialization function.\n", m->pszName);
			continue;
		}

		if (!m->pPluginInfo)
		{
			pEngine->Con_Printf("WARNING! Module %s does not contain plugin info function.\n", m->pszName);
			continue;
		}

		plugin_info_t *info;

		m->pPluginInfo(&info);

		if (m->pGetGameVars)
			m->pGetGameVars(&gCoFData);

		if (!m->pInit(ghInstance, COFMGR_VERSION_MAJOR, COFMGR_VERSION_MINOR))
		{
			pEngine->Con_Printf("WARNING! Initialization failed for plugin %s.\n", m->pszName);
			continue;
		}

		pEngine->Con_Printf("%s loaded. ", info->pszName);
		if (!info->nVerMajor && !info->nVerMinor)
			pEngine->Con_Printf("Build: %d\n", info->nBuild);
		else
			pEngine->Con_Printf("Version: %d.%d.%d\n", info->nVerMajor, info->nVerMinor, info->nBuild);

		m->bInited = true;
	}
}

void ManagerFrame(double time)
{
	gPlugins.DoFrame(time);

	Client.pHudFrame(time);
}

void LoadPlugins()
{
	pEngine->Con_Printf("Loading plugins\n");

	char szPluginsDir[MAX_PATH];
	char szPluginsTmp[MAX_PATH];

	sprintf(szPluginsDir, "%s\\cryoffear\\addons", gszExeDir);
	sprintf(szPluginsTmp, "%s\\*.dll", szPluginsDir);

	WIN32_FIND_DATA file = { 0 };
	void *hFile = FindFirstFileA(szPluginsTmp, &file);

	if (hFile != INVALID_HANDLE_VALUE)
	{
		do
		{
			char szPluginName[MAX_PATH];

			sprintf(szPluginName, "%s\\%s", szPluginsDir, file.cFileName);
			HMODULE hModule = LoadLibraryA(szPluginName);

			if (!hModule)
			{
				pEngine->Con_Printf("WARNING! Could not load CoF module %s\n", szPluginName);
				continue;
			}

			auto mod = new cof_module_t;

			mod->pszName = _strdup(file.cFileName);
			mod->bInited = false;
			GetProcAddress(mod->pInit, hModule, "Init");
			GetProcAddress(mod->pPluginInfo, hModule, "GetPluginInfo");
			GetProcAddress(mod->pGetGameVars, hModule, "GetGameVars");

			pEngine->Con_Printf("Loaded plugin %s\n", mod->pszName);
			gCofPlugins.push_back(mod);
		} while (FindNextFileA(hFile, &file) != 0);

		FindClose(hFile);
	}


	InitPlugins();
}

void ManagerInitialize(double time)
{
	pClient->pHudFrame = ManagerFrame;


	FindModule("client.dll", gCLBase, gCLEnd, gCLSize);
	FindModule("hl.dll", gModBase, gModEnd, gModSize);

	InitAdditionalStructs();
	CreateCoFDataStructure();

	LoadPlugins();
}

void UnloadPlugins() {
	pEngine->Con_Printf("Unloading plugins\n");

	for (auto it = gCofPlugins.begin(); it != gCofPlugins.end(); ) {
		auto m = *it;
		pEngine->Con_Printf("Attempting to unload %s\n", m->pszName);
		HMODULE hModule = GetModuleHandleA(m->pszName);
		if (hModule && FreeLibrary(hModule)) {
			pEngine->Con_Printf("%s unloaded\n", m->pszName);
			free((void*)m->pszName);
			it = gCofPlugins.erase(it);
		}
		else {
			pEngine->Con_Printf("Failed unloading %s\n", m->pszName);
			++it;
		}
	}
}


void ReloadPlugins() {
	UnloadPlugins();
	LoadPlugins();
}

void InitManager()
{
	InitMainStructs();
	pEngine->pfnAddCommand("cm_reload_plugins", ReloadPlugins);
	pEngine->pfnAddCommand("cm_unload_plugins", UnloadPlugins);
	pEngine->pfnAddCommand("cm_load_plugins",   LoadPlugins);
}