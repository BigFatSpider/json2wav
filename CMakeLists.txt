# Copyright Dan Price. All rights reserved.

cmake_minimum_required(VERSION 3.20)

project(json2wav)

set(CMAKE_CXX_STANDARD 23)

list(APPEND COMPILER_WARNINGS -Wall -Wextra -Wpedantic -Wshadow -Wno-unused-parameter -Wno-missing-field-initializers)

add_library(JsonToWav
	src/Bessel.cpp src/DrumHit.cpp src/InfiniSaw.cpp
	src/JsonToWav.cpp src/Random.cpp src/Sample.cpp
	src/AdditiveHitSynth.h src/AirFilter.h src/AudioFile.h
	src/Bessel.h src/BesselPoly.h src/Binomial.h
	src/ChebyDist.h src/CircleQueue.h src/CompositeSynth.h
	src/Compressor.h src/Cubic.h src/Delay.h
	src/DrumHit.h src/DrumHitRT60.h src/DrumHitSynth.h
	src/DrumHitTypes.h src/Envelope.h src/EnveloperComposable.h
	src/Fader.h src/FastSin.h src/FastTan.h
	src/FDNVerb.h src/Filter.h src/FilterComposable.h
	src/FourCC.h src/GaussBoost.h src/IAudioObject.h
	src/IControlObject.h src/InfiniSaw.h src/InfiniSawComposable.h
	src/Instrument.h src/JsonInterpreter.h src/JsonParser.h
	src/JsonToWav.h src/Math.h src/Memory.h
	src/MetaArray.h src/MSProc.h src/NoiseSynth.h
	src/NoiseSynthComposable.h src/Nonic.h src/NoteData.h
	src/Oversampler.h src/OversamplerFilters.h src/Panner.h
	src/Presets.h src/PWMage.h src/PWMageComposable.h
	src/Quintic.h src/Ramp.h src/Random.h
	src/RiffData.h src/RiffFile.h src/Sample.h
	src/Septic.h src/SineSynth.h src/Synth.h
	src/Thread.h src/Utility.h src/WavFile.h
	src/ZeroInit.h
)

target_compile_options(JsonToWav PRIVATE "${COMPILER_WARNINGS}")

add_executable(json2wav src/json2wav.cpp)
target_compile_options(json2wav PRIVATE "${COMPILER_WARNINGS}")
target_link_libraries(json2wav JsonToWav)

