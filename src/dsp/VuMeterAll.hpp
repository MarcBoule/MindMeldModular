//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//Based on code from the Fundamental plugins by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************



// Code below adapted from Andrew Belt's dsp::VuMeter2 struct in VCV Rack's source code

enum VuIds {VU_PEAK_L, VU_PEAK_R, VU_RMS_L, VU_RMS_R};

struct VuMeterAllDual {
	float vuValues[4];// organized according to VuIds
	static constexpr float lambda = 30.f;// Inverse time constant in 1/seconds

	void reset() {
		for (int i = 0; i < 4; i++) {
			vuValues[i] = 0.0f;
		}
	}

	void process(float deltaTime, const float *values) {// L and R
		for (int i = 0; i < 2; i++) {
			// RMS
			float valueSquared = std::pow(values[i], 2);
			vuValues[VU_RMS_L + i] += (valueSquared - vuValues[VU_RMS_L + i]) * lambda * deltaTime;

			// PEAK
			float valueAbs = std::fabs(values[i]);
			if (valueAbs >= vuValues[VU_PEAK_L + i]) {
				vuValues[VU_PEAK_L + i] = valueAbs;
			}
			else {
				vuValues[VU_PEAK_L + i] += (valueAbs - vuValues[VU_PEAK_L + i]) * lambda * deltaTime;
			}
		}
	}
	
	float getPeak(int chan) {// chan0 is L, chan1 is R
		return vuValues[VU_PEAK_L + chan];
	}
	float getRms(int chan) {// chan0 is L, chan1 is R
		return std::sqrt(vuValues[VU_RMS_L + chan]);
	}
};
