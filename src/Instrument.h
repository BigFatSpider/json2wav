// Copyright Dan Price 2026.

#pragma once

#include "IAudioObject.h"
#include "IControlObject.h"

namespace json2wav
{
	// TODO: "Instrument" is a dumb name for this, since this has nothing to do with source vs. signal processor
	template<typename EventType>
	class Instrument : public IAudioObject, public ControlObject<EventType>
	{
	};
}

