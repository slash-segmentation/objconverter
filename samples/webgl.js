'use strict';

var DEFAULT_VERTEX_FORMAT = [
  { name: "a_position",
    size: 3,
    stride: 8,
    offset: 0
  },
  { name: "a_texcoord",
    size: 2,
    stride: 8,
    offset: 3
  },
  { name: "a_normal",
    size: 3,
    stride: 8,
    offset: 5
  }
];

var NO_TEXCOORD_VERTEX_FORMAT = [
  { name: "a_position",
    size: 3,
    stride: 8,
    offset: 0
  },
  { name: "a_normal",
    size: 3,
    stride: 8,
    offset: 5
  }
];

var BBOX_VERTEX_FORMAT = [
  { name: "a_position",
    size: 3,
    stride: 6,
    offset: 0
  },
  { name: "a_radius",
    size: 3,
    stride: 6,
    offset: 3
  }
];

function createContextFromCanvas(canvas) {
  var context = canvas.getContext('experimental-webgl');
  // Automatically use debug wrapper context, if available.
  return typeof WebGLDebugUtils !== 'undefined' ?
    WebGLDebugUtils.makeDebugContext(context, function(err, funcName, args) {
      throw WebGLDebugUtils.glEnumToString(err) + " by " + funcName;
    }) : context;
};

function Shader(gl, source, shaderType) {
  this.gl_ = gl;
  this.handle_ = gl.createShader(shaderType);
  gl.shaderSource(this.handle_, source);
  gl.compileShader(this.handle_);
  if (!gl.getShaderParameter(this.handle_, gl.COMPILE_STATUS)) {
    throw this.info();
  }
}

Shader.prototype.info = function() {
  return this.gl_.getShaderInfoLog(this.handle_);
}

Shader.prototype.type = function() {
  return this.gl_.getShaderParameter(this.handle_, this.gl_.SHADER_TYPE);
}

function vertexShader(gl, source) {
  return new Shader(gl, source, gl.VERTEX_SHADER);
}

function fragmentShader(gl, source) {
  return new Shader(gl, source, gl.FRAGMENT_SHADER);
}

function Program(gl, shaders) {
  this.gl_ = gl;
  this.handle_ = gl.createProgram();
  shaders.forEach(function(shader) {
    gl.attachShader(this.handle_, shader.handle_);
  }, this);
  gl.linkProgram(this.handle_);
  if (!gl.getProgramParameter(this.handle_, gl.LINK_STATUS)) {
    throw this.info();
  }

  // TODO: turn these into properties.
  var numActiveAttribs = gl.getProgramParameter(this.handle_,
                                                gl.ACTIVE_ATTRIBUTES);
  this.attribs = [];
  this.set_attrib = {};
  for (var i = 0; i < numActiveAttribs; i++) {
    var active_attrib = gl.getActiveAttrib(this.handle_, i);
    var loc = gl.getAttribLocation(this.handle_, active_attrib.name);
    this.attribs[loc] = active_attrib;
    this.set_attrib[active_attrib.name] = loc;
  }

  var numActiveUniforms = gl.getProgramParameter(this.handle_,
                                                 gl.ACTIVE_UNIFORMS);
  this.uniforms = [];
  this.set_uniform = {};
  for (var j = 0; j < numActiveUniforms; j++) {
    var active_uniform = gl.getActiveUniform(this.handle_, j);
    this.uniforms[j] = active_uniform;
    this.set_uniform[active_uniform.name] = gl.getUniformLocation(
      this.handle_, active_uniform.name);
  }
};

Program.prototype.info = function() {
  return this.gl_.getProgramInfoLog(this.handle_);
};

Program.prototype.use = function() {
  this.gl_.useProgram(this.handle_);
};

Program.prototype.enableVertexAttribArrays = function(vertexFormat) {
  var numAttribs = vertexFormat.length;
  for (var i = 0; i < numAttribs; ++i) {
    var attrib = vertexFormat[i];
    var loc = this.set_attrib[attrib.name];
    if (loc !== undefined) {
      this.gl_.enableVertexAttribArray(loc);
    }
  }
};

Program.prototype.disableVertexAttribArrays = function(vertexFormat) {
  var numAttribs = vertexFormat.length;
  for (var i = 0; i < numAttribs; ++i) {
    var attrib = vertexFormat[i];
    var loc = this.set_attrib[attrib.name];
    if (loc !== undefined) {
      this.gl_.disableVertexAttribArray(loc);
    }
  }
};

Program.prototype.vertexAttribPointers = function(vertexFormat) {
  var numAttribs = vertexFormat.length;
  for (var i = 0; i < numAttribs; ++i) {
    var attrib = vertexFormat[i];
    var loc = this.set_attrib[attrib.name];
    var typeBytes = 4;  // TODO: 4 assumes gl.FLOAT, use params.type
    this.gl_.vertexAttribPointer(loc, attrib.size, this.gl_.FLOAT,
                                 !!attrib.normalized, typeBytes*attrib.stride,
                                 typeBytes*attrib.offset);
  }
};

