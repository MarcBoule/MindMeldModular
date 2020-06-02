//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//Butterworth low/high pass filters
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#ifndef IM_BUTTERWORTHFILTERS_HPP
#define IM_BUTTERWORTHFILTERS_HPP

#include "FirstOrderFilter.hpp"


class ButterworthSecondOrder {
	float b[3];// coefficients b0, b1 and b2
	float a[3 - 1];// coefficients a1 and a2
	float x[3 - 1];
	float y[3 - 1];
	float midCoef = M_SQRT2;
	
	public:
	
	void setMidCoef(float _midCoef) {
		midCoef = _midCoef;
	}
	
	void reset() {
		for (int i = 0; i < 2; i++) {
			x[i] = 0.0f;
			y[i] = 0.0f;
		}
	}

	void setParameters(bool isHighPass, float nfc) {// normalized freq
		// nfc: normalized cutoff frequency (cutoff frequency / sample rate), must be > 0
		// freq pre-warping with inclusion of M_PI factor; 
		//   avoid tan() if fc is low (< 1102.5 Hz @ 44.1 kHz, since error at this freq is 2 Hz)
		float nfcw = nfc < 0.025f ? M_PI * nfc : std::tan(M_PI * std::min(0.499f, nfc));

		// denominator coefficients (same for both LPF and HPF)
		float acst = nfcw * nfcw + nfcw * midCoef + 1.0f;
		a[0] = 2.0f * (nfcw * nfcw - 1.0f) / acst;
		a[1] = (nfcw * nfcw - nfcw * midCoef + 1.0f) / acst;
		
		// numerator coefficients
		float hbcst = 1.0f / acst;
		float lbcst = hbcst * nfcw * nfcw;			
		b[0] = isHighPass ? hbcst : lbcst;
		b[1] = (isHighPass ? -hbcst : lbcst) * 2.0f;
		b[2] = b[0];
	}
	
	float process(float in) {
		float out = b[0] * in + b[1] * x[0] + b[2] * x[1] - a[0] * y[0] - a[1] * y[1];
		x[1] = x[0];
		x[0] = in;
		y[1] = y[0];
		y[0] = out;
		return out;
	}
};


class ButterworthThirdOrder {
	FirstOrderFilter f1;
	ButterworthSecondOrder f2;
	
	public:
	
	ButterworthThirdOrder() {
		f2.setMidCoef(1.0f);
	}
	
	void reset() {
		f1.reset();
		f2.reset();
	}
	
	void setParameters(bool isHighPass, float nfc) {// normalized freq
		f1.setParameters(isHighPass, nfc);
		f2.setParameters(isHighPass, nfc);
	}
	
	float process(float in) {
		return f2.process(f1.process(in));
	}
};


#endif
