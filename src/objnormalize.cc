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

#define _CRT_SECURE_NO_WARNINGS

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <float.h>

char *trim(char *s)
{
	// left-trim
	while (isspace(*s)) { ++s; }

	// right-trim
	int l = strlen(s);
	while (l && isspace(s[l-1])) { --l; }
	s[l] = 0;

	return s;
}

typedef union _point3d
{
	float vals[3];
	struct { float x, y, z; };
} point3d;

point3d read_point3d(char *s)
{
	point3d p;
	p.x = (float)strtod(s, &s);
	p.y = (float)strtod(s, &s);
	p.z = (float)strtod(s, &s);
	return p;
}

int main(int argc, char* argv[])
{
	if (argc != 3)
	{
		printf("Usage: %s input.obj output.obj\n", argv[0]);
		return 1;
	}

	char line_buf[1024]; // max line length is 1 kb

	char *in_name = argv[1];
	char *out_name = argv[2];

	// Read input file and find bounding box
	point3d min = { { FLT_MAX, FLT_MAX, FLT_MAX } }, max = { { FLT_MIN, FLT_MIN, FLT_MIN } };
	int count = 0;
	FILE *in = fopen(in_name, "r");
	while (fgets(line_buf, sizeof(line_buf), in) != NULL)
	{
		if (line_buf[0] == 0) { /* TODO: error! */ }
		char *line = trim(line_buf);
		if (line[0] == 0 || line[0] == '#') { continue; } // blank lines and comments
		if (line[0] == 'v' && isspace(line[1]))
		{
			// vertex
			point3d v = read_point3d(trim(line+2));
			if (v.x < min.x) min.x = v.x; if (v.y < min.y) min.y = v.y; if (v.z < min.z) min.z = v.z;
			if (v.x > max.x) max.x = v.x; if (v.y > max.y) max.y = v.y; if (v.z > max.z) max.z = v.z;
			++count;
		}

		// TODO:
		//   vn (normal vector)
		//   g? (group)
		//   p (point)
		//   l (line)

		// Ignore:
		//   vt
		//   f
		//   fo
		//   usemtl
		//   mtllib
		//   s
		//   anything else
	}
	rewind(in);
	point3d shift = { { -(max.x + min.x) / 2, -(max.y + min.y) / 2, -(max.z + min.z) / 2 } };
	printf("Read %d vertices\n", count);
	printf("Bounding box: (%f,%f,%f),(%f,%f,%f)\n", min.x, min.y, min.z, max.x, max.y, max.z);
	printf("Shift: (%+f,%+f,%+f)\n", shift.x, shift.y, shift.z);

	// Re-read input file, shifting all vertices
	FILE *out = fopen(out_name, "wb");
	while (fgets(line_buf, sizeof(line_buf), in) != NULL)
	{
		if (line_buf[0] == 0) { /* TODO: error! */ }
		char *line = trim(line_buf);
		if (line[0] == 0 || line[0] == '#')
		{
			fputs(line, out);
			fputc('\n', out);
		}
		else if (line[0] == 'v' && isspace(line[1]))
		{
			// vertex
			point3d v = read_point3d(trim(line+2));
			fprintf(out, "v %f %f %f\n", v.x + shift.x, v.y + shift.y, v.z + shift.z);
		}
		// TODO:
		//   vn (normal vector)
		//   g? (group)
		//   p (point)
		//   l (line)
		else
		{
		// Ignore:
		//   vt
		//   f
		//   fo
		//   usemtl
		//   mtllib
		//   s
		//   anything else
			fputs(line, out);
			fputc('\n', out);
		}
	}

	fclose(in);
	fclose(out);

	return 0;
}

