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

#ifndef WEBGL_LOADER_MESH_H_
#define WEBGL_LOADER_MESH_H_

#define _CRT_SECURE_NO_WARNINGS

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base.h"
#include "utf8.h"

void DumpJsonFromQuantizedAttribs(const QuantizedAttribList& attribs) {
  puts("var attribs = new Uint16Array([");
  for (size_t i = 0; i < attribs.size(); i += 8) {
    printf("%u,%hu,%hu,%hu,%hu,%hu,%hu,%hu,\n",
           attribs[i + 0], attribs[i + 1], attribs[i + 2], attribs[i + 3],
           attribs[i + 4], attribs[i + 5], attribs[i + 6], attribs[i + 7]);
  }
  puts("]);");
}

void DumpJsonFromInterleavedAttribs(const AttribList& attribs) {
  puts("var attribs = new Float32Array([");
  for (size_t i = 0; i < attribs.size(); i += 8) {
    printf("%f,%f,%f,%f,%f,%f,%f,%f,\n",
           attribs[i + 0], attribs[i + 1], attribs[i + 2], attribs[i + 3],
           attribs[i + 4], attribs[i + 5], attribs[i + 6], attribs[i + 7]);
  }
  puts("]);");
}

void DumpJsonFromIndices(const IndexList& indices) {
  puts("var indices = new Uint16Array([");
  for (size_t i = 0; i < indices.size(); i += 3) {
    printf("%d,%d,%d,\n", indices[i + 0], indices[i + 1], indices[i + 2]);
  }
  puts("]);");
}

// A short list of floats, useful for parsing a single vector
// attribute.
class ShortFloatList {
 public:
  // MeshLab can create position attributes with
  // color coordinates like: v x y z r g b
  static const size_t kMaxNumFloats = 6;
  ShortFloatList()
      : size_(0)
  {
    clear();
  }

  void clear() {
    for (size_t i = 0; i < kMaxNumFloats; ++i) {
      a_[i] = 0.f;
    }
  }

  // Parse up to kMaxNumFloats from C string.
  // TODO: this should instead return endptr, since size
  // is recoverable.
  size_t ParseLine(const char* line) {
    for (size_ = 0; size_ != kMaxNumFloats; ++size_) {
      char* endptr = NULL;
      a_[size_] = strtof(line, &endptr);
      if (endptr == NULL || line == endptr) break;
      line = endptr;
    }
    return size_;
  }

  float operator[](size_t idx) const {
    return a_[idx];
  }

  void AppendTo(AttribList* attribs) const {
    AppendNTo(attribs, size_);
  }

  void AppendNTo(AttribList* attribs, const size_t sz) const {
    attribs->insert(attribs->end(), a_, a_ + sz);
  }

  bool empty() const { return size_ == 0; }

  size_t size() const { return size_; }
 private:
  float a_[kMaxNumFloats];
  size_t size_;
};

class IndexFlattener {
 public:
  explicit IndexFlattener(size_t num_positions)
      : count_(0),
        table_(num_positions) {
  }

  int count() const { return count_; }

  void reserve(size_t size) {
    table_.reserve(size);
  }

  // Returns a pair of: < flattened index, newly inserted >.
  std::pair<int, bool> GetFlattenedIndex(int position_index,
                                         int texcoord_index,
                                         int normal_index) {
    if (position_index >= static_cast<int>(table_.size())) {
      table_.resize(position_index + 1);
    }
    // First, optimistically look up position_index in the table.
    IndexType& index = table_[position_index];
    if (index.position_or_flat == kIndexUnknown) {
      // This is the first time we've seen this position in the table,
      // so fill it. Since the table is indexed by position, we can
      // use the position_or_flat_index field to store the flat index.
      const int flat_index = count_++;
      index.position_or_flat = flat_index;
      index.texcoord = texcoord_index;
      index.normal = normal_index;
      return std::make_pair(flat_index, true);
    } else if (index.position_or_flat == kIndexNotInTable) {
      // There are multiple flattened indices at this position index,
      // so resort to the map.
      return GetFlattenedIndexFromMap(position_index,
                                      texcoord_index,
                                      normal_index);
    } else if (index.texcoord == texcoord_index &&
               index.normal == normal_index) {
      // The other indices match, so we can use the value cached in
      // the table.
      return std::make_pair(index.position_or_flat, false);
    }
    // The other indices don't match, so we mark this table entry,
    // and insert both the old and new indices into the map.
    const IndexType old_index(position_index, index.texcoord, index.normal);
    map_.insert(std::make_pair(old_index, index.position_or_flat));
    index.position_or_flat = kIndexNotInTable;
    const IndexType new_index(position_index, texcoord_index, normal_index);
    const int flat_index = count_++;
    map_.insert(std::make_pair(new_index, flat_index));
    return std::make_pair(flat_index, true);
  }
 private:
  std::pair<int, bool> GetFlattenedIndexFromMap(int position_index,
                                                int texcoord_index,
                                                int normal_index) {
    IndexType index(position_index, texcoord_index, normal_index);
    MapType::iterator iter = map_.lower_bound(index);
    if (iter == map_.end() || iter->first != index) {
      const int flat_index = count_++;
      map_.insert(iter, std::make_pair(index, flat_index));
      return std::make_pair(flat_index, true);
    } else {
      return std::make_pair(iter->second, false);
    }
  }

