//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.txt for all licenses
//***********************************************************************************************

#ifndef MMM_MIXER_HPP
#define MMM_MIXER_HPP


#include "MindMeldModular.hpp"
#include "dsp/Biquad.hpp"
#include "dsp/OnePole.hpp"
#include "dsp/VuMeterAll.hpp"
#include <pmmintrin.h>


enum ParamIds {
	ENUMS(TRACK_FADER_PARAMS, 16),
	ENUMS(GROUP_FADER_PARAMS, 4),
	ENUMS(TRACK_PAN_PARAMS, 16),
	ENUMS(GROUP_PAN_PARAMS, 4),
	ENUMS(TRACK_MUTE_PARAMS, 16),
	ENUMS(GROUP_MUTE_PARAMS, 4),
	ENUMS(TRACK_SOLO_PARAMS, 16),
	ENUMS(GROUP_SOLO_PARAMS, 4),// must follow TRACK_SOLO_PARAMS since code assumes contiguous
	MAIN_FADER_PARAM,
	MAIN_MUTE_PARAM,
	MAIN_DIM_PARAM,
	MAIN_MONO_PARAM,
	ENUMS(GRP_INC_PARAMS, 16),
	ENUMS(GRP_DEC_PARAMS, 16),
	ENUMS(TRACK_LOWCUT_PARAMS, 16),// NUM_PARAMS_JR's last param is on previous line
	NUM_PARAMS
}; 
static const int NUM_PARAMS_JR = TRACK_LOWCUT_PARAMS;


enum InputIds {
	ENUMS(TRACK_SIGNAL_INPUTS, 16 * 2), // Track 0: 0 = L, 1 = R, Track 1: 2 = L, 3 = R, etc...
	ENUMS(TRACK_VOL_INPUTS, 16),
	ENUMS(GROUP_VOL_INPUTS, 4),
	ENUMS(TRACK_PAN_INPUTS, 16), 
	ENUMS(GROUP_PAN_INPUTS, 4), 
	NUM_INPUTS
};


enum OutputIds {
	ENUMS(DIRECT_OUTPUTS, 4), // Track 1-8, Track 9-16, Groups and Aux
	ENUMS(MAIN_OUTPUTS, 2),
	NUM_OUTPUTS
};


enum LightIds {
	ENUMS(TRACK_HPF_LIGHTS, 16),
	ENUMS(TRACK_LPF_LIGHTS, 16),
	NUM_LIGHTS
};


//*****************************************************************************


// managed by Mixer, not by tracks (tracks read only)
struct GlobalInfo {
	
	// constants
	// none
	
	// need to save, no reset
	// none
	
	// need to save, with reset
	int panLawMono;// +0dB (no compensation),  +3 (equal power, default),  +4.5 (compromize),  +6dB (linear)
	int panLawStereo;// Stereo balance (+3dB boost since one channel lost, default),  True pan (linear redistribution but is not equal power)
	int directOutsMode;// 0 is pre-fader, 1 is post-fader
	bool nightMode;// turn off track VUs only, keep master VUs (also called "Cloaked mode")

	// no need to save, with reset
	unsigned long soloBitMask;// when = 0ul, nothing to do, when non-zero, a track must check its solo to see if it should play
	float sampleTime;

	// no need to save, no reset
	Param *paSolo;
	
	
	void construct(Param *_params) {
		paSolo = &_params[TRACK_SOLO_PARAMS];
	}
	
	
	void onReset() {
		panLawMono = 1;
		panLawStereo = 0;
		directOutsMode = 1;// post should be default
		nightMode = false;
		resetNonJson();
	}
	
	void resetNonJson() {
		updateSoloBitMask();
		sampleTime = APP->engine->getSampleTime();
	}
	
