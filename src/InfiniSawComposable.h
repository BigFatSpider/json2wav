// Copyright Dan Price. All rights reserved.

#pragma once

#include "InfiniSaw.h"
#include "EnveloperComposable.h"

namespace json2wav
{
	using InfiniSawComposable = EnveloperComposable<InfiniSaw, ESynthParam, ESynthParam::Amplitude, ESynthParam::Frequency, false>;
}

