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


struct ExpansionInterface {
	enum MotherFromExpIds { // for messages from expander to mother
		ENUMS(MFE_TRACK_CVS, 16 * 4), // room for 4 poly cables
		MFE_TRACK_CVS_CONNECTED,// only 4 lsbits used, so float number is from 0 to 15 and can be easily cast to uint32_t in eqmaster
		MFE_TRACK_CVS_INDEX6,
		MFE_TRACK_ENABLE, // one of the 24 enable cvs
		MFE_TRACK_ENABLE_INDEX,// 
		MFE_NUM_VALUES
	};	
};

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
enum SpecModes {SPEC_NONE, SPEC_PRE, SPEC_POST, SPEC_FREEZE};

static const bool DEFAULT_trackActive = true;
static const bool DEFAULT_bandActive = 1.0f;
// static constexpr float DEFAULT_freq[4] = {100.0f, 	350.0f, 	1600.0f, 	4500.0f};// Hz
static constexpr float DEFAULT_logFreq[4] = {2.0f, 		2.544068f, 	3.20412f, 	3.6532125f};// log(Hz)
// static const simd::float_4 MIN_freq(20.0f, 60.0f, 500.0f, 1000.0f);// Hz
static const simd::float_4 MIN_logFreq(1.30103f, 1.77815125f, 2.69897f, 3.0f);// log(Hz)
// static const simd::float_4 MAX_freq(500.0f, 2000.0f, 5000.0f, 20000.0f);// Hz
static const simd::float_4 MAX_logFreq(2.69897f, 3.30103f, 3.69897f, 4.30103f);// log(Hz)
static const float DEFAULT_gain = 0.0f;// dB
static const float DEFAULT_q[4] = {1.0f, 3.0f, 3.0f, 1.0f};
static const bool DEFAULT_lowPeak = false;
static const bool DEFAULT_highPeak = false;
static const float DEFAULT_trackGain = 0.0f;// dB

static const int FFT_N = 2048;// must be a multiple of 32 (if adjust this, should adjust left side spectrum cheating when drawing which was setup with 2048)
static const int FFT_N_2 = FFT_N >> 1;

// static constexpr float minFreq = 20.0f;// update minLogFreq when changing this !
static constexpr float minLogFreq = 1.30103f;// std::log10(minFreq);// commented for old compilers
// static constexpr float maxFreq = 22000.0f;// update maxLogFreq when changing this !
static constexpr float maxLogFreq = 4.3424227f;// std::log10(maxFreq);// commented for old compilers

static const float eqCurveWidth = 107.685f * SVG_DPI / MM_PER_IN;// mm2px()


static const NVGcolor SCHEME_GRAY = nvgRGB(130, 130, 130);



union PackedBytes4 {
	int32_t cc1;
	int8_t cc4[4];
};

class TrackEq {
	static constexpr float antipopSlewLogHz = 12.0f;// calibrated to properly slew a log(Hz) float in the rough range 1.3f to 4.3f (but less since freq knobs not full spectrum)
	static constexpr float antipopSlewDb = 200.0f;// calibrated to properly slew a dB float in the range -20.0f to 20.0f for antipop
	int trackNum;
	float sampleRate;
	float sampleTime;
	uint32_t *cvConnected;
	
	// automatically managed internally by member functions
	int dirty;// 4 bits, one for each band (automatically managed by member methods, no need to handle in init() and copyFrom())
	QuattroBiQuad::Type bandTypes[4]; // only [0] and [3] are dependants, [1] and [2] are set to their permanent values in init()
	
	// need saving
	bool trackActive;
	simd::float_4 bandActive;// 0.0f or 1.0f values: frequency band's eq is active, one for LF, LMF, HMF, HF
	simd::float_4 freq;// in log(Hz) to match params, converted to scaled linear freq before pushing params to eq
	simd::float_4 gain;// in dB to match params, is converted to linear before pushing params to eqs
	simd::float_4 q;
	bool lowPeak;// LF is peak when true (false is lowshelf)
	bool highPeak;// HF is peak when true (false is highshelf)
	float trackGain;// in dB to match params, is converted to linear before applying to post; dirty does not apply to this

	// don't need saving
	simd::float_4 freqCv;// adding-type cvs
	simd::float_4 gainCv;// adding-type cvs
	simd::float_4 qCv;// adding-type cvs

