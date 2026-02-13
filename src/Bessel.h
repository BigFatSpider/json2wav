// Copyright Dan Price. All rights reserved.

#pragma once

#include "Bessel.gen.h"
#include <cmath>

namespace json2wav
{
	inline float jn_drum(const size_t num_pie_nodes, const size_t num_circle_nodes, const float x)
	{
		return fast_jn_norm(num_pie_nodes, bessel_harmonics_by_order[num_pie_nodes][num_circle_nodes] * x);
	}

	inline void get_drum_harmonics(float (&harmonics)[NUM_BESSEL_ORDERS][NUM_POSITIVE_BESSEL_ZEROS_PER_ORDER], const float d)
	{
		constexpr const float sqrt2 = 1.4142135623730951f;
		float amps_by_zero[NUM_POSITIVE_BESSEL_ZEROS_PER_ORDER];
		for (size_t zero = 0; zero < NUM_POSITIVE_BESSEL_ZEROS_PER_ORDER; ++zero)
		{
			const float f(1.0f / static_cast<float>(zero + 1));
			amps_by_zero[zero] = f*f;
		}
		const float order_amp_adj(1.0f / (sqrt2 * std::powf(d + 1.0f, 2.5f)));
		for (size_t order = 0; order < NUM_BESSEL_ORDERS; ++order)
		{
			const size_t orderNum(order + 1);
			const float order_amp((1.0f - order_amp_adj * (orderNum & 1)) / static_cast<float>(orderNum));
			for (size_t zero = 0; zero < NUM_POSITIVE_BESSEL_ZEROS_PER_ORDER; ++zero)
			{
				harmonics[order][zero] = order_amp * amps_by_zero[zero] * jn_drum(order, zero, d);
			}
		}
	}
}

