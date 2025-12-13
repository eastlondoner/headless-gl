function isProbablyBun() {
  // Bun sets process.versions.bun.
  return typeof process !== 'undefined' &&
    process.versions != null &&
    typeof process.versions.bun === 'string'
}

function loadNativeWebGL() {
  const bindings = require('bindings')

  // Prefer N-API build on Bun to avoid NAN/V8 binary incompatibility.
  // For Node, keep using the existing "webgl" binary by default.
  const bindingName = isProbablyBun() ? 'webgl_napi' : 'webgl'

  return bindings(bindingName)
}

const NativeWebGL = loadNativeWebGL()
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
