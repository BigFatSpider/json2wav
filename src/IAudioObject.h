// Copyright Dan Price 2026.

#pragma once

#include "Utility.h"
#include "Sample.h"
#include "Memory.h"
#include "ZeroInit.h"
#include "Oversampler.h"
#include <unordered_map>
#include <iostream>
#include <mutex>
#include <utility>
#include <type_traits>
#include <cstdint>
#include <cstdlib>

// Parallelism TS, standardized 5 years ago as of writing, not implemented on Clang yet, grrr
#define ALBUMBOT_USE_PARALLELISM_TS 0

#if ALBUMBOT_USE_PARALLELISM_TS
#include <algorithm>
#include <execution>
#else
#include <future>
#endif

namespace json2wav
{
	class IAudioObject
	{
	public:
		virtual ~IAudioObject() noexcept {}

		virtual void GetSamples(
			Sample* const* const bufs,
			const size_t numChannels,
			const size_t bufSize,
			const unsigned long sampleRate,
			IAudioObject* const requester) noexcept = 0;

		virtual size_t GetNumChannels() const noexcept = 0;

		virtual void OnAddedAsInput(IAudioObject* const pNewOutput) {}
		virtual void OnRemovedFromInput(IAudioObject* const pFormerOutput) {}

		virtual size_t GetSampleDelay() const noexcept { return 0; }
	};

	template<bool bOwner = false, bool bSmartPtr = true>
	class AudioJoin : public IAudioObject
	{
	public:
		static constexpr const bool bIsOwner = bOwner;

		using InputPtr = Utility::Ptr_t<IAudioObject, bOwner, bSmartPtr>;

		bool AddInput(const Utility::StrongPtr_t<IAudioObject, bSmartPtr> inputNode)
		{
			if (Utility::Find(inputNode, inputs) == inputs.end())
			{
				inputs.push_back(inputNode);
				inputNode->OnAddedAsInput(this);
				CalculateInputDelays();
				return true;
			}
			return false;
		}

		bool RemoveInput(const Utility::StrongPtr_t<IAudioObject, bSmartPtr> inputNode)
		{
			if (Utility::Remove(inputNode, inputs))
			{
				inputNode->OnRemovedFromInput(this);
				CalculateInputDelays();
				return true;
			}
			return false;
		}

		void ClearInputs()
		{
			inputs.clear();
			*maxInputDelay = 0;
		}

		const Vector<InputPtr>& GetInputs() const noexcept
		{
			return inputs;
		}

		virtual size_t GetSampleDelay() const noexcept override
		{
			CalculateInputDelays();
			return *maxInputDelay;
		}

	protected:
		enum class EGetInputSamplesResult
		{
			None, SamplesWritten, ChannelMismatch, BadAlloc, NullOutputBuffer, ExcessiveDelay
		};

