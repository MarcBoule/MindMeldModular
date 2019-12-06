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
static const bool DEFAULT_bandActive = false;
static const float DEFAULT_freq = 0.1f;
static const float DEFAULT_gain = 1.0f;
static const float DEFAULT_q = 0.707f;
static const bool DEFAULT_lowPeak = false;
static const bool DEFAULT_highPeak = false;


class TrackEq {
	
	// need saving
	bool active;
	bool bandActive[4];// frequency band's eq is active, one for LF, LMF, HMF, HF
	float freq[4];
	float gain[4];// linear gain, should be from 0.1 to 10 (-20dB to +20dB). gain = 10^(dB/20)
	float q[4];// 0.5 to 15 ?
	bool lowPeak;// LF is peak when true (false is lowshelf)
	bool highPeak;// HF is peak when true (false is highshelf)

	// dependants
	QuattroBiQuad::Type bandTypes[4]; 
	QuattroBiQuad eqs[2];
	
	
	public:
	
	void init() {
		// need saving
		setActive(DEFAULT_active);
		for (int i = 0; i < 4; i++) {
			setBandActive(i, DEFAULT_bandActive);
			setFreq(i, DEFAULT_freq);
			setGain(i, DEFAULT_gain);
			setQ(i, DEFAULT_q);
		}
		setLowPeak(DEFAULT_lowPeak);
		setHighPeak(DEFAULT_highPeak);
		
		// dependants
		initBandTypes();
		eqs[0].reset();
		eqs[1].reset();
		pushAllParametersToEqs();
	}

	bool getActive() {return active;}
	bool getBandActive(int b) {return bandActive[b];}
	float getFreq(int b) {return freq[b];}
	float getGain(int b) {return gain[b];}
	float getQ(int b) {return q[b];}
	float getLowPeak() {return lowPeak;}
	float getHighPeak() {return highPeak;}
	
	void setActive(bool _active) {active = _active;}
	void setBandActive(int b, bool _bandActive) {bandActive[b] = _bandActive;}
	void setFreq(int b, float _freq) {
		freq[b] = _freq;
		pushBandParametersToEqs(b);
	}
	void setGain(int b, float _gain) {
		gain[b] = _gain;
		pushBandParametersToEqs(b);
	}
	void setQ(int b, float _q) {
		q[b] = _q;
		pushBandParametersToEqs(b);
	}
	void setLowPeak(bool _lowPeak) {
		lowPeak = _lowPeak;
		setBand0Type();
		pushBandParametersToEqs(0);
	}
	void setHighPeak(bool _highPeak) {
		highPeak = _highPeak;
		setBand3Type();
		pushBandParametersToEqs(3);
	}
	
	float processL(float inL) {return eqs[0].process(inL);}
	float processR(float inR) {return eqs[1].process(inR);}
	
	
	private: 
	
	void initBandTypes() {// make sure params are pushed afterwards (this method does not do it)
		setBand0Type();
		bandTypes[1] = QuattroBiQuad::PEAK;
		bandTypes[2] = QuattroBiQuad::PEAK;
		setBand3Type();
	}
	void setBand0Type() {// make sure params are pushed afterwards (this method does not do it)
		bandTypes[0] = lowPeak ? QuattroBiQuad::PEAK : QuattroBiQuad::LOWSHELF;
	}
	void setBand3Type() {// make sure params are pushed afterwards (this method does not do it)
		bandTypes[3] = highPeak ? QuattroBiQuad::PEAK : QuattroBiQuad::HIGHSHELF;
	}

	void pushAllParametersToEqs() {
		for (int b = 0; b < 4; b++) {	
			pushBandParametersToEqs(b);
		}
	}
	void pushBandParametersToEqs(int b) {
		eqs[0].setParameters(bandTypes[b], b, freq[b], gain[b], q[b]);
		eqs[1].setParameters(bandTypes[b], b, freq[b], gain[b], q[b]);
	}
};


#endif