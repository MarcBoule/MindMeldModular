//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//Based on the BiQuad filter by Andrew Belt, in Rack's sources at ./include/dsp/filter.hpp
//Modifications by Marc Boulé
//See ./LICENSE.md for all licenses
//***********************************************************************************************


// Code below adapted from Andrew Belt's dsp::TBiquadFilter struct in VCV Rack's source code

// Four biquad filters in series, where each biquad's parameters can be set separately
struct QuattroBiQuad {
	
	enum Type {
		LOWSHELF,
		HIGHSHELF,
		PEAK,
	};
	
	simd::float_4 b0;
	simd::float_4 b1;
	simd::float_4 b2;
	simd::float_4 a1;
	simd::float_4 a2;
	
	simd::float_4 x0;
	simd::float_4 x1;
	simd::float_4 x2;
	simd::float_4 y1;
	simd::float_4 y2;
	
	simd::float_4 y0;
	
	int8_t gainsDifferentThanOne; // 4 ls bits are bool bits, when all zero, can bypass filter calc
	
	void reset() {
		x0 = 0.0f;
		x1 = 0.0f;
		x2 = 0.0f;
		y1 = 0.0f;
		y2 = 0.0f;
		gainsDifferentThanOne = 0xF;
	}
	
	
	float process(float in) {
		if (gainsDifferentThanOne == 0) {
			x0 = in;
			y0 = in;
			return in;
		}
		else {
			x0[0] = in;
			y0 = b0 * x0 + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;// https://en.wikipedia.org/wiki/Infinite_impulse_response
		
			x2 = x1;
			x1 = x0;
			x0[1] = y0[0];
			x0[2] = y0[1];
			x0[3] = y0[2];
			
			y2 = y1;
			y1 = y0;
			return y0[3];
		}
	}	
	
		
	void setParameters(Type type, int i, float f, float V, float Q) {
		// type: type of filter/eq, see Type enum in dsp::TBiquadFilter
		// i: eq index (0 to 3),
		// f: normalized frequency (fc/sampleRate)
		// V: linearGain for peak or shelving
		// Q: quality (0.1 to 10 ?)
		float K = std::tan(M_PI * f);
		if (V == 1.0f) {
			gainsDifferentThanOne &= ~(0x1 << i);
		}
		else {
			gainsDifferentThanOne |= (0x1 << i);
		}
		switch (type) {
			case LOWSHELF: {
				float sqrtV = std::sqrt(V);
				if (V >= 1.f) {// when V = 1, b0 = 1, a1 = b1, a2 = b2
					float norm = 1.f / (1.f + M_SQRT2 * K + K * K);
					b0[i] = (1.f + M_SQRT2 * sqrtV * K + V * K * K) * norm;
					b1[i] = 2.f * (V * K * K - 1.f) * norm;
					b2[i] = (1.f - M_SQRT2 * sqrtV * K + V * K * K) * norm;
					a1[i] = 2.f * (K * K - 1.f) * norm;
					a2[i] = (1.f - M_SQRT2 * K + K * K) * norm;
				}
				else {
					float norm = 1.f / (1.f + M_SQRT2 / sqrtV * K + K * K / V);
					b0[i] = (1.f + M_SQRT2 * K + K * K) * norm;
					b1[i] = 2.f * (K * K - 1) * norm;
					b2[i] = (1.f - M_SQRT2 * K + K * K) * norm;
					a1[i] = 2.f * (K * K / V - 1.f) * norm;
					a2[i] = (1.f - M_SQRT2 / sqrtV * K + K * K / V) * norm;
				}
			} break;

			case HIGHSHELF: {
				float sqrtV = std::sqrt(V);
				if (V >= 1.f) {// when V = 1, b0 = 1, a1 = b1, a2 = b2
					float norm = 1.f / (1.f + M_SQRT2 * K + K * K);
					b0[i] = (V + M_SQRT2 * sqrtV * K + K * K) * norm;
					b1[i] = 2.f * (K * K - V) * norm;
					b2[i] = (V - M_SQRT2 * sqrtV * K + K * K) * norm;
					a1[i] = 2.f * (K * K - 1.f) * norm;
					a2[i] = (1.f - M_SQRT2 * K + K * K) * norm;
				}
				else {
					float norm = 1.f / (1.f / V + M_SQRT2 / sqrtV * K + K * K);
					b0[i] = (1.f + M_SQRT2 * K + K * K) * norm;
					b1[i] = 2.f * (K * K - 1.f) * norm;
					b2[i] = (1.f - M_SQRT2 * K + K * K) * norm;
					a1[i] = 2.f * (K * K - 1.f / V) * norm;
					a2[i] = (1.f / V - M_SQRT2 / sqrtV * K + K * K) * norm;
				}
			} break;

			case PEAK: {
				if (V >= 1.f) {
					float norm = 1.f / (1.f + K / Q + K * K);
					b0[i] = (1.f + K / Q * V + K * K) * norm;
					b1[i] = 2.f * (K * K - 1.f) * norm;
					b2[i] = (1.f - K / Q * V + K * K) * norm;
					a1[i] = b1[i];
					a2[i] = (1.f - K / Q + K * K) * norm;
				}
				else {
					float norm = 1.f / (1.f + K / Q / V + K * K);
					b0[i] = (1.f + K / Q + K * K) * norm;
					b1[i] = 2.f * (K * K - 1.f) * norm;
					b2[i] = (1.f - K / Q + K * K) * norm;
					a1[i] = b1[i];
					a2[i] = (1.f - K / Q / V + K * K) * norm;
				}
			} break;

			default: break;
		}
	}
};