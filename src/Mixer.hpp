//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.txt for all licenses
//***********************************************************************************************

#ifndef MMM_MIXER_HPP
#define MMM_MIXER_HPP


#include "MindMeldModular.hpp"
#include "dsp/OnePole.hpp"
#include "dsp/VuMeterAll.hpp"


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

// Utility


inline float updateFadeGain(float fadeGain, float target, float timeStep, float shape) {
	float delta = timeStep;// linear
	if (shape > 0.0f) {
		float expDelta = 4.2f * fadeGain * timeStep;// 4.2 hand tuned to take same time as linear fade when exp at 100%
		delta = crossfade(delta, expDelta, shape * 0.9f);// never go to 1.0f, since will stay stuck at 0 when exp!
	}
	else if (shape < 0.0f) {
		float logDelta = 4.2f * (1.0f - fadeGain) * timeStep;// 4.2 hand tuned to take same time as linear fade when log at 100%
		delta = crossfade(delta, logDelta, shape * -0.9f);// never go to 1.0f, since will stay stuck at 1 when log!
	}
	
	if (target < fadeGain) {
		fadeGain -= delta;
		if (fadeGain < target) {
			return target;
		}
	}
	else {
		fadeGain += delta;
		if (fadeGain > target) {
			return target;
		}
	}
	return fadeGain;
}


struct TrackSettingsCpBuffer {
	// first level of copy paste (copy copy-paste of track settings)
	float gainAdjust;
	float fadeRate;
	float fadeProfile;
	float hpfCutoffFreq;// !! user must call filters' setCutoffs manually when copy pasting these
	float lpfCutoffFreq;// !! user must call filters' setCutoffs manually when copy pasting these
	int directOutsMode;
	int vuColorTheme;

	// second level of copy paste (for track re-ordering)
	int group;
	float paFade;
	float paMute;
	float paSolo;
	float paPan;
	char trackName[4];// track names are not null terminated in MixerTracks
	float fadeGain;
	
	void reset() {
		// first level
		gainAdjust = 1.0f;
		fadeRate = 0.0f;
		fadeProfile = 0.0f;
		hpfCutoffFreq = 13.0f;// !! user must call filters' setCutoffs manually when copy pasting these
		lpfCutoffFreq = 20010.0f;// !! user must call filters' setCutoffs manually when copy pasting these
		directOutsMode = 1;
		
		// second level
		group = 0;
		paFade = 1.0f;
		paMute = 0.0f;
		paSolo = 0.0f;
		paPan = 0.5f;
		trackName[0] = '-'; trackName[0] = '0'; trackName[0] = '0'; trackName[0] = '-';
		fadeGain = 1.0f;
	}
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
	int directOutsMode;// 0 is pre-fader, 1 is post-fader, 2 is per-track choice
	bool nightMode;// turn off track VUs only, keep master VUs (also called "Cloaked mode")
	int vuColor;// 0 is green, 1 is blue, 2 is purple, 3 is individual colors for each track/group/master (every user of vuColor must first test for != 3 before using as index into color table)
	int groupUsage[4];// bit 0 of first element shows if first track mapped to first group, etc... managed by MixerTrack except for onReset()

	// no need to save, with reset
	unsigned long soloBitMask;// when = 0ul, nothing to do, when non-zero, a track must check its solo to see if it should play
	float sampleTime;
	TrackSettingsCpBuffer trackSettingsCpBuffer;

	// no need to save, no reset
	Param *paSolo;// all 20 solos are here
	
	
	void construct(Param *_params) {
		paSolo = &_params[TRACK_SOLO_PARAMS];
	}
	
	
	void onReset() {
		panLawMono = 1;
		panLawStereo = 0;
		directOutsMode = 1;// post should be default
		nightMode = false;
		vuColor = 0;
		for (int i = 0; i < 4; i++) {
			groupUsage[i] = 0;
		}
		resetNonJson();
	}
	void resetNonJson() {
		updateSoloBitMask();
		sampleTime = APP->engine->getSampleTime();
		trackSettingsCpBuffer.reset();
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

		// vuColor
		json_object_set_new(rootJ, "vuColor", json_integer(vuColor));
		
		// groupUsage does not need to be saved here, it is computed indirectly in MixerTrack::dataFromJson();
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

		// vuColor
		json_t *vuColorJ = json_object_get(rootJ, "vuColor");
		if (vuColorJ)
			vuColor = json_integer_value(vuColorJ);
		
		// groupUsage does not need to be loaded here, it is computed indirectly in MixerTrack::dataFromJson();
		
		// extern must call resetNonJson()
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
		
};// struct GlobalInfo


//*****************************************************************************


struct MixerMaster {
	// Constants
	static constexpr float masterFaderScalingExponent = 3.0f; 
	static constexpr float masterFaderMaxLinearGain = 2.0f;
	static constexpr float minFadeRate = 0.1f;
	
