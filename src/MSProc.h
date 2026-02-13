// Copyright Dan Price. All rights reserved.

#pragma once

#include "IAudioObject.h"

namespace json2wav
{
	template<bool bHalfAmp, bool bOwner = false>
	class Butterfly : public AudioSum<bOwner>
	{
	public:
		virtual void GetSamples(
			Sample* const* const bufs,
			const size_t numChannels,
			const size_t bufSize,
			const unsigned long sampleRate,
			IAudioObject* const requester) noexcept override
		{
			this->GetInputSamples(bufs, numChannels, bufSize, sampleRate);
			if (numChannels != 2)
				return;

			for (size_t i = 0; i < bufSize; ++i)
			{
				const float sum = bufs[0][i].AsFloat32() + bufs[1][i].AsFloat32();
				const float diff = bufs[0][i].AsFloat32() - bufs[1][i].AsFloat32();
				if constexpr (bHalfAmp)
				{
					bufs[0][i] = 0.5f*sum;
					bufs[1][i] = 0.5f*diff;
				}
				else
				{
					bufs[0][i] = sum;
					bufs[1][i] = diff;
				}
			}
		}

		virtual size_t GetNumChannels() const noexcept override
		{
			return 2;
		}
	};

	template<bool bOwner = false>
	using MSConverter = Butterfly<false, bOwner>;
	template<bool bOwner = false>
	using LRConverter = Butterfly<true, bOwner>;
}

