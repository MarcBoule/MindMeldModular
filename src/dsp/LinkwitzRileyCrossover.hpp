//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//See ./LICENSE.md for all licenses
//***********************************************************************************************

// Linkwitz-Riley audio crossover, based on Linkwitz-Riley filters
// https://en.wikipedia.org/wiki/Linkwitz%E2%80%93Riley_filter

// Cascaded 2nd-order butterworth filters are used when offering 4th order Linkwitz-Riley crossover
// Cascaded 1st-order filters are used when offering 2nd order Linkwitz-Riley crossover


#pragma once


class LinkwitzRileyCrossover {	
	bool secondOrderFilters = false;// local memory of what is in iirs		
	
	simd::float_4 b[3];// coefficients b0, b1 and b2, where each float of float_4 is LeftLow, LeftHigh, RightLow, RightHigh
	simd::float_4 a[3 - 1];// coefficients a1 and a2, where each float of float_4 is LeftLow, LeftHigh, RightLow, RightHigh
	simd::float_4 xS1[3 - 1];
	simd::float_4 yS1[3 - 1];
	simd::float_4 xS2[3 - 1];
	simd::float_4 yS2[3 - 1];
	
	
	public: 
		
	void reset() {
		for (int i = 0; i < 2; i++) {
			xS1[i] = 0.0f;
			yS1[i] = 0.0f;
			xS2[i] = 0.0f;
			yS2[i] = 0.0f;
		}
	}
	
	void setFilterCutoffs(float nfc, bool _secondOrder) {
		secondOrderFilters = _secondOrder;
		
		// nfc: normalized cutoff frequency (cutoff frequency / sample rate), must be > 0
		// freq pre-warping with inclusion of M_PI factor; 
		//   avoid tan() if fc is low (< 1102.5 Hz @ 44.1 kHz, since error at this freq is 2 Hz)
		float nfcw = nfc < 0.025f ? M_PI * nfc : std::tan(M_PI * std::min(0.499f, nfc));
		
		if (secondOrderFilters) {	
			// denominator coefficients (same for both LPF and HPF)
			float acst = nfcw * nfcw + nfcw * (float)M_SQRT2 + 1.0f;
			a[0] = simd::float_4(2.0f * (nfcw * nfcw - 1.0f) / acst);
			a[1] = simd::float_4((nfcw * nfcw - nfcw * (float)M_SQRT2 + 1.0f) / acst);
			
			// numerator coefficients
			float hbcst = 1.0f / acst;
			float lbcst = hbcst * nfcw * nfcw;			
			b[0] = simd::float_4(lbcst, hbcst, lbcst, hbcst);
			b[1] = simd::float_4(lbcst, -hbcst, lbcst, -hbcst) * 2.0f;
			b[2] = b[0];
		}
		else {
			// denominator coefficients (same for both LPF and HPF)
			float acst = (nfcw - 1.0f) / (nfcw + 1.0f);
			a[0] = simd::float_4(acst);
			a[1] = simd::float_4(0.0f);
			
			// numerator coefficients
			float hbcst = 1.0f / (1.0f + nfcw);
			float lbcst = 1.0f - hbcst;// equivalent to: hbcst * nfcw;
			b[0] = simd::float_4(lbcst, hbcst, lbcst, hbcst);
			b[1] = simd::float_4(lbcst, -hbcst, lbcst, -hbcst);
			b[2] = simd::float_4(0.0f);
		}
	}

	simd::float_4 process(float left, float right) {
		// return [0] = left low, left high, right low, [3] = right high
		simd::float_4 in = simd::float_4(left, left, right, right);
		if (!secondOrderFilters) {
			in[0] *= -1.0f;// phase correction needed for first order filters (used to make 2nd order L-R crossover)
			in[2] *= -1.0f;
		}

		// stage 1
		simd::float_4 outS1 = b[0] * in + b[1] * xS1[0] + b[2] * xS1[1] - a[0] * yS1[0] - a[1] * yS1[1];
		xS1[1] = xS1[0];
		xS1[0] = in;
		yS1[1] = yS1[0];
		yS1[0] = outS1;

		// stage 2 (outS1 used as in)
		simd::float_4 outS2 = b[0] * outS1 + b[1] * xS2[0] + b[2] * xS2[1] - a[0] * yS2[0] - a[1] * yS2[1];
		xS2[1] = xS2[0];
		xS2[0] = outS1;
		yS2[1] = yS2[0];
		yS2[0] = outS2;

		return outS2;
	}
};
