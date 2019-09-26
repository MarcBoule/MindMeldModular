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
	ENUMS(GROUP_FADER_PARAMS, 4),// must follow TRACK_FADER_PARAMS since code assumes contiguous
	ENUMS(TRACK_PAN_PARAMS, 16),
	ENUMS(GROUP_PAN_PARAMS, 4),
	ENUMS(TRACK_MUTE_PARAMS, 16),
	ENUMS(GROUP_MUTE_PARAMS, 4),// must follow TRACK_MUTE_PARAMS since code assumes contiguous
	ENUMS(TRACK_SOLO_PARAMS, 16),
	ENUMS(GROUP_SOLO_PARAMS, 4),// must follow TRACK_SOLO_PARAMS since code assumes contiguous
	MAIN_FADER_PARAM,
	MAIN_MUTE_PARAM,
	MAIN_DIM_PARAM,
	MAIN_MONO_PARAM,
	ENUMS(GROUP_SELECT_PARAMS, 16),
	NUM_PARAMS
}; 


enum InputIds {
	ENUMS(TRACK_SIGNAL_INPUTS, 16 * 2), // Track 0: 0 = L, 1 = R, Track 1: 2 = L, 3 = R, etc...
	ENUMS(TRACK_VOL_INPUTS, 16),
	ENUMS(GROUP_VOL_INPUTS, 4),
	ENUMS(TRACK_PAN_INPUTS, 16), 
	ENUMS(GROUP_PAN_INPUTS, 4), 
	ENUMS(CHAIN_INPUTS, 2),
	ENUMS(INSERT_TRACK_INPUTS, 2),
	INSERT_GRP_AUX_INPUT,
	TRACK_MUTE_INPUT,
	TRACK_SOLO_INPUT,
	GRPM_MUTESOLO_INPUT,// 1-4 Group mutes, 5-8 Group solos, 9 Master Mute, 10 Master Dim, 11 Master Mono, 12 Master VOL
	MASTER_CV_INPUT,
	NUM_INPUTS
};


enum OutputIds {
	ENUMS(DIRECT_OUTPUTS, 3), // Track 1-8, Track 9-16, Groups and Aux
	ENUMS(MAIN_OUTPUTS, 2),
	ENUMS(INSERT_TRACK_OUTPUTS, 2),
	INSERT_GRP_AUX_OUTPUT,
	NUM_OUTPUTS
};


enum LightIds {
	ENUMS(TRACK_HPF_LIGHTS, 16),
	ENUMS(TRACK_LPF_LIGHTS, 16),
	NUM_LIGHTS
};


//*****************************************************************************

// Communications between mixer and auxspander
// Fields in the float arrays

enum AuxFromMotherIds { // for expander messages from main to aux panel
	ENUMS(AFM_TRACK_GROUP_NAMES, 16 + 4),
	AFM_UPDATE_SLOW, // (track/group names, panelTheme, colorAndCloak)
	AFM_PANEL_THEME,
	AFM_COLOR_AND_CLOAK,
	ENUMS(AFM_AUX_SENDS, 8), // left A, B, C, D, right A, B, C, D
	ENUMS(AFM_AUX_VUS, 8), // A-L, A-R,  B-L, B-R, etc
	AFM_DIRECT_AND_PAN_MODES,
	AFM_TRACK_MOVE,
	AFM_AUXSENDMUTE_GROUPED_RETURN,
	AFM_NUM_VALUES
};

enum MotherFromAuxIds { // for expander messages from aux panel to main
	ENUMS(MFA_AUX_RETURNS, 8), // left A, B, C, D, right A, B, C, D
	MFA_VALUE80_INDEX,// a send value, 80 of such values to bring back to main, one per sample
	MFA_VALUE80,
	MFA_VALUE20_INDEX,// a return value, 20 of such values to bring back to main, one per sample
	MFA_VALUE20,
	MFA_AUX_DIR_OUTS,// direct outs modes for all four aux
	MFA_AUX_STEREO_PANS,// stereo pan modes for all four aux
	ENUMS(MFA_AUX_RET_FADER, 4),
	ENUMS(MFA_AUX_RET_PAN, 4),// must be contiguous with MFA_AUX_RET_FADER
	MFA_NUM_VALUES
};



//*****************************************************************************

// Utility

inline float updateFadeGain(float fadeGain, float target, float *fadeGainX, float timeStepX, float shape, bool symmetricalFade) {
	static const float A = 4.0f;
	static const float E_A_M1 = (std::exp(A) - 1.0f);// e^A - 1
	
	float newFadeGain;
	
	if (symmetricalFade) {
		// in here, target is intepreted as a targetX, which is same levels as a target on fadeGain (Y) since both are 0.0f or 1.0f
		// in here, fadeGain is not used since we compute a new fadeGain directly, as opposed to a delta
		if (target < *fadeGainX) {
			*fadeGainX -= timeStepX;
			if (*fadeGainX < target) {
				*fadeGainX = target;
			}
		}
		else if (target > *fadeGainX) {
			*fadeGainX += timeStepX;
			if (*fadeGainX > target) {
				*fadeGainX = target;
			}
		}
		
		newFadeGain = *fadeGainX;// linear
		if (*fadeGainX != target) {
			if (shape > 0.0f) {	
				float expY = (std::exp(A * *fadeGainX) - 1.0f)/E_A_M1;
				newFadeGain = crossfade(newFadeGain, expY, shape);
			}
			else if (shape < 0.0f) {
				float logY = std::log(*fadeGainX * E_A_M1 + 1.0f) / A;
				newFadeGain = crossfade(newFadeGain, logY, -1.0f * shape);		
			}
		}
	}
	else {// asymmetrical fade
		float fadeGainDelta = timeStepX;// linear
		
		if (shape > 0.0f) {	
			float fadeGainDeltaExp = (std::exp(A * (*fadeGainX + timeStepX)) - std::exp(A * (*fadeGainX))) / E_A_M1;
			fadeGainDelta = crossfade(fadeGainDelta, fadeGainDeltaExp, shape);
		}
		else if (shape < 0.0f) {
			float fadeGainDeltaLog = (std::log((*fadeGainX + timeStepX) * E_A_M1 + 1.0f) - std::log((*fadeGainX) * E_A_M1 + 1.0f)) / A;
			fadeGainDelta = crossfade(fadeGainDelta, fadeGainDeltaLog, -1.0f * shape);		
		}
		
		newFadeGain = fadeGain;
		if (target > fadeGain) {
			newFadeGain += fadeGainDelta;
		}
		else if (target < fadeGain) {
			newFadeGain -= fadeGainDelta;
		}	

		if (target > fadeGain && target < newFadeGain) {
			newFadeGain = target;
		}
		else if (target < fadeGain && target > newFadeGain) {
			newFadeGain = target;
		}
		else {
			*fadeGainX += timeStepX;
		}
	}
	
	return newFadeGain;
}


struct TrackSettingsCpBuffer {
	// first level of copy paste (copy copy-paste of track settings)
	float gainAdjust;
	float fadeRate;
	float fadeProfile;
	float hpfCutoffFreq;// !! user must call filters' setCutoffs manually when copy pasting these
	float lpfCutoffFreq;// !! user must call filters' setCutoffs manually when copy pasting these
	int8_t directOutsMode;
	int8_t auxSendsMode;
	int8_t panLawStereo;
	int8_t vuColorThemeLocal;
	bool linkedFader;

	// second level of copy paste (for track re-ordering)
	float paGroup;
	float paFade;
	float paMute;
	float paSolo;
	float paPan;
	char trackName[4];// track names are not null terminated in MixerTracks
	float fadeGain;
	float fadeGainX;
	
	void reset() {
		// first level
		gainAdjust = 1.0f;
		fadeRate = 0.0f;
		fadeProfile = 0.0f;
		hpfCutoffFreq = 13.0f;// !! user must call filters' setCutoffs manually when copy pasting these
		lpfCutoffFreq = 20010.0f;// !! user must call filters' setCutoffs manually when copy pasting these
		directOutsMode = 3;
		auxSendsMode = 3;
		panLawStereo = 0;
		vuColorThemeLocal = 0;
		linkedFader = false;
		
		// second level
		paGroup = 0.0f;
		paFade = 1.0f;
		paMute = 0.0f;
		paSolo = 0.0f;
		paPan = 0.5f;
		trackName[0] = '-'; trackName[0] = '0'; trackName[0] = '0'; trackName[0] = '-';
		fadeGain = 1.0f;
		fadeGainX = 0.0f;
	}
};