	void updateSoloBitMask() {
		unsigned long newSoloBitMask = 0ul;// separate variable so no glitch generated
		for (unsigned long i = 0; i < (16 + 4); i++) {
			if (paSolo[i].getValue() > 0.5) {
				newSoloBitMask |= (1 << i);
			}
		}
		soloBitMask = newSoloBitMask;
	}
	void updateSoloBit(unsigned int trkOrGrp) {
		if (paSolo[trkOrGrp].getValue() > 0.5f) 
			soloBitMask |= (1 << trkOrGrp);
		else
			soloBitMask &= ~(1 << trkOrGrp);
	}
	
	void onRandomize(bool editingSequence) {
		
	}
	
	void dataToJson(json_t *rootJ) {
		// panLawMono 
		json_object_set_new(rootJ, "panLawMono", json_integer(panLawMono));

		// panLawStereo
		json_object_set_new(rootJ, "panLawStereo", json_integer(panLawStereo));

		// directOutsMode
		json_object_set_new(rootJ, "directOutsMode", json_integer(directOutsMode));
		
		// nightMode
		json_object_set_new(rootJ, "nightMode", json_boolean(nightMode));
	}
	
	void dataFromJson(json_t *rootJ) {
		// panLawMono
		json_t *panLawMonoJ = json_object_get(rootJ, "panLawMono");
		if (panLawMonoJ)
			panLawMono = json_integer_value(panLawMonoJ);
		
		// panLawStereo
		json_t *panLawStereoJ = json_object_get(rootJ, "panLawStereo");
		if (panLawStereoJ)
			panLawStereo = json_integer_value(panLawStereoJ);
		
		// directOutsMode
		json_t *directOutsModeJ = json_object_get(rootJ, "directOutsMode");
		if (directOutsModeJ)
			directOutsMode = json_integer_value(directOutsModeJ);
		
		// nightMode
		json_t *nightModeJ = json_object_get(rootJ, "nightMode");
		if (nightModeJ)
			nightMode = json_is_true(nightModeJ);

		// extern must call resetNonJson()
	}
};// struct GlobalInfo


//*****************************************************************************


struct MixerMaster {
	// Constants
	static constexpr float masterFaderScalingExponent = 3.0f; 
	static constexpr float masterFaderMaxLinearGain = 2.0f;
	
	// need to save, no reset
	// none
	
	// need to save, with reset
	// none

	// no need to save, with reset
	float slowGain;// gain that we don't want to computer each sample (gainAdjust * fader)
	dsp::TSlewLimiter<simd::float_4> gainSlewers;
	VuMeterAll vu[2];// use mix[0..1]

	// no need to save, no reset
	GlobalInfo *gInfo;
	Param *params;


	void construct(GlobalInfo *_gInfo, Param *_params) {
		gInfo = _gInfo;
		params = _params;
		gainSlewers.setRiseFall(simd::float_4(30.0f), simd::float_4(30.0f)); // slew rate is in input-units per second (ex: V/s)
	}
	
	
	void onReset() {
		resetNonJson();
	}
	void resetNonJson() {
		updateSlowValues();
		gainSlewers.reset();
		vu[0].reset();
		vu[1].reset();
	}
	
	
	// Contract: 
	//  * calc slowGain
	void updateSlowValues() {
		// slowGain
		slowGain = std::pow(params[MAIN_FADER_PARAM].getValue(), masterFaderScalingExponent);
		if (params[MAIN_DIM_PARAM].getValue() > 0.5f) {
			slowGain *= 0.1;// -20 dB dim
		}
	}
	
	
	// Contract: 
	//  * calc mix[] and vu[] vectors
	void process(float *mix) {// takes mix[0..1] and redeposits post in same place, since don't need pre
		// no optimization of mix[0..1] == {0, 0}, since it could be a zero crossing of a mono source!
		
		// Calc master gains
		simd::float_4 gains = simd::float_4::zero();
		// Mute test
		if (params[MAIN_MUTE_PARAM].getValue() < 0.5f) {
			// fader
			float gain = slowGain;
			
			// vol CV input
			// if (inVol->isConnected()) {
				// gain *= clamp(inVol->getVoltage() / 10.f, 0.f, 1.f);
			// }
									
			// Create gains vectors and handle mono
			if (params[MAIN_MONO_PARAM].getValue() > 0.5f) {
				gains = simd::float_4(gain * 0.5f);
			}
			else {
				gains[0] = gains[1] = gain;
			}
		}
		
		// Gain slewer
		if (movemask(gains == gainSlewers.out) != 0xF) {// movemask returns 0xF when 4 floats are equal
			gains = gainSlewers.process(gInfo->sampleTime, gains);
		}
		
		// Test for all gains equal to 0
		if (movemask(gains == simd::float_4::zero()) == 0xF) {// movemask returns 0xF when 4 floats are equal
			mix[0] = 0.0f;	
			mix[1] = 0.0f;
		}
		else {
			// Apply gains
			simd::float_4 sigs(mix[0], mix[1], mix[1], mix[0]);
			sigs = sigs * gains;
			
			// Add to mix
			mix[0] = clamp(sigs[0] + sigs[2], -20.0f, 20.0f);
			mix[1] = clamp(sigs[1] + sigs[3], -20.0f, 20.0f);
		}
		
		// VUs
		if (!gInfo->nightMode) {
			vu[0].process(gInfo->sampleTime, mix[0]);
			vu[1].process(gInfo->sampleTime, mix[1]);
		}
		else {
			vu[0].reset();
			vu[1].reset();
		}
	}
};// struct MixerMaster



