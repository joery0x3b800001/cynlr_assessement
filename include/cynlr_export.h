#pragma once
/**
 * @file    cynlr_export.h
 * @brief   DLL export/import annotation macro for the cynlr library.
 *
 * When building the shared library itself, define CYNLR_BUILDING_DLL.
 * Consumers of the DLL get dllimport automatically.
 * On non-Windows (Linux / macOS shared libraries) the macro is empty
 * because all public symbols are visible by default.
 *
 * Usage in headers:
 *   class CYNLR_API Foo { ... };
 *   CYNLR_API void bar();
 */

#if defined(_WIN32) || defined(_WIN64)
#ifdef CYNLR_BUILDING_DLL
#define CYNLR_API __declspec(dllexport)
#else
#define CYNLR_API __declspec(dllimport)
#endif
#else
#define CYNLR_API
#endif