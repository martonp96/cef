// Copyright (c) 2024 Marshall A. Greenblatt. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the name Chromium Embedded
// Framework nor the names of its contributors may be used to endorse
// or promote products derived from this software without specific prior
// written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// ---------------------------------------------------------------------------
//
// This file was generated by the make_api_hash_header.py tool.
//

#ifndef CEF_INCLUDE_API_HASH_H_
#define CEF_INCLUDE_API_HASH_H_

#include "include/internal/cef_export.h"

// The API hash is created by analyzing CEF header files for C API type
// definitions. The hash value will change when header files are modified in a
// way that may cause binary incompatibility with other builds. The universal
// hash value will change if any platform is affected whereas the platform hash
// values will change only if that particular platform is affected.
#define CEF_API_HASH_UNIVERSAL "ed9b17c85e769cb4d087390a77bed6613fed9166"
#if defined(OS_WIN)
#define CEF_API_HASH_PLATFORM "5f47de7bf681782160a81ee201cef71f82ac9d69"
#elif defined(OS_MAC)
#define CEF_API_HASH_PLATFORM "d4ca78dc9bc6357c4a05679b2da0863b543f96fa"
#elif defined(OS_LINUX)
#define CEF_API_HASH_PLATFORM "2dfd768af8ca7aa0474a2014b05cec33f64d83b0"
#endif

#ifdef __cplusplus
extern "C" {
#endif

///
// Returns CEF API hashes for the libcef library. The returned string is owned
// by the library and should not be freed. The |entry| parameter describes which
// hash value will be returned:
// 0 - CEF_API_HASH_PLATFORM
// 1 - CEF_API_HASH_UNIVERSAL
// 2 - CEF_COMMIT_HASH (from cef_version.h)
///
CEF_EXPORT const char* cef_api_hash(int entry);

#ifdef __cplusplus
}
#endif
#endif  // CEF_INCLUDE_API_HASH_H_