		EGetInputSamplesResult GetInputSamples(
			Sample* const* const bufs,
			const size_t numChannels,
			const size_t bufSize,
			const unsigned long sampleRate) noexcept
		{
			if (!bufs)
				return EGetInputSamplesResult::NullOutputBuffer;

			for (size_t i = 0; i < numChannels; ++i)
				if (!bufs[i])
					return EGetInputSamplesResult::NullOutputBuffer;

			if (inputs.size() == 1)
			{
				if (const Utility::StrongPtr_t<IAudioObject, bSmartPtr> inptr = Utility::Lock(inputs[0]))
				{
					inptr->GetSamples(bufs, numChannels, bufSize, sampleRate, this);
					return EGetInputSamplesResult::SamplesWritten;
				}
				else
					return EGetInputSamplesResult::None;
			}

			size_t bufidx = 0;

			{
#if ALBUMBOT_USE_PARALLELISM_TS
				// Serial
				using LockedInputType = std::pair<Utility::StrongPtr_t<IAudioObject, bSmartPtr>, Sample* const*>;
				Vector<LockedInputType> lockedInputs;
				lockedInputs.reserve(inputs.size());
				for (const auto& inwkptr : inputs)
				{
					if (Utility::StrongPtr_t<IAudioObject, bSmartPtr> inptr = Utility::Lock(inwkptr))
					{
						if (bufidx >= inbufs.size())
							inbufs.emplace_back(numChannels, bufSize, false);
						else
							inbufs[bufidx].Reinitialize(numChannels, bufSize);
						if (bufidx >= inbufs.size() || inbufs[bufidx].GetBufSize() != bufSize)
							return EGetInputSamplesResult::BadAlloc;
						lockedInputs.emplace_back(std::move(inptr), inbufs[bufidx++].get());
					}
				}

				// Parallel
				std::for_each(std::execution::parallel_policy, lockedInputs.begin(), lockedInputs.end(),
					[this, numChannels, bufSize, sampleRate](const LockedInputType& lockedInput)
					{ lockedInput.first->GetSamples(lockedInput.second, numChannels, bufSize, sampleRate, this); });

#else
				Vector<std::future<void>> futs;
				futs.reserve(inputs.size());
				inbufs.reserve(inputs.size());
				dlybufs.reserve(inputs.size());
				for (const auto& inwkptr : inputs)
				{
					if (const Utility::StrongPtr_t<IAudioObject, bSmartPtr> inptr = Utility::Lock(inwkptr))
					{
						if (bufidx >= inbufs.size())
						{
							inbufs.emplace_back(numChannels, bufSize, false);
							dlybufs.emplace_back(numChannels, delays[bufidx], false);
						}
						else
						{
							inbufs[bufidx].Reinitialize(numChannels, bufSize);
							dlybufs[bufidx].Reinitialize(numChannels, delays[bufidx]);
						}
						if (bufidx >= inbufs.size() || inbufs[bufidx].GetBufSize() != bufSize)
						{
							return EGetInputSamplesResult::BadAlloc;
						}

						Sample* const* const inbuf(inbufs[bufidx++].get());
						futs.emplace_back(std::async(std::launch::async,
							[this, inptr, inbuf, numChannels, bufSize, sampleRate]()
							{
								inptr->GetSamples(inbuf, numChannels, bufSize, sampleRate, this);
							}));
					}
				}
#endif
			}

			const size_t bufsWritten(bufidx);
			if (bufsWritten > 0)
			{
				for (size_t ch = 0; ch < numChannels; ++ch)
				{
					Sample* const chbuf = bufs[ch];

					for (size_t bufnum = 0; bufnum < bufsWritten; ++bufnum)
					{
						Sample* const inbuf = inbufs[bufnum][ch];
						const size_t delay = delays[bufnum];
						if (delay > 0)
						{
							if (delay > bufSize)
								return EGetInputSamplesResult::ExcessiveDelay;

							Sample* const dlybuf = dlybufs[bufnum][ch];
							if (work.size() < bufSize)
								work.resize(bufSize);
							for (size_t i = 0; i < bufSize; ++i)
								work[i] = inbuf[i];
							for (size_t i = 0; i < delay; ++i)
							{
								inbuf[i] = dlybuf[i];
								dlybuf[i] = work[bufSize - delay + i];
							}
							for (size_t i = delay; i < bufSize; ++i)
								inbuf[i] = work[i - delay];
						}
					}

#if 0
					size_t bufnum = 0;
					{
						Sample* const inbuf = inbufs[bufnum][ch];
						for (size_t i = 0; i < bufSize; ++i)
						{
							chbuf[i] = inbuf[i];
						}
					}
					++bufnum;

					for ( ; bufnum < bufsWritten; ++bufnum)
					{
						Sample* const inbuf = inbufs[bufnum][ch];
						for (size_t i = 0; i < bufSize; ++i)
						{
							chbuf[i] += inbuf[i];
						}
					}
#else

#if 0
					// Pairwise summation
					for (size_t skip = 1; skip < bufsWritten; skip <<= 1)
					{
						for (size_t bufnum = skip; bufnum < bufsWritten; bufnum += skip << 1)
						{
							Sample* const inbuflo = inbufs[bufnum - skip][ch];
							Sample* const inbufhi = inbufs[bufnum][ch];
							for (size_t i = 0; i < bufSize; ++i)
								inbuflo[i] += inbufhi[i];
						}
					}
					const Sample* const inbuf = inbufs[0][ch];
					for (size_t i = 0; i < bufSize; ++i)
						chbuf[i] = inbuf[i];
#endif

#endif
					JoinChannel(ch, inbufs, chbuf, bufSize, bufsWritten);
				}

				return EGetInputSamplesResult::SamplesWritten;
			}

			return EGetInputSamplesResult::None;
		}

