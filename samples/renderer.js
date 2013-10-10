'use strict';

function Renderer(canvas) {
  var self = this;
  this.canvas_ = canvas;

  var gl = createContextFromCanvas(canvas);
  this.gl_ = gl;

  // Camera.
  this.zNear_ = Math.sqrt(3);
  this.model_ = mat4.identity(mat4.create());
  this.view_ = mat4.identity(mat4.create());
  this.proj_ = mat4.create();
  this.mvp_ = mat4.create();

  // Meshes.
  this.meshes_ = [];

  // Resize.
  this.maxWidth = 20480;
  this.maxHeight = 20480;
  this.scaleX = 1.0;
  this.scaleY = 1.0;
  window.addEventListener('resize', this.postRedisplay.bind(this));

  // WebGL
  gl.clearColor(0, 0, 0, 0);
  gl.enable(gl.CULL_FACE);
  gl.enable(gl.DEPTH_TEST);
  gl.pixelStorei(gl.UNPACK_FLIP_Y_WEBGL, true);
}

Renderer.prototype.setViewport_ = function () {
  var canvas = this.canvas_;

  var newWidth = Math.round(this.scaleX * canvas.clientWidth);
  var newHeight = Math.round(this.scaleY * canvas.clientHeight);

  newWidth = clamp(newWidth, 1, this.maxWidth);
  newHeight = clamp(newHeight, 1, this.maxHeight);

  if (canvas.width !== newWidth || canvas.height !== newHeight) {
    canvas.width = newWidth;
    canvas.height = newHeight;

    this.gl_.viewport(0, 0, newWidth, newHeight);
  }
}

Renderer.prototype.drawAll_ = function() {
  var numMeshes = this.meshes_.length;
  for (var i = 0; i < numMeshes; i++) {
    this.meshes_[i].bindAndDraw(this.program_);
  }
};

Renderer.prototype.drawLists_ = function(displayLists) {
  var numLists = displayLists.length;
  for (var i = 0; i < numLists; i++) {
    var displayList = displayLists[i];
    var mesh = this.meshes_[i];
    mesh.bind(this.program_);
    mesh.drawList(displayList);
  }
};

Renderer.prototype.draw_ = function() {
  var gl = this.gl_;
  var canvas = this.canvas_;

  gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT | gl.STENCIL_BUFFER_BIT);
  var fudge = .01;  // TODO: tighter z-fitting.
  var aspectRatio = fudge*canvas.clientWidth/canvas.clientHeight;
  mat4.frustum(-aspectRatio, aspectRatio, -fudge, fudge,
               fudge*this.zNear_, 100, this.proj_);
  mat4.multiply(this.view_, this.model_, this.mvp_);
  mat4.multiply(this.proj_, this.mvp_, this.mvp_);
  gl.uniformMatrix4fv(this.program_.set_uniform.u_mvp, false, this.mvp_);
  gl.uniformMatrix3fv(this.program_.set_uniform.u_model, false, 
                      mat4.toMat3(this.model_));
  this.drawAll_();
};

Renderer.prototype.postRedisplay = function() {
  var self = this;
  if (!this.frameStart_) {
    this.frameStart_ = Date.now();
    window.requestAnimFrame(function() { 
      self.setViewport_();
      self.draw_();
      self.frameStart_ = 0;
    }, this.canvas_);
  }
};
