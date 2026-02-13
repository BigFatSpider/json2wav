// Copyright Dan Price 2026.

#pragma once

#include "IAudioObject.h"
#include "FastSin.h"
#include "Ramp.gen.h"
#include <cmath>

namespace json2wav
{
	enum class ERampShape
	{
		Instant, // Instantly adopts the end value i.e. no ramp
		Linear, // Follows the line that interpolates start and end values
		QuarterSin, // Follows the first or, if decreasing, second quarter of a sine curve
		//QuarterSinNeg, // Follows the fourth or, if decreasing, third quarter of a sine curve
		SCurve, // Follows an S-shaped curve between start and end values
		SCurveEqualPower, // Follows an S-shaped curve that is 1/sqrt(2) at the halfway point instead of 1/2
		Hit, // Transient peaking at gain of 2
		Hit262, // Transient peaking at gain of 2
		Hit272, // Transient peaking at gain of 2
		Hit282, // Transient peaking at gain of 2
		Hit292, // Transient peaking at gain of 2
		Hit2A2, // Transient peaking at gain of 2
		Hit2624, // Transient peaking at gain of 2
		LogScaleLinear, // Follows the exponential curve with asymptote y = 0 that interpolates start and end values
		LogScaleSCurve, // Follows an S-shaped curve on a log scale
		LogScaleHalfSin, // Similar to LogScaleSCurve but more expensive
		Mod, // Modulation source
		Parabola, // Parabola
		Blabola // Band-limited parabola
	};

	namespace RampDetail
	{
		template<typename T> struct IsFloatType { static constexpr const bool value = false; };
		template<> struct IsFloatType<float> { static constexpr const bool value = true; };
		template<> struct IsFloatType<double> { static constexpr const bool value = true; };
		template<> struct IsFloatType<long double> { static constexpr const bool value = true; };

		template<typename FloatType>
		inline FloatType SPoly(const FloatType x) noexcept
		{
			static_assert(IsFloatType<FloatType>::value, "json2wav::RampDetail::SPoly() only operates on float types");

			/** S-shaped polynomial on [0, 1] with the following constraints:
			 * 1) Start at 0:                           f(0) = 0
			 * 2) Start with continuous derivative:     f'(0) = 0
			 * 3) End at 1:                             f(1) = 1
			 * 4) End with continuous derivative:       f'(1) = 0
			 *
			 * y = a*x^3 + b*x^2
			 * a = -2
			 * b = 3
			 */

			static const FloatType a = static_cast<FloatType>(-2.0);
			static const FloatType b = static_cast<FloatType>(3.0);

			return (a*x + b)*x*x;
		}

		template<typename FloatType>
		inline FloatType SPolyEqualPowerFastSafe(const FloatType x) noexcept
		{
			static_assert(IsFloatType<FloatType>::value, "json2wav::RampDetail::SPolyEqualPowerFastSafe() only operates on float types");

			/** S-shaped polynomial on [0, 1] with the following constraints:
			 * 1) Start at 0:                           f(0) = 0
			 * 2) Start with continuous derivative:     f'(0) = 0
			 * 3) End at 1:                             f(1) = 1
			 * 4) End with continuous derivative:       f'(1) = 0
			 * 5) Prevent overshoot:                    f''(1) = 0
			 *
			 * y = a*x^4 + b*x^3 + c*x^2
			 * a = 3
			 * b = -8
			 * c = 6
			 *
			 * No xfade power sum overshoot
			 * Xfade power sum max undershoot: 0.244 dB  at x=0.5
			 */

			static const FloatType a = static_cast<FloatType>(3.0);
			static const FloatType b = static_cast<FloatType>(-8.0);
			static const FloatType c = static_cast<FloatType>(6.0);

			return ((a*x + b)*x + c)*x*x;
		}

		template<typename FloatType>
		inline FloatType SPolyEqualPowerFastPrecise(const FloatType x) noexcept
		{
			static_assert(IsFloatType<FloatType>::value, "json2wav::RampDetail::SPolyEqualPowerFastPrecise() only operates on float types");

			/** S-shaped polynomial on [0, 1] with the following constraints:
			 * 1) Start at 0:                           f(0) = 0
			 * 2) Start with continuous derivative:     f'(0) = 0
			 * 3) End at 1:                             f(1) = 1
			 * 4) End with continuous derivative:       f'(1) = 0
			 * 5) -3 dB half way:                       f(1/2) = 1/sqrt(2)
			 *
			 * y = a*x^4 + b*x^3 + c*x^2
			 * a = 8*sqrt(2) - 8
			 * b = 14 - 16*sqrt(2)
			 * c = 8*sqrt(2) - 5
			 *
			 * Xfade power sum max overshoot:  0.00372 dB at x=0.0762694 and x=0.923731
			 * Xfade power sum max undershoot: 0.0164 dB  at x=0.282641  and x=0.717359
			 */

			static const FloatType a = static_cast<FloatType>(8.0*sqrt(2.0) - 8.0);
			static const FloatType b = static_cast<FloatType>(14.0 - 16.0*sqrt(2.0));
			static const FloatType c = static_cast<FloatType>(8.0*sqrt(2.0) - 5.0);

			return ((a*x + b)*x + c)*x*x;
		}