	private:
		void CalculateInputDelays() const
		{
			*maxInputDelay = 0;
			for (const auto& inwkptr : inputs)
				if (const Utility::StrongPtr_t<IAudioObject, bSmartPtr> inptr = Utility::Lock(inwkptr))
					if (const size_t inputDelay = inptr->GetSampleDelay(); inputDelay > *maxInputDelay)
						*maxInputDelay = inputDelay;
			delays.resize(inputs.size());
			for (size_t i = 0; i < inputs.size(); ++i)
				if (const Utility::StrongPtr_t<IAudioObject, bSmartPtr> inptr = Utility::Lock(inputs[i]))
					delays[i] = *maxInputDelay - inptr->GetSampleDelay();
		}

		virtual void JoinChannel(
			const size_t ch,
			Vector<SampleBuf>& inbufs,
			Sample* const chbuf,
			const size_t bufSize,
			const size_t bufsWritten) noexcept = 0;

	private:
		Vector<InputPtr> inputs;
		Vector<SampleBuf> inbufs;
		Vector<SampleBuf> dlybufs;
		Vector<Sample> work;
		mutable Vector<size_t> delays;
		mutable zeroinit_t<size_t> maxInputDelay;
	};

	class AudioSumJoin
	{
	public:
		void JoinChannel(
			const size_t ch,
			Vector<SampleBuf>& inbufs,
			Sample* const chbuf,
			const size_t bufSize,
			const size_t bufsWritten) noexcept
		{
			// Pairwise summation
			for (size_t skip = 1; skip < bufsWritten; skip <<= 1)
			{
				for (size_t bufnum = skip; bufnum < bufsWritten; bufnum += skip << 1)
				{
					Sample* const inbuflo = inbufs[bufnum - skip][ch];
					Sample* const inbufhi = inbufs[bufnum][ch];
					for (size_t i = 0; i < bufSize; ++i)
						inbuflo[i] += inbufhi[i];
				}
			}
			const Sample* const inbuf = inbufs[0][ch];
			for (size_t i = 0; i < bufSize; ++i)
				chbuf[i] = inbuf[i];
		}
	};

	template<bool bOwner = false, bool bSmartPtr = true>
	class AudioSum : public AudioJoin<bOwner, bSmartPtr>
	{
	private:
		virtual void JoinChannel(
			const size_t ch,
			Vector<SampleBuf>& inbufs,
			Sample* const chbuf,
			const size_t bufSize,
			const size_t bufsWritten) noexcept override
		{
			sumjoin.JoinChannel(ch, inbufs, chbuf, bufSize, bufsWritten);

#if 0
			// Pairwise summation
			for (size_t skip = 1; skip < bufsWritten; skip <<= 1)
			{
				for (size_t bufnum = skip; bufnum < bufsWritten; bufnum += skip << 1)
				{
					Sample* const inbuflo = inbufs[bufnum - skip][ch];
					Sample* const inbufhi = inbufs[bufnum][ch];
					for (size_t i = 0; i < bufSize; ++i)
						inbuflo[i] += inbufhi[i];
				}
			}
			const Sample* const inbuf = inbufs[0][ch];
			for (size_t i = 0; i < bufSize; ++i)
				chbuf[i] = inbuf[i];
#endif
		}

	private:
		AudioSumJoin sumjoin;
	};