  static const int kIndexUnknown = -1;
  static const int kIndexNotInTable = -2;

  struct IndexType {
    IndexType()
        : position_or_flat(kIndexUnknown),
          texcoord(kIndexUnknown),
          normal(kIndexUnknown)
    { }

    IndexType(int position_index, int texcoord_index, int normal_index)
        : position_or_flat(position_index),
          texcoord(texcoord_index),
          normal(normal_index)
    { }

    // I'm being tricky/lazy here. The table_ stores the flattened
    // index in the first field, since it is indexed by position. The
    // map_ stores position and uses this struct as a key to lookup the
    // flattened index.
    int position_or_flat;
    int texcoord;
    int normal;

    // An ordering for std::map.
    bool operator<(const IndexType& that) const {
      if (position_or_flat == that.position_or_flat) {
        if (texcoord == that.texcoord) {
          return normal < that.normal;
        } else {
          return texcoord < that.texcoord;
        }
      } else {
        return position_or_flat < that.position_or_flat;
      }
    }

    bool operator==(const IndexType& that) const {
      return position_or_flat == that.position_or_flat &&
          texcoord == that.texcoord && normal == that.normal;
    }

    bool operator!=(const IndexType& that) const {
      return !operator==(that);
    }
  };
  typedef std::map<IndexType, int> MapType;

  int count_;
  std::vector<IndexType> table_;
  MapType map_;
};

static inline size_t positionDim() { return 3; }
static inline size_t texcoordDim() { return 2; }
static inline size_t normalDim() { return 3; }

struct Bounds {
  float mins[8];
  float maxes[8];

  void Clear() {
    for (size_t i = 0; i < 8; ++i) {
      mins[i] = FLT_MAX;
      maxes[i] = -FLT_MAX;
    }
  }

  void EncloseAttrib(const float* attribs) {
    for (size_t i = 0; i < 8; ++i) {
      const float attrib = attribs[i];
      if (mins[i] > attrib) {
        mins[i] = attrib;
      }
      if (maxes[i] < attrib) {
        maxes[i] = attrib;
      }
    }
  }

  void Enclose(const AttribList& attribs) {
    for (size_t i = 0; i < attribs.size(); i += 8) {
      EncloseAttrib(&attribs[i]);
    }
  }

  float UniformScale() const {
    const float x = maxes[0] - mins[0];
    const float y = maxes[1] - mins[1];
    const float z = maxes[2] - mins[2];
    return (x > y)  // TODO: max3
        ? ((x > z) ? x : z)
        : ((y > z) ? y : z);
  }
};

// TODO(wonchun): Make a c'tor to properly initialize.
struct GroupStart {
  size_t offset;  // offset into draw_mesh_.indices.
  unsigned int group_line;
  int min_index, max_index;  // range into attribs.
  Bounds bounds;
};

class DrawBatch {
 public:
  DrawBatch()
      : flattener_(0),
        current_group_line_(0xFFFFFFFF) {
  }

  const std::vector<GroupStart>& group_starts() const {
    return group_starts_;
  }

  void Init(AttribList* positions, AttribList* texcoords, AttribList* normals) {
    positions_ = positions;
    texcoords_ = texcoords;
    normals_ = normals;
    flattener_.reserve(1024);
  }

