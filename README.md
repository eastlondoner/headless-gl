# gl-bun

> **Experimental** - This is a fork of [headless-gl](https://github.com/stackgl/headless-gl) with N-API bindings for [Bun](https://bun.sh) compatibility.

[![js-standard-style](https://img.shields.io/badge/code%20style-standard-brightgreen.svg)](http://standardjs.com/)

`gl-bun` lets you create a WebGL context in [Bun](https://bun.sh) or [Node.js](https://nodejs.org/en/) without making a window or loading a full browser environment.

It aspires to fully conform to the [WebGL 1.0.3 specification](https://www.khronos.org/registry/webgl/specs/1.0.3/).

## Why gl-bun?

The original `headless-gl` package uses NAN (Native Abstractions for Node.js) which relies on V8-specific APIs. Bun uses JavaScriptCore instead of V8, so NAN bindings don't work. This fork adds N-API bindings which work with both Bun and Node.js.

## Example

```javascript
// Create context
const createGL = require('gl-bun')

const width = 64
const height = 64

const gl = createGL(width, height, { preserveDrawingBuffer: true })

//Clear screen to red
gl.clearColor(1, 0, 0, 1)
gl.clear(gl.COLOR_BUFFER_BIT)

//Write output as a PPM formatted image
const pixels = new Uint8Array(width * height * 4)
gl.readPixels(0, 0, width, height, gl.RGBA, gl.UNSIGNED_BYTE, pixels)
process.stdout.write(['P3\n# gl.ppm\n', width, " ", height, '\n255\n'].join(''))

for(let i = 0; i < pixels.length; i += 4) {
  for(let j = 0; j < 3; ++j) {
    process.stdout.write(pixels[i + j] + ' ')
  }
}
```

## Install

Using [npm](https://www.npmjs.com/) or [bun](https://bun.sh):

```bash
# With npm
npm install gl-bun

# With bun
bun add gl-bun
```

**Note:** This package currently requires building from source. Make sure you have the [system dependencies](#system-dependencies) installed.

## Supported platforms and runtimes

gl-bun runs on Linux, macOS, and Windows.

Tested with:
- **Bun** 1.0+ (primary target)
- **Node.js** 20+ (also supported via N-API)

## API

`gl-bun` exports exactly one function which you can use to create a WebGL context with the given context attributes:

```javascript
const createGL = require('gl-bun')

const gl = createGL(width, height)
```

* `width` is the width of the drawing buffer
* `height` is the height of the drawing buffer
* `contextAttributes` is an optional object whose properties are the [context attributes for the WebGLRendering context](https://www.khronos.org/registry/webgl/specs/latest/1.0/#5.2)

**Returns** A new `WebGLRenderingContext` object

### Extensions

In addition to all the usual WebGL methods, `gl-bun` exposes some custom extensions to make it easier to manage WebGL context resources in a server side environment:

#### `STACKGL_resize_drawingbuffer`

This extension provides a mechanism to resize the drawing buffer of a WebGL context once it is created.

In a pure DOM implementation, this method would implemented by resizing the WebGLContext's canvas element by modifying its `width/height` properties. This canvas manipulation is not possible in gl-bun, since a headless context doesn't have a DOM or a canvas element associated to it.

#### Example

```javascript
const assert = require('assert')
const gl = require('gl')(10, 10)
assert(gl.drawingBufferHeight === 10 && gl.drawingBufferWidth === 10)

const ext = gl.getExtension('STACKGL_resize_drawingbuffer')
ext.resize(20, 5)
assert(gl.drawingBufferHeight === 20 && gl.drawingBufferWidth === 5)
```

#### IDL
```
[NoInterfaceObject]
interface STACKGL_resize_drawingbuffer {
    void resize(GLint width, GLint height);
};
```

#### `ext.resize(width, height)`
Resizes the drawing buffer of a WebGL rendering context

* `width` is the new width of the drawing buffer for the context
* `height` is the new height of the drawing buffer for the context

### `STACKGL_destroy_context`

Destroys the WebGL context immediately, reclaiming all resources associated with it.

For long running jobs, garbage collection of contexts is often not fast enough. To prevent the system from becoming overloaded with unused contexts, you can force the system to reclaim a WebGL context immediately by calling `.destroy()`.

#### Example

```javascript
const gl = require('gl')(10, 10)

const ext = gl.getExtension('STACKGL_destroy_context')
ext.destroy()
```

#### IDL

```
[NoInterfaceObject]
interface STACKGL_destroy_context {
    void destroy();
};
```

#### `gl.getExtension('STACKGL_destroy_context').destroy()`
Immediately destroys the context and all associated resources.

### Expiremental WebGL2 support

To create a WebGL 2 context, set the `createWebGL2Context` property to `true` in the `contextAttributes` argument.

## System dependencies

Since `gl-bun` currently requires building from source, you'll need to install the system dependencies for your platform. For general information on building native modules, see the [`node-gyp`](https://github.com/nodejs/node-gyp) documentation.

#### Mac OS X

* [Python 3](https://www.python.org/)
* [XCode](https://developer.apple.com/xcode/)

#### Ubuntu/Debian

* [Python 3](https://www.python.org/)
* A GNU C++ environment (available via the `build-essential` package on `apt`)
* [libxi-dev](http://www.x.org/wiki/)
* Working and up to date OpenGL drivers
* [GLEW](http://glew.sourceforge.net/)
* [pkg-config](https://www.freedesktop.org/wiki/Software/pkg-config/)

```
$ sudo apt-get install -y build-essential libxi-dev libglu1-mesa-dev libglew-dev pkg-config
```

#### Windows

* [Python 3](https://www.python.org/)
* [Microsoft Visual Studio](https://www.microsoft.com/en-us/download/details.aspx?id=5555)
* d3dcompiler_47.dll should be in c:\windows\system32, but if isn't then you can find another copy in the deps/ folder

## FAQ

### How can I use gl-bun with a continuous integration service?

`gl-bun` should work out of the box on most CI systems. Some notes on specific CI systems:

* GitHub Actions: refer to the workflow files, specifially the `ci` workflow for an example.
* [CircleCI](https://circleci.com/): `gl-bun` should just work in the default node environment.
* [AppVeyor](http://www.appveyor.com/): Again it should just work
* [TravisCI](https://travis-ci.org/): Works out of the box on the OS X image. For Linux VMs, you need to install mesa and xvfb. To do this, create a file in the root of your repo called `.travis.yml` and paste the following into it:

```
language: node_js
os: linux
sudo: required
dist: trusty
addons:
  apt:
    packages:
    - mesa-utils
    - xvfb
    - libgl1-mesa-dri
    - libglapi-mesa
    - libosmesa6
node_js:
  - '20'
before_script:
  - export DISPLAY=:99.0; sh -e /etc/init.d/xvfb start
```

### How can `gl-bun` be used on a headless Linux machine?

If you are running your own minimal Linux server, such as the one one would want to use on Amazon AWS or equivalent, it will likely not provide an X11 nor an OpenGL environment. To setup such an environment you can use those two packages:

1. [Xvfb](https://en.wikipedia.org/wiki/Xvfb) is a lightweight X11 server which provides a back buffer for displaying X11 application offscreen and reading back the pixels which were drawn offscreen. It is typically used in Continuous Integration systems. It can be installed on CentOS with `yum install -y Xvfb`, and comes preinstalled on Ubuntu.
2. [Mesa](https://docs.mesa3d.org) is the reference open source software implementation of OpenGL. It can be installed on CentOS with `yum install -y mesa-dri-drivers`, or `apt-get install libgl1-mesa-dev`. Since a cloud Linux instance will typically run on a machine that does not have a GPU, a software implementation of OpenGL will be required.

Interacting with `Xvfb` requires you to start it on the background and to execute your `node` program with the DISPLAY environment variable set to whatever was configured when running Xvfb (the default being :99). If you want to do that reliably you'll have to start Xvfb from an init.d script at boot time, which is extra configuration burden. Fortunately there is a wrapper script shipped with Xvfb known as `xvfb-run` which can start Xvfb on the fly, execute your Node.js program and finally shut Xvfb down. Here's how to run it:

    xvfb-run -s "-ac -screen 0 1280x1024x24" <node program>

### Does gl-bun work in a browser?

Yes, with [browserify](http://browserify.org/). The `STACKGL_destroy_context` and `STACKGL_resize_drawingbuffer` extensions are emulated as well.

### How are `<image>` and `<video>` elements implemented?

They aren't for now. If you want to upload data to a texture, you will need to unpack the pixels into a `Uint8Array` and feed it into `texImage2D`. To help reading and saving images, you should check out the following modules:

* [`get-pixels`](https://www.npmjs.com/package/get-pixels)
* [`save-pixels`](https://www.npmjs.com/package/save-pixels)

### What extensions are supported?

Only the following for now:

* [`STACKGL_resize_drawingbuffer`](https://github.com/stackgl/headless-gl#stackgl_resize_drawingbuffer)
* [`STACKGL_destroy_context`](https://github.com/stackgl/headless-gl#stackgl_destroy_context)
* [`ANGLE_instanced_arrays`](https://www.khronos.org/registry/webgl/extensions/ANGLE_instanced_arrays/)
* [`OES_element_index_uint`](https://www.khronos.org/registry/webgl/extensions/OES_element_index_uint/)
* [`OES_texture_float`](https://www.khronos.org/registry/webgl/extensions/OES_texture_float/)
* [`OES_texture_float_linear`](https://www.khronos.org/registry/webgl/extensions/OES_texture_float_linear/)
* [`OES_vertex_array_object`](https://www.khronos.org/registry/webgl/extensions/OES_vertex_array_object/)
* [`OES_standard_derivatives`](https://www.khronos.org/registry/webgl/extensions/OES_standard_derivatives/)
* [`WEBGL_draw_buffers`](https://www.khronos.org/registry/webgl/extensions/WEBGL_draw_buffers/)
* [`EXT_blend_minmax`](https://www.khronos.org/registry/webgl/extensions/EXT_blend_minmax/)
* [`EXT_texture_filter_anisotropic`](https://www.khronos.org/registry/webgl/extensions/EXT_texture_filter_anisotropic/)
* [`EXT_shader_texture_lod`](https://www.khronos.org/registry/webgl/extensions/EXT_shader_texture_lod/)

### Why use this thing instead of `node-webgl`?

Despite the name, [node-webgl](https://github.com/mikeseven/node-webgl) doesn't actually implement WebGL - rather it gives you "WebGL"-flavored bindings to whatever OpenGL driver is configured on your system. If you are starting from an existing WebGL application or library, this means you'll have to do a bunch of work rewriting your WebGL code and shaders to deal with all the idiosyncrasies and bugs present on whatever platforms you try to run on. The upside though is that `node-webgl` exposes a lot of non-WebGL stuff that might be useful for games like window creation, mouse and keyboard input, requestAnimationFrame emulation, and some native OpenGL features.

`gl-bun` on the other hand just implements WebGL. It is built on top of [ANGLE](https://bugs.chromium.org/p/angleproject/issues/list) and passes the full Khronos ARB conformance suite, which means it works exactly the same on all supported systems. This makes it a great choice for running on a server or in a command line tool. You can use it to run tests, generate images or perform GPGPU computations using shaders.

### Why use this thing instead of [electron](http://electron.atom.io/)?

Electron is fantastic if you are writing a desktop application or if you need a full DOM implementation. On the other hand, because it is a larger dependency electron is more difficult to set up and configure in a server-side/CI environment. `gl-bun` is more modular in the sense that it just implements WebGL and nothing else. As a result creating a `gl-bun` context takes just a few milliseconds on most systems, while spawning a full electron instance can take upwards of 15-30 seconds. If you are using WebGL in a command line interface or need to execute WebGL in a service, `gl-bun` might be a more efficient and simpler choice.

### How should I set up a development environment for gl-bun?

After you have your [system dependencies installed](#system-dependencies), do the following:

1. Clone this repo: `git clone git@github.com:eastlondoner/headless-gl.git`
1. Switch to the headless gl directory: `cd headless-gl`
1. Install npm dependencies: `npm install`
1. Run node-gyp to generate build scripts: `npm run rebuild`

Once this is done, you should be good to go! A few more things

* To run the test cases, use the command `npm test`, or execute specific tests by just running them using `node`.
* On a Unix-like platform, you can do incremental rebuilds by going into the `build/` directory and running `make`. This is **way faster** running `npm build` each time you make a change.

Note: the `gl-conformance` package is currently hosted on GitHub Package Registry. To install from there, you may need to add `//npm.pkg.github.com` to your user directory's `.npmrc` file

## License

See LICENSES