	// need to save, no reset
	// none
	
	// need to save, with reset
	bool dcBlock;
	float voltageLimiter;
	float fadeRate; // mute when < minFadeRate, fade when >= minFadeRate. This is actually the fade time in seconds
	float fadeProfile; // exp when +100, lin when 0, log when -100
	VuMeterAll vu[2];// use mix[0..1]
	
	// no need to save, with reset
	private:
	simd::float_4 gainMatrix;// L, R, RinL, LinR
	dsp::TSlewLimiter<simd::float_4> gainSlewers;
	OnePoleFilter dcBlocker[2];// 6dB/oct
	public:
	float fadeGain; // target of this gain is the value of the mute/fade button's param (i.e. 0.0f or 1.0f)

	// no need to save, no reset
	GlobalInfo *gInfo;
	Param *params;


	void construct(GlobalInfo *_gInfo, Param *_params) {
		gInfo = _gInfo;
		params = _params;
		gainSlewers.setRiseFall(simd::float_4(30.0f), simd::float_4(30.0f)); // slew rate is in input-units per second (ex: V/s)
	}
	
	
	void onReset() {
		dcBlock = true;
		voltageLimiter = 10.0f;
		fadeRate = 0.0f;
		fadeProfile = 0.0f;
		vu[0].vuColorTheme = 0;// vu[1]'s color theme is not used
		resetNonJson();
	}
	void resetNonJson() {
		updateSlowValues();
		gainSlewers.reset();
		setupDcBlocker();
		vu[0].reset();
		vu[1].reset();
		fadeGain = clamp(1.0f - params[MAIN_MUTE_PARAM].getValue(), 0.0f, 1.0f);
	}
	
	
	void setupDcBlocker() {
		float dcCutoffFreq = 10.0f;// Hz
		dcCutoffFreq *= gInfo->sampleTime;// fc is in normalized freq for rest of method
		dcBlocker[0].setCutoff(dcCutoffFreq);
		dcBlocker[1].setCutoff(dcCutoffFreq);
	}
	
	
	void onSampleRateChange() {
		setupDcBlocker();
	}