//*****************************************************************************



struct MixerGroup {
	// Constants
	static constexpr float trackFaderScalingExponent = 3.0f; // for example, 3.0f is x^3 scaling
	static constexpr float trackFaderMaxLinearGain = 2.0f; // for example, 2.0f is +6 dB
	
	// need to save, no reset
	// none
	
	// need to save, with reset
	// none

	// no need to save, with reset
	simd::float_4 panCoeffs;// L, R, RinL, LinR
	float slowGain;// gain that we don't want to computer each sample (gainAdjust * fader)
	dsp::TSlewLimiter<simd::float_4> gainSlewers;
	VuMeterAll vu[2];// use post[]

	// no need to save, no reset
	int groupNum;// 0 to 3
	//std::string ids;
	GlobalInfo *gInfo;
	Input *inVol;
	Input *inPan;
	Param *paFade;
	Param *paMute;
	Param *paSolo;
	Param *paPan;
	char  *groupName;// write 4 chars always (space when needed), no null termination since all tracks names are concat and just one null at end of all
	float post[2];// post-group monitor outputs (don't need pre-group variable, since those are already in mix[] and they are not affected here)

	void construct(int _groupNum, GlobalInfo *_gInfo, Input *_inputs, Param *_params, char* _groupName) {
		groupNum = _groupNum;
		gInfo = _gInfo;
		inVol = &_inputs[GROUP_VOL_INPUTS + groupNum];
		inPan = &_inputs[GROUP_PAN_INPUTS + groupNum];
		paFade = &_params[GROUP_FADER_PARAMS + groupNum];
		paMute = &_params[GROUP_MUTE_PARAMS + groupNum];
		paSolo = &_params[GROUP_SOLO_PARAMS + groupNum];
		paPan = &_params[GROUP_PAN_PARAMS + groupNum];
		groupName = _groupName;
		gainSlewers.setRiseFall(simd::float_4(30.0f), simd::float_4(30.0f)); // slew rate is in input-units per second (ex: V/s)
	}
	
	
	void onReset() {
		resetNonJson();
	}
	void resetNonJson() {
		updateSlowValues();
		gainSlewers.reset();
		vu[0].reset();
		vu[1].reset();
	}
	
	
	// Contract: 
	//  * calc panCoeffs
	//  * calc slowGain
	void updateSlowValues() {
		// calc panCoeffs
		float pan = paPan->getValue();
		if (inPan->isConnected()) {
			pan += inPan->getVoltage() * 0.1f;// this is a -5V to +5V input
		}
		pan = clamp(pan, 0.0f, 1.0f);
		
		panCoeffs[3] = 0.0f;
		panCoeffs[2] = 0.0f;
		
		// implicitly stereo for groups
		if (gInfo->panLawStereo == 0) {
			// Stereo balance (+3dB), same as mono equal power
			panCoeffs[1] = std::sin(pan * M_PI_2) * M_SQRT2;
			panCoeffs[0] = std::cos(pan * M_PI_2) * M_SQRT2;
		}
		else {
			// True panning, equal power
			panCoeffs[1] = pan >= 0.5f ? (1.0f) : (std::sin(pan * M_PI));
			panCoeffs[2] = pan >= 0.5f ? (0.0f) : (std::cos(pan * M_PI));
			panCoeffs[0] = pan <= 0.5f ? (1.0f) : (std::cos((pan - 0.5f) * M_PI));
			panCoeffs[3] = pan <= 0.5f ? (0.0f) : (std::sin((pan - 0.5f) * M_PI));
		}

		// slowGain
		slowGain = std::pow(paFade->getValue(), trackFaderScalingExponent);
	}
	
	
	// Contract: 
	//  * calc post[] and vu[] vectors
	//  * add post[] to mix[0..1] when post is non-zero
	void process(float *mix) {// give the full mix[10] vector, proper offset will be computed in here
		post[0] = mix[groupNum * 2 + 2];
		post[1] = mix[groupNum * 2 + 3];

		// no optimization of post[0..1] == {0, 0}, since it could be a zero crossing of a mono source!
		
		// Calc group gains
		simd::float_4 gains = simd::float_4::zero();
		// Mute test (solo done only in tracks)
		if (paMute->getValue() < 0.5f) {
			// fader
			float gain = slowGain;
			
			// vol CV input
			if (inVol->isConnected()) {
				gain *= clamp(inVol->getVoltage() / 10.f, 0.f, 1.f);
			}
			
			// Create gains vectors and handle panning
			gains = simd::float_4(gain);
			gains *= panCoeffs;
		}
		
		// Gain slewer
		if (movemask(gains == gainSlewers.out) != 0xF) {// movemask returns 0xF when 4 floats are equal
			gains = gainSlewers.process(gInfo->sampleTime, gains);
		}
		
		// Test for all gains equal to 0
		if (movemask(gains == simd::float_4::zero()) == 0xF) {// movemask returns 0xF when 4 floats are equal
			post[0] = 0.0f;	
			post[1] = 0.0f;
		}
		else {
			// Apply gains
			simd::float_4 sigs(post[0], post[1], post[1], post[0]);
			sigs = sigs * gains;
			
			// Post signals
			post[0] = sigs[0] + sigs[2];
			post[1] = sigs[1] + sigs[3];
				
			// Add to mix
			mix[0] += post[0];
			mix[1] += post[1];
		}
		
		// VUs
		if (!gInfo->nightMode) {
			vu[0].process(gInfo->sampleTime, post[0]);
			vu[1].process(gInfo->sampleTime, post[1]);
		}
		else {
			vu[0].reset();
			vu[1].reset();
		}
	}
};// struct MixerGroup