	// dependants
	QuattroBiQuad eqs;
	dsp::TSlewLimiter<simd::float_4> freqSlewers;// in log(Hz)
	dsp::TSlewLimiter<simd::float_4> gainSlewers;// in dB
	dsp::SlewLimiter trackGainSlewer;// in dB
	
	
	public:
	
	TrackEq() {
		lowPeak = !DEFAULT_lowPeak;// to force bandTypes[0] to be set when first init() will call setLowPeak()
		bandTypes[1] = QuattroBiQuad::PEAK;
		bandTypes[2] = QuattroBiQuad::PEAK;
		highPeak = !DEFAULT_highPeak;// to force bandTypes[3] to be set when first init() will call setLowPeak()
		freqSlewers.setRiseFall(simd::float_4(antipopSlewLogHz), simd::float_4(antipopSlewLogHz)); // slew rate is in input-units per second (ex: V/s)
		gainSlewers.setRiseFall(simd::float_4(antipopSlewDb), simd::float_4(antipopSlewDb)); // slew rate is in input-units per second (ex: V/s)
		trackGainSlewer.setRiseFall(antipopSlewDb, antipopSlewDb);
	}
	
	void init(int _trackNum, float _sampleRate, uint32_t *_cvConnected) {
		trackNum = _trackNum;
		sampleRate = _sampleRate;
		sampleTime = 1.0f / sampleRate;
		cvConnected = _cvConnected;
		
		// need saving
		setTrackActive(DEFAULT_trackActive);
		for (int i = 0; i < 4; i++) {
			setBandActive(i, DEFAULT_bandActive);
			setFreq(i, DEFAULT_logFreq[i]);
			setGain(i, DEFAULT_gain);
			setQ(i, DEFAULT_q[i]);
		}
		setLowPeak(DEFAULT_lowPeak);
		setHighPeak(DEFAULT_highPeak);
		setTrackGain(DEFAULT_trackGain);
		
		// don't need saving
		freqCv = 0.0f;
		gainCv = 0.0f;
		qCv = 0.0f;
		
		// dependants
		eqs.reset();
		freqSlewers.reset();
		gainSlewers.reset();
		trackGainSlewer.reset();
	}

	bool getTrackActive() {return trackActive;}
	float getBandActive(int b) {return bandActive[b];}
	float getFreq(int b) {return freq[b];}
	simd::float_4 getFreqWithCvVec(bool _cvConnected) {
		if (!_cvConnected) {return freq;}
		return simd::clamp(freq + freqCv * 0.1f * (MAX_logFreq - MIN_logFreq), MIN_logFreq, MAX_logFreq);
	}
	float getGain(int b) {return gain[b];}
	simd::float_4 getGainWithCvVec(bool _cvConnected) {
		if (!_cvConnected) {return gain;}
		return simd::clamp(gain + gainCv * 4.0f, -20.0f, 20.0f);
	}
	float getQ(int b) {return q[b];}
	simd::float_4 getQWithCvVec(bool _cvConnected) {
		if (!_cvConnected) {return q;}
		return simd::clamp(q + qCv * 0.1f * (20.0f - 0.3f), 0.3f, 20.0f);
	}
	float getLowPeak() {return lowPeak;}
	float getHighPeak() {return highPeak;}
	float getTrackGain() {return trackGain;}
	QuattroBiQuad::Type getBandType(int b) {return bandTypes[b];}
	float getSampleRate() {return sampleRate;}
	bool getCvConnected() {return (*cvConnected & (1 << trackNum)) != 0;}
	
