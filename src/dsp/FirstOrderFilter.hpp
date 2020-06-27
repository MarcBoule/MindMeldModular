//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//A first-order low/high pass filter
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#pragma once


class FirstOrderCoefficients {
	protected: 

	float b[2];// coefficients b0, b1
	float a;// coefficient a1
	
	
	public: 
	
	void setParameters(bool isHighPass, float nfc) {// normalized freq
		// nfc: normalized cutoff frequency (cutoff frequency / sample rate), must be > 0
		// freq pre-warping with inclusion of M_PI factor; 
		//   avoid tan() if fc is low (< 1102.5 Hz @ 44.1 kHz, since error at this freq is 2 Hz)
		float nfcw = nfc < 0.025f ? float(M_PI) * nfc : std::tan(float(M_PI) * std::min(0.499f, nfc));
		
		// denominator coefficient (same for both LPF and HPF)
		a = (nfcw - 1.0f) / (nfcw + 1.0f);
		
		// numerator coefficients
		float hbcst = 1.0f / (1.0f + nfcw);
		float lbcst = 1.0f - hbcst;// equivalent to: hbcst * nfcw;
		b[0] = isHighPass ? hbcst : lbcst;
		b[1] = isHighPass ? -hbcst : lbcst;
	}		
};


class FirstOrderFilter : public FirstOrderCoefficients {
	float x;
	float y;
	
	public: 
		
	void reset() {
		x = 0.0f;
		y = 0.0f;
	}

	float process(float in) {
		y = b[0] * in + b[1] * x - a * y;
		x = in;
		return y;
	}
};


class FirstOrderStereoFilter : public FirstOrderCoefficients {
	float x[2];
	float y[2];
	
	public: 
		
	void reset() {
		x[0] = 0.0f;
		x[1] = 0.0f;
		y[0] = 0.0f;
		y[1] = 0.0f;
	}
	
	void process(float* out, float* in) {
		y[0] = b[0] * in[0] + b[1] * x[0] - a * y[0];
		y[1] = b[0] * in[1] + b[1] * x[1] - a * y[1];
		x[0] = in[0];
		x[1] = in[1];
		out[0] = y[0];
		out[1] = y[1];
	}
};