//*****************************************************************************


struct MixerTrack {
	// Constants
	static constexpr float trackFaderScalingExponent = 3.0f; // for example, 3.0f is x^3 scaling
	static constexpr float trackFaderMaxLinearGain = 2.0f; // for example, 2.0f is +6 dB
	static constexpr float minHPFCutoffFreq = 20.0f;
	static constexpr float maxLPFCutoffFreq = 20000.0f;
	
	// need to save, no reset
	// none
	
	// need to save, with reset
	int group;// 0 is no group (i.e. deposit post in mix[0..1]), 1 to 4 is group (i.e. deposit in mix[2*g..2*g+1]
	float gainAdjust;// this is a gain here (not dB)
	private:
	float hpfCutoffFreq;// always use getter and setter since tied to Biquad
	float lpfCutoffFreq;// always use getter and setter since tied to Biquad
	public:

	// no need to save, with reset
	bool stereo;// pan coefficients use this, so set up first
	simd::float_4 panCoeffs;// L, R, RinL, LinR
	float slowGain;// gain that we don't want to computer each sample (gainAdjust * fader)
	dsp::TSlewLimiter<simd::float_4> gainSlewers;
	VuMeterAll vu[2];// use post[]

	// no need to save, no reset
	int trackNum;
	std::string ids;
	GlobalInfo *gInfo;
	Input *inL;
	Input *inR;
	Input *inVol;
	Input *inPan;
	Param *paFade;
	Param *paMute;
	Param *paSolo;
	Param *paPan;
	char  *trackName;// write 4 chars always (space when needed), no null termination since all tracks names are concat and just one null at end of all
	float pre[2];// for pre-track (aka fader+pan) monitor outputs
	float post[2];// post-track monitor outputs
	OnePoleFilter hpPreFilter[2];// 6dB/oct
	Biquad hpFilter[2];// 12dB/oct
	Biquad lpFilter[2];// 12db/oct