// TODO: seems like texture ought to be a class...

function textureFromArray(gl, width, height, hasAlpha, array, opt_texture) {
  opt_texture = opt_texture || gl.createTexture();
  gl.bindTexture(gl.TEXTURE_2D, opt_texture);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
  if (hasAlpha)
  {
    console.log('Has alpha: '+array[3]);
    console.dir(array);
    gl.enable(gl.BLEND);
    gl.blendFunc(gl.SRC_ALPHA, gl.ONE_MINUS_SRC_ALPHA);

    //gl.uniform1f(shaderProgram.alphaUniform, zeroToOne);
     //vec4 textureColor = texture2D(uSampler, vec2(vTextureCoord.s, vTextureCoord.t));
     //gl_FragColor = vec4(textureColor.rgb * vLightWeighting, textureColor.a * uAlpha);
  }
  var type = hasAlpha ? gl.RGBA : gl.RGB;
  gl.texImage2D(gl.TEXTURE_2D, 0, type, width, height, 0, type, gl.UNSIGNED_BYTE, array);
  //void texImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, ArrayBufferView pixels)
  return opt_texture;
}

function textureFromImage(gl, image, opt_texture) {
  // TODO: texture formats. Color, MIP-mapping, etc.
  opt_texture = opt_texture || gl.createTexture();
  gl.bindTexture(gl.TEXTURE_2D, opt_texture);
  gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGB, gl.RGB, gl.UNSIGNED_BYTE, image);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR_MIPMAP_LINEAR);
  gl.generateMipmap(gl.TEXTURE_2D);
  return opt_texture;
}

function textureFromUrl(gl, url, opt_callback) {
  var texture = gl.createTexture();
  var image = new Image;
  image.onload = function() {
    textureFromImage(gl, image, texture);
    opt_callback && opt_callback(gl, texture);
  };
  image.onerror = function() {
    textureFromArray(gl, 1, 1, new Uint8Array([255, 255, 255]), false, texture);
    opt_callback && opt_callback(gl, texture);
  };
  image.src = url;
  return texture;
}

function attribBufferData(gl, attribArray) {
  var attribBuffer = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, attribBuffer);
  gl.bufferData(gl.ARRAY_BUFFER, attribArray, gl.STATIC_DRAW);
  return attribBuffer;
}

function indexBufferData(gl, indexArray) {
  var indexBuffer = gl.createBuffer();
  gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, indexBuffer);
  gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, indexArray, gl.STATIC_DRAW);
  return indexBuffer;
}

function addToDisplayList(displayList, begin, end) {
  var back = displayList.length - 1;
  var lastEnd = displayList[back];
  if (begin === lastEnd) {
    displayList[back] = end;
  } else {
    displayList.push(begin, end);
  }
}

// TODO: names/lengths don't really belong here; they probably belong
// with the displayList stuff.
function Mesh(gl, attribArray, indexArray, attribArrays, texture,
              opt_names, opt_lengths, opt_bboxen) {
  this.gl_ = gl;
  this.attribArrays_ = attribArrays;  // TODO: rename to vertex format!
  this.numIndices_ = indexArray.length;
  this.texture_ = texture || null;

  if (opt_bboxen) {
    this.bboxen_ = attribBufferData(gl, opt_bboxen);
  }

  this.vbo_ = attribBufferData(gl, attribArray);
  this.ibo_ = indexBufferData(gl, indexArray);

  this.names_ = opt_names || [];
  this.lengths_ = opt_lengths || [];
  this.starts_ = [];  // TODO: typed array?
  var numLengths = this.lengths_.length;
  var start = 0;
  for (var i = 0; i < numLengths; ++i) {
    this.starts_[i] = start;
    start += this.lengths_[i];
  }
}

Mesh.prototype.bind = function(program) {
  var gl = this.gl_;
  gl.bindTexture(gl.TEXTURE_2D, this.texture_);
  gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, this.ibo_);
  gl.bindBuffer(gl.ARRAY_BUFFER, this.vbo_);
  program.vertexAttribPointers(this.attribArrays_);

};

Mesh.prototype.draw = function() {
  var gl = this.gl_;
  gl.drawElements(gl.TRIANGLES, this.numIndices_, gl.UNSIGNED_SHORT, 0);
};

Mesh.prototype.drawRange = function(length, opt_offset) {
  opt_offset = opt_offset || 0;
  var gl = this.gl_;
  gl.drawElements(gl.TRIANGLES, length, gl.UNSIGNED_SHORT, 2*opt_offset);
};

Mesh.prototype.drawList = function(displayList) {
  var numDraws = displayList.length;
  for (var i = 0; i < numDraws; i += 2) {
    var drawStart = displayList[i];
    var drawLength = displayList[i+1] - drawStart;
    this.drawRange(drawLength, drawStart);
  }
};

Mesh.prototype.bindAndDraw = function(program) {
  this.bind(program);
  this.draw();
};
