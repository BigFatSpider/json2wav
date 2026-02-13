// Copyright Dan Price 2026.

#pragma once

#include "InfiniSaw.h"
#include "EnveloperComposable.h"

namespace json2wav
{
	using InfiniSawComposable = EnveloperComposable<InfiniSaw, ESynthParam, ESynthParam::Amplitude, ESynthParam::Frequency, false>;
}