	// Contract: 
	//  * calc fadeGain
	//  * calc gainMatrix
	void updateSlowValues() {
		// calc fadeGain 
		// prepare local slowGain
		float slowGain = 0.0f;
		if (fadeRate >= minFadeRate) {// if we are in fade mode
			float newTarget = clamp(1.0f - params[MAIN_MUTE_PARAM].getValue(), 0.0f, 1.0f);
			if (fadeGain != newTarget) {
				float delta = (gInfo->sampleTime / fadeRate) * (float)(1 + RefreshCounter::userInputsStepSkipMask) * 4.0f;// last value is sub refresh in master
				fadeGain = updateFadeGain(fadeGain, newTarget, delta, fadeProfile);
			}
			
			if (fadeGain != 0.0f) {
				slowGain = params[MAIN_FADER_PARAM].getValue();
				if (fadeGain != 1.0f) {
					slowGain *= fadeGain;
				}
				slowGain = std::pow(slowGain, masterFaderScalingExponent);
				if (params[MAIN_DIM_PARAM].getValue() > 0.5f) {
					slowGain *= 0.1f;// -20 dB dim
				}
			}
		}
		else {// we are in mute mode
			fadeGain = clamp(1.0f - params[MAIN_MUTE_PARAM].getValue(), 0.0f, 1.0f);
			if (params[MAIN_MUTE_PARAM].getValue() < 0.5f) {// if not muted
				slowGain = std::pow(params[MAIN_FADER_PARAM].getValue(), masterFaderScalingExponent);
				if (params[MAIN_DIM_PARAM].getValue() > 0.5f) {
					slowGain *= 0.1f;// -20 dB dim
				}
			}
		}		
		// if (inVol->isConnected()) {
			// gain *= clamp(inVol->getVoltage() / 10.f, 0.f, 1.f);
		// }
		

		// calc gainMatrix
		if (slowGain == 0.0f) {
			gainMatrix = simd::float_4::zero();
		}
		else {
			// mono
			if (params[MAIN_MONO_PARAM].getValue() > 0.5f) {
				gainMatrix = simd::float_4(slowGain * 0.5f);
			}
			else {
				gainMatrix[0] = gainMatrix[1] = slowGain;
				gainMatrix[2] = gainMatrix[3] = 0.0f;
			}
		}		
	}
	
	
	void dataToJson(json_t *rootJ) {
		// dcBlock
		json_object_set_new(rootJ, "dcBlock", json_boolean(dcBlock));

		// voltageLimiter
		json_object_set_new(rootJ, "voltageLimiter", json_real(voltageLimiter));
		
		// fadeRate
		json_object_set_new(rootJ, "fadeRate", json_real(fadeRate));
		
		// fadeProfile
		json_object_set_new(rootJ, "fadeProfile", json_real(fadeProfile));
		
		// vuColorTheme
		json_object_set_new(rootJ, "vuColorTheme", json_integer(vu[0].vuColorTheme));
	}

	
	void dataFromJson(json_t *rootJ) {
		// dcBlock
		json_t *dcBlockJ = json_object_get(rootJ, "dcBlock");
		if (dcBlockJ)
			dcBlock = json_is_true(dcBlockJ);
		
		// voltageLimiter
		json_t *voltageLimiterJ = json_object_get(rootJ, "voltageLimiter");
		if (voltageLimiterJ)
			voltageLimiter = json_number_value(voltageLimiterJ);

		// fadeRate
		json_t *fadeRateJ = json_object_get(rootJ, "fadeRate");
		if (fadeRateJ)
			fadeRate = json_number_value(fadeRateJ);
		
		// fadeProfile
		json_t *fadeProfileJ = json_object_get(rootJ, "fadeProfile");
		if (fadeProfileJ)
			fadeProfile = json_number_value(fadeProfileJ);
		
		// vuColorTheme
		json_t *vuColorThemeJ = json_object_get(rootJ, "vuColorTheme");
		if (vuColorThemeJ)
			vu[0].vuColorTheme = json_integer_value(vuColorThemeJ);
		
		// extern must call resetNonJson()
	}
	
	
	// Contract: 
	//  * calc mix[] and vu[] vectors
	void process(float *mix) {// takes mix[0..1] and redeposits post in same place, since don't need pre
		// no optimization of mix[0..1] == {0, 0}, since it could be a zero crossing of a mono source!
		
		// Calc master gain with slewer
		simd::float_4 gains = gainMatrix;
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
			
			// Set mix
			mix[0] = sigs[0] + sigs[2];
			mix[1] = sigs[1] + sigs[3];
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
		
		
		// DC blocker (post VU)
		if (dcBlock) {
			mix[0] = dcBlocker[0].processHP(mix[0]);
			mix[1] = dcBlocker[1].processHP(mix[1]);
		}
		
		// Voltage limiter (post VU, so that we can see true range)
		mix[0] = clamp(mix[0], -voltageLimiter, voltageLimiter);
		mix[1] = clamp(mix[1], -voltageLimiter, voltageLimiter);
	}
};// struct MixerMaster



//*****************************************************************************



struct MixerGroup {
	// Constants
	static constexpr float groupFaderScalingExponent = 3.0f; // for example, 3.0f is x^3 scaling
	static constexpr float groupFaderMaxLinearGain = 2.0f; // for example, 2.0f is +6 dB
	static constexpr float minFadeRate = 0.1f;
	
	// need to save, no reset
	// none
	
	// need to save, with reset
	float fadeRate; // mute when < minFadeRate, fade when >= minFadeRate. This is actually the fade time in seconds
	float fadeProfile; // exp when +100, lin when 0, log when -100
	int directOutsMode;// 0 is pre-fader, 1 is post-fader; (when per-track choice)
	VuMeterAll vu[2];// use post[]