//*****************************************************************************

enum ccIds {
	cloakedMode, // turn off track VUs only, keep master VUs (also called "Cloaked mode")
	vuColorGlobal, // 0 is green, 1 is blue, 2 is purple, 3 is individual colors for each track/group/master (every user of vuColor must first test for != 3 before using as index into color table, or else array overflow)
	dispColor // 0 is yellow, 1 is blue, 2 is green, 3 is light-gray
};
union PackedBytes4 {
	int32_t cc1;
	int8_t cc4[4];
};

// managed by Mixer, not by tracks (tracks read only)
struct GlobalInfo {
	
	// constants
	static constexpr float trkAndGrpFaderScalingExponent = 3.0f; // for example, 3.0f is x^3 scaling
	static constexpr float trkAndGrpFaderMaxLinearGain = 2.0f; // for example, 2.0f is +6 dB
	static constexpr float individualAuxSendScalingExponent = 2.0f; // for example, 3.0f is x^3 scaling
	static constexpr float individualAuxSendMaxLinearGain = 1.0f; // for example, 2.0f is +6 dB
	static constexpr float globalAuxSendScalingExponent = 2.0f; // for example, 3.0f is x^3 scaling
	static constexpr float globalAuxSendMaxLinearGain = 4.0f; // for example, 2.0f is +6 dB
	static constexpr float globalAuxReturnScalingExponent = 3.0f; // for example, 3.0f is x^3 scaling
	static constexpr float globalAuxReturnMaxLinearGain = 2.0f; // for example, 2.0f is +6 dB
	static constexpr float antipopSlew = 150.0f;
	
	
	// need to save, no reset
	// none
	
	// need to save, with reset
	int panLawMono;// +0dB (no compensation),  +3 (equal power, default),  +4.5 (compromize),  +6dB (linear)
	int8_t panLawStereo;// Stereo balance (+3dB boost since one channel lost, default),  True pan (linear redistribution but is not equal power), Per-track
	int8_t directOutsMode;// 0 is pre-insert, 1 is post-insert, 2 is post-fader, 3 is post-solo, 4 is per-track choice
	int8_t auxSendsMode;// 0 is pre-insert, 1 is post-insert, 2 is post-fader, 3 is post-solo, 4 is per-track choice
	int auxReturnsMutedWhenMainSolo;
	int auxReturnsSolosMuteDry;
	int chainMode;// 0 is pre-master, 1 is post-master
	PackedBytes4 colorAndCloak;// see enum above called ccIds for fields
	int groupUsage[4];// bit 0 of first element shows if first track mapped to first group, etc... managed by MixerTrack except for onReset()
	bool symmetricalFade;
	unsigned long linkBitMask;// 20 bits for 16 tracks (trk1 = lsb) and 4 groups (grp4 = msb)
	int8_t filterPos;// 0 = pre insert, 1 = post insert, 2 = per track
	int8_t groupedAuxReturnFeedbackProtection;

	// no need to save, with reset
	unsigned long soloBitMask;// when = 0ul, nothing to do, when non-zero, a track must check its solo to see if it should play
	int returnSoloBitMask;

	float sampleTime;

	// no need to save, no reset
	Param *paSolo;// all 20 solos are here (track and group)
	Param *paFade;// all 20 faders are here (track and group)
	Input *inTrackSolo;
	Input *inGroupSolo;
	float *values20;
	float oldFaders[16 + 4] = {-100.0f};
	float maxTGFader;

	
	inline bool isLinked(int index) {return (linkBitMask & (1 << index)) != 0;}
	inline void clearLinked(int index) {linkBitMask &= ~(1 << index);}
	inline void setLinked(int index) {linkBitMask |= (1 << index);}
	inline void setLinked(int index, bool state) {if (state) setLinked(index); else clearLinked(index);}
	inline void toggleLinked(int index) {linkBitMask ^= (1 << index);}
	
	// track and group solos
	void updateSoloBitMask() {
		soloBitMask = 0ul;
		for (unsigned long trkOrGrp = 0; trkOrGrp < 20; trkOrGrp++) {
			updateSoloBit(trkOrGrp);
		}
	}
	void updateSoloBit(unsigned long trkOrGrp) {
		if (trkOrGrp < 16) {
			if ( (paSolo[trkOrGrp].getValue() + inTrackSolo->getVoltage(trkOrGrp) * 0.1f) > 0.5f) {
				soloBitMask |= (1 << trkOrGrp);
			}
			else {
				soloBitMask &= ~(1 << trkOrGrp);
			}	
		}
		else {// trkOrGrp >= 16
			if ( (paSolo[trkOrGrp].getValue() + inGroupSolo->getVoltage(-12 + trkOrGrp) * 0.1f) > 0.5f) {
				soloBitMask |= (1 << trkOrGrp);
			}
			else {
				soloBitMask &= ~(1 << trkOrGrp);
			}
		}
	}		
			
	
	// aux return solos
	inline void updateReturnSoloBits() {
		int newReturnSoloBitMask = 0;
		for (int aux = 0; aux < 4; aux++) {
			if (values20[12 + aux] > 0.5f) {
				newReturnSoloBitMask |= (1 << aux);
			}
		}
		returnSoloBitMask = newReturnSoloBitMask;
	}
		
	// linked faders
	inline void processLinked(int trgOrGrpNum, float slowGain) {
		if (slowGain != oldFaders[trgOrGrpNum]) {
			if (linkBitMask != 0l && isLinked(trgOrGrpNum) && oldFaders[trgOrGrpNum] != -100.0f) {
				float delta = slowGain - oldFaders[trgOrGrpNum];
				for (int i = 0; i < 20; i++) {
					if (isLinked(i) && i != trgOrGrpNum) {
						float newValue = paFade[i].getValue() + delta;
						newValue = clamp(newValue, 0.0f, maxTGFader);
						paFade[i].setValue(newValue);
						oldFaders[i] = newValue;// so that the other fader doesn't trigger a link propagation 
					}
				}
			}
			oldFaders[trgOrGrpNum] = slowGain;
		}
	}

	
	void construct(Param *_params, Input *_inputs, float* _values20) {
		paSolo = &_params[TRACK_SOLO_PARAMS];
		paFade = &_params[TRACK_FADER_PARAMS];
		inTrackSolo = &_inputs[TRACK_SOLO_INPUT];
		inGroupSolo = &_inputs[GRPM_MUTESOLO_INPUT];
		values20 = _values20;
		maxTGFader = std::pow(trkAndGrpFaderMaxLinearGain, 1.0f / trkAndGrpFaderScalingExponent);
	}
	
	
	void onReset() {
		panLawMono = 1;
		panLawStereo = 0;
		directOutsMode = 3;// post-solo should be default
		auxSendsMode = 3;// post-solo should be default
		auxReturnsMutedWhenMainSolo = 0;
		auxReturnsSolosMuteDry = 0;
		chainMode = 1;// post should be default
		colorAndCloak.cc4[cloakedMode] = false;
		colorAndCloak.cc4[vuColorGlobal] = 0;
		colorAndCloak.cc4[dispColor] = 0;
		for (int i = 0; i < 4; i++) {
			groupUsage[i] = 0;
		}
		symmetricalFade = false;
		linkBitMask = 0;
		filterPos = 1;// default is post-insert
		groupedAuxReturnFeedbackProtection = 1;// protection is on by default
		resetNonJson();
	}
	void resetNonJson() {
		updateSoloBitMask();
		updateReturnSoloBits();
		sampleTime = APP->engine->getSampleTime();
	}
	
