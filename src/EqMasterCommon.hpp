//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************

#ifndef MMM_EQ_COMMON_HPP
#define MMM_EQ_COMMON_HPP


#include "MindMeldModular.hpp"
#include "dsp/QuattroBiQuad.hpp"
#include "dsp/VuMeterAll.hpp"

static const bool DEFAULT_active = false;
static const bool DEFAULT_freqActive = false;
static const float DEFAULT_freq = 0.5f;
static const float DEFAULT_gain = 0.5f;
static const float DEFAULT_q = 0.5f;
static const bool DEFAULT_lowBP = false;
static const bool DEFAULT_highBP = false;


class TrackEq {
	bool active;
	bool freqActive[4];// frequency part is active, one for LF, LMF, HMF, HF
	float freq[4];
	float gain[4];
	float q[4];
	bool lowBP;// LF is band pass/cut when true (false is LPF)
	bool highBP;// HF is band pass/cut when true (false is HPF)
	// don't forget to also update to/from Json when adding to this struct
	
	public:
	
	void init() {
		active = DEFAULT_active;
		for (int i = 0; i < 4; i++) {
			freqActive[i] = DEFAULT_freqActive;
			freq[i] = DEFAULT_freq;
			gain[i] = DEFAULT_gain;
			q[i] = DEFAULT_q;
		}
		lowBP = DEFAULT_lowBP;
		highBP = DEFAULT_highBP;
	}
	
	bool getActive() {return active;}
	bool getFreqActive(int eqNum) {return freqActive[eqNum];}
	float getFreq(int eqNum) {return freq[eqNum];}
	float getGain(int eqNum) {return gain[eqNum];}
	float getQ(int eqNum) {return q[eqNum];}
	float getLowBP() {return lowBP;}
	float getHighBP() {return highBP;}
	
	void setActive(bool _active) {active = _active;}
	void setFreqActive(int eqNum, bool _freqActive) {freqActive[eqNum] = _freqActive;}
	void setFreq(int eqNum, float _freq) {freq[eqNum] = _freq;}
	void setGain(int eqNum, float _gain) {gain[eqNum] = _gain;}
	void setQ(int eqNum, float _q) {q[eqNum] = _q;}
	void setLowBP(bool _lowBP) {lowBP = _lowBP;}
	void setHighBP(bool _highBP) {highBP = _highBP;}
};


#endif