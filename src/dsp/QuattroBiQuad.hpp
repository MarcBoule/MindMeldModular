//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boul�
//
//Based on the BiQuad filter by Andrew Belt, in Rack's sources at ./include/dsp/filter.hpp
//Modifications by Marc Boul�
//See ./LICENSE.md for all licenses
//***********************************************************************************************


// Code below adapted from Andrew Belt's dsp::TBiquadFilter struct in VCV Rack's source code

// Four stereo biquad filters in pipeline series, where each biquad's parameters can be set separately
class QuattroBiQuad {

	// coefficients
	simd::float_4 b0;
	simd::float_4 b1;
	simd::float_4 b2;
	simd::float_4 a1;
	simd::float_4 a2;
	
	// input/output shift registers
	simd::float_4 x0L, x0R;// input is x0[0]
	simd::float_4 x1L, x1R;
	simd::float_4 x2L, x2R;
	simd::float_4 y0L, y0R;// output is y0[3]
	simd::float_4 y1L, y1R;
	simd::float_4 y2L, y2R;
	
	// other
	int8_t gainsDifferentThanOne; // 4 ls bits are bool bits, when all zero, can bypass y0 math
	bool optResetDone;
	
	
	public:


	enum Type {
		LOWSHELF,
		HIGHSHELF,
		PEAK,
	};
	
	void reset() {
		x0L = 0.0f;
		x0R = 0.0f;
		x1L = 0.0f;
		x1R = 0.0f;
		x2L = 0.0f;
		x2R = 0.0f;
		y0L = 0.0f;
		y0R = 0.0f;
		y1L = 0.0f;
		y1R = 0.0f;
		y2L = 0.0f;
		y2R = 0.0f;
		gainsDifferentThanOne = 0xF;
		optResetDone = false;
	}
	

	void process(float* out, float* in) {
		if (gainsDifferentThanOne == 0) {
			if (!optResetDone) {
				x2L = 0.0f;
				x2R = 0.0f;
				x1L = 0.0f;
				x1R = 0.0f;
				y2L = 0.0f;
				y2R = 0.0f;
				y1L = 0.0f;
				y1R = 0.0f;
				optResetDone = true;
			}
			
			x0L[0] = in[0];
			x0L[1] = y0L[0];
			x0L[2] = y0L[1];
			x0L[3] = y0L[2];
			y0L = x0L;
			
			x0R[0] = in[1];
			x0R[1] = y0R[0];
			x0R[2] = y0R[1];
			x0R[3] = y0R[2];
			y0R = x0R;
		}
		else {
			optResetDone = false;
			x2L = x1L;
			x1L = x0L;
			x0L[0] = in[0];
			x0L[1] = y0L[0];
			x0L[2] = y0L[1];
			x0L[3] = y0L[2];
			y2L = y1L;
			y1L = y0L;
			y0L = b0 * x0L + b1 * x1L + b2 * x2L - a1 * y1L - a2 * y2L;// https://en.wikipedia.org/wiki/Infinite_impulse_response

			x2R = x1R;
			x1R = x0R;
			x0R[0] = in[1];
			x0R[1] = y0R[0];
			x0R[2] = y0R[1];
			x0R[3] = y0R[2];
			y2R = y1R;
			y1R = y0R;
			y0R = b0 * x0R + b1 * x1R + b2 * x2R - a1 * y1R - a2 * y2R;// https://en.wikipedia.org/wiki/Infinite_impulse_response
		}
		
		out[0] = y0L[3];
		out[1] = y0R[3];
	}	

		
	void setParameters(Type type, int i, float f, float V, float Q) {
		// type: type of filter/eq
		// i: eq index (0 to 3),
		// f: normalized frequency (fc/sampleRate)
		// V: linearGain for peak or shelving
		// Q: quality factor
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