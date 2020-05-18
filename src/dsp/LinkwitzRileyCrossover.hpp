//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//See ./LICENSE.md for all licenses
//***********************************************************************************************



class LinkwitzRileyCrossover {	
	bool secondOrder;// local memory of what is in iirs		
	dsp::IIRFilter<2 + 1, 2 + 1, float> iirs[8];// Left low 1, low 2, Right low 1, low 2, Left high 1, high 2, Right high 1, high 2
	
	public: 
		
	void reset() {
		secondOrder = false;
		for (int i = 0; i < 8; i++) {
			iirs[i].reset();
		}	
	}
	
	void setFilterCutoffs(float nfc, bool _secondOrder) {
		secondOrder = _secondOrder;
		
		// nfc: normalized cutoff frequency (cutoff frequency / sample rate), must be > 0
		// freq pre-warping with inclusion of M_PI factor; 
		//   avoid tan() if fc is low (< 1102.5 Hz @ 44.1 kHz, since error at this freq is 2 Hz)
		float nfcw = nfc < 0.025f ? M_PI * nfc : std::tan(M_PI * std::min(0.499f, nfc));
		
		if (secondOrder) {
			// butterworth LPF and HPF filters (for 4th order Linkwitz-Riley crossover)
			// denominator coefficients (same for both LPF and HPF)
			float acst = nfcw * nfcw + nfcw * (float)M_SQRT2 + 1.0f;
			float a[2] = {2.0f * (nfcw * nfcw - 1.0f) / acst, (nfcw * nfcw - nfcw * (float)M_SQRT2 + 1.0f) / acst};
			// numerator coefficients
			float hbcst = 1.0f / acst;
			float hb[2 + 1] = {hbcst, -2.0f * hbcst, hbcst};
			float lbcst = hbcst * nfcw * nfcw;
			float lb[2 + 1] = {lbcst, 2.0f * lbcst, lbcst};
			for (int i = 0; i < 4; i++) { 
				iirs[i].setCoefficients(lb, a);
				iirs[i + 4].setCoefficients(hb, a);
			}
		}
		else {
			// first order LPF and HPF filters (for 2nd order Linkwitz-Riley crossover)
			// denominator coefficients (same for both LPF and HPF)
			float acst = (nfcw - 1.0f) / (nfcw + 1.0f);
			float a[2] = {acst, 0.0f};
			// numerator coefficients
			float hbcst = 1.0f / (1.0f + nfcw);
			float hb[2 + 1] = {hbcst, -hbcst, 0.0f};
			float lbcst = 1.0f - hbcst;// equivalent to: hbcst * nfcw;
			float lb[2 + 1] = {lbcst, lbcst, 0.0f};
			for (int i = 0; i < 4; i++) { 
				iirs[i].setCoefficients(lb, a);
				iirs[i + 4].setCoefficients(hb, a);
			}
		}
	}

	void process(float left, float right, float* dest) {
		// dest[0] = left low, left high, right low, [3] = right high
		float leftLow = left;
		float leftHigh = left;
		float rightLow = right;
		float rightHigh = right;
		if (!secondOrder) {
			leftLow *= -1.0f;
			rightLow *= -1.0f;
		}
		dest[0] = iirs[0].process(iirs[1].process(leftLow));
		dest[1] = iirs[4].process(iirs[5].process(leftHigh));
		dest[2] = iirs[2].process(iirs[3].process(rightLow));
		dest[3] = iirs[6].process(iirs[7].process(rightHigh));
	}
};

