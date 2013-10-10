// Copyright 2011 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you
// may not use this file except in compliance with the License. You
// may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
// implied. See the License for the specific language governing
// permissions and limitations under the License.
#ifndef WEBGL_LOADER_BASE_H_
#define WEBGL_LOADER_BASE_H_

#define _CRT_SECURE_NO_WARNINGS

#include <ctype.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#ifdef _WIN32
#define SIZET_FORMAT "%Iu"
#else
#define SIZET_FORMAT "%zu"
#endif

typedef unsigned short uint16;
typedef short int16;
typedef unsigned int uint32;

#ifndef isfinite
# define isfinite _finite
#endif

typedef std::vector<float> AttribList;
typedef std::vector<int> IndexList;
typedef std::vector<uint16> QuantizedAttribList;
typedef std::vector<uint16> OptimizedIndexList;

// TODO: these data structures ought to go elsewhere.
struct DrawMesh {
  // Interleaved vertex format:
  //  3-D Position
  //  3-D Normal
  //  2-D TexCoord
  // Note that these
  AttribList attribs;
  // Indices are 0-indexed.
  IndexList indices;
};

struct WebGLMesh {
  QuantizedAttribList attribs;
  OptimizedIndexList indices;
};

typedef std::vector<WebGLMesh> WebGLMeshList;

static inline int strtoint(const char* str, const char** endptr) {
  return static_cast<int>(strtol(str, const_cast<char**>(endptr), 10));
}

#ifdef _MSC_VER
union _MSVC_EVIL_FLOAT_HACK { unsigned __int8 Bytes[4]; float Value; };
const static union _MSVC_EVIL_FLOAT_HACK _INFINITY_HACK = {{0x00, 0x00, 0x80, 0x7F}}; // sign: 1 [31] -> either 0 or 1, exponent: 8 [30-23] -> all 1s, fraction: 23 [22-00] -> all 0s
#define HUGE_VALF (_INFINITY_HACK.Value)
static inline float strtof(const char * str, char ** endPtr) {
  double d = strtod(str, endPtr);
  return (float)((d > 0 && (d > FLT_MAX || d < FLT_MIN)) ? HUGE_VALF : ((d < 0 && (d < -FLT_MAX || d > -FLT_MIN)) ? -HUGE_VALF : d));
}
#endif

static inline const char* StripLeadingWhitespace(const char* str) {
  while (isspace(*str)) {
    ++str;
  }
  return str;
}

static inline char* StripLeadingWhitespace(char* str) {
  while (isspace(*str)) {
    ++str;
  }
  return str;
}

// Like basename.
static inline const char* StripLeadingDir(const char* const str) {
  const char* last_slash = NULL;
  const char* pos = str;
  while (const char ch = *pos) {
    if (ch == '/' || ch == '\\') {
      last_slash = pos;
    }
    ++pos;
  }
  return last_slash ? (last_slash + 1) : str;
}

static inline void TerminateAtNewlineOrComment(char* str) {
  char* newline = strpbrk(str, "#\r\n");
  if (newline) {
    *newline = '\0';
  }
}

static inline const char* ConsumeFirstToken(const char* const line,
                                            std::string* token) {
  const char* curr = line;
  while (char ch = *curr) {
    if (isspace(ch)) {
      token->assign(line, curr);
      return curr + 1;
    }
    ++curr;
  }
  if (curr == line) {
    return NULL;
  }
  token->assign(line, curr);
  return curr;
}

static inline void ToLower(const char* in, std::string* out) {
  while (char ch = *in) {
    out->push_back(tolower(ch));
    ++in;
  }
}

static inline void ToLowerInplace(std::string* in) {
  std::string& s = *in;
  for (size_t i = 0; i < s.size(); ++i) {
    s[i] = tolower(s[i]);
  }
}

// Jenkin's One-at-a-time Hash. Not the best, but simple and
// portable.
uint32 SimpleHash(char *key, size_t len, uint32 seed = 0) {
  uint32 hash = seed;
  for(size_t i = 0; i < len; ++i) {
    hash += static_cast<unsigned char>(key[i]);
    hash += (hash << 10);
    hash ^= (hash >> 6);
  }
  hash += (hash << 3);
  hash ^= (hash >> 11);
  hash += (hash << 15);
  return hash;
}

void ToHex(uint32 w, char out[9]) {
  const char kOffset0 = '0';
  const char kOffset10 = 'a' - 10;
  out[8] = '\0';
  for (size_t i = 8; i > 0;) {
    uint32 bits = w & 0xF;
    out[--i] = bits + ((bits < 10) ? kOffset0 : kOffset10);
    w >>= 4;
  }
}

uint16 Quantize(float f, float in_min, float in_scale, uint16 out_max) {
  return static_cast<uint16>(out_max * ((f-in_min) / in_scale));
}

// TODO: Visual Studio calls this someting different.
#ifdef putc_unlocked
# define PutChar putc_unlocked
#else
# define PutChar putc
#endif  // putc_unlocked

#ifndef CHECK
# define CHECK(PRED) if (!(PRED)) {                                     \
    fprintf(stderr, "%s:%d CHECK failed: " #PRED "\n", __FILE__, __LINE__); \
    exit(-1); } else
#endif  // CHECK

#endif  // WEBGL_LOADER_BASE_H_