  void AddTriangle(unsigned int group_line, int* indices) {
    if (group_line != current_group_line_) {
      current_group_line_ = group_line;
      GroupStart group_start;
      group_start.offset = draw_mesh_.indices.size();
      group_start.group_line = group_line;
      group_start.min_index = INT_MAX;
      group_start.max_index = INT_MIN;
      group_start.bounds.Clear();
      group_starts_.push_back(group_start);
    }
    GroupStart& group = group_starts_.back();
    for (size_t i = 0; i < 9; i += 3) {
      // .OBJ files use 1-based indexing.
      const int position_index = indices[i + 0] - 1;
      const int texcoord_index = indices[i + 1] - 1;
      const int normal_index = indices[i + 2] - 1;
      const std::pair<int, bool> flattened = flattener_.GetFlattenedIndex(
          position_index, texcoord_index, normal_index);
      const int flat_index = flattened.first;
      CHECK(flat_index >= 0);
      draw_mesh_.indices.push_back(flat_index);
      if (flattened.second) {
        // This is a new index. Keep track of index ranges and vertex
        // bounds.
        if (flat_index > group.max_index) {
          group.max_index = flat_index;
        }
        if (flat_index < group.min_index) {
          group.min_index = flat_index;
        }
        const size_t new_loc = draw_mesh_.attribs.size();
        CHECK(8*size_t(flat_index) == new_loc);
        for (size_t i = 0; i < positionDim(); ++i) {
          draw_mesh_.attribs.push_back(
              positions_->at(positionDim() * position_index + i));
        }
        if (texcoord_index == -1) {
          for (size_t i = 0; i < texcoordDim(); ++i) {
            draw_mesh_.attribs.push_back(0);
          }
        } else {
          for (size_t i = 0; i < texcoordDim(); ++i) {
            draw_mesh_.attribs.push_back(
                texcoords_->at(texcoordDim() * texcoord_index + i));
          }
        }
        if (normal_index == -1) {
          for (size_t i = 0; i < normalDim(); ++i) {
            draw_mesh_.attribs.push_back(0);
          }
        } else {
          for (size_t i = 0; i < normalDim(); ++i) {
            draw_mesh_.attribs.push_back(
                normals_->at(normalDim() * normal_index + i));
          }
        }
        // TODO: is the covariance body useful for anything?
        group.bounds.EncloseAttrib(&draw_mesh_.attribs[new_loc]);
      }
    }
  }

  const DrawMesh& draw_mesh() const {
    return draw_mesh_;
  }
 private:
  AttribList* positions_, *texcoords_, *normals_;
  DrawMesh draw_mesh_;
  IndexFlattener flattener_;
  unsigned int current_group_line_;
  std::vector<GroupStart> group_starts_;
};

struct Material {
  std::string name;
  float Ka[3], Kd[3], Ks[3], Ns, d;
  std::string map_Ka, map_Kd, map_Ks, map_Ns, map_d;

  Material() : Ns(HUGE_VALF), d(HUGE_VALF) {
    Ka[0] = HUGE_VALF; Ka[1] = HUGE_VALF; Ka[2] = HUGE_VALF;
    Kd[0] = HUGE_VALF; Kd[1] = HUGE_VALF; Kd[2] = HUGE_VALF;
    Ks[0] = HUGE_VALF; Ks[1] = HUGE_VALF; Ks[2] = HUGE_VALF;
  }