	// no need to save, with reset
	private:
	simd::float_4 gainMatrix;// L, R, RinL, LinR
	dsp::TSlewLimiter<simd::float_4> gainSlewers;
	public:
	float fadeGain; // target of this gain is the value of the mute/fade button's param (i.e. 0.0f or 1.0f)

	// no need to save, no reset
	int groupNum;// 0 to 3
	std::string ids;
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
		ids = "id_g" + std::to_string(groupNum) + "_";
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
		fadeRate = 0.0f;
		fadeProfile = 0.0f;
		directOutsMode = 1;// post should be default
		vu[0].vuColorTheme = 0;// vu[1]'s color theme is not used
		resetNonJson();
	}
	void resetNonJson() {
		updateSlowValues();
		gainSlewers.reset();
		vu[0].reset();
		vu[1].reset();
		fadeGain = clamp(1.0f - paMute->getValue(), 0.0f, 1.0f);
	}
	
	
	// Contract: 
	//  * calc fadeGain
	//  * calc gainMatrix
	void updateSlowValues() {
		// calc fadeGain 
		// prepare local slowGain
		float slowGain = 0.0f;
		if (fadeRate >= minFadeRate) {// if we are in fade mode
			float newTarget = clamp(1.0f - paMute->getValue(), 0.0f, 1.0f);
			if (fadeGain != newTarget) {
				float delta = (gInfo->sampleTime / fadeRate) * (float)(1 + RefreshCounter::userInputsStepSkipMask) * 16.0f;// last value is sub refresh in master (same as tracks in this case)
				fadeGain = updateFadeGain(fadeGain, newTarget, delta, fadeProfile);
			}
			
			if (fadeGain != 0.0f) {
				slowGain = paFade->getValue();
				if (fadeGain != 1.0f) {
					slowGain *= fadeGain;
				}
				if (slowGain != 1.0f) {// since unused groups are not optimized and are likely in their default state
					slowGain = std::pow(slowGain, groupFaderScalingExponent);
				}
			}
		}
		else {// we are in mute mode
			fadeGain = clamp(1.0f - paMute->getValue(), 0.0f, 1.0f);
			if (paMute->getValue() < 0.5f) {// if not muted
				slowGain = std::pow(paFade->getValue(), groupFaderScalingExponent);
			}
		}
		if (inVol->isConnected()) {
			slowGain *= clamp(inVol->getVoltage() / 10.f, 0.f, 1.f);
		}

		// calc gainMatrix
		if (slowGain == 0.0f) {
			gainMatrix = simd::float_4::zero();
		}
		else {
			float pan = paPan->getValue();
			if (inPan->isConnected()) {
				pan += inPan->getVoltage() * 0.1f;// this is a -5V to +5V input
			}
			
			if (pan == 0.5f) {
					gainMatrix[1] = slowGain;
					gainMatrix[0] = slowGain;
					gainMatrix[3] = 0.0f;
					gainMatrix[2] = 0.0f;
			}
			else {
				pan = clamp(pan, 0.0f, 1.0f);
				
				// implicitly stereo for groups
				if (gInfo->panLawStereo == 0) {
					// Stereo balance (+3dB), same as mono equal power
					gainMatrix[1] = std::sin(pan * M_PI_2) * M_SQRT2;
					gainMatrix[0] = std::cos(pan * M_PI_2) * M_SQRT2;
					gainMatrix[3] = 0.0f;
					gainMatrix[2] = 0.0f;
				}
				else {
					// True panning, equal power
					gainMatrix[1] = pan >= 0.5f ? (1.0f) : (std::sin(pan * M_PI));
					gainMatrix[2] = pan >= 0.5f ? (0.0f) : (std::cos(pan * M_PI));
					gainMatrix[0] = pan <= 0.5f ? (1.0f) : (std::cos((pan - 0.5f) * M_PI));
					gainMatrix[3] = pan <= 0.5f ? (0.0f) : (std::sin((pan - 0.5f) * M_PI));
				}
				gainMatrix *= slowGain;
			}
		}	
	}
	

