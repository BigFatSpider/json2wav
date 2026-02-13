# Copyright Dan Price. All rights reserved.

cmake_minimum_required(VERSION 3.20)

project(json2wav)

set(CMAKE_CXX_STANDARD 23)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-Wall" COMPILER_SUPPORTS_WALL)
if(COMPILER_SUPPORTS_WALL)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
endif()
CHECK_CXX_COMPILER_FLAG("-Wextra" COMPILER_SUPPORTS_WEXTRA)
if(COMPILER_SUPPORTS_WEXTRA)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra")
endif()
CHECK_CXX_COMPILER_FLAG("-Wpedantic" COMPILER_SUPPORTS_WPEDANTIC)
if(COMPILER_SUPPORTS_WPEDANTIC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wpedantic")
endif()
CHECK_CXX_COMPILER_FLAG("-Wshadow" COMPILER_SUPPORTS_WSHADOW)
if(COMPILER_SUPPORTS_WSHADOW)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wshadow")
endif()
CHECK_CXX_COMPILER_FLAG("-Wno-unused-parameter" COMPILER_SUPPORTS_WNO_UNUSED_PARAMETER)
if(COMPILER_SUPPORTS_WNO_UNUSED_PARAMETER)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter")
endif()
CHECK_CXX_COMPILER_FLAG("-Wno-missing-field-initializers" COMPILER_SUPPORTS_WNO_MISSING_FIELD_INITIALIZERS)
if(COMPILER_SUPPORTS_WNO_MISSING_FIELD_INITIALIZERS)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-missing-field-initializers")
endif()
CHECK_CXX_COMPILER_FLAG("-m64" COMPILER_SUPPORTS_M64)
if(COMPILER_SUPPORTS_M64)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64")
endif()
CHECK_CXX_COMPILER_FLAG("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
if(COMPILER_SUPPORTS_MARCH_NATIVE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
endif()
CHECK_CXX_COMPILER_FLAG("-flto" COMPILER_SUPPORTS_FLTO)
if(COMPILER_SUPPORTS_FLTO)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
endif()

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

add_executable(json2wav src/json2wav.cpp)
target_link_libraries(json2wav JsonToWav)