  void DumpJson() const {
    // TODO: JSON serialization needs to be better!
#ifdef MINI_JS
    const char* sep = "";
    printf("'%s':{", name.c_str());
    if (Ka[0] != HUGE_VALF && Ka[1] != HUGE_VALF && Ka[2] != HUGE_VALF) { printf("%sKa:[%hu,%hu,%hu]", sep, Quantize(Ka[0], 0, 1, 255), Quantize(Ka[1], 0, 1, 255), Quantize(Ka[2], 0, 1, 255)); sep = ","; }
    if (Kd[0] != HUGE_VALF && Kd[1] != HUGE_VALF && Kd[2] != HUGE_VALF) { printf("%sKd:[%hu,%hu,%hu]", sep, Quantize(Kd[0], 0, 1, 255), Quantize(Kd[1], 0, 1, 255), Quantize(Kd[2], 0, 1, 255)); sep = ","; }
    if (Ks[0] != HUGE_VALF && Ks[1] != HUGE_VALF && Ks[2] != HUGE_VALF) { printf("%sKs:[%hu,%hu,%hu]", sep, Quantize(Ks[0], 0, 1, 255), Quantize(Ks[1], 0, 1, 255), Quantize(Ks[2], 0, 1, 255)); sep = ","; }
    if (Ns != HUGE_VALF) { printf("%sNs:%f", sep, Ns); sep = ","; }
    if (d  != HUGE_VALF) { printf("%sd:%hu", sep, Quantize(d, 0, 1, 255)); sep = ","; }
    if (!map_Ka.empty()) { printf("%smap_Ka:'%s'", sep, map_Ka.c_str()); sep = ","; }
    if (!map_Kd.empty()) { printf("%smap_Kd:'%s'", sep, map_Kd.c_str()); sep = ","; }
    if (!map_Ks.empty()) { printf("%smap_Ks:'%s'", sep, map_Ks.c_str()); sep = ","; }
    if (!map_Ns.empty()) { printf("%smap_Ns:'%s'", sep, map_Ns.c_str()); sep = ","; }
    if (!map_d.empty())  { printf("%smap_d:'%s'",  sep, map_d.c_str()); sep = ","; }
    printf("}");
#else
    const char* sep = "\n";
    printf("    '%s': {", name.c_str());
    if (Ka[0] != HUGE_VALF && Ka[1] != HUGE_VALF && Ka[2] != HUGE_VALF) { printf("%s      Ka: [%hu, %hu, %hu]", sep, Quantize(Ka[0], 0, 1, 255), Quantize(Ka[1], 0, 1, 255), Quantize(Ka[2], 0, 1, 255)); sep = ",\n"; }
    if (Kd[0] != HUGE_VALF && Kd[1] != HUGE_VALF && Kd[2] != HUGE_VALF) { printf("%s      Kd: [%hu, %hu, %hu]", sep, Quantize(Kd[0], 0, 1, 255), Quantize(Kd[1], 0, 1, 255), Quantize(Kd[2], 0, 1, 255)); sep = ",\n"; }
    if (Ks[0] != HUGE_VALF && Ks[1] != HUGE_VALF && Ks[2] != HUGE_VALF) { printf("%s      Ks: [%hu, %hu, %hu]", sep, Quantize(Ks[0], 0, 1, 255), Quantize(Ks[1], 0, 1, 255), Quantize(Ks[2], 0, 1, 255)); sep = ",\n"; }
    if (Ns != HUGE_VALF) { printf("%s      Ns: %f", sep, Ns); sep = ",\n"; }
    if (d  != HUGE_VALF) { printf("%s      d: %hu", sep, Quantize(d, 0, 1, 255)); sep = ",\n"; }
    if (!map_Ka.empty()) { printf("%s      map_Ka: '%s'", sep, map_Ka.c_str()); sep = ",\n"; }
    if (!map_Kd.empty()) { printf("%s      map_Kd: '%s'", sep, map_Kd.c_str()); sep = ",\n"; }
    if (!map_Ks.empty()) { printf("%s      map_Ks: '%s'", sep, map_Ks.c_str()); sep = ",\n"; }
    if (!map_Ns.empty()) { printf("%s      map_Ns: '%s'", sep, map_Ns.c_str()); sep = ",\n"; }
    if (!map_d.empty())  { printf("%s      map_d: '%s'",  sep, map_d.c_str()); sep = ",\n"; }
    printf("\n    }");
#endif
  }
};

typedef std::vector<Material> MaterialList;

class WavefrontMtlFile {
 public:
  explicit WavefrontMtlFile(FILE* fp) {
    ParseFile(fp);
  }

  const MaterialList& materials() const {
    return materials_;
  }

 private:
  // TODO: factor this parsing stuff out.
  void ParseFile(FILE* fp) {
    // TODO: don't use a fixed-size buffer.
    const size_t kLineBufferSize = 256;
    char buffer[kLineBufferSize];
    unsigned int line_num = 1;
    while (fgets(buffer, kLineBufferSize, fp) != NULL) {
      char* stripped = StripLeadingWhitespace(buffer);
      TerminateAtNewlineOrComment(stripped);
      ParseLine(stripped, line_num++);
    }
  }

  void ParseLine(const char* line, unsigned int line_num) {
    switch (*line) {
      case 'K': ParseColor(line + 1, line_num); break;
      case 'N': if (line[1] == 's') { current_->Ns = strtof(line + 2, NULL); } break;
      case 'd': current_->d = strtof(line + 1, NULL); break;
      case 'T': if (line[1] == 'r') { current_->d = strtof(line + 2, NULL); } break;
      case 'm': if (0 == strncmp(line + 1, "ap_", 3)) { ParseMap(line + 4, line_num); } break;
      case 'n': if (0 == strncmp(line + 1, "ewmtl", 5)) { ParseNewmtl(line + 6, line_num); } 
      default: break;
    }
  }

