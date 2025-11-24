/**
 * @file Minimal emulation of POSIX dlopen/dlsym/dlclose on Windows.
 * @license Public domain.
 *
 * This code works fine for the common scenario of loading a
 * specific DLL and calling one (or more) functions within it.
 * No attempt is made to emulate POSIX symbol table semantics.
 */

#ifndef _INCLUDE_DLFCN_H_
#define _INCLUDE_DLFCN_H_

#ifdef __cplusplus
extern "C" {
#endif

// POSIX-style flags (ignored on Windows)
#define RTLD_LAZY       0
#define RTLD_NOW        0
#define RTLD_GLOBAL     0
#define RTLD_LOCAL      0

#define RTLD_DEFAULT    ((void*) NULL)
#define RTLD_NEXT       ((void*) NULL)

/**
 * Open DLL, returning a handle.
 *
 * @param file DLL filename (UTF-8)
 * @param mode Mode flags (ignored)
 * @return DLL handle or NULL on failure
 */
void* dlopen(const char* file, int mode);

/**
 * Close DLL.
 *
 * @param handle Handle from dlopen()
 * @return 0 on success, non-zero on failure
 */
int dlclose(void* handle);

/**
 * Look up symbol exported by DLL.
 *
 * @param handle Handle from dlopen()
 * @param name Name of exported symbol (ASCII)
 * @return Address of symbol or NULL on failure
 */
void* dlsym(void* handle, const char* name);

/**
 * Return message describing last error.
 *
 * @return Pointer to static string describing the last error
 */
char* dlerror(void);

#ifdef __cplusplus
}
#endif

#endif // _INCLUDE_DLFCN_H_