	void dataToJson(json_t *rootJ) {
		// fadeRate
		json_object_set_new(rootJ, (ids + "fadeRate").c_str(), json_real(fadeRate));
		
		// fadeProfile
		json_object_set_new(rootJ, (ids + "fadeProfile").c_str(), json_real(fadeProfile));
		
		// directOutsMode
		json_object_set_new(rootJ, (ids + "directOutsMode").c_str(), json_integer(directOutsMode));
		
		// vuColorTheme
		json_object_set_new(rootJ, (ids + "vuColorTheme").c_str(), json_integer(vu[0].vuColorTheme));
	}
	
	void dataFromJson(json_t *rootJ) {
		// fadeRate
		json_t *fadeRateJ = json_object_get(rootJ, (ids + "fadeRate").c_str());
		if (fadeRateJ)
			fadeRate = json_number_value(fadeRateJ);
		
		// fadeProfile
		json_t *fadeProfileJ = json_object_get(rootJ, (ids + "fadeProfile").c_str());
		if (fadeProfileJ)
			fadeProfile = json_number_value(fadeProfileJ);

		// directOutsMode
		json_t *directOutsModeJ = json_object_get(rootJ, (ids + "directOutsMode").c_str());
		if (directOutsModeJ)
			directOutsMode = json_integer_value(directOutsModeJ);
		
		// vuColorTheme
		json_t *vuColorThemeJ = json_object_get(rootJ, (ids + "vuColorTheme").c_str());
		if (vuColorThemeJ)
			vu[0].vuColorTheme = json_integer_value(vuColorThemeJ);
		
		// extern must call resetNonJson()
	}
	

	// Contract: 
	//  * calc post[] and vu[] vectors
	//  * add post[] to mix[0..1] when post is non-zero
	void process(float *mix) {// give the full mix[10] vector, proper offset will be computed in here
		post[0] = mix[groupNum * 2 + 2];
		post[1] = mix[groupNum * 2 + 3];

		// no optimization of post[0..1] == {0, 0}, since it could be a zero crossing of a mono source!
		
		// Calc group gains with slewer
		simd::float_4 gains = gainMatrix;
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
	static constexpr float minFadeRate = 0.1f;
	static constexpr float hpfBiquadQ = 1.0f;// 1.0 Q since preceeeded by a one pole filter to get 18dB/oct
	
	// need to save, no reset
	// none
	
	// need to save, with reset
	float gainAdjust;// this is a gain here (not dB)
	float fadeRate; // mute when < minFadeRate, fade when >= minFadeRate. This is actually the fade time in seconds
	float fadeProfile; // exp when +100, lin when 0, log when -100
	private:
	int group;// 0 is no group (i.e. deposit post in mix[0..1]), 1 to 4 is group (i.e. deposit in mix[2*g..2*g+1]. Always use setter since need to update gInfo!
	float hpfCutoffFreq;// always use getter and setter since tied to Biquad
	float lpfCutoffFreq;// always use getter and setter since tied to Biquad
	public:
	int directOutsMode;// 0 is pre-fader, 1 is post-fader; (when per-track choice)
	VuMeterAll vu[2];// use post[]

	// no need to save, with reset
	private:
	bool stereo;// pan coefficients use this, so set up first
	simd::float_4 gainMatrix;// L, R, RinL, LinR
	dsp::TSlewLimiter<simd::float_4> gainSlewers;
	OnePoleFilter hpPreFilter[2];// 6dB/oct
	dsp::BiquadFilter hpFilter[2];// 12dB/oct
	dsp::BiquadFilter lpFilter[2];// 12db/oct
	public:
	float fadeGain; // target of this gain is the value of the mute/fade button's param (i.e. 0.0f or 1.0f)

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


	void construct(int _trackNum, GlobalInfo *_gInfo, Input *_inputs, Param *_params, char* _trackName) {
		trackNum = _trackNum;
		ids = "id_t" + std::to_string(trackNum) + "_";
		gInfo = _gInfo;
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
			hpFilter[i].setParameters(dsp::BiquadFilter::HIGHPASS, 0.1, hpfBiquadQ, 0.0);
			lpFilter[i].setParameters(dsp::BiquadFilter::LOWPASS, 0.4, 0.707, 0.0);
		}
	}
	
	
	void onReset() {
		gainAdjust = 1.0f;
		fadeRate = 0.0f;
		fadeProfile = 0.0f;
		group = 0;// no need to use setter since it is reset in GlobalInfo::onReset()
		setHPFCutoffFreq(13.0f);// off
		setLPFCutoffFreq(20010.0f);// off
		directOutsMode = 1;// post should be default
		vu[0].vuColorTheme = 0;// vu[1]'s color theme is not used
		resetNonJson();
	}
	void resetNonJson() {
		updateSlowValues();
		gainSlewers.reset();
		vu[0].reset();
		vu[1].reset();
		fadeGain = clamp(1.0f - paMute->getValue(), 0.0f, 1.0f);
	}
	
	
	// level 1 read and write
	void write(TrackSettingsCpBuffer *dest) {
		dest->gainAdjust = gainAdjust;
		dest->fadeRate = fadeRate;
		dest->fadeProfile = fadeProfile;
		dest->hpfCutoffFreq = hpfCutoffFreq;
		dest->lpfCutoffFreq = lpfCutoffFreq;	
		dest->directOutsMode = directOutsMode;
		dest->vuColorTheme = vu[0].vuColorTheme;
	}
	void read(TrackSettingsCpBuffer *src) {
		gainAdjust = src->gainAdjust;
		fadeRate = src->fadeRate;
		fadeProfile = src->fadeProfile;
		setHPFCutoffFreq(src->hpfCutoffFreq);
		setLPFCutoffFreq(src->lpfCutoffFreq);	
		directOutsMode = src->directOutsMode;
		vu[0].vuColorTheme = src->vuColorTheme;
	}
	