	void setHPFCutoffFreq(float fc) {// always use this instead of directly accessing hpfCutoffFreq
		hpfCutoffFreq = fc;
		fc *= gInfo->sampleTime;// fc is in normalized freq for rest of method
		for (int i = 0; i < 2; i++) {
			hpPreFilter[i].setCutoff(fc);
			hpFilter[i].setFc(fc);
		}
	}
	float getHPFCutoffFreq() {return hpfCutoffFreq;}
	void setLPFCutoffFreq(float fc) {// always use this instead of directly accessing lpfCutoffFreq
		lpfCutoffFreq = fc;
		fc *= gInfo->sampleTime;// fc is in normalized freq for rest of method
		lpFilter[0].setFc(fc);
		lpFilter[1].setFc(fc);
	}
	float getLPFCutoffFreq() {return lpfCutoffFreq;}

	void incGroup() {
		if (group == 4) group = 0;
		else group++;		
	}
	void decGroup() {
		if (group == 0) group = 4;
		else group--;		
	}
	
	bool calcSoloEnable() {// returns true when the check for solo means this track should play 
		// returns true when all solos off or (at least one solo is on and (this solo is on, or the group that we are tied to has its solo on, if group))
		if (gInfo->soloBitMask == 0ul || paSolo->getValue() > 0.5f) {
			return true;
		}
		if ( (group != 0) && ( (gInfo->soloBitMask & (1ul << (16 + group - 1))) != 0ul ) ) {
			return true;
		}
		return false;
	}