		template<typename FloatType>
		inline FloatType SPolyEqualPowerSafe(const FloatType x) noexcept
		{
			static_assert(IsFloatType<FloatType>::value, "json2wav::RampDetail::SPolyEqualPowerSafe() only operates on float types");

			/** S-shaped polynomial on [0, 1] with the following constraints:
			 * 1) Start at 0:                           f(0) = 0
			 * 2) Start with continuous derivative:     f'(0) = 0
			 * 3) End at 1:                             f(1) = 1
			 * 4) End with continuous derivative:       f'(1) = 0
			 * 5) Prevent overshoot:                    f''(1) = 0
			 * 6) -3 dB half way:                       f(1/2) = 1/sqrt(2)
			 *
			 * y = a*x^5 + b*x^4 + c*x^3 + d*x^2
			 * a = 22 - 16*sqrt(2)
			 * b = 48*sqrt(2) - 63
			 * c = 58 - 48*sqrt(2)
			 * d = 16*sqrt(2) - 16
			 *
			 * No xfade power sum overshoot
			 * Xfade power sum max undershoot: 0.0483 dB at x=0.249828 and x=0.750172
			 */

			static const FloatType a = static_cast<FloatType>(22.0 - 16.0*sqrt(2.0));
			static const FloatType b = static_cast<FloatType>(48.0*sqrt(2.0) - 63.0);
			static const FloatType c = static_cast<FloatType>(58.0 - 48.0*sqrt(2.0));
			static const FloatType d = static_cast<FloatType>(16.0*sqrt(2.0) - 16.0);

			return (((a*x + b)*x + c)*x + d)*x*x;
		}

		template<typename FloatType>
		inline FloatType SPolyEqualPower2i3o(const FloatType x) noexcept
		{
			static_assert(IsFloatType<FloatType>::value, "json2wav::RampDetail::SPolyEqualPower2i3o() only operates on float types");

			/** S-shaped polynomial on [0, 1] with the following constraints:
			 * 1) Start at 0:                           f(0) = 0
			 * 2) Start with continuous 1st derivative: f'(0) = 0
			 * 3) Start with continuous 2nd derivative: f''(0) = 0
			 * 4) End at 1:                             f(1) = 1
			 * 5) End with continuous 1st derivatve:    f'(1) = 0
			 * 6) End with continuous 2nd derivative:   f''(1) = 0
			 * 7) Prevent overshoot:                    f'''(1) = 0
			 * 8) -3 dB half way:                       f(1/2) = 1/sqrt(2)
			 *
			 * y = a*x^7 + b*x^6 + c*x^5 + d*x^4 + e*x^3
			 * a = -84 + 64*sqrt(2)
			 * b = 326 - 256*sqrt(2)
			 * c = -468 + 384*sqrt(2)
			 * d = 291 - 256*sqrt(2)
			 * e = -64 + 64*sqrt(2)
			 */

			static const FloatType a = static_cast<FloatType>(64.0*sqrt(2.0) - 84.0);
			static const FloatType b = static_cast<FloatType>(326.0 - 256.0*sqrt(2.0));
			static const FloatType c = static_cast<FloatType>(384.0*sqrt(2.0) - 468.0);
			static const FloatType d = static_cast<FloatType>(291.0 - 256.0*sqrt(2.0));
			static const FloatType e = static_cast<FloatType>(64.0*sqrt(2.0) - 64.0);

			// y = a*x^7 + b*x^6 + c*x^5 + d*x^4 + e*x^3
			// 1. x*x, a*x, b*x, d*x
			// 2. bx+c, dx+e
			// 3. x2*x, x2*x2, ax*x2, bxsc*x2
			// 4. bx3scx2+dxse
			// 5. bx3scx2sdxse*x3, ax3*x4
			// 6. bx3scx2sdxsetx3+ax7
			// Should compile to 3 128-bit multiplies for 32-bit float

			const FloatType x2 = x*x;
			const FloatType ax = a*x;
			const FloatType bx = b*x;
			const FloatType dx = d*x;
			const FloatType bxsc = bx+c;
			const FloatType dxse = dx+e;
			const FloatType x3 = x2*x;
			const FloatType x4 = x2*x2;
			const FloatType ax3 = ax*x2;
			const FloatType bx3scx2 = bxsc*x2;
			const FloatType bx3scx2sdxse = bx3scx2+dxse;
			const FloatType bx6scx5sdx4sex3 = bx3scx2sdxse*x3;
			const FloatType ax7 = ax3*x4;
			return ax7+bx6scx5sdx4sex3;
		}
	}

