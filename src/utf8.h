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

#ifndef WEBGL_LOADER_UTF8_H_
#define WEBGL_LOADER_UTF8_H_

#include <vector>

#include "base.h"

const char kUtf8MoreBytesPrefix = (char)0x80; //static_cast<char>(0x80);
const uint16 kUtf8MoreBytesMask = 0x3F;
const char kUtf8TwoBytePrefix = (char)0xC0; //static_cast<char>(0xC0);
const char kUtf8ThreeBytePrefix = (char)0xE0; //static_cast<char>(0xE0);

bool Uint16ToUtf8(uint16 word, std::vector<char>* utf8) {
  if (word < 0x80) {
    utf8->push_back(static_cast<char>(word));
  } else if (word < 0x800) {
    utf8->push_back(kUtf8TwoBytePrefix + static_cast<char>(word >> 6));
    utf8->push_back(kUtf8MoreBytesPrefix +
                    static_cast<char>(word & kUtf8MoreBytesMask));
  } else if (word < 0xF800) {
    // We can only encode 65535 - 2048 values because of illegal UTF-8
    // characters, such as surrogate pairs in [0xD800, 0xDFFF].
    if (word >= 0xD800) {
      // Shift the result to avoid the surrogate pair range.
      word += 0x0800;
    }
    utf8->push_back(kUtf8ThreeBytePrefix + static_cast<char>(word >> 12));
    utf8->push_back(kUtf8MoreBytesPrefix +
                    static_cast<char>((word >> 6) & kUtf8MoreBytesMask));
    utf8->push_back(kUtf8MoreBytesPrefix +
                    static_cast<char>(word & kUtf8MoreBytesMask));
  } else {
    return false;
  }
  return true;
}

#endif  // WEBGL_LOADER_UTF8_H_
