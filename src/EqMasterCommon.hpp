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



enum EqParamIds {
	TRACK_PARAM,
	TRACK_ACTIVE_PARAM,
	TRACK_GAIN_PARAM,
	ENUMS(FREQ_ACTIVE_PARAMS, 4),
	ENUMS(FREQ_PARAMS, 4),
	ENUMS(GAIN_PARAMS, 4),
	ENUMS(Q_PARAMS, 4),
	LOW_PEAK_PARAM,
	HIGH_PEAK_PARAM,
	GLOBAL_BYPASS_PARAM,
	NUM_EQ_PARAMS
};


static const float trackVuMaxLinearGain = 2.0f;// has to be 2.0f if linked with the Track VU scaling used in MixMaster's panel
static const int trackVuScalingExponent = 3;// has to be 3 if linked with the Track VU scaling used in MixMaster's panel

static const std::string bandNames[4] = {"LF", "LMF", "HMF", "HF"};

static const bool DEFAULT_trackActive = true;
static const bool DEFAULT_bandActive = 1.0f;
static constexpr float DEFAULT_freq[4] = {100.0f, 1000.0f, 2000.0f, 10000.0f};// Hz
static const float DEFAULT_gain = 0.0f;// dB
static const float DEFAULT_q = 3.0f;
static const bool DEFAULT_lowPeak = false;
static const bool DEFAULT_highPeak = false;
static const float DEFAULT_trackGain = 0.0f;// dB

static const int FFT_N = 2048;// must be a multiple of 32


union PackedBytes4 {
	int32_t cc1;
	int8_t cc4[4];
};

class TrackEq {
	static constexpr float antipopSlewDb = 200.0f;// calibrated to properly slew a dB float in the range -20.0f to 20.0f for antipop
	float sampleRate;
	float sampleTime;
	
	// need saving
	bool trackActive;
	simd::float_4 bandActive;// 0.0f or 1.0f values: frequency band's eq is active, one for LF, LMF, HMF, HF
	simd::float_4 freq;// in Hz to match params
	simd::float_4 gain;// in dB to match params, is converted to linear before pushing params to eqs
	float q[4];
	bool lowPeak;// LF is peak when true (false is lowshelf)
	bool highPeak;// HF is peak when true (false is highshelf)
	float trackGain;// in dB to match params, is converted to linear before applying to post

	// dependants
	QuattroBiQuad::Type bandTypes[4]; 
	QuattroBiQuad eqs;
	dsp::TSlewLimiter<simd::float_4> gainSlewers;// in dB
	dsp::SlewLimiter trackGainSlewer;// in dB
	
	public:
	
	void init(float _sampleRate) {
		sampleRate = _sampleRate;
		sampleTime = 1.0f / sampleRate;
		
		// need saving
		setTrackActive(DEFAULT_trackActive);
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
		eqs.reset();
		gainSlewers.setRiseFall(simd::float_4(antipopSlewDb), simd::float_4(antipopSlewDb)); // slew rate is in input-units per second (ex: V/s)
		gainSlewers.reset();
		trackGainSlewer.setRiseFall(antipopSlewDb, antipopSlewDb);
		trackGainSlewer.reset();
		pushAllParametersToEqs();
	}

	bool getTrackActive() {return trackActive;}
	float getBandActive(int b) {return bandActive[b];}
	float getFreq(int b) {return freq[b];}
	float getGain(int b) {return gain[b];}
	float getQ(int b) {return q[b];}
	float getLowPeak() {return lowPeak;}
	float getHighPeak() {return highPeak;}
	float getTrackGain() {return trackGain;}
	QuattroBiQuad::Type getBandType(int b) {return bandTypes[b];}
	float getSampleRate() {return sampleRate;}
	
	void setTrackActive(bool _trackActive) {
		trackActive = _trackActive;
		pushAllParametersToEqs();
	}
	void setBandActive(int b, float _bandActive) {
		bandActive[b] = _bandActive;
		pushBandParametersToEqs(b);
	}
	void setFreq(int b, float _freq) {
		freq[b] = _freq;
		pushBandParametersToEqs(b);
	}
	void setGain(int b, float _gain, bool pushToEq = false) {
		gain[b] = _gain;
		if (pushToEq) {// by default gains are not pushed, since they are done in process() because of gainSlewers
			pushBandParametersToEqs(b);
		}
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
	
	void copyFrom(TrackEq* srcTrack) {
		// need saving
		setTrackActive(srcTrack->trackActive);
		for (int i = 0; i < 4; i++) {
			setBandActive(i, srcTrack->bandActive[i]);
			setFreq(i, srcTrack->freq[i]);
			setGain(i, srcTrack->gain[i]);
			setQ(i, srcTrack->q[i]);
		}
		setLowPeak(srcTrack->lowPeak);
		setHighPeak(srcTrack->highPeak);
		trackGain = srcTrack->trackGain;
		
		// dependants
		initBandTypes();
		//eqs.reset();
		pushAllParametersToEqs();		
	}
	
	
	void updateSampleRate(float _sampleRate) {
		sampleRate = _sampleRate;
		sampleTime = 1.0f / sampleRate;
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
	void process(float* out, float* in, bool globalEnable) {
		
		simd::float_4 newGain;// in dB
		if (trackActive && globalEnable) {
			newGain = bandActive * gain;
		}		
		else {
			newGain = 0.0f;
		}
		if (movemask(newGain == gainSlewers.out) != 0xF) {// movemask returns 0xF when 4 floats are equal
			gainSlewers.process(sampleTime, newGain);
			
			simd::float_4 linearGain = simd::pow(10.0f, gainSlewers.out / 20.0f);
			simd::float_4 normalizedFreq = simd::fmin(0.5f, freq / sampleRate);
			eqs.setParameters(0, bandTypes[0], normalizedFreq[0], linearGain[0], q[0]);
			eqs.setParameters(1, bandTypes[1], normalizedFreq[1], linearGain[1], q[1]);
			eqs.setParameters(2, bandTypes[2], normalizedFreq[2], linearGain[2], q[2]);
			eqs.setParameters(3, bandTypes[3], normalizedFreq[3], linearGain[3], q[3]);
		}
				
		eqs.process(out, in); 
		
		if (trackGain != trackGainSlewer.out) {
			trackGainSlewer.process(sampleTime, trackGain);
		}
		float linearTrackGain = std::pow(10.0f, trackGainSlewer.out / 20.0f);
		out[0] *= linearTrackGain;
		out[1] *= linearTrackGain;
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
		float linearGain = (trackActive && (bandActive[b] > 0.5f)) ? std::pow(10.0f, gainSlewers.out[b] / 20.0f) : 1.0f;
		float normalizedFreq = std::min(0.5f, freq[b] / sampleRate);
		eqs.setParameters(b, bandTypes[b], normalizedFreq, linearGain, q[b]);
	}
};


#endif