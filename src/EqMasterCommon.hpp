//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************

#ifndef MMM_EQ_COMMON_HPP
#define MMM_EQ_COMMON_HPP


#include "MindMeldModular.hpp"
#include "dsp/VuMeterAll.hpp"


struct TrackSettings {
	bool active;
	bool freqActive[4];// frequency part is active, one for LF, LMF, HMF, HF
	float freq[4];
	float gain[4];
	float q[4];
	bool lowBP;// LF is band pass/cut when true (false is LPF)
	bool highBP;// HF is band pass/cut when true (false is HPF)
	// don't forget to also update to/from Json when adding to this struct
	
	void init() {
		active = false;
		for (int i = 0; i < 4; i++) {
			freqActive[i] = false;
			freq[i] = 0.5f;
			gain[i] = 0.5f;
			q[i] = 0.5f;
		}
		lowBP = false;
		highBP = false;
	}
};


#endif