	class RingModJoin
	{
	public:
		void JoinChannel(
			const size_t ch,
			Vector<SampleBuf>& inbufs,
			Sample* const chbuf,
			const size_t bufSize,
			const size_t bufsWritten) noexcept
		{
			const size_t bufSizeX2 = bufSize << 1;

			if (us2.size() <= ch)
			{
				us2.resize(ch + 1);
				ds2.resize(ch + 1);
			}

			if (worklo.size() < bufSizeX2)
			{
				worklo.resize(bufSizeX2);
				workhi.resize(bufSizeX2);
			}

			// Upsampled x2 pairwise multiplication
			double* const workbuflo = worklo.data();
			double* const workbufhi = workhi.data();
			auto& chus(us2[ch]);
			auto& chds(ds2[ch]);
			for (size_t skip = 1, dsidx = 0; skip < bufsWritten; skip <<= 1, ++dsidx)
			{
				const size_t usidxlo = dsidx << 1;
				const size_t usidxhi = usidxlo + 1;
				if (chus.size() <= usidxhi)
					chus.resize(usidxhi + 1);
				if (chds.size() <= dsidx)
					chds.resize(dsidx + 1);
				auto& vuslo(chus[usidxlo]);
				auto& vushi(chus[usidxhi]);
				auto& vds(chds[dsidx]);
				for (size_t bufnum = skip, osidx = 0; bufnum < bufsWritten; bufnum += skip << 1, ++osidx)
				{
					Sample* const inbuflo = inbufs[bufnum - skip][ch];
					Sample* const inbufhi = inbufs[bufnum       ][ch];
					if (vuslo.size() <= osidx)
					{
						vuslo.resize(osidx + 1);
						vushi.resize(osidx + 1);
						vds.resize(osidx + 1);
					}
					vuslo[osidx].process_unsafe(bufSize, inbuflo, workbuflo);
					vushi[osidx].process_unsafe(bufSize, inbufhi, workbufhi);
					for (size_t i = 0; i < bufSizeX2; ++i)
						workbuflo[i] *= workbufhi[i];
					vds[osidx].process_unsafe(bufSize, workbuflo, inbuflo);
				}
			}

			const Sample* const inbuf = inbufs[0][ch];
			for (size_t i = 0; i < bufSize; ++i)
				chbuf[i] = inbuf[i];
		}

	private:
		Vector<double> worklo;
		Vector<double> workhi;
		Vector<Vector<Vector<oversampling::upsampler441_x2<double>>>> us2;
		Vector<Vector<Vector<oversampling::downsampler441_x2<double>>>> ds2;
	};

	template<bool bOwner = false, bool bSmartPtr = true>
	class RingMod : public AudioJoin<bOwner, bSmartPtr>
	{
	public:
		virtual size_t GetSampleDelay() const noexcept override
		{
			return AudioJoin<bOwner, bSmartPtr>::GetSampleDelay() + Utility::ceil_log2(this->GetInputs().size())*128;
		}

	private:
		virtual void JoinChannel(
			const size_t ch,
			Vector<SampleBuf>& inbufs,
			Sample* const chbuf,
			const size_t bufSize,
			const size_t bufsWritten) noexcept override
		{
			rmjoin.JoinChannel(ch, inbufs, chbuf, bufSize, bufsWritten);

#if 0
			const size_t bufSizeX2 = bufSize << 1;

			if (us2.size() <= ch)
			{
				us2.resize(ch + 1);
				ds2.resize(ch + 1);
			}

			if (worklo.size() < bufSizeX2)
			{
				worklo.resize(bufSizeX2);
				workhi.resize(bufSizeX2);
			}

			// Upsampled x2 pairwise multiplication
			double* const workbuflo = worklo.data();
			double* const workbufhi = workhi.data();
			auto& chus(us2[ch]);
			auto& chds(ds2[ch]);
			for (size_t skip = 1, dsidx = 0; skip < bufsWritten; skip <<= 1, ++dsidx)
			{
				const size_t usidxlo = dsidx << 1;
				const size_t usidxhi = usidxlo + 1;
				if (chus.size() <= usidxhi)
					chus.resize(usidxhi + 1);
				if (chds.size() <= dsidx)
					chds.resize(dsidx + 1);
				auto& vuslo(chus[usidxlo]);
				auto& vushi(chus[usidxhi]);
				auto& vds(chds[dsidx]);
				for (size_t bufnum = skip, osidx = 0; bufnum < bufsWritten; bufnum += skip << 1, ++osidx)
				{
					Sample* const inbuflo = inbufs[bufnum - skip][ch];
					Sample* const inbufhi = inbufs[bufnum       ][ch];
					if (vuslo.size() <= osidx)
					{
						vuslo.resize(osidx + 1);
						vushi.resize(osidx + 1);
						vds.resize(osidx + 1);
					}
					vuslo[osidx].process_unsafe(bufSize, inbuflo, workbuflo);
					vushi[osidx].process_unsafe(bufSize, inbufhi, workbufhi);
					for (size_t i = 0; i < bufSizeX2; ++i)
						workbuflo[i] *= workbufhi[i];
					vds[osidx].process_unsafe(bufSize, workbuflo, inbuflo);
				}
			}

			const Sample* const inbuf = inbufs[0][ch];
			for (size_t i = 0; i < bufSize; ++i)
				chbuf[i] = inbuf[i];
#endif
		}