	// level 2 read and write
	void write2(TrackSettingsCpBuffer *dest) {
		write(dest);
		dest->group = group;
		dest->paFade = paFade->getValue();
		dest->paMute = paMute->getValue();
		dest->paSolo = paSolo->getValue();
		dest->paPan = paPan->getValue();
		for (int chr = 0; chr < 4; chr++) {
			dest->trackName[chr] = trackName[chr];
		}
		dest->fadeGain = fadeGain;
	}
	void read2(TrackSettingsCpBuffer *src) {
		read(src);
		setGroup(src->group);
		paFade->setValue(src->paFade);
		paMute->setValue(src->paMute);
		paSolo->setValue(src->paSolo);
		paPan->setValue(src->paPan);
		for (int chr = 0; chr < 4; chr++) {
			trackName[chr] = src->trackName[chr];
		}
		fadeGain = src->fadeGain;
	}
	
	
	void setGroup(int _group) {
		if (group > 0) {
			gInfo->groupUsage[group - 1] &= ~(1 << trackNum);// clear groupUsage for this track in the old group
		}
		if (_group > 0) {
			gInfo->groupUsage[_group - 1] |= (1 << trackNum);// set groupUsage for this track in the new group
		}
		group = _group;
	}
	int getGroup() {
		return group;
	}
	
	void setHPFCutoffFreq(float fc) {// always use this instead of directly accessing hpfCutoffFreq
		hpfCutoffFreq = fc;
		fc *= gInfo->sampleTime;// fc is in normalized freq for rest of method
		for (int i = 0; i < 2; i++) {
			hpPreFilter[i].setCutoff(fc);
			hpFilter[i].setParameters(dsp::BiquadFilter::HIGHPASS, fc, hpfBiquadQ, 0.0);
		}
	}
	float getHPFCutoffFreq() {return hpfCutoffFreq;}
	
	void setLPFCutoffFreq(float fc) {// always use this instead of directly accessing lpfCutoffFreq
		lpfCutoffFreq = fc;
		fc *= gInfo->sampleTime;// fc is in normalized freq for rest of method
		lpFilter[0].setParameters(dsp::BiquadFilter::LOWPASS, fc, 0.707, 0.0);
		lpFilter[1].setParameters(dsp::BiquadFilter::LOWPASS, fc, 0.707, 0.0);
	}
	float getLPFCutoffFreq() {return lpfCutoffFreq;}

	bool calcSoloEnable() {// returns true when the check for solo means this track should play 
		if (gInfo->soloBitMask == 0ul || paSolo->getValue() > 0.5f) {
			return true;
		}
		// here solo is off for this track and there is at least one track or group that has their solo on.
		if ( (group != 0) && ( (gInfo->soloBitMask & (1ul << (16 + group - 1))) != 0ul ) ) {// if going through soloed group  
			// check all solos of all tracks mapped to group, and return true if all those solos are off
			return (gInfo->groupUsage[group - 1] & gInfo->soloBitMask) == 0;
		}
		return false;
	}


