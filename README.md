# json2wav

## Building and running

### Mac/Linux

Install cmake, then open Terminal, navigate to the json2wav root directory, and build using cmake:

```
json2wav % mkdir build
json2wav % cd build
build % cmake ..
build % cmake --build .
```

build/json2wav can now be run:

```
build % ./json2wav ~/path/to/some/song.json
```

If a song uses a preset named "presetname", ./presets/presetname.json must be a valid JSON file when running json2wav:

```
build % ./json2wav ../songs/groovoove.json
Couldn't read preset "titerhythm3_17vox". Do you need to copy/move the presets folder to the current working directory?
build % cd ..
json2wav % build/json2wav songs/groovoove.json
Rendering audio for groovoove.wav...
```

If program shutdown takes a long time, try building with the custom smart pointers:

```
build % cmake -D CMAKE_CXX_FLAGS='-DJSON2WAV_CUSTOM_SMARTPTRS' ..
build % cmake --build .
```