	private:
		//Vector<double> worklo;
		//Vector<double> workhi;
		//Vector<Vector<Vector<oversampling::upsampler441_x2<double>>>> us2;
		//Vector<Vector<Vector<oversampling::downsampler441_x2<double>>>> ds2;

		RingModJoin rmjoin;
	};

	template<bool bOwner = false, bool bSmartPtr = true>
	class RingModSum : public AudioJoin<bOwner, bSmartPtr>
	{
	public:
		virtual size_t GetSampleDelay() const noexcept override
		{
			return AudioJoin<bOwner, bSmartPtr>::GetSampleDelay() + Utility::ceil_log2(this->GetInputs().size())*128;
		}

		void SetBalance(const float balanceVal)
		{
			*balance = (balanceVal < -1.0f) ? -1.0f : (balanceVal > 1.0f) ? 1.0f : balanceVal;
		}

		float GetBalance() const
		{
			return *balance;
		}

	private:
		virtual void JoinChannel(
			const size_t ch,
			Vector<SampleBuf>& inbufs,
			Sample* const chbuf,
			const size_t bufSize,
			const size_t bufsWritten) noexcept override
		{
			if (ch >= sumDelays.size())
				sumDelays.resize(ch + 1);
			Vector<Sample>& sumdly = sumDelays[ch];
			const size_t dlylen = Utility::ceil_log2(inbufs.size())*128;
			if (sumdly.size() < dlylen)
				sumdly.resize(dlylen);
			inbufsCopy = inbufs;
			sumbuf.Reinitialize(1, bufSize);
			rmjoin.JoinChannel(ch, inbufs, chbuf, bufSize, bufsWritten);
			Sample* const sumout = sumbuf[0];
			sumjoin.JoinChannel(ch, inbufsCopy, sumout, bufSize, bufsWritten);
			const float rmamp = 0.5f - 0.5f*(*balance); // -1 is all ring mod; 1 is no ring mod
			const float sumamp = 0.5f + 0.5f*(*balance); // 1 is all sum; -1 is no sum
			for (size_t i = 0; i < dlylen; ++i)
				chbuf[i] = rmamp*chbuf[i] + sumamp*sumdly[i];
			for (size_t i = dlylen; i < bufSize; ++i)
				chbuf[i] = rmamp*chbuf[i] + sumamp*sumout[i - dlylen];
			for (size_t i = 0; i < dlylen; ++i)
				sumdly[i] = sumout[bufSize - dlylen + i];
		}

	private:
		RingModJoin rmjoin;
		AudioSumJoin sumjoin;
		Vector<SampleBuf> inbufsCopy;
		SampleBuf sumbuf;
		Vector<Vector<Sample>> sumDelays;
		zeroinit_t<float> balance;
	};

	template<bool bOwner = false, bool bSmartPtr = true>
	class BasicAudioSum : public AudioSum<bOwner, bSmartPtr>
	{
	public:
		virtual void GetSamples(
			Sample* const* const bufs,
			const size_t numChannels,
			const size_t bufSize,
			const unsigned long sampleRate,
			IAudioObject* const requester) noexcept override
		{
			lastNumChannels = numChannels;
			this->GetInputSamples(bufs, numChannels, bufSize, sampleRate);
		}

		virtual size_t GetNumChannels() const noexcept override
		{
			return lastNumChannels;
		}

	private:
		size_t lastNumChannels;
	};