	void onSampleRateChange() {
		setHPFCutoffFreq(hpfCutoffFreq);
		setLPFCutoffFreq(lpfCutoffFreq);
	}

	
	// Contract: 
	//  * calc stereo if L connected
	//  * calc fadeGain
	//  * calc gainMatrix if L connected
	void updateSlowValues() {
		if (!inL->isConnected()) {
			// stereo and gainMatrix will only be read in process(), which aborts also when no input, so no need to calc here
			fadeGain = clamp(1.0f - paMute->getValue(), 0.0f, 1.0f);
			return;
		}
		
		// calc stereo
		stereo = inR->isConnected();

		// calc fadeGain 
		// prepare local slowGain
		float slowGain = 0.0f;
		if (!calcSoloEnable()) {
			fadeGain = clamp(1.0f - paMute->getValue(), 0.0f, 1.0f);
		}
		else {
			if (fadeRate >= minFadeRate) {// if we are in fade mode
				float newTarget = clamp(1.0f - paMute->getValue(), 0.0f, 1.0f);
				if (fadeGain != newTarget) {
					float delta = (gInfo->sampleTime / fadeRate) * (float)(1 + RefreshCounter::userInputsStepSkipMask) * 16.0f;// last value is sub refresh in master (number of tracks in this case)
					fadeGain = updateFadeGain(fadeGain, newTarget, delta, fadeProfile);
				}
				
				if (fadeGain != 0.0f) {
					slowGain = paFade->getValue();
					if (fadeGain != 1.0f) {
						slowGain *= fadeGain;
					}
					slowGain = std::pow(slowGain, trackFaderScalingExponent);
					if (gainAdjust != 1.0f) {
						slowGain *= gainAdjust;
					}
				
				}
			}
			else {// we are in mute mode
				fadeGain = clamp(1.0f - paMute->getValue(), 0.0f, 1.0f);
				if (paMute->getValue() < 0.5f) {// if not muted
					slowGain = std::pow(paFade->getValue(), trackFaderScalingExponent);
					if (gainAdjust != 1.0f) {
						slowGain *= gainAdjust;
					}
				}
			}
		}
		if (inVol->isConnected()) {
			slowGain *= clamp(inVol->getVoltage() / 10.f, 0.f, 1.f);
		}

		// calc gainMatrix
		if (slowGain == 0.0f) {
			gainMatrix = simd::float_4::zero();
		}
		else {
			float pan = paPan->getValue();
			if (inPan->isConnected()) {
				pan += inPan->getVoltage() * 0.1f;// this is a -5V to +5V input
			}
			
			gainMatrix[3] = 0.0f;
			gainMatrix[2] = 0.0f;
			
			if (pan == 0.5f) {
				gainMatrix[1] = slowGain;
				gainMatrix[0] = slowGain;
			}
			else {
				pan = clamp(pan, 0.0f, 1.0f);		
				if (!stereo) {// mono
					if (gInfo->panLawMono == 1) {
						// Equal power panning law (+3dB boost)
						gainMatrix[1] = std::sin(pan * M_PI_2) * M_SQRT2;
						gainMatrix[0] = std::cos(pan * M_PI_2) * M_SQRT2;
					}
					else if (gInfo->panLawMono == 0) {
						// No compensation (+0dB boost)
						gainMatrix[1] = std::min(1.0f, pan * 2.0f);
						gainMatrix[0] = std::min(1.0f, 2.0f - pan * 2.0f);
					}
					else if (gInfo->panLawMono == 2) {
						// Compromise (+4.5dB boost)
						gainMatrix[1] = std::sqrt( std::abs( std::sin(pan * M_PI_2) * M_SQRT2   *   (pan * 2.0f) ) );
						gainMatrix[0] = std::sqrt( std::abs( std::cos(pan * M_PI_2) * M_SQRT2   *   (2.0f - pan * 2.0f) ) );
					}
					else {
						// Linear panning law (+6dB boost)
						gainMatrix[1] = pan * 2.0f;
						gainMatrix[0] = 2.0f - gainMatrix[1];
					}
				}
				else {// stereo
					if (gInfo->panLawStereo == 0) {
						// Stereo balance (+3dB), same as mono equal power
						gainMatrix[1] = std::sin(pan * M_PI_2) * M_SQRT2;
						gainMatrix[0] = std::cos(pan * M_PI_2) * M_SQRT2;
					}
					else {
						// True panning, equal power
						gainMatrix[1] = pan >= 0.5f ? (1.0f) : (std::sin(pan * M_PI));
						gainMatrix[2] = pan >= 0.5f ? (0.0f) : (std::cos(pan * M_PI));
						gainMatrix[0] = pan <= 0.5f ? (1.0f) : (std::cos((pan - 0.5f) * M_PI));
						gainMatrix[3] = pan <= 0.5f ? (0.0f) : (std::sin((pan - 0.5f) * M_PI));
					}
				}
				gainMatrix *= slowGain;
			}
		}
	}
	
		
	void dataToJson(json_t *rootJ) {
		// gainAdjust
		json_object_set_new(rootJ, (ids + "gainAdjust").c_str(), json_real(gainAdjust));
		
		// fadeRate
		json_object_set_new(rootJ, (ids + "fadeRate").c_str(), json_real(fadeRate));

		// fadeProfile
		json_object_set_new(rootJ, (ids + "fadeProfile").c_str(), json_real(fadeProfile));
		
		// group
		json_object_set_new(rootJ, (ids + "group").c_str(), json_integer(group));

		// hpfCutoffFreq
		json_object_set_new(rootJ, (ids + "hpfCutoffFreq").c_str(), json_real(getHPFCutoffFreq()));
		
		// lpfCutoffFreq
		json_object_set_new(rootJ, (ids + "lpfCutoffFreq").c_str(), json_real(getLPFCutoffFreq()));
		
		// directOutsMode
		json_object_set_new(rootJ, (ids + "directOutsMode").c_str(), json_integer(directOutsMode));
		
		// vuColorTheme
		json_object_set_new(rootJ, (ids + "vuColorTheme").c_str(), json_integer(vu[0].vuColorTheme));
	}
	
