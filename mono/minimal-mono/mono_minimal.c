/**
 * Minimal mono.dll - Only exports mono_image_open_from_data_with_name for dumping
 * 
 * This is a stripped-down version that only saves the raw bytes to disk.
 * No actual Mono runtime functionality is included.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>

/* Type definitions to match Mono's API */
typedef int32_t gboolean;
typedef uint32_t guint32;
typedef void* gpointer;

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

/* MonoImage - opaque pointer, we don't actually use it */
typedef struct _MonoImage MonoImage;

/* MonoImageOpenStatus - matches Mono's definition */
typedef enum {
    MONO_IMAGE_OK,
    MONO_IMAGE_ERROR_ERRNO,
    MONO_IMAGE_MISSING_ASSEMBLYREF,
    MONO_IMAGE_IMAGE_INVALID
} MonoImageOpenStatus;

/* Global dump index counter */
static volatile LONG g_dump_index = 0;

/**
 * Internal function to dump bytes to file
 */
static void
dump_image_bytes_to_file(const char *name, const void *data, guint32 len)
{
    LONG index;
    char filename[512];
    FILE *f;
    size_t i, j;
    char safe_name[256];

    /* Atomic increment for thread safety */
    index = InterlockedIncrement(&g_dump_index);

    /* Sanitize filename - replace invalid chars */
    if (name && *name) {
        j = 0;
        for (i = 0; name[i] != '\0' && j < sizeof(safe_name) - 1; i++) {
            char c = name[i];
            /* Only allow safe characters */
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.') {
                safe_name[j++] = c;
            } else if (c == '/' || c == '\\' || c == ':') {
                safe_name[j++] = '_';
            }
        }
        safe_name[j] = '\0';
        
        if (j > 0) {
            snprintf(filename, sizeof(filename), "dump_%ld_%s", index, safe_name);
        } else {
            snprintf(filename, sizeof(filename), "dump_%ld_memory.dll", index);
        }
    } else {
        snprintf(filename, sizeof(filename), "dump_%ld_memory.dll", index);
    }

    f = fopen(filename, "wb");
    if (!f)
        return;

    fwrite(data, 1, len, f);
    fclose(f);
}

/**
 * mono_image_open_from_data_with_name:
 * 
 * Minimal implementation that only dumps the data to a file.
 * Returns NULL as we don't actually load images.
 */
__declspec(dllexport) MonoImage*
mono_image_open_from_data_with_name(char *data, guint32 data_len,
                                    gboolean need_copy,
                                    MonoImageOpenStatus *status,
                                    gboolean refonly,
                                    const char *name)
{
    /* Dump bytes to file */
    if (data && data_len > 0) {
        dump_image_bytes_to_file(name, data, data_len);
    }

    /* Return success status but NULL image (we don't load) */
    if (status)
        *status = MONO_IMAGE_OK;
    
    return NULL;
}

/**
 * DllMain - DLL entry point
 */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hinstDLL);
            break;
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}