	template<bool bOwner = false, bool bSmartPtr = true>
	class BasicRingMod : public RingMod<bOwner, bSmartPtr>
	{
	public:
		virtual void GetSamples(
			Sample* const* const bufs,
			const size_t numChannels,
			const size_t bufSize,
			const unsigned long sampleRate,
			IAudioObject* const requester) noexcept override
		{
			lastNumChannels = numChannels;
			this->GetInputSamples(bufs, numChannels, bufSize, sampleRate);
		}

		virtual size_t GetNumChannels() const noexcept override
		{
			return lastNumChannels;
		}

	private:
		size_t lastNumChannels;
	};

	template<bool bOwner = false, bool bSmartPtr = true>
	class BasicRingModSum : public RingModSum<bOwner, bSmartPtr>
	{
	public:
		virtual void GetSamples(
			Sample* const* const bufs,
			const size_t numChannels,
			const size_t bufSize,
			const unsigned long sampleRate,
			IAudioObject* const requester) noexcept override
		{
			lastNumChannels = numChannels;
			this->GetInputSamples(bufs, numChannels, bufSize, sampleRate);
		}

		virtual size_t GetNumChannels() const noexcept override
		{
			return lastNumChannels;
		}

	private:
		size_t lastNumChannels;
	};

	template<bool bOwner = false, bool bSmartPtr = true>
	class AudioMult : public AudioSum<bOwner, bSmartPtr>
	{
	public:
		AudioMult()
			: queueLength(256), queueStart(0), queueEnd(0), bQueueInitialized(false)
		{
		}

	protected:
		enum class EPullSamplesResult
		{
			None, SamplesPulled, NullPuller, CannotTrackOutput, QueueNotInitialized, NullOutputBuffer, ChannelMismatch
		};

		EPullSamplesResult PullSamples(
			Sample* const* const bufs,
			const size_t numChannels,
			const size_t bufSize,
			const unsigned long sampleRate,
			IAudioObject* const puller) noexcept
		{
			std::scoped_lock lock(mtx);

			if (!bufs)
				return EPullSamplesResult::NullOutputBuffer;

			for (size_t i = 0; i < numChannels; ++i)
				if (!bufs[i])
					return EPullSamplesResult::NullOutputBuffer;

			if (numChannels != this->GetNumChannels())
				return EPullSamplesResult::ChannelMismatch;

			if (!bQueueInitialized)
				return EPullSamplesResult::QueueNotInitialized;

			if (!puller)
				return EPullSamplesResult::NullPuller;

			auto pos = queuePositions.find(puller);
			if (pos == queuePositions.end())
			{
				auto emplacedPuller = queuePositions.emplace(puller, queueStart);
				if (!emplacedPuller.second || emplacedPuller.first == queuePositions.end())
					return EPullSamplesResult::CannotTrackOutput;

				pos = emplacedPuller.first;
			}

			const size_t minQueueLength = GetQueuePosition(pos->second) + bufSize;
			ReserveQueue(minQueueLength, numChannels);

			FillQueue(minQueueLength, sampleRate);
			for (size_t ch = 0; ch < numChannels; ++ch)
				for (size_t bidx = 0, qidx = pos->second; bidx < bufSize; ++bidx, qidx = (qidx + 1) & (queueLength - 1))
					bufs[ch][bidx] = queue[ch][qidx];
			pos->second = (pos->second + bufSize) & (queueLength - 1);

			size_t minPos = queueEnd;
			for (auto& position : queuePositions)
				if (GetQueuePosition(position.second) < GetQueuePosition(minPos))
					minPos = position.second;

			queueStart = minPos;

			return EPullSamplesResult::SamplesPulled;
		}

		void ReserveQueue(const size_t minQueueLength, const size_t numChannels)
		{
			if (minQueueLength >= queueLength)
			{
				size_t newQueueLength = queueLength;
				while (newQueueLength <= minQueueLength)
					newQueueLength <<= 1;

				const size_t newQueueStart = 0;
				const size_t newQueueEnd = QueueSize();
				SampleBuf newQueue(numChannels, newQueueLength);

				for (size_t ch = 0; ch < numChannels; ++ch)
					for (size_t smpnum = 0; smpnum < newQueueEnd; ++smpnum)
						newQueue[ch][smpnum] = queue[ch][(queueStart + smpnum) & (queueLength - 1)];

				for (auto& position : queuePositions)
					position.second = (position.second + queueLength - queueStart) & (queueLength - 1);

				queue = std::move(newQueue);
				queueLength = newQueueLength;
				queueStart = newQueueStart;
				queueEnd = newQueueEnd;
			}
		}