  void ParseColor(const char* line, unsigned int line_num) {
    float* out = NULL;
    switch (*line) {
      case 'a': out = current_->Ka; break;
      case 'd': out = current_->Kd; break;
      case 's': out = current_->Ks; break;
      default: break;
    }
    if (out) {
        ShortFloatList floats;
        floats.ParseLine(line + 1);
        out[0] = floats[0];
        out[1] = floats[1];
        out[2] = floats[2];
    }
  }

  void ParseMap(const char* line, unsigned int line_num) {
    std::string* out = NULL;
    switch (*line) {
      case 'K':
        switch (line[1]) {
          case 'a': out = &current_->map_Ka; break;
          case 'd': out = &current_->map_Kd; break;
          case 's': out = &current_->map_Ks; break;
          default: break;
        }
        break;
      case 'N': if (line[1] == 's') { out = &current_->map_Ns; } break;
      case 'd': line -= 1; out = &current_->map_d; break;
      case 'T': if (line[1] == 'r') { out = &current_->map_d; } break;
      default: break;
    }
    if (out) {
        *out = StripLeadingWhitespace(line + 1);
    }
  }
  
  void ParseNewmtl(const char* line, unsigned int line_num) {
    materials_.push_back(Material());
    current_ = &materials_.back();
    ToLower(StripLeadingWhitespace(line), &current_->name);
  }

  Material* current_;
  MaterialList materials_;
};

typedef std::map<std::string, DrawBatch> MaterialBatches;

// TODO: consider splitting this into a low-level parser and a high-level
// object.
class WavefrontObjFile {
 public:
  explicit WavefrontObjFile(FILE* fp, bool missingMaterialsAsWhite = false) : missingMaterialsAsWhite_(missingMaterialsAsWhite) {
    current_batch_ = &material_batches_[""];
    current_batch_->Init(&positions_, &texcoords_, &normals_);
    current_group_line_ = 0;
    line_to_groups_.insert(std::make_pair(0, "default"));
    ParseFile(fp);
  }

  const MaterialList& materials() const {
    return materials_;
  }

  const MaterialBatches& material_batches() const {
    return material_batches_;
  }

  const std::string& LineToGroup(unsigned int line) const {
    typedef LineToGroups::const_iterator Iterator;
    typedef std::pair<Iterator, Iterator> EqualRange;
    EqualRange equal_range = line_to_groups_.equal_range(line);
    const std::string* best_group = NULL;
    int best_count = 0;
    for (Iterator iter = equal_range.first; iter != equal_range.second;
         ++iter) {
      const std::string& group = iter->second;
      const int count = group_counts_.find(group)->second;
      if (!best_group || (count < best_count)) {
        best_group = &group;
        best_count = count;
      }
    }
    if (!best_group) {
      ErrorLine("no suitable group found", line);
    }
    return *best_group;
  }

  void DumpDebug() const {
    printf("positions size: "SIZET_FORMAT"\ntexcoords size: "SIZET_FORMAT"\nnormals size: "SIZET_FORMAT"\n",
           positions_.size(), texcoords_.size(), normals_.size());
  }
 private:
  WavefrontObjFile() : missingMaterialsAsWhite_(false) { }  // For testing.

  void ParseFile(FILE* fp) {
    // TODO: don't use a fixed-size buffer.
    const size_t kLineBufferSize = 256;
    char buffer[kLineBufferSize] = { 0 };
    unsigned int line_num = 1;
    while (fgets(buffer, kLineBufferSize, fp) != NULL) {
      char* stripped = StripLeadingWhitespace(buffer);
      TerminateAtNewlineOrComment(stripped);
      ParseLine(stripped, line_num++);
    }
  }

  void ParseLine(const char* line, unsigned int line_num) {
    switch (*line) {
      case 'v':
        ParseAttrib(line + 1, line_num);
        break;
      case 'f':
        ParseFace(line + 1, line_num);
        break;
      case 'g':
        if (isspace(line[1])) {
          ParseGroup(line + 2, line_num);
        } else {
          goto unknown;
        }
        break;
      case '\0':
      case '#':
        break;  // Do nothing for comments or blank lines.
      case 'p':
        WarnLine("point unsupported", line_num);
        break;
      case 'l':
        WarnLine("line unsupported", line_num);
        break;
      case 'u':
        if (0 == strncmp(line + 1, "semtl", 5)) {
          ParseUsemtl(line + 6, line_num);
        } else {
          goto unknown;
        }
        break;
      case 'm':
        if (0 == strncmp(line + 1, "tllib", 5)) {
          ParseMtllib(line + 6, line_num);
        } else {
          goto unknown;
        }
        break;
      case 's':
        ParseSmoothingGroup(line + 1, line_num);
        break;
      unknown:
      default:
        WarnLine("unknown keyword", line_num);
        break;
    }
  }

