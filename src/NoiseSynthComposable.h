// Copyright Dan Price 2026.

#pragma once

#include "NoiseSynth.h"
#include "EnveloperComposable.h"

namespace json2wav
{
	using NoiseSynthComposable = EnveloperComposable<NoiseSynth, ESynthParam, ESynthParam::Amplitude, ESynthParam::Frequency, false>;
}