	void dataFromJson(json_t *rootJ) {
		// gainAdjust
		json_t *gainAdjustJ = json_object_get(rootJ, (ids + "gainAdjust").c_str());
		if (gainAdjustJ)
			gainAdjust = json_number_value(gainAdjustJ);
		
		// fadeRate
		json_t *fadeRateJ = json_object_get(rootJ, (ids + "fadeRate").c_str());
		if (fadeRateJ)
			fadeRate = json_number_value(fadeRateJ);
		
		// fadeProfile
		json_t *fadeProfileJ = json_object_get(rootJ, (ids + "fadeProfile").c_str());
		if (fadeProfileJ)
			fadeProfile = json_number_value(fadeProfileJ);

		// group
		json_t *groupJ = json_object_get(rootJ, (ids + "group").c_str());
		if (groupJ)
			setGroup(json_integer_value(groupJ));
		
		// hpfCutoffFreq
		json_t *hpfCutoffFreqJ = json_object_get(rootJ, (ids + "hpfCutoffFreq").c_str());
		if (hpfCutoffFreqJ)
			setHPFCutoffFreq(json_number_value(hpfCutoffFreqJ));
		
		// lpfCutoffFreq
		json_t *lpfCutoffFreqJ = json_object_get(rootJ, (ids + "lpfCutoffFreq").c_str());
		if (lpfCutoffFreqJ)
			setLPFCutoffFreq(json_number_value(lpfCutoffFreqJ));
		
		// directOutsMode
		json_t *directOutsModeJ = json_object_get(rootJ, (ids + "directOutsMode").c_str());
		if (directOutsModeJ)
			directOutsMode = json_integer_value(directOutsModeJ);
		
		// vuColorTheme
		json_t *vuColorThemeJ = json_object_get(rootJ, (ids + "vuColorTheme").c_str());
		if (vuColorThemeJ)
			vu[0].vuColorTheme = json_integer_value(vuColorThemeJ);
		
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
		
		// Calc track gains with slewer
		simd::float_4 gains = gainMatrix;
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