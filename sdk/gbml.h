/* SPDX-License-Identifier: MIT
   Copyright (c) 2026 Dusty926. Full text: LICENSES\MIT.txt. Only this header is MIT; see LICENSE.txt. */
/* gbml.h - GB Mod Loader plugin interface.
 *
 * The loader runs inside IL-2 Great Battles, checks the game build, and loads each plugin from
 * bin\game\gbml\<name>.dll, calling its exported gbml_plugin_init() once, from the loader's own
 * worker thread, early in game start-up.
 *
 * A plugin:
 *   #include "gbml.h"
 *   static const GBML_API *G;
 *   GBML_PLUGIN_EXPORT int gbml_plugin_init(const GBML_API *api)
 *   {
 *       if (api->api_version < 1) return 1;
 *       G = api;                                   // the API struct lives for the whole process
 *       G->log("myplugin", "hello, build %s", api->game_build);
 *       ...verify the exact bytes you will patch, then patch them...
 *       return GBML_OK;                            // 0 = installed; anything else = failed
 *   }
 * Rules:
 *  - Return 0 only if everything went in. On failure, undo what you changed before returning
 *    non-zero. The loader cannot undo your changes and does not unload a plugin whose init ran.
 *  - The loader only proves the game build. Check the bytes you patch before patching.
 *  - Plain C ABI, 64-bit only. Build the plugin with /MT so it carries its own C runtime.
 */
#pragma once
#include <windows.h>
#include <stddef.h>

#define GBML_API_VERSION 4      /* check api->api_version before using newer fields */

/* process (API 3): which program the loader runs in. */
#define GBML_PROCESS_GAME   1   /* the game: gbml_plugin_init() is called                       */
#define GBML_PROCESS_SERVER 2   /* the dedicated server: only plugins that export
                                   gbml_server_plugin_init() and are listed under
                                   [server_plugins] in gbml.ini are loaded; api->il2 = NULL.    */

/* mp_state() values (API 2): what the loader knows about the current multiplayer session. */
#define GBML_MP_NONE        0   /* no multiplayer session                                      */
#define GBML_MP_HOST        1   /* this game hosts                                             */
#define GBML_MP_VERIFIED    2   /* joined a host with the same loader and mods                 */
#define GBML_MP_UNVERIFIED  3   /* joined (or joining) a host that is not verified             */

/* Return codes of write_mem and swap_ptr (0 = success, like gbml_plugin_init). */
#define GBML_OK          0
#define GBML_E_ARG       1   /* NULL pointer, size 0 or too large, misaligned slot    */
#define GBML_E_MEMORY    2   /* target is not usable memory                           */
#define GBML_E_PROTECT   3   /* the page could not be made writable                   */
#define GBML_E_BUSY      4   /* another thread kept executing inside the target range */

#define GBML_WRITE_MAX   65536   /* largest single write_mem */

typedef struct GBML_API {
    int         api_version;      /* GBML_API_VERSION */
    const char *game_build;       /* the game build */
    HMODULE     il2;              /* game module base */
    HMODULE     rse;              /* flight model module base */

    /* Write to bin\game\gbml_log.txt. printf format. Thread-safe. */
    void (*log)(const char *plugin, const char *fmt, ...);

    /* Copy n bytes (1..GBML_WRITE_MAX) from src to dst, code or read-only data. For install-time
       patches, not per-frame writes. Returns GBML_OK or a GBML_E_* code. */
    int  (*write_mem)(void *dst, const void *src, size_t n);

    /* Atomically replace the 8-byte aligned pointer at *slot with new_value. If old is not NULL,
       *old receives the previous value first. Returns GBML_OK or a GBML_E_* code. */
    int  (*swap_ptr)(void **slot, void *new_value, void **old);

    /* Full path of bin\game\gbml\<plugin>.ini. The file may not exist. NULL if the name is not
       1-32 characters of A-Z a-z 0-9 _ -. */
    const char *(*plugin_ini)(const char *plugin);

    /* API 2: the multiplayer state, GBML_MP_*. Read it when it matters, not once at init. */
    int (*mp_state)(void);

    /* API 3: the program's own module and GBML_PROCESS_*. */
    HMODULE     exe;
    int         process;
} GBML_API;

/* A plugin that supports the dedicated server also exports
       GBML_PLUGIN_EXPORT int gbml_server_plugin_init(const GBML_API *api);   (0 = OK) */

#ifdef __cplusplus
#define GBML_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define GBML_PLUGIN_EXPORT __declspec(dllexport)
#endif

/* Optional: the keys of your .ini that do not matter to other players, as a constant string:
       GBML_PLUGIN_EXPORT const char gbml_mp_local_keys[] = "myplugin.log Debug.*";
   Items are "section.key", separated by spaces or commas; '*' = any text. */

/* Optional: a plugin that changes nothing another player could perceive may declare itself
   exempt from the multiplayer match:
       GBML_PLUGIN_EXPORT const char gbml_mp_local[] = "1";
   Do not export this from a plugin that changes anything other players need to agree with. */

/* Optional: a plugin that registers a new plane class exports
       GBML_PLUGIN_EXPORT int gbml_plane_class(const char *plane_class);
   returning 1 only if this plugin registered that class and its install worked, else 0. */
typedef int (*GBML_PLANE_CLASS)(const char *plane_class);

/* Every plugin exports this (name exactly gbml_plugin_init, C linkage). 0 = installed. */
typedef int (*GBML_PLUGIN_INIT)(const GBML_API *api);
