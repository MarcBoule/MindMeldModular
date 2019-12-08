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


static const float trackVuMaxLinearGain = 2.0f;// has to be 2.0f if linked with the Track VU scaling used in MixMaster's panel
static const int trackVuScalingExponent = 3;// has to be 3 if linked with the Track VU scaling used in MixMaster's panel

static const std::string bandNames[4] = {"LF", "LMF", "HMF", "HF"};

static const bool DEFAULT_active = false;
static const bool DEFAULT_bandActive = true;
static constexpr float DEFAULT_freq[4] = {100.0f, 1000.0f, 2000.0f, 10000.0f};// Hz
static const float DEFAULT_gain = 0.0f;// dB
static const float DEFAULT_q = 3.0f;
static const bool DEFAULT_lowPeak = false;
static const bool DEFAULT_highPeak = false;
static const float DEFAULT_trackGain = 0.0f;// dB


class TrackEq {
	float sampleRate;
	
	// need saving
	bool active;
	bool bandActive[4];// frequency band's eq is active, one for LF, LMF, HMF, HF
	float freq[4];// in Hz to match params
	float gain[4];// in dB to match params, is converted to linear before pushing params to eqs
	float q[4];
	bool lowPeak;// LF is peak when true (false is lowshelf)
	bool highPeak;// HF is peak when true (false is highshelf)
	float trackGain;// in dB to match params, is converted to linear before applying to post

	// dependants
	QuattroBiQuad::Type bandTypes[4]; 
	QuattroBiQuad eqs[2];
	
	
	public:
	
	void init(float _sampleRate) {
		sampleRate = _sampleRate;
		
		// need saving
		setActive(DEFAULT_active);
		for (int i = 0; i < 4; i++) {
			setBandActive(i, DEFAULT_bandActive);
			setFreq(i, DEFAULT_freq[i]);
			setGain(i, DEFAULT_gain);
			setQ(i, DEFAULT_q);
		}
		setLowPeak(DEFAULT_lowPeak);
		setHighPeak(DEFAULT_highPeak);
		trackGain = DEFAULT_trackGain;
		
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
	float getTrackGain() {return trackGain;}
	
	void setActive(bool _active) {
		active = _active;
		pushAllParametersToEqs();
	}
	void setBandActive(int b, bool _bandActive) {
		bandActive[b] = _bandActive;
		pushBandParametersToEqs(b);
	}
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
	void setTrackGain(float _trackGain) {
		trackGain = _trackGain;
	}
	
	void updateSampleRate(float _sampleRate) {
		sampleRate = _sampleRate;
		pushAllParametersToEqs();
	}		
	bool isNonDefaultState() {
		for (int b = 0; b < 4; b++) {
			if (bandActive[b] != DEFAULT_bandActive) return true;
			if (freq[b] != DEFAULT_freq[b]) return true;
			if (gain[b] != DEFAULT_gain) return true;
			if (q[b] != DEFAULT_q) return true;
		}
		if (lowPeak != DEFAULT_lowPeak) return true;
		if (highPeak != DEFAULT_highPeak) return true;
		if (trackGain != DEFAULT_trackGain) return true;
		return false;
	}
	void process(float* out, float inL, float inR) {
		float linearTrackGain = std::pow(10.0f, trackGain / 20.0f);
		out[0] = eqs[0].process(inL) * linearTrackGain;
		out[1] = eqs[1].process(inR) * linearTrackGain;
	}
	
	
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
		float linearGain = (active && bandActive[b]) ? std::pow(10.0f, gain[b] / 20.0f) : 1.0f;
		float normalizedFreq = std::min(0.5f, freq[b] / sampleRate);
		eqs[0].setParameters(bandTypes[b], b, normalizedFreq, linearGain, q[b]);
		eqs[1].setParameters(bandTypes[b], b, normalizedFreq, linearGain, q[b]);
	}
};


#endif