		void FillQueue(const size_t filledLength, const unsigned long sampleRate)
		{
			const size_t numChannels = this->GetNumChannels();
			if (filledLength >= queueLength)
				ReserveQueue(filledLength, numChannels);

			const size_t queueSize = QueueSize();
			if (filledLength <= queueSize)
				return;

			const size_t numToRead = filledLength - queueSize;
			Vector<Sample*> fillStart(numChannels, nullptr);
			for (size_t ch = 0; ch < numChannels; ++ch)
				fillStart[ch] = queue[ch] + queueEnd;

			if (queueEnd + numToRead <= queueLength)
			{
				//GetSamples(fillStart.data(), numChannels, numToRead, sampleRate, this);
				this->GetInputSamples(fillStart.data(), numChannels, numToRead, sampleRate);
				queueEnd = (queueEnd + numToRead) & (queueLength - 1);
			}
			else
			{
				const size_t filledToEnd = queueLength - queueEnd;
				//GetSamples(fillStart.data(), numChannels, filledToEnd, sampleRate, this);
				this->GetInputSamples(fillStart.data(), numChannels, filledToEnd, sampleRate);
				queueEnd = numToRead - filledToEnd;
				//GetSamples(queue.get(), numChannels, queueEnd, sampleRate, this);
				this->GetInputSamples(queue.get(), numChannels, queueEnd, sampleRate);
			}
		}

		void InitializeQueue(const size_t numChannels)
		{
			std::scoped_lock lock(mtx);

			if (bQueueInitialized)
				return;

			if (!queue.Initialized())
				queue.Initialize(numChannels, queueLength);
			bQueueInitialized = true;
		}

		void InitializeQueue(const size_t numChannels, const size_t bufSize)
		{
			std::scoped_lock lock(mtx);

			if (bQueueInitialized)
				return;

			if (!queue.Initialized())
			{
				size_t newQueueLength = queueLength;
				while (newQueueLength <= bufSize)
					newQueueLength <<= 1;
				queueLength = newQueueLength;
				queue.Initialize(numChannels, queueLength);
			}
			bQueueInitialized = true;
		}

		size_t QueueSize() const noexcept
		{
			return GetQueuePosition(queueEnd);
		}

		size_t GetQueuePosition(const size_t pos) const noexcept
		{
			return (pos < queueStart) ? queueLength - queueStart + pos : pos - queueStart;
		}

	private:
		virtual void OnAddedAsInput(IAudioObject* const output) override
		{
			queuePositions.emplace(std::make_pair(output, 0));
		}

		virtual void OnRemovedFromInput(IAudioObject* const output) override
		{
			queuePositions.erase(output);
		}

	private:
		std::unordered_map<IAudioObject*, size_t> queuePositions;
		size_t queueLength;
		size_t queueStart;
		size_t queueEnd;
		bool bQueueInitialized;
		SampleBuf queue;
		std::mutex mtx;
	};

	template<bool bOwner = false, bool bSmartPtr = true>
	class BasicMult : public AudioMult<bOwner, bSmartPtr>
	{
	public:
		virtual void GetSamples(
			Sample* const* const bufs,
			const size_t numChannels,
			const size_t bufSize,
			const unsigned long sampleRate,
			IAudioObject* const requester) noexcept
		{
			lastNumChannels = numChannels;
			this->InitializeQueue(numChannels, bufSize);
			this->PullSamples(bufs, numChannels, bufSize, sampleRate, requester);
		}

		virtual size_t GetNumChannels() const noexcept
		{
			return lastNumChannels;
		}

	private:
		size_t lastNumChannels;
	};

	template<bool bOwner = false>
	class AudioGraph : public IAudioObject
	{
	public:

	private:
		Vector<SharedPtr<IAudioObject>> sources;
		Vector<SharedPtr<AudioSum<bOwner>>> nodes;
	};
}