	void construct(int _trackNum, GlobalInfo *_gInfo, Input *_inputs, Param *_params, char* _trackName) {
		trackNum = _trackNum;
		gInfo = _gInfo;
		ids = "id" + std::to_string(trackNum) + "_";
		inL = &_inputs[TRACK_SIGNAL_INPUTS + 2 * trackNum + 0];
		inR = &_inputs[TRACK_SIGNAL_INPUTS + 2 * trackNum + 1];
		inVol = &_inputs[TRACK_VOL_INPUTS + trackNum];
		inPan = &_inputs[TRACK_PAN_INPUTS + trackNum];
		paFade = &_params[TRACK_FADER_PARAMS + trackNum];
		paMute = &_params[TRACK_MUTE_PARAMS + trackNum];
		paSolo = &_params[TRACK_SOLO_PARAMS + trackNum];
		paPan = &_params[TRACK_PAN_PARAMS + trackNum];
		trackName = _trackName;
		gainSlewers.setRiseFall(simd::float_4(30.0f), simd::float_4(30.0f)); // slew rate is in input-units per second (ex: V/s)
		for (int i = 0; i < 2; i++) {
			hpFilter[i].setBiquad(BQ_TYPE_HIGHPASS, 0.1, 1.0, 0.0);// 1.0 Q since preceeeded by a one pole filter to get 18dB/oct
			lpFilter[i].setBiquad(BQ_TYPE_LOWPASS, 0.4, 0.707, 0.0);
		}
	}
	
	
	void onReset() {
		group = 0;
		gainAdjust = 1.0f;
		setHPFCutoffFreq(13.0f);// off
		setLPFCutoffFreq(20010.0f);// off
		resetNonJson();
	}
	void resetNonJson() {
		updateSlowValues();
		gainSlewers.reset();
		vu[0].reset();
		vu[1].reset();
	}
	
	
	// Contract: 
	//  * calc stereo
	//  * calc panCoeffs
	//  * calc slowGain
	void updateSlowValues() {
		// calc stereo
		stereo = inR->isConnected();
		
		// calc panCoeffs
		float pan = paPan->getValue();
		if (inPan->isConnected()) {
			pan += inPan->getVoltage() * 0.1f;// this is a -5V to +5V input
		}
		pan = clamp(pan, 0.0f, 1.0f);
		
		panCoeffs[3] = 0.0f;
		panCoeffs[2] = 0.0f;
		if (!stereo) {// mono
			if (gInfo->panLawMono == 1) {
				// Equal power panning law (+3dB boost)
				panCoeffs[1] = std::sin(pan * M_PI_2) * M_SQRT2;
				panCoeffs[0] = std::cos(pan * M_PI_2) * M_SQRT2;
			}
			else if (gInfo->panLawMono == 0) {
				// No compensation (+0dB boost)
				panCoeffs[1] = std::min(1.0f, pan * 2.0f);
				panCoeffs[0] = std::min(1.0f, 2.0f - pan * 2.0f);
			}
			else if (gInfo->panLawMono == 2) {
				// Compromise (+4.5dB boost)
				panCoeffs[1] = std::sqrt( std::abs( std::sin(pan * M_PI_2) * M_SQRT2   *   (pan * 2.0f) ) );
				panCoeffs[0] = std::sqrt( std::abs( std::cos(pan * M_PI_2) * M_SQRT2   *   (2.0f - pan * 2.0f) ) );
			}
			else {
				// Linear panning law (+6dB boost)
				panCoeffs[1] = pan * 2.0f;
				panCoeffs[0] = 2.0f - panCoeffs[1];
			}
		}
		else {// stereo
			if (gInfo->panLawStereo == 0) {
				// Stereo balance (+3dB), same as mono equal power
				panCoeffs[1] = std::sin(pan * M_PI_2) * M_SQRT2;
				panCoeffs[0] = std::cos(pan * M_PI_2) * M_SQRT2;
			}
			else {
				// True panning, equal power
				panCoeffs[1] = pan >= 0.5f ? (1.0f) : (std::sin(pan * M_PI));
				panCoeffs[2] = pan >= 0.5f ? (0.0f) : (std::cos(pan * M_PI));
				panCoeffs[0] = pan <= 0.5f ? (1.0f) : (std::cos((pan - 0.5f) * M_PI));
				panCoeffs[3] = pan <= 0.5f ? (0.0f) : (std::sin((pan - 0.5f) * M_PI));
			}
		}
		
		// slowGain
		slowGain = std::pow(paFade->getValue(), trackFaderScalingExponent);
		if (gainAdjust != 1.0f) {
			slowGain *= gainAdjust;
		}
	}
	
	
	void onRandomize(bool editingSequence) {
		
	}
	
	
	
	void dataToJson(json_t *rootJ) {
		// group
		json_object_set_new(rootJ, (ids + "group").c_str(), json_integer(group));

		// gainAdjust
		json_object_set_new(rootJ, (ids + "gainAdjust").c_str(), json_real(gainAdjust));
		
		// hpfCutoffFreq
		json_object_set_new(rootJ, (ids + "hpfCutoffFreq").c_str(), json_real(getHPFCutoffFreq()));
		
		// lpfCutoffFreq
		json_object_set_new(rootJ, (ids + "lpfCutoffFreq").c_str(), json_real(getLPFCutoffFreq()));
	}
	
