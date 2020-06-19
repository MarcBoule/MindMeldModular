//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//Based on the BiQuad filter by Andrew Belt, in Rack's sources at ./include/dsp/filter.hpp
//Modifications by Marc Boulé
//See ./LICENSE.md for all licenses
//***********************************************************************************************


// Code below adapted from Andrew Belt's dsp::TBiquadFilter struct in VCV Rack's source code
// Modifications (non-exhaustive list):
//  * implement Q for shelving
//  * make signal path stereo, but just one set of coefficients


#pragma once


class QuattroBiQuadCoeff {
	protected: 
	
	// coefficients
	simd::float_4 b0;
	simd::float_4 b1;
	simd::float_4 b2;
	simd::float_4 a1;
	simd::float_4 a2;
	
	
	public: 
	
	
	enum Type {
		LOWSHELF,
		HIGHSHELF,
		PEAK,
	};
	
	
	void setParameters(int i, Type type, float nfc, float V, float Q) {
		// i: eq index (0 to 3),
		// type: type of filter/eq
		// nfc: normalized cutoff frequency (fc/sampleRate)
		// V: linearGain for peak or shelving
		// Q: quality factor
		
		// nfc: normalized cutoff frequency (cutoff frequency / sample rate), must be > 0
		// freq pre-warping with inclusion of M_PI factor; 
		//   avoid tan() if fc is low (< 1102.5 Hz @ 44.1 kHz, since error at this freq is 2 Hz)
		float K = nfc < 0.025f ? M_PI * nfc : std::tan(M_PI * std::min(0.499f, nfc));

		switch (type) {
			case LOWSHELF: {
				float sqrtV = std::sqrt(V);
				Q = std::sqrt(Q) / M_SQRT2;
				if (V >= 1.f) {// when V = 1, b0 = 1, a1 = b1, a2 = b2
					float norm = 1.f / (1.f + K / Q + K * K);
					b0[i] = (1.f + sqrtV * K / Q + V * K * K) * norm;
					b1[i] = 2.f * (V * K * K - 1.f) * norm;
					b2[i] = (1.f - sqrtV * K / Q + V * K * K) * norm;
					a1[i] = 2.f * (K * K - 1.f) * norm;
					a2[i] = (1.f - K / Q + K * K) * norm;
				}
				else {
					float norm = 1.f / (1.f + K / (Q * sqrtV) + K * K / V);
					b0[i] = (1.f + K / Q + K * K) * norm;
					b1[i] = 2.f * (K * K - 1) * norm;
					b2[i] = (1.f - K / Q + K * K) * norm;
					a1[i] = 2.f * (K * K / V - 1.f) * norm;
					a2[i] = (1.f - K / (Q * sqrtV) + K * K / V) * norm;
				}
			} break;

			case HIGHSHELF: {
				float sqrtV = std::sqrt(V);
				Q = std::sqrt(Q) / M_SQRT2;
				if (V >= 1.f) {// when V = 1, b0 = 1, a1 = b1, a2 = b2
					float norm = 1.f / (1.f + K / Q + K * K);
					b0[i] = (V + sqrtV * K / Q + K * K) * norm;
					b1[i] = 2.f * (K * K - V) * norm;
					b2[i] = (V - sqrtV * K / Q + K * K) * norm;
					a1[i] = 2.f * (K * K - 1.f) * norm;
					a2[i] = (1.f - K / Q + K * K) * norm;
				}
				else {
					float norm = 1.f / (1.f / V + K / (Q * sqrtV) + K * K);
					b0[i] = (1.f + K / Q + K * K) * norm;
					b1[i] = 2.f * (K * K - 1.f) * norm;
					b2[i] = (1.f - K / Q + K * K) * norm;
					a1[i] = 2.f * (K * K - 1.f / V) * norm;
					a2[i] = (1.f / V - K / (Q * sqrtV) + K * K) * norm;
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


	// add all 4 values in return vector to get total gain (dB) since each float is gain (dB) of one biquad
	simd::float_4 getFrequencyResponse(float f) {
		// Compute sum(b_k z^-k) / sum(a_k z^-k) where z = e^(i s)
		
		float s = 2 * M_PI * f;// s: normalized angular frequency equal to $2 \pi f / f_{sr}$ ($\pi$ is the Nyquist frequency)
		
		simd::float_4 bSum[2] = {b0, 0.0f};
		simd::float_4 aSum[2] = {1.0f, 0.0f};
			
		float p = -1 * s;
		simd::float_4 z[2] = {std::cos(p), std::sin(p)};
		bSum[0] += b1 * z[0];
		bSum[1] += b1 * z[1];
		aSum[0] += a1 * z[0];
		aSum[1] += a1 * z[1];
	
		p = -2 * s;
		z[0] = std::cos(p); z[1] = std::sin(p);
		bSum[0] += b2 * z[0];
		bSum[1] += b2 * z[1];
		aSum[0] += a2 * z[0];
		aSum[1] += a2 * z[1];
		
		simd::float_4 num[2] = {bSum[0] * aSum[0] +	bSum[1] * aSum[1],  bSum[1] * aSum[0] - bSum[0] * aSum[1]};
		simd::float_4 denom = aSum[0] * aSum[0] + aSum[1] * aSum[1];
		simd::float_4 norm = simd::hypot(num[0] / denom,  num[1] / denom);
		return 20.0f * simd::log10(norm);// return in dB
	}
};



// Four stereo biquad filters in pipeline series, where each biquad's parameters can be set separately
class QuattroBiQuad : public QuattroBiQuadCoeff {
	
	// input/output shift registers
	simd::float_4 x0L, x0R;// input is x0[0]
	simd::float_4 x1L, x1R;
	simd::float_4 x2L, x2R;
	simd::float_4 y0L, y0R;// output is y0[3]
	simd::float_4 y1L, y1R;
	simd::float_4 y2L, y2R;
	
	// other
	bool optResetDone;
	int8_t gainsDifferentThanOne; // 4 ls bits are bool bits, when all zero, can bypass y0 math
	
	
	public:


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
	
	
	void setParameters(int i, Type type, float f, float V, float Q) {
		// type: type of filter/eq
		// i: eq index (0 to 3),
		// f: normalized frequency (fc/sampleRate)
		// V: linearGain for peak or shelving
		// Q: quality factor
		if (V == 1.0f) {
			gainsDifferentThanOne &= ~(0x1 << i);
		}
		else {
			gainsDifferentThanOne |= (0x1 << i);
		}
		QuattroBiQuadCoeff::setParameters(i, type, f, V, Q);
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
};