	void setTrackActive(bool _trackActive) {
		if (trackActive != _trackActive) {
			trackActive = _trackActive;
			dirty = 0xF;
		}
	}
	void setBandActive(int b, float _bandActive) {
		if (bandActive[b] != _bandActive) {
			bandActive[b] = _bandActive;
			dirty |= (1 << b);
		}
	}
	void setFreq(int b, float _freq) {
		if (freq[b] != _freq) {
			freq[b] = _freq;
			dirty |= (1 << b);
		}
	}
	void setGain(int b, float _gain) {
		if (gain[b] != _gain) {
			gain[b] = _gain;
			dirty |= (1 << b);
		}
	}
	void setQ(int b, float _q) {
		if (q[b] != _q) {
			q[b] = _q;
			dirty |= (1 << b);
		}
	}
	void setLowPeak(bool _lowPeak) {
		if (lowPeak != _lowPeak) {
			lowPeak = _lowPeak;
			bandTypes[0] = lowPeak ? QuattroBiQuad::PEAK : QuattroBiQuad::LOWSHELF;
			dirty |= (1 << 0);
		}
	}
	void setHighPeak(bool _highPeak) {
		if (highPeak != _highPeak) {
			highPeak = _highPeak;
			bandTypes[3] = highPeak ? QuattroBiQuad::PEAK : QuattroBiQuad::HIGHSHELF;
			dirty |= (1 << 3);
		}
	}
	void setTrackGain(float _trackGain) {
		trackGain = _trackGain;
		// dirty does not apply to track gain
	}
	void setFreqCv(int b, float _freqCv) {
		if (freqCv[b] != _freqCv) {
			freqCv[b] = _freqCv;
			dirty |= (1 << b);
		}
	}
	void setGainCv(int b, float _gainCv) {
		if (gainCv[b] != _gainCv) {
			gainCv[b] = _gainCv;
			dirty |= (1 << b);
		}
	}
	void setQCv(int b, float _qCv) {
		if (qCv[b] != _qCv) {
			qCv[b] = _qCv;
			dirty |= (1 << b);
		}
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
		setTrackGain(srcTrack->getTrackGain());
	}
	
	
	void updateSampleRate(float _sampleRate) {
		sampleRate = _sampleRate;
		sampleTime = 1.0f / sampleRate;
		dirty = 0xF;
	}		
	bool isNonDefaultState() {
		for (int b = 0; b < 4; b++) {
			if (bandActive[b] != DEFAULT_bandActive) return true;
			if (freq[b] != DEFAULT_logFreq[b]) return true;
			if (gain[b] != DEFAULT_gain) return true;
			if (q[b] != DEFAULT_q[b]) return true;
		}
		if (lowPeak != DEFAULT_lowPeak) return true;
		if (highPeak != DEFAULT_highPeak) return true;
		if (trackGain != DEFAULT_trackGain) return true;
		return false;
	}
	void process(float* out, float* in, bool globalEnable) {
		bool _cvConnected = getCvConnected();
		
		// freq slewers with freq cvs
		simd::float_4 newFreq = getFreqWithCvVec(_cvConnected);// in log(Hz)
		int freqSlewersComparisonMask = movemask(newFreq == freqSlewers.out);
		if (freqSlewersComparisonMask != 0xF) {// movemask returns 0xF when 4 floats are equal
			freqSlewers.process(sampleTime, newFreq);
			dirty |= ~freqSlewersComparisonMask;
		}
		
		// gain slewers with gain cvs
		simd::float_4 newGain;// in dB
		if (trackActive && globalEnable) {
			newGain = bandActive * getGainWithCvVec(_cvConnected);
		}		
		else {
			newGain = 0.0f;
		}
		int gainSlewersComparisonMask = movemask(newGain == gainSlewers.out);
		if (gainSlewersComparisonMask != 0xF) {// movemask returns 0xF when 4 floats are equal
			gainSlewers.process(sampleTime, newGain);
			dirty |= ~gainSlewersComparisonMask;
		}
		
		// update eq parameters according to dirty flags
		if (dirty != 0) {
			simd::float_4 normalizedFreq = simd::fmin(0.5f, simd::pow(10.0f, freqSlewers.out) / sampleRate);
			simd::float_4 linearGain = simd::pow(10.0f, gainSlewers.out / 20.0f);
			simd::float_4 qWithCv = getQWithCvVec(_cvConnected);
			for (int b = 0; b < 4; b++) {
				if ((dirty & (1 << b)) != 0) {
					eqs.setParameters(b, bandTypes[b], normalizedFreq[b], linearGain[b], qWithCv[b]);
				}
			}
		}
		dirty = 0x0;		
				
		// perform equalization		
		eqs.process(out, in);
		
		// apply track gain (with slewer)
		if (trackGain != trackGainSlewer.out) {
			trackGainSlewer.process(sampleTime, trackGain);
		}
		float linearTrackGain = std::pow(10.0f, trackGainSlewer.out / 20.0f);
		out[0] *= linearTrackGain;
		out[1] *= linearTrackGain;
	}
};


#endif