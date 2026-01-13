/**
 * UnityPlayer.dll Proxy - Assembly-CSharp Dumper
 * Extract Assembly-CSharp.dll from Unity games
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

// ============================================================================
// Global Variables
// ============================================================================

static HMODULE g_hOriginalUnityPlayer = NULL;
static BOOL g_dumpSuccess = FALSE;
static char g_dumpDir[MAX_PATH] = { 0 };
static FILE* g_logFile = NULL;
static char g_logFilePath[MAX_PATH] = { 0 };
static char g_dumpFilePath[MAX_PATH] = { 0 };

// Target DLL name
static const char* TARGET_DLL_NAME = "Assembly-CSharp.dll";

// Mono function prototype
typedef void* (*mono_image_open_from_data_with_name_t)(
    const char* data,
    unsigned int data_len,
    int need_copy,
    void* status,
    int refonly,
    const char* name
);

static mono_image_open_from_data_with_name_t g_originalMonoFunc = NULL;
static BYTE g_originalBytes[14] = { 0 };

// ============================================================================
// Logging
// ============================================================================

static BOOL g_hasError = FALSE;
static char g_lastError[512] = { 0 };

static void LogPrint(const char* format, ...) {
    va_list args;
    
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    fflush(stdout);
    
    if (g_logFile) {
        va_start(args, format);
        vfprintf(g_logFile, format, args);
        va_end(args);
        fflush(g_logFile);
    }
}

static void InitLogFile() {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    char* lastSlash = strrchr(path, '\\');
    if (lastSlash) *lastSlash = '\0';
    
    time_t now = time(NULL);
    struct tm tm_info;
    localtime_s(&tm_info, &now);
    
    snprintf(g_logFilePath, sizeof(g_logFilePath), 
        "%s\\dumper_log.txt", path);
    
    fopen_s(&g_logFile, g_logFilePath, "w");
    if (g_logFile) {
        fprintf(g_logFile, "Assembly-CSharp Dumper Log\n");
        fprintf(g_logFile, "Time: %04d-%02d-%02d %02d:%02d:%02d\n\n",
            tm_info.tm_year + 1900, tm_info.tm_mon + 1, tm_info.tm_mday,
            tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec);
        fflush(g_logFile);
    }
}

static void CreateDebugConsole() {
    AllocConsole();
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
    freopen_s(&fp, "CONIN$", "r", stdin);
    
    InitLogFile();
    SetConsoleTitleA("Assembly-CSharp Dumper");
    
    HWND hwnd = GetConsoleWindow();
    if (hwnd) {
        HMENU hMenu = GetSystemMenu(hwnd, FALSE);
        if (hMenu) {
            DeleteMenu(hMenu, SC_CLOSE, MF_BYCOMMAND);
        }
    }
}

static void LogError(const char* message) {
    g_hasError = TRUE;
    DWORD errorCode = GetLastError();
    
    char errorMsg[256] = { 0 };
    FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        errorMsg, sizeof(errorMsg), NULL
    );
    
    size_t len = strlen(errorMsg);
    while (len > 0 && (errorMsg[len-1] == '\n' || errorMsg[len-1] == '\r')) {
        errorMsg[--len] = '\0';
    }
    
    LogPrint("\n[ERROR] %s\n", message);
    if (errorCode != 0) {
        LogPrint("[DETAIL] %s\n", errorMsg);
    }
    
    snprintf(g_lastError, sizeof(g_lastError), "%s", message);
}

static void WaitForKeyPress(const char* message) {
    LogPrint("\n%s\n", message);
    LogPrint("Press any key to continue...\n");
    
    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
    if (hInput != INVALID_HANDLE_VALUE) {
        FlushConsoleInputBuffer(hInput);
        INPUT_RECORD ir;
        DWORD read;
        while (ReadConsoleInput(hInput, &ir, 1, &read)) {
            if (ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown) {
                break;
            }
        }
    }
}

// ============================================================================
// File Handling
// ============================================================================

static BOOL IsTargetDll(const char* name) {
    if (!name) return FALSE;
    
    const char* fileName = strrchr(name, '/');
    const char* fileName2 = strrchr(name, '\\');
    if (fileName2 && fileName2 > fileName) fileName = fileName2;
    if (fileName) fileName++; else fileName = name;
    
    return (_stricmp(fileName, TARGET_DLL_NAME) == 0);
}

static void DumpDllData(const char* data, unsigned int dataLen, const char* name) {
    snprintf(g_dumpFilePath, sizeof(g_dumpFilePath), 
        "%s\\Assembly-CSharp_dumped.dll", g_dumpDir);

    FILE* fp;
    if (fopen_s(&fp, g_dumpFilePath, "wb") == 0 && fp) {
        fwrite(data, 1, dataLen, fp);
        fclose(fp);
        g_dumpSuccess = TRUE;
        LogPrint("[SUCCESS] Saved: %s\n", g_dumpFilePath);
        LogPrint("[INFO] File size: %u bytes\n", dataLen);
    } else {
        LogPrint("[ERROR] Failed to create file: %s\n", g_dumpFilePath);
    }
}

// ============================================================================
// Hook Function
// ============================================================================

static void* HookedMonoImageOpenFromDataWithName(
    const char* data,
    unsigned int data_len,
    int need_copy,
    void* status,
    int refonly,
    const char* name
) {
    // Only process target DLL
    if (IsTargetDll(name) && !g_dumpSuccess) {
        LogPrint("\n[FOUND] Target detected: %s\n", TARGET_DLL_NAME);
        
        if (data && data_len > 0) {
            if (data_len >= 2 && data[0] == 'M' && data[1] == 'Z') {
                DumpDllData(data, data_len, name);
            } else {
                LogPrint("[WARNING] Unexpected file format, saving anyway...\n");
                DumpDllData(data, data_len, name);
            }
        }
    }

    // Call original function
    DWORD oldProtect;
    VirtualProtect((LPVOID)g_originalMonoFunc, sizeof(g_originalBytes), PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy((void*)g_originalMonoFunc, g_originalBytes, sizeof(g_originalBytes));
    VirtualProtect((LPVOID)g_originalMonoFunc, sizeof(g_originalBytes), oldProtect, &oldProtect);

    void* result = g_originalMonoFunc(data, data_len, need_copy, status, refonly, name);

    // Reinstall hook if not yet dumped
    if (!g_dumpSuccess) {
        BYTE jumpPatch[14];
        jumpPatch[0] = 0xFF;
        jumpPatch[1] = 0x25;
        jumpPatch[2] = 0x00;
        jumpPatch[3] = 0x00;
        jumpPatch[4] = 0x00;
        jumpPatch[5] = 0x00;
        *(UINT64*)(jumpPatch + 6) = (UINT64)HookedMonoImageOpenFromDataWithName;

        VirtualProtect((LPVOID)g_originalMonoFunc, sizeof(jumpPatch), PAGE_EXECUTE_READWRITE, &oldProtect);
        memcpy((void*)g_originalMonoFunc, jumpPatch, sizeof(jumpPatch));
        VirtualProtect((LPVOID)g_originalMonoFunc, sizeof(jumpPatch), oldProtect, &oldProtect);
    }

    return result;
}

static BOOL InstallHook() {
    HMODULE hMono = NULL;
    
    for (int i = 0; i < 100; i++) {
        hMono = GetModuleHandleA("mono.dll");
        if (!hMono) hMono = GetModuleHandleA("mono-2.0-bdwgc.dll");
        if (!hMono) hMono = GetModuleHandleA("mono-2.0-sgen.dll");
        if (!hMono) hMono = GetModuleHandleA("MonoBleedingEdge.dll");
        if (hMono) break;
        Sleep(100);
    }

    if (!hMono) {
        LogError("mono.dll not found. Make sure this is a Mono-based Unity game.");
        return FALSE;
    }

    g_originalMonoFunc = (mono_image_open_from_data_with_name_t)
        GetProcAddress(hMono, "mono_image_open_from_data_with_name");
    
    if (!g_originalMonoFunc) {
        LogError("Target function not found. Unsupported Mono version.");
        return FALSE;
    }

    memcpy(g_originalBytes, (void*)g_originalMonoFunc, sizeof(g_originalBytes));

    BYTE jumpPatch[14];
    jumpPatch[0] = 0xFF;
    jumpPatch[1] = 0x25;
    jumpPatch[2] = 0x00;
    jumpPatch[3] = 0x00;
    jumpPatch[4] = 0x00;
    jumpPatch[5] = 0x00;
    *(UINT64*)(jumpPatch + 6) = (UINT64)HookedMonoImageOpenFromDataWithName;

    DWORD oldProtect;
    if (!VirtualProtect((LPVOID)g_originalMonoFunc, sizeof(jumpPatch), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        LogError("Memory protection modification failed.");
        return FALSE;
    }
    memcpy((void*)g_originalMonoFunc, jumpPatch, sizeof(jumpPatch));
    VirtualProtect((LPVOID)g_originalMonoFunc, sizeof(jumpPatch), oldProtect, &oldProtect);

    return TRUE;
}

// ============================================================================
// Proxy Functions
// ============================================================================

static BOOL LoadOriginalUnityPlayer() {
    char path[MAX_PATH];
    char originalPath[MAX_PATH];

    GetModuleFileNameA(NULL, path, MAX_PATH);
    char* lastSlash = strrchr(path, '\\');
    if (lastSlash) *lastSlash = '\0';

    strcpy_s(g_dumpDir, sizeof(g_dumpDir), path);

    snprintf(originalPath, sizeof(originalPath), "%s\\UnityPlayer_Original.dll", path);
    
    g_hOriginalUnityPlayer = LoadLibraryA(originalPath);
    if (!g_hOriginalUnityPlayer) {
        g_hOriginalUnityPlayer = LoadLibraryA("UnityPlayer_Original.dll");
    }

    if (g_hOriginalUnityPlayer) {
        return TRUE;
    } else {
        LogError("UnityPlayer_Original.dll not found. Please rename original UnityPlayer.dll first.");
        return FALSE;
    }
}

#define PROXY_EXPORT extern "C" __declspec(dllexport)

static FARPROC GetOriginalProc(const char* name) {
    if (g_hOriginalUnityPlayer) {
        return GetProcAddress(g_hOriginalUnityPlayer, name);
    }
    return NULL;
}

// ============================================================================
// Hook Thread
// ============================================================================

static DWORD WINAPI HookThreadProc(LPVOID lpParameter) {
    HMODULE hMono = NULL;
    
    hMono = GetModuleHandleA("mono.dll");
    if (!hMono) hMono = GetModuleHandleA("mono-2.0-bdwgc.dll");
    if (!hMono) hMono = GetModuleHandleA("mono-2.0-sgen.dll");
    if (!hMono) hMono = GetModuleHandleA("MonoBleedingEdge.dll");
    
    int waitCount = 0;
    while (!hMono && waitCount < 1000) {
        Sleep(10);
        hMono = GetModuleHandleA("mono.dll");
        if (!hMono) hMono = GetModuleHandleA("mono-2.0-bdwgc.dll");
        if (!hMono) hMono = GetModuleHandleA("mono-2.0-sgen.dll");
        if (!hMono) hMono = GetModuleHandleA("MonoBleedingEdge.dll");
        waitCount++;
    }
    
    BOOL success = InstallHook();
    
    if (!success) {
        LogPrint("\n========================================\n");
        LogPrint("Initialization Failed\n");
        LogPrint("========================================\n");
        LogPrint("Reason: %s\n", g_lastError);
        WaitForKeyPress("Please check and try again.");
    } else {
        LogPrint("[READY] Monitoring for %s...\n", TARGET_DLL_NAME);
    }
    
    return 0;
}

// ============================================================================
// DLL Entry Point
// ============================================================================

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        CreateDebugConsole();
        
        LogPrint("========================================\n");
        LogPrint("  Assembly-CSharp Dumper\n");
        LogPrint("========================================\n");
        LogPrint("\n");
        
        if (!LoadOriginalUnityPlayer()) {
            LogPrint("\nInitialization failed!\n");
            WaitForKeyPress("Please rename original UnityPlayer.dll to UnityPlayer_Original.dll");
        }
        
        // Try to install hook immediately
        {
            HMODULE hMono = GetModuleHandleA("mono.dll");
            if (!hMono) hMono = GetModuleHandleA("mono-2.0-bdwgc.dll");
            if (!hMono) hMono = GetModuleHandleA("mono-2.0-sgen.dll");
            if (!hMono) hMono = GetModuleHandleA("MonoBleedingEdge.dll");
            
            if (hMono) {
                InstallHook();
                LogPrint("[READY] Monitoring for %s...\n", TARGET_DLL_NAME);
            } else {
                CreateThread(NULL, 0, HookThreadProc, NULL, 0, NULL);
            }
        }
        break;

    case DLL_PROCESS_DETACH:
        LogPrint("\n========================================\n");
        if (g_dumpSuccess) {
            LogPrint("Extraction Complete!\n");
            LogPrint("File: %s\n", g_dumpFilePath);
        } else {
            LogPrint("Failed to extract %s\n", TARGET_DLL_NAME);
        }
        LogPrint("========================================\n");
        
        if (g_hasError || !g_dumpSuccess) {
            WaitForKeyPress("Press any key to close...");
        }
        
        if (g_logFile) {
            fclose(g_logFile);
            g_logFile = NULL;
        }
        
        if (g_hOriginalUnityPlayer) {
            FreeLibrary(g_hOriginalUnityPlayer);
        }
        break;
    }
    return TRUE;
}

// ============================================================================
// Exported Functions
// ============================================================================

PROXY_EXPORT void UnityMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                            LPWSTR lpCmdLine, int nShowCmd) {
    typedef void (*UnityMain_t)(HINSTANCE, HINSTANCE, LPWSTR, int);
    UnityMain_t func = (UnityMain_t)GetOriginalProc("UnityMain");
    if (func) func(hInstance, hPrevInstance, lpCmdLine, nShowCmd);
}