	template<typename ValueType>
	class FloatRamp
	{
		static_assert(RampDetail::IsFloatType<ValueType>::value, "Ramps only operate on floating point types");

	public:
		FloatRamp() : topTail{ NAN, 0.0f }, expBase(NAN), timeLength(NAN), time(0.0), shape(ERampShape::Linear), mod(nullptr) {}
		FloatRamp(const FloatRamp& other) noexcept = default;
		FloatRamp(FloatRamp&& other) noexcept = default;
		explicit FloatRamp(const ValueType targetValueInit, const double timeInit, const ERampShape shapeInit = ERampShape::Linear) noexcept
			: topTail{ NAN, targetValueInit }, expBase(NAN), timeLength(NAN), time((timeInit <= 0.0) ? 1.0 : timeInit), shape((timeInit <= 0.0) ? ERampShape::Instant : shapeInit), mod(nullptr)
		{
		}
		explicit FloatRamp(IAudioObject& modInit, const ValueType modAmt = 1.0)
			: topTail{ NAN, NAN }, expBase(NAN), timeLength(NAN), time(modAmt), shape(ERampShape::Mod), mod(&modInit)
		{
		}

		FloatRamp& operator=(const FloatRamp& other) noexcept = default;
		FloatRamp& operator=(FloatRamp&& other) noexcept = default;