  void ParseAttrib(const char* line, unsigned int line_num) {
    ShortFloatList floats;
    floats.ParseLine(line + 1);
    if (isspace(*line)) {
      ParsePosition(floats, line_num);
    } else if (*line == 't') {
      ParseTexCoord(floats, line_num);
    } else if (*line == 'n') {
      ParseNormal(floats, line_num);
    } else {
      WarnLine("unknown attribute format", line_num);
    }
  }

  void ParsePosition(const ShortFloatList& floats, unsigned int line_num) {
    if (floats.size() != positionDim() &&
        floats.size() != 6) {  // ignore r g b for now.
      ErrorLine("bad position", line_num);
    }
    floats.AppendNTo(&positions_, positionDim());
  }

  void ParseTexCoord(const ShortFloatList& floats, unsigned int line_num) {
    if ((floats.size() < 1) || (floats.size() > 3)) {
      // TODO: correctly handle 3-D texcoords instead of just
      // truncating.
      ErrorLine("bad texcoord", line_num);
    }
    floats.AppendNTo(&texcoords_, texcoordDim());
  }

  void ParseNormal(const ShortFloatList& floats, unsigned int line_num) {
    if (floats.size() != normalDim()) {
      ErrorLine("bad normal", line_num);
    }
    // Normalize to avoid out-of-bounds quantization. This should be
    // optional, in case someone wants to be using the normal magnitude as
    // something meaningful.
    const float x = floats[0];
    const float y = floats[1];
    const float z = floats[2];
    const float scale = 1.0/sqrt(x*x + y*y + z*z);
    if (isfinite(scale)) {
      normals_.push_back(scale * x);
      normals_.push_back(scale * y);
      normals_.push_back(scale * z);
    } else {
      normals_.push_back(0);
      normals_.push_back(0);
      normals_.push_back(0);
    }
  }

  // Parses faces and converts to triangle fans. This is not a
  // particularly good tessellation in general case, but it is really
  // simple, and is perfectly fine for triangles and quads.
  void ParseFace(const char* line, unsigned int line_num) {
    // Also handle face outlines as faces.
    if (*line == 'o') ++line;

    // TODO: instead of storing these indices as-is, it might make
    // sense to flatten them right away. This can reduce memory
    // consumption and improve access locality, especially since .OBJ
    // face indices are so needlessly large.
    int indices[9] = { 0 };
    // The first index acts as the pivot for the triangle fan.
    line = ParseIndices(line, line_num, indices + 0, indices + 1, indices + 2);
    if (line == NULL) {
      ErrorLine("bad first index", line_num);
    }
    line = ParseIndices(line, line_num, indices + 3, indices + 4, indices + 5);
    if (line == NULL) {
      ErrorLine("bad second index", line_num);
    }
    // After the first two indices, each index introduces a new
    // triangle to the fan.
    while ((line = ParseIndices(line, line_num,
                                indices + 6, indices + 7, indices + 8))) {
      current_batch_->AddTriangle(current_group_line_, indices);
      // The most recent vertex is reused for the next triangle.
      indices[3] = indices[6];
      indices[4] = indices[7];
      indices[5] = indices[8];
      indices[6] = indices[7] = indices[8] = 0;
    }
  }

  // Parse a single group of indices, separated by slashes ('/').
  // TODO: convert negative indices (that is, relative to the end of
  // the current vertex positions) to more conventional positive
  // indices.
  const char* ParseIndices(const char* line, unsigned int line_num,
                           int* position_index, int* texcoord_index,
                           int* normal_index) {
    const char* endptr = NULL;
    *position_index = strtoint(line, &endptr);
    if (*position_index == 0) {
      return NULL;
    }
    if (endptr != NULL && *endptr == '/') {
      *texcoord_index = strtoint(endptr + 1, &endptr);
    } else {
      *texcoord_index = *normal_index = 0;
    }
    if (endptr != NULL && *endptr == '/') {
      *normal_index = strtoint(endptr + 1, &endptr);
    } else {
      *normal_index = 0;
    }
    return endptr;
  }

