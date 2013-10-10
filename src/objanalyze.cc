#if 0  // A cute trick to making this .cc self-building from shell.
g++ $0 -O2 -Wall -Werror -o `basename $0 .cc`;
exit;
#endif
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

#include "mesh.h"
#include "optimize.h"

// Return cache misses from a simulated FIFO cache.
template <typename IndexListT>
size_t CountFifoCacheMisses(const IndexListT& indices, const size_t cache_size) {
  static const size_t kMaxCacheSize = 32;
  static const int kUnknownIndex = -1;
  CHECK(cache_size <= kMaxCacheSize);
  int fifo[kMaxCacheSize + 1];
  for (size_t i = 0; i < cache_size; ++i) {
    fifo[i] = kUnknownIndex;
  }
  size_t misses = 0;
  for (size_t i = 0; i < indices.size(); ++i) {
    const int idx = indices[i];
    // Use a sentry to simplify the FIFO search.
    fifo[cache_size] = idx;
    size_t at = 0;
    while (fifo[at] != idx) ++at;
    if (at == cache_size) {
      ++misses;
      int write_idx = idx;
      for (size_t j = 0; j < cache_size; ++j) {
        const int swap_idx = fifo[j];
        fifo[j] = write_idx;
        write_idx = swap_idx;
      }
    }
  }
  return misses;
}

template <typename IndexListT>
void PrintCacheAnalysisRow(const IndexListT& indices, const size_t cache_size,
                           const size_t num_verts, const size_t num_tris) {
  const size_t misses = CountFifoCacheMisses(indices, cache_size);
  const double misses_as_double = static_cast<double>(misses);
  printf("||"SIZET_FORMAT"||"SIZET_FORMAT"||%f||%f||\n", cache_size, misses,
         misses_as_double / num_verts, misses_as_double / num_tris);
}

template <typename IndexListT>
void PrintCacheAnalysisTable(const size_t count, const char** args,
                             const IndexListT& indices, 
                             const size_t num_verts, const size_t num_tris) {
  printf(SIZET_FORMAT" vertices, "SIZET_FORMAT" triangles\n\n", num_verts, num_tris);
  puts("||Cache Size||# misses||ATVR||ACMR||");
  for (size_t i = 0; i < count; ++i) {
    int cache_size = atoi(args[i]);
    if (cache_size > 1) {
      PrintCacheAnalysisRow(indices, cache_size, num_verts, num_tris);
    }
  }
}

int main(int argc, const char* argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s in.obj [list of cache sizes]\n\n"
            "\tPerform vertex cache analysis on in.obj using specified sizes.\n"
            "\tFor example: %s in.obj 6 16 24 32\n"
            "\tMaximum cache size is 32.\n\n",
            argv[0], argv[0]);
    return -1;
  }
  FILE* fp = fopen(argv[1], "r");
  WavefrontObjFile obj(fp);
  fclose(fp);
  std::vector<DrawMesh> meshes;
  obj.CreateDrawMeshes(&meshes);
  const DrawMesh& draw_mesh = meshes[0];

  size_t count = 4;
  const char* default_args[] = { "6", "16", "24", "32" };
  const char** args = &default_args[0];
  if (argc > 2) {
    count = argc - 1;
    args = argv + 1;
  }

  puts("\nBefore:\n");
  PrintCacheAnalysisTable(count, args, draw_mesh.indices,
                          draw_mesh.attribs.size() / 8,
                          draw_mesh.indices.size() / 3);

  QuantizedAttribList attribs;
  BoundsParams bounds_params;
  AttribsToQuantizedAttribs(meshes[0].attribs, &bounds_params, &attribs);
  VertexOptimizer vertex_optimizer(attribs, meshes[0].indices);
  WebGLMeshList webgl_meshes;
  vertex_optimizer.GetOptimizedMeshes(&webgl_meshes);
  for (size_t i = 0; i < webgl_meshes.size(); ++i) {
    puts("\nAfter:\n");
    PrintCacheAnalysisTable(count, args, webgl_meshes[i].indices,
                            webgl_meshes[i].attribs.size() / 8,
                            webgl_meshes[i].indices.size() / 3);
  }
  return 0;
}
