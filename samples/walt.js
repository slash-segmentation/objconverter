var WALT_DECODE_PARAMS = {
  decodeOffsets: [-6000, -4095, -6000, 0, 0, 0, 0, 0],
  decodeScales: [1/8191, 1/8191, 1/8191, 0, 0, 0, 0, 0]
};

function computeNormals(attribArray, indexArray, bboxen, meshEntry,
                        opt_normalize) {
  var positionParams = NO_TEXCOORD_VERTEX_FORMAT[0];
  var normalParams = NO_TEXCOORD_VERTEX_FORMAT[1];
  var positionOffset = positionParams.offset;
  var normalOffset = normalParams.offset;
  var stride = normalParams.stride;

  var numIndices = indexArray.length;
  var p0 = vec3.create();
  var p1 = vec3.create();
  var p2 = vec3.create();
  for (var i = 0; i < numIndices; i += 3) {
    var v0 = indexArray[i];
    var v1 = indexArray[i + 1];
    var v2 = indexArray[i + 2];
    for (var j = 0; j < 3; ++j) {
      p0[j] = attribArray[stride * v0 + positionOffset + j];
      p1[j] = attribArray[stride * v1 + positionOffset + j];
      p2[j] = attribArray[stride * v2 + positionOffset + j];
    }
    vec3.subtract(p1, p0);
    vec3.subtract(p2, p0);
    vec3.cross(p1, p2, p0);
    for (var j = 0; j < 3; ++j) {
      attribArray[stride * v0 + normalOffset + j] += p0[j];
      attribArray[stride * v1 + normalOffset + j] += p0[j];
      attribArray[stride * v2 + normalOffset + j] += p0[j];
    }
  }

  if (opt_normalize) {
    var numAttribs = attribArray.length;
    for (var i = normalOffset; i < numAttribs; i += stride) {
      var x = attribArray[i];
      var y = attribArray[i + 1];
      var z = attribArray[i + 2];
      var n = 1.0/Math.sqrt(x*x + y*y + z*z);
      attribArray[i] *= n;
      attribArray[i + 1] *= n;
      attribArray[i + 2] *= n;
    }
  }

  onLoad(attribArray, indexArray, bboxen, meshEntry);
}

downloadMeshes('', {
'walt.utf8': [
  { material: '',
    attribRange: [0, 55294],
    indexRange: [442352, 108806],
  },
  { material: '',
    attribRange: [768770, 27187],
    indexRange: [986266, 52810],
  },
],
}, WALT_DECODE_PARAMS, computeNormals);