	void dataFromJson(json_t *rootJ) {
		// group
		json_t *groupJ = json_object_get(rootJ, (ids + "group").c_str());
		if (groupJ)
			group = json_integer_value(groupJ);
		
		// gainAdjust
		json_t *gainAdjustJ = json_object_get(rootJ, (ids + "gainAdjust").c_str());
		if (gainAdjustJ)
			gainAdjust = json_number_value(gainAdjustJ);
		
		// hpfCutoffFreq
		json_t *hpfCutoffFreqJ = json_object_get(rootJ, (ids + "hpfCutoffFreq").c_str());
		if (hpfCutoffFreqJ)
			setHPFCutoffFreq(json_number_value(hpfCutoffFreqJ));
		
		// lpfCutoffFreq
		json_t *lpfCutoffFreqJ = json_object_get(rootJ, (ids + "lpfCutoffFreq").c_str());
		if (lpfCutoffFreqJ)
			setLPFCutoffFreq(json_number_value(lpfCutoffFreqJ));
		
		// extern must call resetNonJson()
	}
	
	
	// Contract: 
	//  * calc pre[], post[] and vu[] vectors
	//  * add post[] to mix[] when post is non-zero
	void process(float *mix) {
		if (!inL->isConnected()) {
			post[0] = pre[0] = 0.0f;
			post[1] = pre[1] = 0.0f;
			vu[0].reset();
			vu[1].reset();
			return;
		}
				
		// Here we have an input, so get it and set the "pre" values
		pre[0] = inL->getVoltage();
		pre[1] = stereo ? inR->getVoltage() : pre[0];
		
		// Calc track gains
		simd::float_4 gains = simd::float_4::zero();
		// Mute and Solo test
		if (calcSoloEnable() && paMute->getValue() < 0.5f) {
			// fader
			float gain = slowGain;
			
			// vol CV input
			if (inVol->isConnected()) {
				gain *= clamp(inVol->getVoltage() / 10.f, 0.f, 1.f);
			}
			
			// Create gains vectors and handle panning
			gains = simd::float_4(gain);
			gains *= panCoeffs;
		}
		
		// Gain slewer
		if (movemask(gains == gainSlewers.out) != 0xF) {// movemask returns 0xF when 4 floats are equal
			gains = gainSlewers.process(gInfo->sampleTime, gains);
		}
		
		// Test for all gains equal to 0
		if (movemask(gains == simd::float_4::zero()) == 0xF) {// movemask returns 0xF when 4 floats are equal
			post[0] = 0.0f;	
			post[1] = 0.0f;
		}
		else {
			post[0] = pre[0];
			post[1] = pre[1];

			// HPF
			if (getHPFCutoffFreq() >= minHPFCutoffFreq) {
				post[0] = hpFilter[0].process(hpPreFilter[0].processHP(post[0]));
				post[1] = stereo ? hpFilter[1].process(hpPreFilter[1].processHP(post[1])) : post[0];
			}
			// LPF
			if (getLPFCutoffFreq() <= maxLPFCutoffFreq) {
				post[0] = lpFilter[0].process(post[0]);
				post[1] = stereo ? lpFilter[1].process(post[1]) : post[0];
			}


			// Apply gains
			simd::float_4 sigs(post[0], post[1], post[1], post[0]);
			sigs = sigs * gains;
			
			// Post signals
			post[0] = sigs[0] + sigs[2];
			post[1] = sigs[1] + sigs[3];
				
			// Add to mix
			mix[group * 2 + 0] += post[0];
			mix[group * 2 + 1] += post[1];
		}
		
		// VUs
		if (!gInfo->nightMode) {
			vu[0].process(gInfo->sampleTime, post[0]);
			vu[1].process(gInfo->sampleTime, post[1]);
		}
		else {
			vu[0].reset();
			vu[1].reset();
		}
	}
};// struct MixerTrack


//*****************************************************************************



#endif