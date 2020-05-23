//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//See ./LICENSE.md for all licenses
//***********************************************************************************************

// Linkwitz-Riley audio crossover, based on Linkwitz-Riley filters
// https://en.wikipedia.org/wiki/Linkwitz%E2%80%93Riley_filter

// Cascaded 2nd-order butterworth filters are used when offering 4th order Linkwitz-Riley crossover
// Cascaded 1st-order filters are used when offering 2nd order Linkwitz-Riley crossover

class LinkwitzRileyCrossover {	
	bool secondOrderFilters = false;// local memory of what is in iirs		
	dsp::IIRFilter<2 + 1, 2 + 1, simd::float_4> iirStage1;// [0] = left low, left high, right low, [3] = right high
	dsp::IIRFilter<2 + 1, 2 + 1, simd::float_4> iirStage2;// idem
	
	public: 
		
	void reset() {
		iirStage1.reset();
		iirStage2.reset();
	}
	
	void setFilterCutoffs(float nfc, bool _secondOrder) {
		secondOrderFilters = _secondOrder;
		
		// nfc: normalized cutoff frequency (cutoff frequency / sample rate), must be > 0
		// freq pre-warping with inclusion of M_PI factor; 
		//   avoid tan() if fc is low (< 1102.5 Hz @ 44.1 kHz, since error at this freq is 2 Hz)
		float nfcw = nfc < 0.025f ? M_PI * nfc : std::tan(M_PI * std::min(0.499f, nfc));
		
		simd::float_4 s_a[2];// coefficients a1 and a2, where each float of float_4 is LeftLow, LeftHigh, RightLow, RightHigh
		simd::float_4 s_b[2 + 1];// coefficients b0, b1 and b2, where each float of float_4  is LeftLow, LeftHigh, RightLow, RightHigh
		if (secondOrderFilters) {	
			// denominator coefficients (same for both LPF and HPF)
			float acst = nfcw * nfcw + nfcw * (float)M_SQRT2 + 1.0f;
			s_a[0] = simd::float_4(2.0f * (nfcw * nfcw - 1.0f) / acst);
			s_a[1] = simd::float_4((nfcw * nfcw - nfcw * (float)M_SQRT2 + 1.0f) / acst);
			
			// numerator coefficients
			float hbcst = 1.0f / acst;
			float lbcst = hbcst * nfcw * nfcw;			
			s_b[0] = simd::float_4(lbcst, hbcst, lbcst, hbcst);
			s_b[1] = simd::float_4(lbcst, -hbcst, lbcst, -hbcst) * 2.0f;
			s_b[2] = s_b[0];
		}
		else {
			// denominator coefficients (same for both LPF and HPF)
			float acst = (nfcw - 1.0f) / (nfcw + 1.0f);
			s_a[0] = simd::float_4(acst);
			s_a[1] = simd::float_4(0.0f);
			
			// numerator coefficients
			float hbcst = 1.0f / (1.0f + nfcw);
			float lbcst = 1.0f - hbcst;// equivalent to: hbcst * nfcw;
			s_b[0] = simd::float_4(lbcst, hbcst, lbcst, hbcst);
			s_b[1] = simd::float_4(lbcst, -hbcst, lbcst, -hbcst);
			s_b[2] = simd::float_4(0.0f);
		}
		iirStage1.setCoefficients(s_b, s_a);
		iirStage2.setCoefficients(s_b, s_a);
	}

	simd::float_4 process(float left, float right) {
		// return [0] = left low, left high, right low, [3] = right high
		simd::float_4 src = simd::float_4(left, left, right, right);
		if (!secondOrderFilters) {
			src[0] *= -1.0f;// phase correction needed for first order filters (used to make 2nd order L-R crossover)
			src[2] *= -1.0f;
		}
		return iirStage1.process(iirStage2.process(src));
	}
};