  // .OBJ files can specify multiple groups for a set of faces. This
  // implementation finds the "most unique" group for a set of faces
  // and uses that for the batch. In the first pass, we use the line
  // number of the "g" command to tag the faces. Afterwards, after we
  // collect group populations, we can go back and give them real
  // names.
  void ParseGroup(const char* line, unsigned int line_num) {
    std::string token;
    while ((line = ConsumeFirstToken(line, &token))) {
      ToLowerInplace(&token);
      group_counts_[token]++;
      line_to_groups_.insert(std::make_pair(line_num, token));
    }
    current_group_line_ = line_num;
  }

  void ParseSmoothingGroup(const char* line, unsigned int line_num) {
    static bool once = true;
    if (once) {
      WarnLine("s ignored", line_num);
      once = false;
    }
  }

  void ParseMtllib(const char* line, unsigned int line_num) {
    FILE* fp = fopen(StripLeadingWhitespace(line), "r");
    if (!fp) {
      WarnLine("mtllib not found", line_num);
      return;
    }
    WavefrontMtlFile mtlfile(fp);
    fclose(fp);
    materials_ = mtlfile.materials();
    for (size_t i = 0; i < materials_.size(); ++i) {
      DrawBatch& draw_batch = material_batches_[materials_[i].name];
      draw_batch.Init(&positions_, &texcoords_, &normals_);
    }
  }

  void ParseUsemtl(const char* line, unsigned int line_num) {
    std::string usemtl;
    ToLower(StripLeadingWhitespace(line), &usemtl);
    MaterialBatches::iterator iter = material_batches_.find(usemtl);
    if (iter == material_batches_.end()) {
      if (missingMaterialsAsWhite_) {
        WarnLine("material not found - using solid white", line_num);
        
        materials_.push_back(Material());
        Material* current_ = &materials_.back();
        current_->name = usemtl;
        current_->Kd[0] = 1;
        current_->Kd[1] = 1;
        current_->Kd[2] = 1;

        DrawBatch& draw_batch = material_batches_[usemtl];
        draw_batch.Init(&positions_, &texcoords_, &normals_);
        current_batch_ = &draw_batch;
      } else {
        ErrorLine("material not found", line_num);
      }
    } else {
      current_batch_ = &iter->second;
    }
  }

  void WarnLine(const char* why, unsigned int line_num) const {
    fprintf(stderr, "WARNING: %s at line %u\n", why, line_num);
  }

  void ErrorLine(const char* why, unsigned int line_num) const {
    fprintf(stderr, "ERROR: %s at line %u\n", why, line_num);
    exit(-1);
  }

  bool missingMaterialsAsWhite_;

  AttribList positions_;
  AttribList texcoords_;
  AttribList normals_;
  MaterialList materials_;

  // Currently, batch by texture (i.e. map_Kd).
  MaterialBatches material_batches_;
  DrawBatch* current_batch_;

  typedef std::multimap<unsigned int, std::string> LineToGroups;
  LineToGroups line_to_groups_;
  std::map<std::string, int> group_counts_;
  unsigned int current_group_line_;
};

// TODO: make maxPosition et. al. configurable.
struct BoundsParams {
  static BoundsParams FromBounds(const Bounds& bounds) {
    BoundsParams ret;
    const float scale = bounds.UniformScale();
    // Position. Use a uniform scale.
    for (size_t i = 0; i < 3; ++i) {
      const int maxPosition = (1 << 14) - 1;  // 16383;
      ret.mins[i] = bounds.mins[i];
      ret.scales[i] = scale;
      ret.outputMaxes[i] = maxPosition;
      ret.decodeOffsets[i] = (int)(maxPosition * bounds.mins[i] / scale);
      ret.decodeScales[i] = scale / maxPosition;
    }
    // TexCoord.
    // TODO: get bounds-dependent texcoords working!
    for (size_t i = 3; i < 5; ++i) {
      // const float texScale = bounds.maxes[i] - bounds.mins[i];
      const int maxTexcoord = (1 << 10) - 1;  // 1023
      ret.mins[i] = 0;  //bounds.mins[i];
      ret.scales[i] = 1;  //texScale;
      ret.outputMaxes[i] = maxTexcoord;
      ret.decodeOffsets[i] = 0;  //maxTexcoord * bounds.mins[i] / texScale;
      ret.decodeScales[i] = 1.0f / maxTexcoord;  // texScale / maxTexcoord;
    }
    // Normal. Always uniform range.
    for (size_t i = 5; i < 8; ++i) {
      ret.mins[i] = -1;
      ret.scales[i] = 2.f;
      ret.outputMaxes[i] = (1 << 10) - 1;  // 1023
      ret.decodeOffsets[i] = 1 - (1 << 9);  // -511
      ret.decodeScales[i] = 1.0f / 511;
    }
    return ret;
  }

