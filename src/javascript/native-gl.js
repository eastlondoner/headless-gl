// Direct .node loading without bindings package
// This is required for Bun bundler compatibility - the bindings package
// doesn't work in Bun compiled executables

const NativeWebGL = require('../../build/Release/webgl_napi.node')
const { WebGLRenderingContext: NativeWebGLRenderingContext } = NativeWebGL

if (typeof NativeWebGL.cleanup === 'function') {
  process.on('exit', NativeWebGL.cleanup)
}

const gl = NativeWebGLRenderingContext.prototype

// from binding.gyp
delete gl['1.0.0']

// from binding.gyp
delete NativeWebGLRenderingContext['1.0.0']

module.exports = { gl, NativeWebGL, NativeWebGLRenderingContext }
