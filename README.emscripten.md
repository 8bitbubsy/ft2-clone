# ft2-clone emscripten note
Notes on the experimental Emscripten target support. Try out the working
prototype:

https://snart.me/demos/emscripten/ft2-clone/

(the track from: https://modarchive.org/index.php?request=view_by_moduleid&query=142756)

## Building
Use ccmake to set `CMAKE_BUILD_TYPE` to either debug or release. Default is
debug. After editing, press **c** to configure. **g** to generate the Makefile
recipe and exit. Then go to the build directory and run make.

```sh
source "YOUR_EMSDK_PATH/emsdk_env.sh"

emcmake ccmake -B build .
cd build
emmake make -j$(nproc)
```

The output will be either in `debug/other` or `release/other`. Host following
files:

- ft2-clone.worker.js
- ft2-clone.js
- ft2-clone.wasm
- ft2-clone.html

You probably want to link `index.html` to `ft2-clone.html`:

```sh
ln -s ft2-clone.html index.html
```

### Embedding mods
In the ccmake step, press **t** to toggle the advanced mode. Edit
`CMAKE_EXE_LINKER_FLAGS` to embed mods. Example:

```
--embed-file pink_noise.xm
```

To make ft2-clone to load the mod on init, the file can be passed in the command
line arguments. Emscripten does not provide an easy way to do this... I normally
edit the output html file. Before loading the main js file, the `Module` object
can be defined to pass parameters to the WASM module. The argument vector is one
of them. In the output html file, add something like this:

```js
//...
      var Module = {

        arguments: [ "pink_noise.xm" ],

        print: (function() {
//...
```

where `"pink_noise.xm"` is the file you embeded in the module and want ft2-clone
to load and play on init.

## Hosting files (header issue)
In order to support SDL threads, pthread option is used. The caveat is that
Emscripten uses SharedArrayBuffer to have a shared memory space for the
threads[^1] and SharedArrayBuffer has been disabled by default since the
discovery of Spectre vulnerability[^2]. Therefore, to host the output files,
the special headers need to be sent by the web server:

```
cross-origin-embedder-policy: require-corp
cross-origin-opener-policy: same-origin
```

The major implementations support addition of arbitrary response headers. Here's
an example config for Nginx:

```
	location /demos/emscripten/ {
		add_header "Cross-Origin-Opener-Policy" "same-origin";
		add_header "Cross-Origin-Embedder-Policy" "require-corp";
	}
```

If you use http-server npm package to debug emscripten programs, you may need to
use the forks below as http-server has no curl `-H` option equivalent.

1. https://github.com/dxdxdt/http-server/commit/4a525a75c0aca9bf567dd0ffc2a0cfe74f29197b
1. https://github.com/http-party/http-server/pull/885

My version is in line with semantics of curl. Here's how you run it to host the
output files locally:

```sh
npm start -- /PATH/TO/ft2-clone \
	-H 'Cross-Origin-Opener-Policy: same-origin' \
	-H 'Cross-Origin-Embedder-Policy: require-corp'
```

(Hopefully, my version will make it to the main branch)

## Debugging
Setting `CMAKE_BUILD_TYPE` to "debug" will produce debugging symbols. Chromium
can be used to utilise them: breakpoints, threads, variable inspection ...

## Other dev notes
### `emscripten_sleep()` yielding rather than `emscripten_set_main_loop()`
For some reason, having a main loop function and letting the JS engine call it
makes the program unstable, especially when it shows a dialog. It probably has
something to do with the logic in `hpc_wait()`. So `-sASYNCIFY` had to be used
and, quite frankly, performs better than doing the main loop using
`requestAnimationFrame()`.

### No MIDI support
The modern browsers do support MIDI[^4], but Emscripten does not(as of writing
of this note). Even when Web MIDI gets comes to Emscripten, it will probably be
not in the form of ALSA backend, so a separate driver will need to be written.

### BUG: mouse and scaling problems
**Work in progress...**: the mouse support isn't quite right at the moment.
There seems to be some issues with scaling factors, too: Chromium honors scaling
factor of the desktop whilst Firefox does not.

### BUG: out of memory in most of menus
**Work in progress...**: the diskop is broken. The out of memory dialog will be
shown, although the error message is obviously bogus: the `ABORTING_MALLOC` and
`ALLOW_MEMORY_GROWTH` options won't allow the application to continue to show
the dialog on memory allocation error[^3].

### QUIRK: placeholder `deleteDirRecursive()` implementation
Emcripten has no `<fts.h>` support. The recursive rm has to be done using
`<dirent.h>`. I didn't see the need to put a good working implementation because

1. Emscripten "emulates" the filesystem strictly in memory. Having a full-blown
   directory structures is a rare case
1. had no time to write a good implementation myself that does not involve
   recursion

Talking about over-engineering. The placeholder implementation is probably
faster than the original `<fts.h>` one, anyways. Trying to use it on Emscripten
will most likely to fail because there's no executable(`/bin/rm`) on the file
system. This is intentional. I just didn't want to leave the function blank.


[^1]: https://emscripten.org/docs/porting/pthreads.html
[^2]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/SharedArrayBuffer#security_requirements
[^3]: https://emscripten.org/docs/tools_reference/settings_reference.html#aborting-malloc
[^4]: https://developer.mozilla.org/en-US/docs/Web/API/Web_MIDI_API