		bool Increment(ValueType& currentValue, const double deltaTime)
		{
			if (time <= 0.0)
				return false;

			ValueType prevValue(0.0f);
			switch (shape)
			{
			case ERampShape::Mod:
				{
					Sample smp;
					Sample* psmp(&smp);
					const unsigned long sr = static_cast<unsigned long>(1.0 / deltaTime);
					mod->GetSamples(&psmp, 1, 1, sr, nullptr);
					currentValue += static_cast<ValueType>(time*static_cast<float>(smp));
				} return true;

			case ERampShape::Instant:
				currentValue = topTail[1];
				time = 0.0;
				return true;

			default:
			case ERampShape::Linear:
				{
					prevValue = currentValue;
					currentValue += static_cast<ValueType>((topTail[1] - currentValue) * static_cast<ValueType>(deltaTime) / static_cast<ValueType>(time));
				} break;

			case ERampShape::QuarterSin:
				{
					if (std::isnan(topTail[0]))
					{
						topTail[0] = currentValue;
						timeLength = time;
					}
					prevValue = currentValue;
					const size_t idx(topTail[0] < topTail[1]);
					const ValueType x(((ValueType)timeLength - (ValueType)time)/(ValueType)timeLength);
					currentValue = (topTail[!idx] - topTail[idx])*static_cast<ValueType>(FastSin<6, ValueType>(QuarterTau<ValueType>()*(x + (ValueType)idx))) + topTail[idx];
				} break;

			case ERampShape::SCurve:
				{
					if (std::isnan(topTail[0]))
					{
						topTail[0] = currentValue;
						timeLength = time;
					}
					prevValue = currentValue;
					const ValueType x(((ValueType)timeLength - (ValueType)time)/(ValueType)timeLength);
					currentValue = topTail[0] + RampDetail::SPoly(x) * (topTail[1] - topTail[0]);
				} break;

			case ERampShape::SCurveEqualPower:
				{
					if (std::isnan(topTail[0]))
					{
						topTail[0] = currentValue;
						timeLength = time;
					}
					prevValue = currentValue;
					const size_t idx(topTail[0] > topTail[1]);
					const ValueType x(((ValueType)timeLength - (ValueType)time)/(ValueType)timeLength);
					const ValueType x_signed[2]{ x, -x };
					currentValue = topTail[0] + RampDetail::SPolyEqualPowerSafe(static_cast<ValueType>(idx) + x_signed[idx]) * (topTail[1] - topTail[0]);
				} break;

			case ERampShape::Hit: RampPoly(&RampDetail::HitPoly2624<ValueType>, currentValue, prevValue); break;
			case ERampShape::Hit262: RampPoly(&RampDetail::HitPoly262<ValueType>, currentValue, prevValue); break;
			case ERampShape::Hit272: RampPoly(&RampDetail::HitPoly272<ValueType>, currentValue, prevValue); break;
			case ERampShape::Hit282: RampPoly(&RampDetail::HitPoly282<ValueType>, currentValue, prevValue); break;
			case ERampShape::Hit292: RampPoly(&RampDetail::HitPoly292<ValueType>, currentValue, prevValue); break;
			case ERampShape::Hit2A2: RampPoly(&RampDetail::HitPoly2A2<ValueType>, currentValue, prevValue); break;
			case ERampShape::Hit2624: RampPoly(&RampDetail::HitPoly2624<ValueType>, currentValue, prevValue); break;

			case ERampShape::LogScaleLinear:
				{
					prevValue = currentValue;
					currentValue *= static_cast<ValueType>(std::pow((topTail[1] / currentValue), static_cast<ValueType>(deltaTime) / static_cast<ValueType>(time)));
				} break;

			case ERampShape::LogScaleSCurve:
				{
					if (std::isnan(topTail[0]))
					{
						topTail[0] = currentValue;
						expBase = topTail[1] / topTail[0];
						timeLength = time;
					}
					prevValue = currentValue;
					const ValueType x(((ValueType)timeLength - (ValueType)time)/(ValueType)timeLength);
					currentValue = topTail[0] * static_cast<ValueType>(std::pow(expBase, RampDetail::SPoly(x)));
				} break;

			case ERampShape::LogScaleHalfSin:
				{
					if (std::isnan(topTail[0]))
					{
						topTail[0] = currentValue;
						expBase = topTail[1] / topTail[0];
						timeLength = time;
					}
					prevValue = currentValue;
					const ValueType x(((ValueType)timeLength - (ValueType)time)/(ValueType)timeLength);
					currentValue = topTail[0] * static_cast<ValueType>(std::pow(expBase, static_cast<ValueType>(0.5f) * (FastSin<6, ValueType>(HalfTau<ValueType>() * (x - (ValueType)0.5f)) + (ValueType)1.0f)));
				} break;

			case ERampShape::Parabola:
				{
					if (std::isnan(topTail[0]))
					{
						topTail[0] = currentValue;
						timeLength = time;
					}
					prevValue = currentValue;
					// ( 0,  0) to ( 1,  1): y = 1 - (x - 1)^2
					// (x0, y0) to (x1, y1): y = (y1 - y0)*(1 - (x/x1 - 1)^2) + y0
					// (x0, y0) to (x1, y1): y = (y1 - y0)*(1 - ((x1 - time)/x1 - 1)^2) + y0
					// (x0, y0) to (x1, y1): y = (y1 - y0)*(1 - (x1/x1 - time/x1 - 1)^2) + y0
					// (x0, y0) to (x1, y1): y = (y1 - y0)*(1 - (-time/x1)^2) + y0
					const double y0 = topTail[0];
					const double y1 = topTail[1];
					const double squared = time/timeLength;
					currentValue = (y1 - y0)*(1.0 - squared*squared) + y0;
				}
			case ERampShape::Blabola:
				{
					prevValue = currentValue;
					currentValue += static_cast<ValueType>((topTail[1] - currentValue) * static_cast<ValueType>(deltaTime) / static_cast<ValueType>(time));
				} break;
			}

			if ((prevValue < topTail[1] && currentValue >= topTail[1]) ||
				(prevValue > topTail[1] && currentValue <= topTail[1]))
			{
				currentValue = topTail[1];
			}

			time -= deltaTime;
			return true;
		}

		double GetTimeLength() const noexcept { return (std::isnan(timeLength)) ? time : timeLength; }

		ERampShape GetShape() const noexcept { return shape; }

	private:
		void RampPoly(ValueType (*poly)(const ValueType), ValueType& currentValue, ValueType& prevValue)
		{
			if (std::isnan(topTail[0]))
			{
				topTail[0] = currentValue;
				timeLength = time;
			}
			prevValue = currentValue;
			const size_t idx(topTail[0] > topTail[1]);
			const ValueType x(((ValueType)timeLength - (ValueType)time)/(ValueType)timeLength);
			const ValueType x_signed[2]{ x, -x };
			currentValue = topTail[0] + poly(static_cast<ValueType>(idx) + x_signed[idx]) * (topTail[1] - topTail[0]);
		}

	private:
		ValueType topTail[2];
		ValueType expBase;
		double timeLength;
		double time;
		ERampShape shape;
		IAudioObject* mod;
	};

	using Ramp = FloatRamp<float>;
	using PreciseRamp = FloatRamp<double>;
}

