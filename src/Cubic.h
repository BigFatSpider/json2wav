// Copyright Dan Price 2026.

#pragma once

namespace json2wav
{
	template<typename T>
	class CubicFunctor
	{
	public:
		CubicFunctor(const T a_init, const T b_init, const T c_init, const T d_init) noexcept
			: a(a_init), b(b_init), c(c_init), d(d_init)
		{
		}

		T operator()(const T x) const
		{
			return ((a*x + b)*x + c)*x + d;
		}

	private:
		T a;
		T b;
		T c;
		T d;
	};

	using Cubic = CubicFunctor<float>;
	using Cubic32 = CubicFunctor<float>;
	using Cubic64 = CubicFunctor<double>;
}

