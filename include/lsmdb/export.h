//
// Created by 刘文景 on 2021/3/26.
//

#ifndef STORAGE_LSMDB_INCLUDE_EXPORT_H_
#define STORAGE_LSMDB_INCLUDE_EXPORT_H_

#if !defined(LSMDB_EXPORT)

#if defined(LSMDB_SHARED_LIBRARY)
#if defined(_WIN32)

#if defined(LSMDB_COMPILE_LIBRARY)
#define LSMDB_EXPORT __declspec(dllexport)
#else
#define LSMDB_EXPORT __declspec(dllimport)
#endif  // defined(LSMDB_COMPILE_LIBRARY)

#else  // defined(_WIN32)
#if defined(LSMDB_COMPILE_LIBRARY)
#define LSMDB_EXPORT __attribute__((visibility("default")))
#else
#define LSMDB_EXPORT
#endif
#endif  // defined(_WIN32)

#else  // defined(LSMDB_SHARED_LIBRARY)
#define LSMDB_EXPORT
#endif

#endif  // !defined(LSMDB_EXPORT)

#endif  // STORAGE_LSMDB_INCLUDE_EXPORT_H_