  void DumpJson() {
#ifdef MINI_JS
    putchar('{');
    printf("decodeOffsets:[%d,%d,%d,%d,%d,%d,%d,%d],",
           decodeOffsets[0], decodeOffsets[1], decodeOffsets[2],
           decodeOffsets[3], decodeOffsets[4], decodeOffsets[5],
           decodeOffsets[6], decodeOffsets[7]);
    printf("decodeScales:[%f,%f,%f,%f,%f,%f,%f,%f]",
           decodeScales[0], decodeScales[1], decodeScales[2], decodeScales[3],
           decodeScales[4], decodeScales[5], decodeScales[6], decodeScales[7]);
    printf("},");
#else
    puts("{");
    printf("    decodeOffsets: [%d,%d,%d,%d,%d,%d,%d,%d],\n",
           decodeOffsets[0], decodeOffsets[1], decodeOffsets[2],
           decodeOffsets[3], decodeOffsets[4], decodeOffsets[5],
           decodeOffsets[6], decodeOffsets[7]);
    printf("    decodeScales: [%f,%f,%f,%f,%f,%f,%f,%f]\n",
           decodeScales[0], decodeScales[1], decodeScales[2], decodeScales[3],
           decodeScales[4], decodeScales[5], decodeScales[6], decodeScales[7]);
    puts("  },");
#endif
  }

  float mins[8];
  float scales[8];
  int outputMaxes[8];
  int decodeOffsets[8];
  float decodeScales[8];
};

void AttribsToQuantizedAttribs(const AttribList& interleaved_attribs,
                               const BoundsParams& bounds_params,
                               QuantizedAttribList* quantized_attribs) {
  quantized_attribs->resize(interleaved_attribs.size());
  for (size_t i = 0; i < interleaved_attribs.size(); i += 8) {
    for (size_t j = 0; j < 8; ++j) {
      quantized_attribs->at(i + j) = Quantize(interleaved_attribs[i + j],
                                              bounds_params.mins[j],
                                              bounds_params.scales[j],
                                              bounds_params.outputMaxes[j]);
    }
  }
}

uint16 ZigZag(int16 word) {
  return (word >> 15) ^ (word << 1);
}

void CompressAABBToUtf8(const Bounds& bounds,
                        const BoundsParams& total_bounds,
                        std::vector<char>* utf8) {
  const int maxPosition = (1 << 14) - 1;  // 16383;
  uint16 mins[3] = { 0 };
  uint16 maxes[3] = { 0 };
  for (int i = 0; i < 3; ++i) {
    float total_min = total_bounds.mins[i];
    float total_scale = total_bounds.scales[i];
    mins[i] = Quantize(bounds.mins[i], total_min, total_scale, maxPosition);
    maxes[i] = Quantize(bounds.maxes[i], total_min, total_scale, maxPosition);
  }
  for (int i = 0; i < 3; ++i) {
    Uint16ToUtf8(mins[i], utf8);
  }
  for (int i = 0; i < 3; ++i) {
    Uint16ToUtf8(maxes[i] - mins[i], utf8);
  }
}

void CompressIndicesToUtf8(const OptimizedIndexList& list,
                           std::vector<char>* utf8) {
  // For indices, we don't do delta from the most recent index, but
  // from the high water mark. The assumption is that the high water
  // mark only ever moves by one at a time. Foruntately, the vertex
  // optimizer does that for us, to optimize for per-transform vertex
  // fetch order.
  uint16 index_high_water_mark = 0;
  for (size_t i = 0; i < list.size(); ++i) {
    const int index = list[i];
    CHECK(index >= 0);
    CHECK(index <= index_high_water_mark);
    CHECK(Uint16ToUtf8(index_high_water_mark - index, utf8));
    if (index == index_high_water_mark) {
      ++index_high_water_mark;
    }
  }
}

void CompressQuantizedAttribsToUtf8(const QuantizedAttribList& attribs,
                                    std::vector<char>* utf8) {
  for (size_t i = 0; i < 8; ++i) {
    // Use a transposed representation, and delta compression.
    uint16 prev = 0;
    for (size_t j = i; j < attribs.size(); j += 8) {
      const uint16 word = attribs[j];
      const uint16 za = ZigZag(static_cast<int16>(word - prev));
      prev = word;
      CHECK(Uint16ToUtf8(za, utf8));
    }
  }
}

#endif  // WEBGL_LOADER_MESH_H_