	void dataToJson(json_t *rootJ) {
		// panLawMono 
		json_object_set_new(rootJ, "panLawMono", json_integer(panLawMono));

		// panLawStereo
		json_object_set_new(rootJ, "panLawStereo", json_integer(panLawStereo));

		// directOutsMode
		json_object_set_new(rootJ, "directOutsMode", json_integer(directOutsMode));
		
		// auxSendsMode
		json_object_set_new(rootJ, "auxSendsMode", json_integer(auxSendsMode));
		
		// auxReturnsMutedWhenMainSolo
		json_object_set_new(rootJ, "auxReturnsMutedWhenMainSolo", json_integer(auxReturnsMutedWhenMainSolo));
		
		// auxReturnsSolosMuteDry
		json_object_set_new(rootJ, "auxReturnsSolosMuteDry", json_integer(auxReturnsSolosMuteDry));
		
		// chainMode
		json_object_set_new(rootJ, "chainMode", json_integer(chainMode));
		
		// colorAndCloak
		json_object_set_new(rootJ, "colorAndCloak", json_integer(colorAndCloak.cc1));
		
		// groupUsage does not need to be saved here, it is computed indirectly in MixerTrack::dataFromJson();
		
		// symmetricalFade
		json_object_set_new(rootJ, "symmetricalFade", json_boolean(symmetricalFade));
		
		// linkBitMask
		json_object_set_new(rootJ, "linkBitMask", json_integer(linkBitMask));

		// filterPos
		json_object_set_new(rootJ, "filterPos", json_integer(filterPos));

		// groupedAuxReturnFeedbackProtection
		json_object_set_new(rootJ, "groupedAuxReturnFeedbackProtection", json_integer(groupedAuxReturnFeedbackProtection));
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
		
		// auxSendsMode
		json_t *auxSendsModeJ = json_object_get(rootJ, "auxSendsMode");
		if (auxSendsModeJ)
			auxSendsMode = json_integer_value(auxSendsModeJ);
		
		// auxReturnsMutedWhenMainSolo
		json_t *auxReturnsMutedWhenMainSoloJ = json_object_get(rootJ, "auxReturnsMutedWhenMainSolo");
		if (auxReturnsMutedWhenMainSoloJ)
			auxReturnsMutedWhenMainSolo = json_integer_value(auxReturnsMutedWhenMainSoloJ);
		
		// auxReturnsSolosMuteDry
		json_t *auxReturnsSolosMuteDryJ = json_object_get(rootJ, "auxReturnsSolosMuteDry");
		if (auxReturnsSolosMuteDryJ)
			auxReturnsSolosMuteDry = json_integer_value(auxReturnsSolosMuteDryJ);
		
		// chainMode
		json_t *chainModeJ = json_object_get(rootJ, "chainMode");
		if (chainModeJ)
			chainMode = json_integer_value(chainModeJ);
		
		// colorAndCloak
		json_t *colorAndCloakJ = json_object_get(rootJ, "colorAndCloak");
		if (colorAndCloakJ)
			colorAndCloak.cc1 = json_integer_value(colorAndCloakJ);
		
		// groupUsage does not need to be loaded here, it is computed indirectly in MixerTrack::dataFromJson();
		
		// symmetricalFade
		json_t *symmetricalFadeJ = json_object_get(rootJ, "symmetricalFade");
		if (symmetricalFadeJ)
			symmetricalFade = json_is_true(symmetricalFadeJ);

		// linkBitMask
		json_t *linkBitMaskJ = json_object_get(rootJ, "linkBitMask");
		if (linkBitMaskJ)
			linkBitMask = json_integer_value(linkBitMaskJ);
		
		// filterPos
		json_t *filterPosJ = json_object_get(rootJ, "filterPos");
		if (filterPosJ)
			filterPos = json_integer_value(filterPosJ);
		
		// groupedAuxReturnFeedbackProtection
		json_t *groupedAuxReturnFeedbackProtectionJ = json_object_get(rootJ, "groupedAuxReturnFeedbackProtection");
		if (groupedAuxReturnFeedbackProtectionJ)
			groupedAuxReturnFeedbackProtection = json_integer_value(groupedAuxReturnFeedbackProtectionJ);
		
		// extern must call resetNonJson()
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
	int8_t vuColorThemeLocal;
	float dimGain;// slider uses this gain, but displays it in dB instead of linear
	
	// no need to save, with reset
	private:
	simd::float_4 gainMatrix;// L, R, RinL, LinR (used for fader-mono block)
	float chainGains[2];// L, R
	dsp::TSlewLimiter<simd::float_4> gainMatrixSlewers;
	dsp::SlewLimiter chainGainSlewers[2];
	OnePoleFilter dcBlocker[2];// 6dB/oct
	public:
	VuMeterAllDual vu;// use mix[0..1]
	float fadeGain; // target of this gain is the value of the mute/fade button's param (i.e. 0.0f or 1.0f)
	float fadeGainX;
	float paramWithCV;
	float dimGainIntegerDB;// corresponds to dimGain, converted to dB, then rounded, then back to linear

	// no need to save, no reset
	GlobalInfo *gInfo;
	Param *params;
	Input *inChain;
	Input *inMuteDimMono;
	Input *inVol;
	float target = -1.0f;

	inline float calcFadeGain() {return (params[MAIN_MUTE_PARAM].getValue() + inMuteDimMono->getVoltage(8) * 0.1f) > 0.5f ? 0.0f : 1.0f;}


	void construct(GlobalInfo *_gInfo, Param *_params, Input *_inputs) {
		gInfo = _gInfo;
		params = _params;
		inChain = &_inputs[CHAIN_INPUTS];
		inMuteDimMono = &_inputs[GRPM_MUTESOLO_INPUT];
		inVol = &_inputs[MASTER_CV_INPUT];
		gainMatrixSlewers.setRiseFall(simd::float_4(GlobalInfo::antipopSlew), simd::float_4(GlobalInfo::antipopSlew)); // slew rate is in input-units per second (ex: V/s)
		chainGainSlewers[0].setRiseFall(GlobalInfo::antipopSlew, GlobalInfo::antipopSlew); // slew rate is in input-units per second (ex: V/s)
		chainGainSlewers[1].setRiseFall(GlobalInfo::antipopSlew, GlobalInfo::antipopSlew); // slew rate is in input-units per second (ex: V/s)
	}
	
	
	void onReset() {
		dcBlock = false;
		voltageLimiter = 10.0f;
		fadeRate = 0.0f;
		fadeProfile = 0.0f;
		vuColorThemeLocal = 0;
		dimGain = 0.25119f;// 0.1 = -20 dB, 0.25119 = -12 dB
		resetNonJson();
	}
	void resetNonJson() {
		updateSlowValues();
		gainMatrixSlewers.reset();
		chainGainSlewers[0].reset();
		chainGainSlewers[1].reset();
		setupDcBlocker();
		vu.reset();
		fadeGain = calcFadeGain();
		fadeGainX = gInfo->symmetricalFade ? fadeGain : 0.0f;
		paramWithCV = -1.0f;
		updateDimGainIntegerDB();
	}
	
	
	void setupDcBlocker() {
		float dcCutoffFreq = 10.0f;// Hz
		dcCutoffFreq *= gInfo->sampleTime;// fc is in normalized freq for rest of method
		dcBlocker[0].setCutoff(dcCutoffFreq);
		dcBlocker[1].setCutoff(dcCutoffFreq);
	}
	
	void updateDimGainIntegerDB() {
		float integerDB = std::round(20.0f * std::log10(dimGain));
		dimGainIntegerDB = std::pow(10.0f, integerDB / 20.0f);
	}
	
	
	void onSampleRateChange() {
		setupDcBlocker();
	}

	// Contract: 
	//  * calc gainMatrix
	//  * calc chainGains
	void updateSlowValues() {
		// calc ** gainMatrix **
		// mono
		if ((params[MAIN_MONO_PARAM].getValue() + inMuteDimMono->getVoltage(10) * 0.1f) > 0.5f) {
			gainMatrix = simd::float_4(0.5f);
		}
		else {
			gainMatrix = simd::float_4(1.0f, 1.0f, 0.0f, 0.0f);
		}
		
		// calc ** chainGains **
		chainGains[0] = inChain[0].isConnected() ? 1.0f : 0.0f;
		chainGains[1] = inChain[1].isConnected() ? 1.0f : 0.0f;
	}
	

	// Contract: 
	//  * calc fadeGain
	//  * calc paramWithCV
	//  * calc mix[0..1] and vu
	void process(float *mix) {// takes mix[0..1] and redeposits post in same place
		// Calc gains for chain input (with antipop when signal connect, impossible for disconnect)
		float gainsC[2] = {chainGains[0], chainGains[1]};
		for (int i = 0; i < 2; i++) {
			if (gainsC[i] != chainGainSlewers[i].out) {
				gainsC[i] = chainGainSlewers[i].process(gInfo->sampleTime, gainsC[i]);
			}
		}
		
		// calc ** fadeGain **
		if (fadeRate >= minFadeRate) {// if we are in fade mode
			float newTarget = calcFadeGain();
			if (!gInfo->symmetricalFade && newTarget != target) {
				fadeGainX = 0.0f;
			}
			target = newTarget;
			if (fadeGain != target) {
				float deltaX = (gInfo->sampleTime / fadeRate);
				fadeGain = updateFadeGain(fadeGain, target, &fadeGainX, deltaX, fadeProfile, gInfo->symmetricalFade);
			}
		}
		else {// we are in mute mode
			fadeGain = calcFadeGain();
			fadeGainX = gInfo->symmetricalFade ? fadeGain : 0.0f;
		}	

		// calc ** paramWithCV **
		float slowGain = params[MAIN_FADER_PARAM].getValue();
		float volCv = inVol->isConnected() ? inVol->getVoltage() : 10.0f;
		if (volCv < 10.0f) {
			slowGain *= clamp(volCv * 0.1f, 0.f, 1.0f);//(multiplying, pre-scaling)
			paramWithCV = slowGain;
		}
		else {
			paramWithCV = -1.0f;// do not show cv pointer
		}
		
		// mute/fade
		slowGain *= fadeGain;
		
		// scaling
		slowGain = std::pow(slowGain, masterFaderScalingExponent);
		
		// dim
		if ((params[MAIN_DIM_PARAM].getValue() + inMuteDimMono->getVoltage(9) * 0.1f) > 0.5f) {
			slowGain *= dimGainIntegerDB;
		}
		
		// Calc master gain with slewer
		simd::float_4 gains = gainMatrix * slowGain;
		if (movemask(gains == gainMatrixSlewers.out) != 0xF) {// movemask returns 0xF when 4 floats are equal
			gains = gainMatrixSlewers.process(gInfo->sampleTime, gains);
		}
		
		// Test for all gains equal to 0
		if (movemask(gains == simd::float_4::zero()) == 0xF) {// movemask returns 0xF when 4 floats are equal
			mix[0] = 0.0f;	
			mix[1] = 0.0f;
		}
		else {
			// Chain inputs when pre master
			if (gInfo->chainMode == 0) {
				if (gainsC[0] != 0.0f) {
					mix[0] += inChain[0].getVoltage() * gainsC[0];
				}
				if (gainsC[1] != 0.0f) {
					mix[1] += inChain[1].getVoltage() * gainsC[1];
				}
			}

			// Apply gains
			simd::float_4 sigs(mix[0], mix[1], mix[1], mix[0]);
			sigs = sigs * gains;
			
			// Set mix
			mix[0] = sigs[0] + sigs[2];
			mix[1] = sigs[1] + sigs[3];
		}
		
		// VUs (no cloaked mode for master, always on)
		vu.process(gInfo->sampleTime, mix);
				
		// Chain inputs when post master
		if (gInfo->chainMode == 1) {
			if (gainsC[0] != 0.0f) {
				mix[0] += inChain[0].getVoltage() * gainsC[0];
			}
			if (gainsC[1] != 0.0f) {
				mix[1] += inChain[1].getVoltage() * gainsC[1];
			}
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
	
	
	void dataToJson(json_t *rootJ) {
		// dcBlock
		json_object_set_new(rootJ, "dcBlock", json_boolean(dcBlock));

		// voltageLimiter
		json_object_set_new(rootJ, "voltageLimiter", json_real(voltageLimiter));
		
		// fadeRate
		json_object_set_new(rootJ, "fadeRate", json_real(fadeRate));
		
		// fadeProfile
		json_object_set_new(rootJ, "fadeProfile", json_real(fadeProfile));
		
		// vuColorThemeLocal
		json_object_set_new(rootJ, "vuColorThemeLocal", json_integer(vuColorThemeLocal));
		
		// dimGain
		json_object_set_new(rootJ, "dimGain", json_real(dimGain));
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
		
		// vuColorThemeLocal
		json_t *vuColorThemeLocalJ = json_object_get(rootJ, "vuColorThemeLocal");
		if (vuColorThemeLocalJ)
			vuColorThemeLocal = json_integer_value(vuColorThemeLocalJ);
		
		// dimGain
		json_t *dimGainJ = json_object_get(rootJ, "dimGain");
		if (dimGainJ)
			dimGain = json_number_value(dimGainJ);
		
		// extern must call resetNonJson()
	}
		
};// struct MixerMaster



//*****************************************************************************



struct MixerGroup {
	// Constants
	static constexpr float minFadeRate = 0.1f;
	
	// need to save, no reset
	// none
	
	// need to save, with reset
	float fadeRate; // mute when < minFadeRate, fade when >= minFadeRate. This is actually the fade time in seconds
	float fadeProfile; // exp when +100, lin when 0, log when -100
	int8_t directOutsMode;// when per track
	int8_t auxSendsMode;// when per track
	int8_t panLawStereo;// when per track
	int8_t vuColorThemeLocal;

	// no need to save, with reset
	private:
	dsp::TSlewLimiter<simd::float_4> gainMatrixSlewers;
	dsp::SlewLimiter muteSoloGainSlewer;
	public:
	VuMeterAllDual vu;// use post[]
	float fadeGain; // target of this gain is the value of the mute/fade button's param (i.e. 0.0f or 1.0f)
	float fadeGainX;
	float paramWithCV;

	// no need to save, no reset
	int groupNum;// 0 to 3
	std::string ids;
	GlobalInfo *gInfo;
	Input *inInsert;
	Input *inVol;
	Input *inPan;
	Input *inMuteSolo;
	Param *paFade;
	Param *paMute;
	Param *paSolo;
	Param *paPan;
	char  *groupName;// write 4 chars always (space when needed), no null termination since all tracks names are concat and just one null at end of all
	float target = -1.0f;
	float *taps;// [0],[1]: pre-insert L R; [32][33]: pre-fader L R, [64][65]: post-fader L R, [96][97]: post-mute-solo L R
	float *insertOuts;// [0][1]: insert outs for this track

	inline float calcFadeGain() {return (paMute->getValue() + inMuteSolo->getVoltage(groupNum) * 0.1f) > 0.5f ? 0.0f : 1.0f;}
	inline bool isLinked() {return gInfo->isLinked(16 + groupNum);}
	inline void toggleLinked() {gInfo->toggleLinked(16 + groupNum);}


	void construct(int _groupNum, GlobalInfo *_gInfo, Input *_inputs, Param *_params, char* _groupName, float* _taps, float* _insertOuts) {
		groupNum = _groupNum;
		ids = "id_g" + std::to_string(groupNum) + "_";
		gInfo = _gInfo;
		inInsert = &_inputs[INSERT_GRP_AUX_INPUT];
		inVol = &_inputs[GROUP_VOL_INPUTS + groupNum];
		inPan = &_inputs[GROUP_PAN_INPUTS + groupNum];
		inMuteSolo = &_inputs[GRPM_MUTESOLO_INPUT];
		paFade = &_params[GROUP_FADER_PARAMS + groupNum];
		paMute = &_params[GROUP_MUTE_PARAMS + groupNum];
		//paSolo = &_params[GROUP_SOLO_PARAMS + groupNum];
		paPan = &_params[GROUP_PAN_PARAMS + groupNum];
		groupName = _groupName;
		taps = _taps;
		insertOuts = _insertOuts;
		gainMatrixSlewers.setRiseFall(simd::float_4(GlobalInfo::antipopSlew), simd::float_4(GlobalInfo::antipopSlew)); // slew rate is in input-units per second (ex: V/s)
		muteSoloGainSlewer.setRiseFall(GlobalInfo::antipopSlew, GlobalInfo::antipopSlew); // slew rate is in input-units per second (ex: V/s)
	}
	
	
	void onReset() {
		fadeRate = 0.0f;
		fadeProfile = 0.0f;
		directOutsMode = 3;// post-solo should be default
		auxSendsMode = 3;// post-solo should be default
		panLawStereo = 0;
		vuColorThemeLocal = 0;
		resetNonJson();
	}
	void resetNonJson() {
		updateSlowValues();
		gainMatrixSlewers.reset();
		muteSoloGainSlewer.reset();
		vu.reset();
		fadeGain = calcFadeGain();
		fadeGainX = gInfo->symmetricalFade ? fadeGain : 0.0f;
		paramWithCV = -1.0f;
	}
	
	
	
	// Contract: 
	//  * process linked
	void updateSlowValues() {
		// ** process linked **
		gInfo->processLinked(16 + groupNum, paFade->getValue());
	}
	
	// Contract: 
	//  * calc fadeGain (computes fade/mute gain, not used directly by mute-solo block, but used for muteSoloGain and fade pointer)
	//  * calc all but the first taps[], insertOuts[] and vu
	//  * calc paramWithCV
	//  * add final tap to mix[0..1]
	void process(float *mix) {
		// Tap[0],[1]: pre-insert (group inputs)
		// nothing to do, already set up by the mix master
		
		// Insert outputs
		insertOuts[0] = taps[0];
		insertOuts[1] = taps[1];
		
		// Tap[8],[9]: pre-fader (post insert)
		if (inInsert->isConnected()) {
			taps[8] = inInsert->getVoltage((groupNum << 1) + 0);
			taps[9] = inInsert->getVoltage((groupNum << 1) + 1);
		}
		else {
			taps[8] = taps[0];
			taps[9] = taps[1];
		}
		
		// calc ** fadeGain **
		if (fadeRate >= minFadeRate) {// if we are in fade mode
			float newTarget = calcFadeGain();
			if (!gInfo->symmetricalFade && newTarget != target) {
				fadeGainX = 0.0f;
			}
			target = newTarget;
			if (fadeGain != target) {
				float deltaX = (gInfo->sampleTime / fadeRate);
				fadeGain = updateFadeGain(fadeGain, target, &fadeGainX, deltaX, fadeProfile, gInfo->symmetricalFade);
			}
		}
		else {// we are in mute mode
			fadeGain = calcFadeGain();
			fadeGainX = gInfo->symmetricalFade ? fadeGain : 0.0f;
		}

		// calc ** calc paramWithCV **
		float slowGain = paFade->getValue();
		float volCv = inVol->isConnected() ? inVol->getVoltage() : 10.0f;
		if (volCv < 10.0f) {
			slowGain *= clamp(volCv * 0.1f, 0.0f, 1.0f);//(multiplying, pre-scaling)
			paramWithCV = slowGain;
		}
		else {
			paramWithCV = -1.0f;// do not show cv pointer
		}
		
		// scaling
		if (slowGain != 1.0f) {// since unused groups are not optimized and are likely in their default state
			slowGain = std::pow(slowGain, GlobalInfo::trkAndGrpFaderScalingExponent);
		}
		
		// panning
		simd::float_4 gainMatrix;// L, R, RinL, LinR (used for fader-pan block)
		float pan = paPan->getValue() + inPan->getVoltage() * 0.1f;// CV is a -5V to +5V input
		if (pan == 0.5f) {
			gainMatrix[1] = slowGain;
			gainMatrix[0] = slowGain;
		}
		else {
			pan = clamp(pan, 0.0f, 1.0f);
			// implicitly stereo for groups
			bool stereoBalance = (gInfo->panLawStereo == 0 || (gInfo->panLawStereo == 2 && panLawStereo == 0));			
			if (stereoBalance) {
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
			gainMatrix *= slowGain;
		}				
		
		// Calc group gains with slewer
		simd::float_4 gainMatrixSlewed = gainMatrix;
		if (movemask(gainMatrixSlewed == gainMatrixSlewers.out) != 0xF) {// movemask returns 0xF when 4 floats are equal
			gainMatrixSlewed = gainMatrixSlewers.process(gInfo->sampleTime, gainMatrixSlewed);
		}
		
		// Test for all gainMatrixSlewed equal to 0
		if (movemask(gainMatrixSlewed == simd::float_4::zero()) == 0xF) {// movemask returns 0xF when 4 floats are equal
			taps[16] = 0.0f;	
			taps[17] = 0.0f;
			taps[24] = 0.0f;	
			taps[25] = 0.0f;
		}
		else {
			// Apply gainMatrixSlewed
			simd::float_4 sigs(taps[8], taps[9], taps[9], taps[8]);
			sigs = sigs * gainMatrixSlewed;
			
			taps[16] = sigs[0] + sigs[2];
			taps[17] = sigs[1] + sigs[3];
			
			// Calc muteSoloGainSlewed
			float muteSoloGainSlewed = fadeGain;// solo not actually in here but in groups
			if (muteSoloGainSlewed != muteSoloGainSlewer.out) {
				muteSoloGainSlewed = muteSoloGainSlewer.process(gInfo->sampleTime, muteSoloGainSlewed);
			}
			
			taps[24] = taps[16] * muteSoloGainSlewed;
			taps[25] = taps[17] * muteSoloGainSlewed;
				
			// Add to mix
			mix[0] += taps[24];
			mix[1] += taps[25];
		}
		
		// VUs
		if (gInfo->colorAndCloak.cc4[cloakedMode]) {
			vu.reset();
		}
		else {
			vu.process(gInfo->sampleTime, &taps[24]);
		}
	}
	
	
	void dataToJson(json_t *rootJ) {
		// fadeRate
		json_object_set_new(rootJ, (ids + "fadeRate").c_str(), json_real(fadeRate));
		
		// fadeProfile
		json_object_set_new(rootJ, (ids + "fadeProfile").c_str(), json_real(fadeProfile));
		
		// directOutsMode
		json_object_set_new(rootJ, (ids + "directOutsMode").c_str(), json_integer(directOutsMode));
		
		// auxSendsMode
		json_object_set_new(rootJ, (ids + "auxSendsMode").c_str(), json_integer(auxSendsMode));
		
		// panLawStereo
		json_object_set_new(rootJ, (ids + "panLawStereo").c_str(), json_integer(panLawStereo));

		// vuColorThemeLocal
		json_object_set_new(rootJ, (ids + "vuColorThemeLocal").c_str(), json_integer(vuColorThemeLocal));
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
		
		// auxSendsMode
		json_t *auxSendsModeJ = json_object_get(rootJ, (ids + "auxSendsMode").c_str());
		if (auxSendsModeJ)
			auxSendsMode = json_integer_value(auxSendsModeJ);
		
		// panLawStereo
		json_t *panLawStereoJ = json_object_get(rootJ, (ids + "panLawStereo").c_str());
		if (panLawStereoJ)
			panLawStereo = json_integer_value(panLawStereoJ);
		
		// vuColorThemeLocal
		json_t *vuColorThemeLocalJ = json_object_get(rootJ, (ids + "vuColorThemeLocal").c_str());
		if (vuColorThemeLocalJ)
			vuColorThemeLocal = json_integer_value(vuColorThemeLocalJ);
		
		// extern must call resetNonJson()
	}
};// struct MixerGroup



//*****************************************************************************


struct MixerTrack {
	// Constants
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
	float hpfCutoffFreq;// always use getter and setter since tied to Biquad
	float lpfCutoffFreq;// always use getter and setter since tied to Biquad
	public:
	int8_t directOutsMode;// when per track
	int8_t auxSendsMode;// when per track
	int8_t panLawStereo;// when per track
	int8_t vuColorThemeLocal;
	int8_t filterPos;// 0 = pre insert, 1 = post insert, 2 = per track

	// no need to save, with reset
	private:
	bool stereo;// pan coefficients use this, so set up first
	float inGain;
	dsp::TSlewLimiter<simd::float_4> gainMatrixSlewers;
	dsp::SlewLimiter inGainSlewer;
	dsp::SlewLimiter muteSoloGainSlewer;
	OnePoleFilter hpPreFilter[2];// 6dB/oct
	dsp::BiquadFilter hpFilter[2];// 12dB/oct
	dsp::BiquadFilter lpFilter[2];// 12db/oct
	public:
	VuMeterAllDual vu;// use post[]
	float fadeGain; // target of this gain is the value of the mute/fade button's param (i.e. 0.0f or 1.0f)
	float fadeGainX;
	float paramWithCV;

	// no need to save, no reset
	int trackNum;
	std::string ids;
	GlobalInfo *gInfo;
	Input *inSig;
	Input *inInsert;
	Input *inVol;
	Input *inPan;
	Input *inMute;
	Param *paGroup;// 0.0 is no group (i.e. deposit post in mix[0..1]), 1.0 to 4.0 is group (i.e. deposit in mix[2*g..2*g+1]. Always use setter since need to update gInfo!
	Param *paFade;
	Param *paMute;
	Param *paSolo;
	Param *paPan;
	char  *trackName;// write 4 chars always (space when needed), no null termination since all tracks names are concat and just one null at end of all
	float target = -1.0f;
	float *taps;// [0],[1]: pre-insert L R; [32][33]: pre-fader L R, [64][65]: post-fader L R, [96][97]: post-mute-solo L R
	float *insertOuts;// [0][1]: insert outs for this track
	bool oldInUse = true;

	inline float calcFadeGain() {return (paMute->getValue() + inMute->getVoltage(trackNum) * 0.1f) > 0.5f ? 0.0f : 1.0f;}
	inline bool isLinked() {return gInfo->isLinked(trackNum);}
	inline void toggleLinked() {gInfo->toggleLinked(trackNum);}


	void construct(int _trackNum, GlobalInfo *_gInfo, Input *_inputs, Param *_params, char* _trackName, float* _taps, float* _insertOuts) {
		trackNum = _trackNum;
		ids = "id_t" + std::to_string(trackNum) + "_";
		gInfo = _gInfo;
		inSig = &_inputs[TRACK_SIGNAL_INPUTS + 2 * trackNum + 0];
		inInsert = &_inputs[INSERT_TRACK_INPUTS];
		inVol = &_inputs[TRACK_VOL_INPUTS + trackNum];
		inPan = &_inputs[TRACK_PAN_INPUTS + trackNum];
		inMute = &_inputs[TRACK_MUTE_INPUT];
		paGroup = &_params[GROUP_SELECT_PARAMS + trackNum];
		paFade = &_params[TRACK_FADER_PARAMS + trackNum];
		paMute = &_params[TRACK_MUTE_PARAMS + trackNum];
		paSolo = &_params[TRACK_SOLO_PARAMS + trackNum];
		paPan = &_params[TRACK_PAN_PARAMS + trackNum];
		trackName = _trackName;
		taps = _taps;
		insertOuts = _insertOuts;
		gainMatrixSlewers.setRiseFall(simd::float_4(GlobalInfo::antipopSlew), simd::float_4(GlobalInfo::antipopSlew)); // slew rate is in input-units per second (ex: V/s)
		inGainSlewer.setRiseFall(GlobalInfo::antipopSlew, GlobalInfo::antipopSlew); // slew rate is in input-units per second (ex: V/s)
		muteSoloGainSlewer.setRiseFall(GlobalInfo::antipopSlew, GlobalInfo::antipopSlew); // slew rate is in input-units per second (ex: V/s)
		for (int i = 0; i < 2; i++) {
			hpFilter[i].setParameters(dsp::BiquadFilter::HIGHPASS, 0.1, hpfBiquadQ, 0.0);
			lpFilter[i].setParameters(dsp::BiquadFilter::LOWPASS, 0.4, 0.707, 0.0);
		}
	}
	
	
	void onReset() {
		gainAdjust = 1.0f;
		fadeRate = 0.0f;
		fadeProfile = 0.0f;
		setHPFCutoffFreq(13.0f);// off
		setLPFCutoffFreq(20010.0f);// off
		directOutsMode = 3;// post-solo should be default
		auxSendsMode = 3;// post-solo should be default
		panLawStereo = 0;
		vuColorThemeLocal = 0;
		filterPos = 1;// default is post-insert
		resetNonJson();
	}
	void resetNonJson() {
		updateSlowValues();
		gainMatrixSlewers.reset();
		inGainSlewer.reset();
		muteSoloGainSlewer.reset();
		vu.reset();
		fadeGain = calcFadeGain();
		fadeGainX = gInfo->symmetricalFade ? fadeGain : 0.0f;
		paramWithCV = -1.0f;
		target = -1;
	}
	
	
	// level 1 read and write
	void write(TrackSettingsCpBuffer *dest) {
		dest->gainAdjust = gainAdjust;
		dest->fadeRate = fadeRate;
		dest->fadeProfile = fadeProfile;
		dest->hpfCutoffFreq = hpfCutoffFreq;
		dest->lpfCutoffFreq = lpfCutoffFreq;	
		dest->directOutsMode = directOutsMode;
		dest->auxSendsMode = auxSendsMode;
		dest->panLawStereo = panLawStereo;
		dest->vuColorThemeLocal = vuColorThemeLocal;
		dest->linkedFader = gInfo->isLinked(trackNum);
	}
	void read(TrackSettingsCpBuffer *src) {
		gainAdjust = src->gainAdjust;
		fadeRate = src->fadeRate;
		fadeProfile = src->fadeProfile;
		setHPFCutoffFreq(src->hpfCutoffFreq);
		setLPFCutoffFreq(src->lpfCutoffFreq);	
		directOutsMode = src->directOutsMode;
		auxSendsMode = src->auxSendsMode;
		panLawStereo = src->panLawStereo;
		vuColorThemeLocal = src->vuColorThemeLocal;
		gInfo->setLinked(trackNum, src->linkedFader);
	}
	
	// level 2 read and write
	void write2(TrackSettingsCpBuffer *dest) {
		write(dest);
		dest->paGroup = paGroup->getValue();;
		dest->paFade = paFade->getValue();
		dest->paMute = paMute->getValue();
		dest->paSolo = paSolo->getValue();
		dest->paPan = paPan->getValue();
		for (int chr = 0; chr < 4; chr++) {
			dest->trackName[chr] = trackName[chr];
		}
		dest->fadeGain = fadeGain;
		dest->fadeGainX = fadeGainX;
	}
	void read2(TrackSettingsCpBuffer *src) {
		read(src);
		paGroup->setValue(src->paGroup);
		paFade->setValue(src->paFade);
		paMute->setValue(src->paMute);
		paSolo->setValue(src->paSolo);
		paPan->setValue(src->paPan);
		for (int chr = 0; chr < 4; chr++) {
			trackName[chr] = src->trackName[chr];
		}
		fadeGain = src->fadeGain;
		fadeGainX = src->fadeGainX;
	}
	
	
	void updateGroupUsage() {
		// clear groupUsage for this track in all groups
		for (int i = 0; i < 4; i++) {
			gInfo->groupUsage[i] &= ~(1 << trackNum);
		}
		
		// set groupUsage for this track in the new group
		int group = (int)(paGroup->getValue() + 0.5f);
		if (group > 0) {
			gInfo->groupUsage[group - 1] |= (1 << trackNum);
		}
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

	float calcSoloGain() {// returns 1.0f when the check for solo means this track should play, 0.0f otherwise
		if (gInfo->soloBitMask == 0ul || (gInfo->soloBitMask & (1 << trackNum)) != 0) {
			return 1.0f;
		}
		// here solo is off for this track and there is at least one track or group that has their solo on.
		int group = (int)(paGroup->getValue() + 0.5f);
		if ( (group != 0) && ( (gInfo->soloBitMask & (1ul << (16 + group - 1))) != 0ul ) ) {// if going through soloed group  
			// check all solos of all tracks mapped to group, and return true if all those solos are off
			return ((gInfo->groupUsage[group - 1] & gInfo->soloBitMask) == 0) ? 1.0f : 0.0f;
		}
		return 0.0f;
	}


	void onSampleRateChange() {
		setHPFCutoffFreq(hpfCutoffFreq);
		setLPFCutoffFreq(lpfCutoffFreq);
	}

	
	// Contract: 
	//  * update group usage
	//  * calc stereo
	//  * calc inGain (computes gainAdjust with input presence, for gain-adjust block, single float)
	//  * process linked
	void updateSlowValues() {
		// ** update group usage **
		updateGroupUsage();
		
		// calc ** stereo **
		stereo = inSig[1].isConnected();

		// calc ** inGain **
		inGain = inSig[0].isConnected() ? gainAdjust : 0.0f;

		// ** process linked **
		gInfo->processLinked(trackNum, paFade->getValue());
	}
	
	
	// Contract: 
	//  * calc taps[], insertOuts[] and vu
	//  * calc fadeGain (computes fade/mute gain, not used directly by mute-solo block, but used for muteSoloGain and fade pointer)
	//  * calc paramWithCV
	//  * when track in use, add final tap to mix[] according to group
	void process(float *mix) {
		int insertPortIndex = trackNum >> 3;

		// optimize unused track
		bool inUse = inSig[0].isConnected();
		if (!inUse) {
			if (oldInUse) {
				taps[0] = 0.0f; taps[1] = 0.0f;
				taps[32] = 0.0f; taps[33] = 0.0f;
				taps[64] = 0.0f; taps[65] = 0.0f;
				taps[96] = 0.0f; taps[97] = 0.0f;
				insertOuts[0] = 0.0f;
				insertOuts[1] = 0.0f;
				vu.reset();
				gainMatrixSlewers.reset();
				inGainSlewer.reset();
				muteSoloGainSlewer.reset();
				paramWithCV = -1.0f;
				oldInUse = false;
			}
			return;
		}
		oldInUse = true;
				
		// Calc inGainSlewed
		float inGainSlewed = inGain;
		if (inGainSlewed != inGainSlewer.out) {
			inGainSlewed = inGainSlewer.process(gInfo->sampleTime, inGainSlewed);
		}
		
		// Tap[0],[1]: pre-insert (Inputs with gain adjust)
		taps[0] = (inSig[0].getVoltage() * inGainSlewed);
		taps[1] = stereo ? (inSig[1].getVoltage() * inGainSlewed) : taps[0];
		
		if (gInfo->filterPos == 1 || (gInfo->filterPos == 2 && filterPos == 1)) {
			// Insert outputs
			insertOuts[0] = taps[0];
			insertOuts[1] = stereo ? taps[1] : 0.0f;// don't send to R of insert outs when mono!
			
			// Post insert (taps[32..33] are provisional, since not yet filtered)
			if (inInsert[insertPortIndex].isConnected()) {
				taps[32] = inInsert[insertPortIndex].getVoltage(((trackNum & 0x7) << 1) + 0);
				taps[33] = inInsert[insertPortIndex].getVoltage(((trackNum & 0x7) << 1) + 1);
			}
			else {
				taps[32] = taps[0];
				taps[33] = taps[1];
			}

			// Filters
			// Tap[32],[33]: pre-fader (post insert)
			// HPF
			if (getHPFCutoffFreq() >= minHPFCutoffFreq) {
				taps[32] = hpFilter[0].process(hpPreFilter[0].processHP(taps[32]));
				taps[33] = stereo ? hpFilter[1].process(hpPreFilter[1].processHP(taps[33])) : taps[32];
			}
			// LPF
			if (getLPFCutoffFreq() <= maxLPFCutoffFreq) {
				taps[32] = lpFilter[0].process(taps[32]);
				taps[33]  = stereo ? lpFilter[1].process(taps[33]) : taps[32];
			}
		}
		else {// filters before inserts
			// Filters
			float filtered[2] = {taps[0], taps[1]};
			// HPF
			if (getHPFCutoffFreq() >= minHPFCutoffFreq) {
				filtered[0] = hpFilter[0].process(hpPreFilter[0].processHP(filtered[0]));
				filtered[1] = stereo ? hpFilter[1].process(hpPreFilter[1].processHP(filtered[1])) : filtered[0];
			}
			// LPF
			if (getLPFCutoffFreq() <= maxLPFCutoffFreq) {
				filtered[0] = lpFilter[0].process(filtered[0]);
				filtered[1] = stereo ? lpFilter[1].process(filtered[1]) : filtered[0];
			}
			
			// Insert outputs
			insertOuts[0] = filtered[0];
			insertOuts[1] = stereo ? filtered[1] : 0.0f;// don't send to R of insert outs when mono!
			
			// Tap[32],[33]: pre-fader (post insert)
			if (inInsert[insertPortIndex].isConnected()) {
				taps[32] = inInsert[insertPortIndex].getVoltage(((trackNum & 0x7) << 1) + 0);
				taps[33] = inInsert[insertPortIndex].getVoltage(((trackNum & 0x7) << 1) + 1);
			}
			else {
				taps[32] = filtered[0];
				taps[33] = filtered[1];
			}
		}// filterPos
		
		
		// calc ** fadeGain ** (does mute also)
		if (fadeRate >= minFadeRate) {// if we are in fade mode
			float newTarget = calcFadeGain();
			if (!gInfo->symmetricalFade && newTarget != target) {
				fadeGainX = 0.0f;
			}
			target = newTarget;
			if (fadeGain != target) {
				float deltaX = (gInfo->sampleTime / fadeRate);
				fadeGain = updateFadeGain(fadeGain, target, &fadeGainX, deltaX, fadeProfile, gInfo->symmetricalFade);
			}
		}
		else {// we are in mute mode
			fadeGain = calcFadeGain();
			fadeGainX = gInfo->symmetricalFade ? fadeGain : 0.0f;
		}

		// calc ** paramWithCV **
		float slowGain = paFade->getValue();
		float volCv = inVol->isConnected() ? inVol->getVoltage() : 10.0f;
		if (volCv < 10.0f) {
			slowGain *= clamp(volCv * 0.1f, 0.0f, 1.0f);//(multiplying, pre-scaling)
			paramWithCV = slowGain;
		}
		else {
			paramWithCV = -1.0f;
		}
		
		// scaling
		slowGain = std::pow(slowGain, GlobalInfo::trkAndGrpFaderScalingExponent);
		

		// calc gainMatrix
		simd::float_4 gainMatrix(0.0f);// L, R, RinL, LinR (used for fader-pan block)
		// panning
		float pan = paPan->getValue() + inPan->getVoltage() * 0.1f;// CV is a -5V to +5V input
		if (pan == 0.5f) {
			if (!stereo) gainMatrix[3] = slowGain;
			else gainMatrix[1] = slowGain;
			gainMatrix[0] = slowGain;
		}
		else {
			pan = clamp(pan, 0.0f, 1.0f);		
			if (!stereo) {// mono
				if (gInfo->panLawMono == 1) {
					// Equal power panning law (+3dB boost)
					gainMatrix[3] = std::sin(pan * M_PI_2) * M_SQRT2;
					gainMatrix[0] = std::cos(pan * M_PI_2) * M_SQRT2;
				}
				else if (gInfo->panLawMono == 0) {
					// No compensation (+0dB boost)
					gainMatrix[3] = std::min(1.0f, pan * 2.0f);
					gainMatrix[0] = std::min(1.0f, 2.0f - pan * 2.0f);
				}
				else if (gInfo->panLawMono == 2) {
					// Compromise (+4.5dB boost)
					gainMatrix[3] = std::sqrt( std::abs( std::sin(pan * M_PI_2) * M_SQRT2   *   (pan * 2.0f) ) );
					gainMatrix[0] = std::sqrt( std::abs( std::cos(pan * M_PI_2) * M_SQRT2   *   (2.0f - pan * 2.0f) ) );
				}
				else {
					// Linear panning law (+6dB boost)
					gainMatrix[3] = pan * 2.0f;
					gainMatrix[0] = 2.0f - gainMatrix[3];
				}
			}
			else {// stereo
				bool stereoBalance = (gInfo->panLawStereo == 0 || (gInfo->panLawStereo == 2 && panLawStereo == 0));			
				if (stereoBalance) {
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

		// Calc gainMatrixSlewed
		simd::float_4 gainMatrixSlewed = gainMatrix;
		if (movemask(gainMatrixSlewed == gainMatrixSlewers.out) != 0xF) {// movemask returns 0xF when 4 floats are equal
			gainMatrixSlewed = gainMatrixSlewers.process(gInfo->sampleTime, gainMatrixSlewed);
		}
		
		// Tap[64],[65]: post-fader
		// Tap[96],[97]: post-mute-solo
		// Test for all gainMatrixSlewed equal to 0
		if (movemask(gainMatrixSlewed == simd::float_4::zero()) == 0xF) {// movemask returns 0xF when 4 floats are equal
			taps[64] = 0.0f;	
			taps[65] = 0.0f;
			taps[96] = 0.0f;	
			taps[97] = 0.0f;
		}
		else {
			// Apply gainMatrixSlewed
			simd::float_4 sigs(taps[32], taps[33], taps[33], taps[32]);
			sigs = sigs * gainMatrixSlewed;
			
			taps[64] = sigs[0] + sigs[2];
			taps[65] = sigs[1] + sigs[3];

			// Calc muteSoloGainSlewed
			float muteSoloGainSlewed = calcSoloGain() * fadeGain;
			if (muteSoloGainSlewed != muteSoloGainSlewer.out) {
				muteSoloGainSlewed = muteSoloGainSlewer.process(gInfo->sampleTime, muteSoloGainSlewed);
			}

			taps[96] = taps[64] * muteSoloGainSlewed;
			taps[97] = taps[65] * muteSoloGainSlewed;
				
			// Add to mix since gainMatrixSlewed non zero
			int groupIndex = (int)(paGroup->getValue() + 0.5f);
			mix[groupIndex * 2 + 0] += taps[96];
			mix[groupIndex * 2 + 1] += taps[97];
		}
		
		// VUs
		if (gInfo->colorAndCloak.cc4[cloakedMode]) {
			vu.reset();
		}
		else {
			vu.process(gInfo->sampleTime, &taps[96]);
		}
	}
		
	void dataToJson(json_t *rootJ) {
		// gainAdjust
		json_object_set_new(rootJ, (ids + "gainAdjust").c_str(), json_real(gainAdjust));
		
		// fadeRate
		json_object_set_new(rootJ, (ids + "fadeRate").c_str(), json_real(fadeRate));

		// fadeProfile
		json_object_set_new(rootJ, (ids + "fadeProfile").c_str(), json_real(fadeProfile));
		
		// hpfCutoffFreq
		json_object_set_new(rootJ, (ids + "hpfCutoffFreq").c_str(), json_real(getHPFCutoffFreq()));
		
		// lpfCutoffFreq
		json_object_set_new(rootJ, (ids + "lpfCutoffFreq").c_str(), json_real(getLPFCutoffFreq()));
		
		// directOutsMode
		json_object_set_new(rootJ, (ids + "directOutsMode").c_str(), json_integer(directOutsMode));
		
		// auxSendsMode
		json_object_set_new(rootJ, (ids + "auxSendsMode").c_str(), json_integer(auxSendsMode));
		
		// panLawStereo
		json_object_set_new(rootJ, (ids + "panLawStereo").c_str(), json_integer(panLawStereo));

		// vuColorThemeLocal
		json_object_set_new(rootJ, (ids + "vuColorThemeLocal").c_str(), json_integer(vuColorThemeLocal));

		// filterPos
		json_object_set_new(rootJ, (ids + "filterPos").c_str(), json_integer(filterPos));
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
		
		// auxSendsMode
		json_t *auxSendsModeJ = json_object_get(rootJ, (ids + "auxSendsMode").c_str());
		if (auxSendsModeJ)
			auxSendsMode = json_integer_value(auxSendsModeJ);
		
		// panLawStereo
		json_t *panLawStereoJ = json_object_get(rootJ, (ids + "panLawStereo").c_str());
		if (panLawStereoJ)
			panLawStereo = json_integer_value(panLawStereoJ);
		
		// vuColorThemeLocal
		json_t *vuColorThemeLocalJ = json_object_get(rootJ, (ids + "vuColorThemeLocal").c_str());
		if (vuColorThemeLocalJ)
			vuColorThemeLocal = json_integer_value(vuColorThemeLocalJ);
		
		// filterPos
		json_t *filterPosJ = json_object_get(rootJ, (ids + "filterPos").c_str());
		if (filterPosJ)
			filterPos = json_integer_value(filterPosJ);
		
		// extern must call resetNonJson()
	}
};// struct MixerTrack


//*****************************************************************************



struct MixerAux {
	// Constants
	// none
	
	// need to save, no reset
	// none
	
	// need to save, with reset
	// none

	// no need to save, with reset
	private:
	dsp::TSlewLimiter<simd::float_4> gainMatrixSlewers;
	dsp::SlewLimiter muteSoloGainSlewer;
	float muteSoloGain; // value of the mute/fade * solo_enable
	public:

	// no need to save, no reset
	int auxNum;// 0 to 3
	std::string ids;
	GlobalInfo *gInfo;
	Input *inInsert;
	//float *flPan;
	//float *flFade;
	float *flMute;
	float *flSolo0;
	float *flGroup;
	float *taps;
	float *insertOuts;// [0][1]: insert outs for this track
	int8_t* panLawStereoLocal;

	inline int getAuxGroup() {return (int)(*flGroup + 0.5f);}


	void construct(int _auxNum, GlobalInfo *_gInfo, Input *_inputs, float* _val20, float* _taps, float* _insertOuts, int8_t* _panLawStereoLocal) {
		auxNum = _auxNum;
		ids = "id_a" + std::to_string(auxNum) + "_";
		gInfo = _gInfo;
		inInsert = &_inputs[INSERT_GRP_AUX_INPUT];
		//flPan = &_val20[auxNum + 0];
		//flFade = &_val20[auxNum + 4];
		flMute = &_val20[auxNum + 8];
		flGroup = &_val20[auxNum + 16];
		taps = _taps;
		insertOuts = _insertOuts;
		panLawStereoLocal = _panLawStereoLocal;
		gainMatrixSlewers.setRiseFall(simd::float_4(GlobalInfo::antipopSlew), simd::float_4(GlobalInfo::antipopSlew)); // slew rate is in input-units per second (ex: V/s)
		muteSoloGainSlewer.setRiseFall(GlobalInfo::antipopSlew, GlobalInfo::antipopSlew); // slew rate is in input-units per second (ex: V/s)
	}
	
	
	void onReset() {
		resetNonJson();
	}
	void resetNonJson() {
		updateSlowValues();
		gainMatrixSlewers.reset();
		muteSoloGainSlewer.reset();
	}
	
	
	// Contract: 
	//  * calc muteSoloGain (computes mute-solo gain, for mute-solo block, single float)
	void updateSlowValues() {
		// calc ** muteSoloGain **
		float soloGain = (gInfo->returnSoloBitMask == 0 || (gInfo->returnSoloBitMask & (1 << auxNum)) != 0) ? 1.0f : 0.0f;
		muteSoloGain = *flMute * soloGain;
		// Handle "Mute aux returns when soloing track"
		// i.e. add aux returns to mix when no solo, or when solo and don't want mutes aux returns
		if (gInfo->soloBitMask != 0 && gInfo->auxReturnsMutedWhenMainSolo) {
			muteSoloGain = 0.0f;
		}
		
		
		// scaling 
		// done in auxspander
	}
	
	// Contract: 
	//  * calc all but the first taps[], calc insertOuts[] and vu
	//  * add final tap to mix[0..1]
	void process(float *mix, float *auxRetFadePan) {
		// Tap[0],[1]: pre-insert (aux inputs)
		// nothing to do, already set up by the auxspander
		// auxRetFadePan[0] points fader value, auxRetFadePan[0] points pan value, all indexed for a given aux
		
		// Insert outputs
		insertOuts[0] = taps[0];
		insertOuts[1] = taps[1];
				
		// Tap[8],[9]: pre-fader (post insert)
		if (inInsert->isConnected()) {
			taps[8] = inInsert->getVoltage((auxNum << 1) + 8);
			taps[9] = inInsert->getVoltage((auxNum << 1) + 9);
		}
		else {
			taps[8] = taps[0];
			taps[9] = taps[1];
		}
		
		// fader and fader cv input (multiplying and pre-scaling) both done in auxspander
		float slowGain = *auxRetFadePan; 
		
		
		// calc ** gainMatrix **
		simd::float_4 gainMatrix(0.0f);// L, R, RinL, LinR (used for fader-pan block)	
		// panning
		float pan = auxRetFadePan[4];// cv input and clamping already done in auxspander
		if (pan == 0.5f) {
			gainMatrix[1] = slowGain;
			gainMatrix[0] = slowGain;
		}
		else {
			// implicitly stereo for aux
			bool stereoBalance = (gInfo->panLawStereo == 0 || (gInfo->panLawStereo == 2 && *panLawStereoLocal == 0));			
			if (stereoBalance) {
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
			gainMatrix *= slowGain;
		}
		
		// Calc group gains with slewer
		simd::float_4 gainMatrixSlewed = gainMatrix;
		if (movemask(gainMatrixSlewed == gainMatrixSlewers.out) != 0xF) {// movemask returns 0xF when 4 floats are equal
			gainMatrixSlewed = gainMatrixSlewers.process(gInfo->sampleTime, gainMatrixSlewed);
		}
		
		// Test for all gainMatrixSlewed equal to 0
		if (movemask(gainMatrixSlewed == simd::float_4::zero()) == 0xF) {// movemask returns 0xF when 4 floats are equal
			taps[16] = 0.0f;	
			taps[17] = 0.0f;
			taps[24] = 0.0f;	
			taps[25] = 0.0f;
		}
		else {
			// Apply gainMatrixSlewed
			simd::float_4 sigs(taps[8], taps[9], taps[9], taps[8]);
			sigs = sigs * gainMatrixSlewed;
			
			taps[16] = sigs[0] + sigs[2];
			taps[17] = sigs[1] + sigs[3];
			
			// Calc muteSoloGainSlewed
			float muteSoloGainSlewed = muteSoloGain;
			if (muteSoloGainSlewed != muteSoloGainSlewer.out) {
				muteSoloGainSlewed = muteSoloGainSlewer.process(gInfo->sampleTime, muteSoloGainSlewed);
			}
			
			taps[24] = taps[16] * muteSoloGainSlewed;
			taps[25] = taps[17] * muteSoloGainSlewed;
				
			mix[0] += taps[24];
			mix[1] += taps[25];
		}
	}
	
	
	void dataToJson(json_t *rootJ) {
		// none
	}
	
	void dataFromJson(json_t *rootJ) {
		// none
		
		// extern must call resetNonJson()
	}
};// struct MixerAux



//*****************************************************************************


#endif