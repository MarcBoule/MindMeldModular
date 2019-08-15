//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//Based on code from the Fundamental plugins by Andrew Belt 
//See ./LICENSE.txt for all licenses
//***********************************************************************************************



// Code below adapted from Andrew Belt's dsp::VuMeter2 struct in VCV Rack's source code

class VuMeterAll {
	float vPeak = 0.f;
	float vRms = 0.f;

	/** Inverse time constant in 1/seconds */
	float lambda = 30.f;


	public:
	
	int vuColorTheme; // 0 to numthemes - 1; (when per-track choice)

	void reset() {
		vPeak = 0.f;
		vRms = 0.f;
	}

	void process(float deltaTime, const float value) {
		// RMS
		float valueSquared = std::pow(value, 2);
		vRms += (valueSquared - vRms) * lambda * deltaTime;

		// PEAK
		float valueAbs = std::fabs(value);
		if (valueAbs >= vPeak) {
			vPeak = valueAbs;
		}
		else {
			vPeak += (valueAbs - vPeak) * lambda * deltaTime;
		}
	}

	float getPeak() {
		return vPeak;
	}
	
	float getRms() {
		return std::sqrt(vRms);
	}
};
