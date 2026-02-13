// Copyright Dan Price. All rights reserved.

#pragma once

#include "Ramp.h"

namespace json2wav
{
	struct Envelope
	{
		Envelope(const float attInit, const float decInit, const float relInit,
			const float attlevInit, const float suslevInit,
			const ERampShape ramp = ERampShape::Linear)
			: attack(attInit), decay(decInit), length(0.0f), release(relInit), attlevel(attlevInit), suslevel(suslevInit),
			expression(0.0f),
			attramp(ramp), decramp(ramp), relramp(ramp)
		{
		}

		Envelope(const float attInit, const float decInit, const float lenInit, const float relInit,
			const float attlevInit, const float suslevInit,
			const ERampShape ramp = ERampShape::Linear)
			: attack(attInit), decay(decInit), length(lenInit), release(relInit), attlevel(attlevInit), suslevel(suslevInit),
			expression(0.0f),
			attramp(ramp), decramp(ramp), relramp(ramp)
		{
		}

		Envelope(const float attInit, const float decInit, const float relInit,
			const float attlevInit, const float suslevInit,
			const ERampShape attrampInit, const ERampShape decrampInit, const ERampShape relrampInit,
			const float expressionInit = 0.0f)
			: attack(attInit), decay(decInit), length(0.0f), release(relInit), attlevel(attlevInit), suslevel(suslevInit),
			expression(expressionInit),
			attramp(attrampInit), decramp(decrampInit), relramp(relrampInit)
		{
		}

		Envelope(const float attInit, const float decInit, const float lenInit, const float relInit,
			const float attlevInit, const float suslevInit,
			const ERampShape attrampInit, const ERampShape decrampInit, const ERampShape relrampInit,
			const float expressionInit = 0.0f)
			: attack(attInit), decay(decInit), length(lenInit), release(relInit), attlevel(attlevInit), suslevel(suslevInit),
			expression(expressionInit),
			attramp(attrampInit), decramp(decrampInit), relramp(relrampInit)
		{
		}

		float attack;
		float decay;
		float length; // Different from sustain; time from attack start to release start
		float release;
		float attlevel;
		float suslevel;
		float expression;
		ERampShape attramp;
		ERampShape decramp;
		ERampShape relramp;
	};
}

