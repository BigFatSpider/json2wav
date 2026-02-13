// Copyright Dan Price. All rights reserved.

#pragma once

#include "PWMage.h"
#include "EnveloperComposable.h"

namespace json2wav
{
	template<uint8_t eChanMask>
	using PWMageComposable = EnveloperComposable<PWMage<eChanMask>, ESynthParam, ESynthParam::Amplitude, ESynthParam::Frequency, false>;
}

