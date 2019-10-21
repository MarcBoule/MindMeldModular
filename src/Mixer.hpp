//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
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
	ENUMS(TRACK_SOLO_PARAMS, 16),// must follow GROUP_MUTE_PARAMS since code assumes contiguous
	ENUMS(GROUP_SOLO_PARAMS, 4),// must follow TRACK_SOLO_PARAMS since code assumes contiguous
	MAIN_MUTE_PARAM,// must follow GROUP_SOLO_PARAMS since code assumes contiguous
	MAIN_DIM_PARAM,// must follow MAIN_MUTE_PARAM since code assumes contiguous
	MAIN_MONO_PARAM,// must follow MAIN_DIM_PARAM since code assumes contiguous
	MAIN_FADER_PARAM,
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
	NUM_INPUTS
};


enum OutputIds {
	ENUMS(DIRECT_OUTPUTS, 3), // Track 1-8, Track 9-16, Groups and Aux
	ENUMS(MAIN_OUTPUTS, 2),
	ENUMS(INSERT_TRACK_OUTPUTS, 2),
	INSERT_GRP_AUX_OUTPUT,
	FADE_CV_OUTPUT,
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
	ENUMS(AFM_AUX_SENDS, 40), // Trk1L, Trk1R, Trk2L, Trk2R ... Trk16L, Trk16R, Grp1L, Grp1R ... Grp4L, Grp4R
	ENUMS(AFM_AUX_VUS, 8), // A-L, A-R,  B-L, B-R, etc
	ENUMS(AFM_TRACK_GROUP_NAMES, 16 + 4),
	AFM_UPDATE_SLOW, // (track/group names, panelTheme, colorAndCloak)
	AFM_PANEL_THEME,
	AFM_COLOR_AND_CLOAK,
	AFM_DIRECT_AND_PAN_MODES,
	AFM_TRACK_MOVE,
	AFM_AUXSENDMUTE_GROUPED_RETURN,
	AFM_TRK_AUX_SEND_MUTED_WHEN_GROUPED,// mute aux send of track when it is grouped and that group is muted
	ENUMS(AFM_TRK_DISP_COL, 5),// 4 tracks per dword, 4 groups in last dword
	AFM_ECO_MODE,
	AFM_NUM_VALUES
};

enum MotherFromAuxIds { // for expander messages from aux panel to main
	ENUMS(MFA_AUX_RETURNS, 8), // left A, B, C, D, right A, B, C, D
	MFA_VALUE12_INDEX,// a return-related value, 12 of such values to bring back to main, one per sample
	MFA_VALUE12,
	MFA_AUX_DIR_OUTS,// direct outs modes for all four aux
	MFA_AUX_STEREO_PANS,// stereo pan modes for all four aux
	ENUMS(MFA_AUX_RET_FADER, 4),
	ENUMS(MFA_AUX_RET_PAN, 4),// must be contiguous with MFA_AUX_RET_FADER
	MFA_NUM_VALUES
};



//*****************************************************************************

// Math

// Calculate std::sin(theta) using MacLaurin series
// Calculate std::cos(theta) using cos(x) = sin(Pi/2 - x)
// Assumes: 0 <= theta <= Pi/2
static inline void sinCos(float *destSin, float *destCos, float theta) {
	*destSin = theta + std::pow(theta, 3) * (-0.166666667f + theta * theta * 0.00833333333f);
	theta = M_PI_2 - theta;
	*destCos = theta + std::pow(theta, 3) * (-0.166666667f + theta * theta * 0.00833333333f);
}
static inline void sinCosSqrt2(float *destSin, float *destCos, float theta) {
	sinCos(destSin, destCos, theta);
	*destSin *= M_SQRT2;
	*destCos *= M_SQRT2;
}


// Utility

float updateFadeGain(float fadeGain, float target, float *fadeGainX, float timeStepX, float shape, bool symmetricalFade);


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
	
	void reset();
};


//*****************************************************************************


enum ccIds {
	cloakedMode, // turn off track VUs only, keep master VUs (also called "Cloaked mode"), this has only two values, 0x0 and 0xFF so that it can be used in bit mask operations
	vuColorGlobal, // 0 is green, 1 is blue, 2 is purple, 3 is individual colors for each track/group/master (every user of vuColor must first test for != 3 before using as index into color table, or else array overflow)
	dispColor, // 0 is yellow, 1 is light-gray, 2 is green, 3 is aqua, 4 is cyan, 5 is blue, 6 is purple, 7 is per track
	detailsShow // bit 0 is knob param arc, bit 1 is knob cv arc, bit 2 is fader cv pointer
};
union PackedBytes4 {
	int32_t cc1;
	int8_t cc4[4];
};

// managed by Mixer, not by tracks (tracks read only)
struct GlobalInfo {
	
	// constants
	static const int trkAndGrpFaderScalingExponent = 3.0f; // for example, 3.0f is x^3 scaling
	static constexpr float trkAndGrpFaderMaxLinearGain = 2.0f; // for example, 2.0f is +6 dB
	static const int individualAuxSendScalingExponent = 2; // for example, 3 is x^3 scaling
	static constexpr float individualAuxSendMaxLinearGain = 1.0f; // for example, 2.0f is +6 dB
	static const int globalAuxSendScalingExponent = 2; // for example, 2 is x^2 scaling
	static constexpr float globalAuxSendMaxLinearGain = 4.0f; // for example, 2.0f is +6 dB
	static const int globalAuxReturnScalingExponent = 3.0f; // for example, 3.0f is x^3 scaling
	static constexpr float globalAuxReturnMaxLinearGain = 2.0f; // for example, 2.0f is +6 dB
	static constexpr float antipopSlewFast = 125.0f;// for mute/solo
	static constexpr float antipopSlewSlow = 25.0f;// for pan/fader
	static constexpr float minFadeRate = 0.1f;
	
	
	// need to save, no reset
	// none
	
	// need to save, with reset
	int panLawMono;// +0dB (no compensation),  +3 (equal power),  +4.5 (compromize),  +6dB (linear, default)
	int8_t panLawStereo;// Stereo balance linear (default), Stereo balance equal power (+3dB boost since one channel lost),  True pan (linear redistribution but is not equal power), Per-track
	int8_t directOutsMode;// 0 is pre-insert, 1 is post-insert, 2 is post-fader, 3 is post-solo, 4 is per-track choice
	int8_t auxSendsMode;// 0 is pre-insert, 1 is post-insert, 2 is post-fader, 3 is post-solo, 4 is per-track choice
	int muteTrkSendsWhenGrpMuted;//  0 = no, 1 = yes
	int auxReturnsMutedWhenMainSolo;
	int auxReturnsSolosMuteDry;
	int chainMode;// 0 is pre-master, 1 is post-master
	PackedBytes4 colorAndCloak;// see enum above called ccIds for fields
	int groupUsage[4];// bit 0 of first element shows if first track mapped to first group, etc... managed by MixerTrack except for onReset()
	bool symmetricalFade;
	bool fadeCvOutsWithVolCv;
	unsigned long linkBitMask;// 20 bits for 16 tracks (trk1 = lsb) and 4 groups (grp4 = msb)
	int8_t filterPos;// 0 = pre insert, 1 = post insert, 2 = per track
	int8_t groupedAuxReturnFeedbackProtection;
	uint16_t ecoMode;// all 1's means yes, 0 means no

	// no need to save, with reset
	unsigned long soloBitMask;// when = 0ul, nothing to do, when non-zero, a track must check its solo to see if it should play
	int returnSoloBitMask;
	float sampleTime;
	float oldFaders[16 + 4];

	// no need to save, no reset
	Param *paMute;// all 20 solos are here (track and group)
	Param *paSolo;// all 20 solos are here (track and group)
	Param *paFade;// all 20 faders are here (track and group)
	float *values12;
	float maxTGFader;
	float fadeRates[16 + 4];// reset and json done in tracks and groups. fade rates for tracks and groups

	
	bool isLinked(int index) {return (linkBitMask & (1 << index)) != 0;}
	void clearLinked(int index) {linkBitMask &= ~(1 << index);}
	void setLinked(int index) {linkBitMask |= (1 << index);}
	void setLinked(int index, bool state) {if (state) setLinked(index); else clearLinked(index);}
	void toggleLinked(int index) {linkBitMask ^= (1 << index);}
	
	// track and group solos
	void updateSoloBitMask() {
		soloBitMask = 0ul;
		for (unsigned long trkOrGrp = 0; trkOrGrp < 20; trkOrGrp++) {
			updateSoloBit(trkOrGrp);
		}
	}
	void updateSoloBit(unsigned long trkOrGrp) {
		if (trkOrGrp < 16) {
			if (paSolo[trkOrGrp].getValue() > 0.5f) {
				soloBitMask |= (1 << trkOrGrp);
			}
			else {
				soloBitMask &= ~(1 << trkOrGrp);
			}	
		}
		else {// trkOrGrp >= 16
			if (paSolo[trkOrGrp].getValue() > 0.5f) {
				soloBitMask |= (1 << trkOrGrp);
			}
			else {
				soloBitMask &= ~(1 << trkOrGrp);
			}
		}
	}		
			
	
	// aux return solos
	void updateReturnSoloBits() {
		int newReturnSoloBitMask = 0;
		for (int aux = 0; aux < 4; aux++) {
			if (values12[4 + aux] > 0.5f) {
				newReturnSoloBitMask |= (1 << aux);
			}
		}
		returnSoloBitMask = newReturnSoloBitMask;
	}
		
	// linked faders
	void processLinked(int trgOrGrpNum, float slowGain) {
		if (slowGain != oldFaders[trgOrGrpNum]) {
			if (linkBitMask != 0l && isLinked(trgOrGrpNum) && oldFaders[trgOrGrpNum] != -100.0f) {
				float delta = slowGain - oldFaders[trgOrGrpNum];
				for (int i = 0; i < 16 + 4; i++) {
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

	// linked fade
	void fadeOtherLinkedTracks(int trkOrGrpNum, float newTarget) {
		if ((linkBitMask == 0l) || !isLinked(trkOrGrpNum)) {
			return;
		}
		for (int trkOrGrp = 0; trkOrGrp < 16 + 4; trkOrGrp++) {
			if (trkOrGrp != trkOrGrpNum && isLinked(trkOrGrp) && fadeRates[trkOrGrp] >= minFadeRate) {
				if (newTarget > 0.5f && paMute[trkOrGrp].getValue() > 0.5f) {
					paMute[trkOrGrp].setValue(0.0f);
				}
				if (newTarget < 0.5f && paMute[trkOrGrp].getValue() < 0.5f) {
					paMute[trkOrGrp].setValue(1.0f);
				}
			}
		}		
	}
	
	void construct(Param *_params, float* _values12);
	void onReset();
	void resetNonJson();
	void dataToJson(json_t *rootJ);
	void dataFromJson(json_t *rootJ);
};// struct GlobalInfo


//*****************************************************************************


struct MixerMaster {
	// Constants
	static const int masterFaderScalingExponent = 3; 
	static constexpr float masterFaderMaxLinearGain = 2.0f;
	
	// need to save, no reset
	// none
	
	// need to save, with reset
	bool dcBlock;
	int clipping; // 0 is soft, 1 is hard (must be single ls bit)
	float fadeRate; // mute when < minFadeRate, fade when >= minFadeRate. This is actually the fade time in seconds
	float fadeProfile; // exp when +100, lin when 0, log when -100
	int8_t vuColorThemeLocal;
	int8_t dispColorLocal;
	float dimGain;// slider uses this gain, but displays it in dB instead of linear
	char masterLabel[7];
	
	// no need to save, with reset
	private:
	simd::float_4 chainGainsAndMute;// 0=L, 1=R, 2=mute
	float faderGain;
	simd::float_4 gainMatrix;// L, R, RinL, LinR (used for fader-mono block)
	dsp::TSlewLimiter<simd::float_4> gainMatrixSlewers;
	dsp::TSlewLimiter<simd::float_4> chainGainAndMuteSlewers;// chain gains are [0] and [1], mute is [2], unused is [3]
	OnePoleFilter dcBlocker[2];// 6dB/oct
	float oldFader;
	public:
	VuMeterAllDual vu;// use mix[0..1]
	float fadeGain; // target of this gain is the value of the mute/fade button's param (i.e. 0.0f or 1.0f)
	float fadeGainX;
	float fadeGainScaled;
	float paramWithCV;
	float dimGainIntegerDB;// corresponds to dimGain, converted to dB, then rounded, then back to linear
	float target;

	// no need to save, no reset
	GlobalInfo *gInfo;
	Param *params;
	Input *inChain;
	Input *inVol;

	float calcFadeGain() {return params[MAIN_MUTE_PARAM].getValue() > 0.5f ? 0.0f : 1.0f;}


	void construct(GlobalInfo *_gInfo, Param *_params, Input *_inputs);
	void onReset();
	void resetNonJson();
	void dataToJson(json_t *rootJ);
	void dataFromJson(json_t *rootJ);
	
	
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
	
	
	// Soft-clipping polynomial function
	// piecewise portion that handles inputs between 6 and 12 V
	// unipolar only, caller must take care of signs
	// assumes that 6 <= x <= 12
	// assumes f(x) := x  when x < 6
	// assumes f(x) := 12  when x > 12
	//
	// Chosen polynomial:
	// f(x) := a + b*x + c*x^2 + d*x^3
	//
	// Coefficient solving constraints:
	// f(6) = 6
	// f(12) = 10
	// f'(6) = 1 (unity slope at 6V)
	// f'(12) = 0 (horizontal at 12V)
	// where:
	// f'(x) := b + 2*c*x + 3*d*x^2
	// 
	// solve(system(f(6)=6,f(12)=10,f'(6)=1,f'(12)=0),{a,b,c,d})
	// 
	// solution:
	// a=2 and b=0 and c=(1/6) and d=(-1/108)
	
	float clipPoly(float inX) {
		return 2.0f + inX * inX * (1.0f/6.0f - inX * (1.0f/108.0f));
	}
	
	float clip(float inX) {// 0 = soft, 1 = hard
		if (inX <= 6.0f && inX >= -6.0f) {
			return inX;
		}
		if (clipping == 1) {// hard clip
			return clamp(inX, -10.0f, 10.0f);
		}
		// here clipping is 0, so do soft clip
		inX = clamp(inX, -12.0f, 12.0f);
		if (inX >= 0.0f)
			return clipPoly(inX);
		return -clipPoly(-inX);
	}


	void updateSlowValues() {		
		// calc ** chainGains **
		chainGainsAndMute[0] = inChain[0].isConnected() ? 1.0f : 0.0f;
		chainGainsAndMute[1] = inChain[1].isConnected() ? 1.0f : 0.0f;
	}
	
	
	void process(float *mix, bool eco) {// master
		// takes mix[0..1] and redeposits post in same place
		
		if (eco) {
			// calc ** fadeGain, fadeGainX, fadeGainScaled **
			if (fadeRate >= GlobalInfo::minFadeRate) {// if we are in fade mode
				float newTarget = calcFadeGain();
				if (!gInfo->symmetricalFade && newTarget != target) {
					fadeGainX = 0.0f;
				}
				target = newTarget;
				if (fadeGain != target) {
					float deltaX = (gInfo->sampleTime / fadeRate) * (1 + (gInfo->ecoMode & 0x3));// last value is sub refresh
					fadeGain = updateFadeGain(fadeGain, target, &fadeGainX, deltaX, fadeProfile, gInfo->symmetricalFade);
					fadeGainScaled = std::pow(fadeGain, masterFaderScalingExponent);
				}
			}
			else {// we are in mute mode
				fadeGain = calcFadeGain(); // do manually below to optimized fader mult
				fadeGainX = gInfo->symmetricalFade ? fadeGain : 0.0f;
				fadeGainScaled = fadeGain;// no pow needed here since 0.0f or 1.0f
			}	
			// dim (this affects fadeGainScaled only, so treated like a partial mute, but no effect on fade pointers or other just effect on sound
			if (params[MAIN_DIM_PARAM].getValue() > 0.5f) {
				fadeGainScaled *= dimGainIntegerDB;
			}
			chainGainsAndMute[2] = fadeGainScaled;
			
			// calc ** fader, paramWithCV **
			float fader = params[MAIN_FADER_PARAM].getValue();
			if (inVol->isConnected() && inVol->getChannels() >= 12) {
				fader *= clamp(inVol->getVoltage(11) * 0.1f, 0.0f, 1.0f);//(multiplying, pre-scaling)
				paramWithCV = fader;
			}
			else {
				paramWithCV = -1.0f;
			}

			// scaling
			if (fader != oldFader) {
				oldFader = fader;
				faderGain = std::pow(fader, masterFaderScalingExponent);
			}
			
			// calc ** gainMatrix **
			// mono
			if (params[MAIN_MONO_PARAM].getValue() > 0.5f) {
				gainMatrix = simd::float_4(0.5f * faderGain);
			}
			else {
				gainMatrix = simd::float_4(faderGain, faderGain, 0.0f, 0.0f);
			}
		}
		
		// Calc master gain with slewer
		if (movemask(gainMatrix == gainMatrixSlewers.out) != 0xF) {// movemask returns 0xF when 4 floats are equal
			gainMatrixSlewers.process(gInfo->sampleTime, gainMatrix);
		}

		// Calc gains for chain input (with antipop when signal connect, impossible for disconnect)
		if (movemask(chainGainsAndMute == chainGainAndMuteSlewers.out) != 0xF) {// movemask returns 0xF when 4 floats are equal
			chainGainAndMuteSlewers.process(gInfo->sampleTime, chainGainsAndMute);
		}
		
		// Chain inputs when pre master
		if (gInfo->chainMode == 0) {
			if (chainGainAndMuteSlewers.out[0] != 0.0f) {
				mix[0] += inChain[0].getVoltage() * chainGainAndMuteSlewers.out[0];
			}
			if (chainGainAndMuteSlewers.out[1] != 0.0f) {
				mix[1] += inChain[1].getVoltage() * chainGainAndMuteSlewers.out[1];
			}
		}

		// Apply gainMatrixSlewed
		simd::float_4 sigs(mix[0], mix[1], mix[1], mix[0]);
		sigs = sigs * gainMatrixSlewers.out * chainGainAndMuteSlewers.out[2];
		
		// Set mix
		mix[0] = sigs[0] + sigs[2];
		mix[1] = sigs[1] + sigs[3];
		
		// VUs (no cloaked mode for master, always on)
		if (eco) {
			vu.process(gInfo->sampleTime * (1 + (gInfo->ecoMode & 0x3)), mix);
		}
				
		// Chain inputs when post master
		if (gInfo->chainMode == 1) {
			if (chainGainAndMuteSlewers.out[0] != 0.0f) {
				mix[0] += inChain[0].getVoltage() * chainGainAndMuteSlewers.out[0];
			}
			if (chainGainAndMuteSlewers.out[1] != 0.0f) {
				mix[1] += inChain[1].getVoltage() * chainGainAndMuteSlewers.out[1];
			}
		}
		
		// DC blocker (post VU)
		if (dcBlock) {
			mix[0] = dcBlocker[0].processHP(mix[0]);
			mix[1] = dcBlocker[1].processHP(mix[1]);
		}
		
		// Clipping (post VU, so that we can see true range)
		mix[0] = clip(mix[0]);
		mix[1] = clip(mix[1]);
	}		
};// struct MixerMaster



//*****************************************************************************



struct MixerGroup {
	// Constants
	// none
	
	// need to save, no reset
	// none
	
	// need to save, with reset
	float* fadeRate; // mute when < minFadeRate, fade when >= minFadeRate. This is actually the fade time in seconds
	float fadeProfile; // exp when +100, lin when 0, log when -100
	int8_t directOutsMode;// when per track
	int8_t auxSendsMode;// when per track
	int8_t panLawStereo;// when per track
	int8_t vuColorThemeLocal;
	int8_t dispColorLocal;

	// no need to save, with reset
	private:
	simd::float_4 panMatrix;
	float faderGain;
	simd::float_4 gainMatrix;	
	dsp::TSlewLimiter<simd::float_4> gainMatrixSlewers;
	dsp::SlewLimiter muteSoloGainSlewer;
	float oldPan;
	float oldFader;
	PackedBytes4 oldPanSignature;// [0] is pan stereo local, [1] is pan stereo global, [2] is pan mono global
	public:
	VuMeterAllDual vu;// use post[]
	float fadeGain; // target of this gain is the value of the mute/fade button's param (i.e. 0.0f or 1.0f)
	float fadeGainX;
	float fadeGainScaled;
	float paramWithCV;
	float panWithCV;
	float target = -1.0f;
	
	// no need to save, no reset
	int groupNum;// 0 to 3
	std::string ids;
	GlobalInfo *gInfo;
	Input *inInsert;
	Input *inVol;
	Input *inPan;
	Param *paFade;
	Param *paMute;
	Param *paPan;
	char  *groupName;// write 4 chars always (space when needed), no null termination since all tracks names are concat and just one null at end of all
	float *taps;// [0],[1]: pre-insert L R; [32][33]: pre-fader L R, [64][65]: post-fader L R, [96][97]: post-mute-solo L R

	float calcFadeGain() {return paMute->getValue() > 0.5f ? 0.0f : 1.0f;}
	bool isLinked() {return gInfo->isLinked(16 + groupNum);}
	void toggleLinked() {gInfo->toggleLinked(16 + groupNum);}


	void construct(int _groupNum, GlobalInfo *_gInfo, Input *_inputs, Param *_params, char* _groupName, float* _taps);
	void onReset();
	void resetNonJson();
	void dataToJson(json_t *rootJ);
	void dataFromJson(json_t *rootJ);
	
	
	void updateSlowValues() {
		// ** process linked **
		gInfo->processLinked(16 + groupNum, paFade->getValue());
		
		// ** detect pan mode change ** (and trigger recalc of panMatrix)
		PackedBytes4 newPanSig;
		newPanSig.cc4[0] = panLawStereo;
		newPanSig.cc4[1] = gInfo->panLawStereo;
		newPanSig.cc4[2] = gInfo->panLawMono;
		newPanSig.cc4[3] = 0xFF;
		if (newPanSig.cc1 != oldPanSignature.cc1) {
			oldPan = -10.0f;
			oldPanSignature.cc1 = newPanSig.cc1;
		}
	}
	

	void process(float *mix, bool eco) {// group
		// Tap[0],[1]: pre-insert (group inputs)
		// nothing to do, already set up by the mix master
		
		// Tap[8],[9]: pre-fader (post insert)
		if (inInsert->isConnected()) {
			taps[8] = inInsert->getVoltage((groupNum << 1) + 0);
			taps[9] = inInsert->getVoltage((groupNum << 1) + 1);
		}
		else {
			taps[8] = taps[0];
			taps[9] = taps[1];
		}

		if (eco) {	
			// calc ** fadeGain, fadeGainX, fadeGainScaled **
			if (*fadeRate >= GlobalInfo::minFadeRate) {// if we are in fade mode
				float newTarget = calcFadeGain();
				if (newTarget != target) {
					if (!gInfo->symmetricalFade) {
						fadeGainX = 0.0f;
					}
					gInfo->fadeOtherLinkedTracks(groupNum + 16, newTarget);
					target = newTarget;
				}
				if (fadeGain != target) {
					float deltaX = (gInfo->sampleTime / *fadeRate) * (1 + (gInfo->ecoMode & 0x3));// last value is sub refresh
					fadeGain = updateFadeGain(fadeGain, target, &fadeGainX, deltaX, fadeProfile, gInfo->symmetricalFade);
					fadeGainScaled = std::pow(fadeGain, GlobalInfo::trkAndGrpFaderScalingExponent);
				}
			}
			else {// we are in mute mode
				fadeGain = calcFadeGain();
				fadeGainX = gInfo->symmetricalFade ? fadeGain : 0.0f;
				fadeGainScaled = fadeGain;// no pow needed here since 0.0f or 1.0f
			}

			// calc ** fader, paramWithCV **
			float fader = paFade->getValue();
			if (inVol->isConnected()) {
				fader *= clamp(inVol->getVoltage() * 0.1f, 0.0f, 1.0f);//(multiplying, pre-scaling)
				paramWithCV = fader;
			}
			else {
				paramWithCV = -1.0f;
			}

			// calc ** pan, panWithCV **
			float pan = paPan->getValue();
			if (inPan->isConnected()) {
				pan += inPan->getVoltage() * 0.1f;// CV is a -5V to +5V input
				pan = clamp(pan, 0.0f, 1.0f);
				panWithCV = pan;
			}
			else {
				panWithCV = -1.0f;
			}

			// calc ** panMatrix **
			if (pan != oldPan) {
				panMatrix = simd::float_4::zero();// L, R, RinL, LinR (used for fader-pan block)
				if (pan == 0.5f) {
					panMatrix[1] = 1.0f;
					panMatrix[0] = 1.0f;
				}
				else {		
					// implicitly stereo for groups
					int stereoPanMode = (gInfo->panLawStereo < 3 ? gInfo->panLawStereo : panLawStereo);
					if (stereoPanMode == 0) {
						// Stereo balance linear, (+0 dB), same as mono No compensation
						panMatrix[1] = std::min(1.0f, pan * 2.0f);
						panMatrix[0] = std::min(1.0f, 2.0f - pan * 2.0f);
					}
					else if (stereoPanMode == 1) {
						// Stereo balance equal power (+3dB), same as mono Equal power
						// panMatrix[1] = std::sin(pan * M_PI_2) * M_SQRT2;
						// panMatrix[0] = std::cos(pan * M_PI_2) * M_SQRT2;
						sinCosSqrt2(&panMatrix[1], &panMatrix[0], pan * M_PI_2);
					}
					else {
						// True panning, equal power
						// panMatrix[1] = pan >= 0.5f ? (1.0f) : (std::sin(pan * M_PI));
						// panMatrix[2] = pan >= 0.5f ? (0.0f) : (std::cos(pan * M_PI));
						// panMatrix[0] = pan <= 0.5f ? (1.0f) : (std::cos((pan - 0.5f) * M_PI));
						// panMatrix[3] = pan <= 0.5f ? (0.0f) : (std::sin((pan - 0.5f) * M_PI));
						if (pan > 0.5f) {
							panMatrix[1] = 1.0f;
							panMatrix[2] = 0.0f;
							sinCos(&panMatrix[3], &panMatrix[0], (pan - 0.5f) * M_PI);
						}
						else {// must be < (not <= since = 0.5 is caught at above)
							sinCos(&panMatrix[1], &panMatrix[2], pan * M_PI);
							panMatrix[0] = 1.0f;
							panMatrix[3] = 0.0f;
						}
					}
				}
			}
			// calc ** faderGain **
			if (fader != oldFader) {
				faderGain = std::pow(fader, GlobalInfo::trkAndGrpFaderScalingExponent);// scaling
			}
			// calc ** gainMatrix **
			if (fader != oldFader || pan != oldPan) {
				oldFader = fader;
				oldPan = pan;	
				gainMatrix = panMatrix * faderGain;
			}
		}
	
		// Calc group gains with slewer
		if (movemask(gainMatrix == gainMatrixSlewers.out) != 0xF) {// movemask returns 0xF when 4 floats are equal
			gainMatrixSlewers.process(gInfo->sampleTime, gainMatrix);
		}
		
		// Apply gainMatrixSlewed
		simd::float_4 sigs(taps[8], taps[9], taps[9], taps[8]);
		sigs = sigs * gainMatrixSlewers.out;
		
		taps[16] = sigs[0] + sigs[2];
		taps[17] = sigs[1] + sigs[3];
		
		// Calc muteSoloGainSlewed (solo not actually in here but in groups)
		if (fadeGainScaled != muteSoloGainSlewer.out) {
			muteSoloGainSlewer.process(gInfo->sampleTime, fadeGainScaled);
		}
		
		taps[24] = taps[16] * muteSoloGainSlewer.out;
		taps[25] = taps[17] * muteSoloGainSlewer.out;
			
		// Add to mix
		mix[0] += taps[24];
		mix[1] += taps[25];
		
		// VUs
		if (gInfo->colorAndCloak.cc4[cloakedMode] != 0) {
			vu.reset();
		}
		else if (eco) {
			vu.process(gInfo->sampleTime * (1 + (gInfo->ecoMode & 0x3)), &taps[24]);
		}
	}
};// struct MixerGroup



//*****************************************************************************


struct MixerTrack {
	// Constants
	static constexpr float minHPFCutoffFreq = 20.0f;
	static constexpr float maxLPFCutoffFreq = 20000.0f;
	static constexpr float hpfBiquadQ = 1.0f;// 1.0 Q since preceeeded by a one pole filter to get 18dB/oct
	
	// need to save, no reset
	// none
	
	// need to save, with reset
	float gainAdjust;// this is a gain here (not dB)
	float* fadeRate; // mute when < minFadeRate, fade when >= minFadeRate. This is actually the fade time in seconds
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
	int8_t dispColorLocal;


	// no need to save, with reset
	private:
	bool stereo;// pan coefficients use this, so set up first
	float inGain;
	simd::float_4 panMatrix;
	float faderGain;
	simd::float_4 gainMatrix;	
	dsp::TSlewLimiter<simd::float_4> gainMatrixSlewers;
	dsp::SlewLimiter inGainSlewer;
	dsp::SlewLimiter muteSoloGainSlewer;
	OnePoleFilter hpPreFilter[2];// 6dB/oct
	dsp::BiquadFilter hpFilter[2];// 12dB/oct
	dsp::BiquadFilter lpFilter[2];// 12db/oct
	float oldPan;
	float oldFader;
	PackedBytes4 oldPanSignature;// [0] is pan stereo local, [1] is pan stereo global, [2] is pan mono global
	public:
	VuMeterAllDual vu;// use post[]
	float fadeGain; // target of this gain is the value of the mute/fade button's param (i.e. 0.0f or 1.0f)
	float fadeGainX;
	float fadeGainScaled;
	float paramWithCV;
	float panWithCV;
	float volCv;
	float target;

	// no need to save, no reset
	int trackNum;
	std::string ids;
	GlobalInfo *gInfo;
	Input *inSig;
	Input *inInsert;
	Input *inVol;
	Input *inPan;
	Param *paGroup;// 0.0 is no group (i.e. deposit post in mix[0..1]), 1.0 to 4.0 is group (i.e. deposit in groupTaps[2*g..2*g+1]. Always use setter since need to update gInfo!
	Param *paFade;
	Param *paMute;
	Param *paSolo;
	Param *paPan;
	char  *trackName;// write 4 chars always (space when needed), no null termination since all tracks names are concat and just one null at end of all
	float *taps;// [0],[1]: pre-insert L R; [32][33]: pre-fader L R, [64][65]: post-fader L R, [96][97]: post-mute-solo L R
	float* groupTaps;// [0..1] tap 0 of group 1, [1..2] tap 0 of group 2, etc.
	float *insertOuts;// [0][1]: insert outs for this track
	bool oldInUse = true;
	float fader = 0.0f;// this is set only in process() when eco, and also used only when eco in another section of this method
	float pan = 0.5f;// this is set only in process() when eco, and also used only when eco in another section of this method


	float calcFadeGain() {return paMute->getValue() > 0.5f ? 0.0f : 1.0f;}
	bool isLinked() {return gInfo->isLinked(trackNum);}
	void toggleLinked() {gInfo->toggleLinked(trackNum);}


	void construct(int _trackNum, GlobalInfo *_gInfo, Input *_inputs, Param *_params, char* _trackName, float* _taps, float* _groupTaps, float* _insertOuts);
	void onReset();
	void resetNonJson();
	void dataToJson(json_t *rootJ);
	void dataFromJson(json_t *rootJ);
		
	// level 1 read and write
	void write(TrackSettingsCpBuffer *dest);
	void read(TrackSettingsCpBuffer *src);
	// level 2 read and write
	void write2(TrackSettingsCpBuffer *dest);
	void read2(TrackSettingsCpBuffer *src);
	
	
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
		if (gInfo->soloBitMask == 0ul) {// no track nor groups are soloed 
			return 1.0f;
		}
		// here at least one track or group is soloed
		int group = (int)(paGroup->getValue() + 0.5f);
		if ( ((gInfo->soloBitMask & (1 << trackNum)) != 0) ) {// if this track is soloed
			if (group == 0 || ((gInfo->soloBitMask & 0xF0000) == 0)) {// not grouped, or grouped but no groups are soloed, play
				return 1.0f;
			}
			// grouped and at least one group is soloed, so play only if its group is itself soloed
			return ( (gInfo->soloBitMask & (1ul << (16 + group - 1))) != 0ul ? 1.0f : 0.0f);
		}
		// here this track is not soloed
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

	
	void updateSlowValues() {
		// ** update group usage **
		updateGroupUsage();
		
		// calc ** stereo **
		bool newStereo = inSig[1].isConnected();
		if (stereo != newStereo) {
			stereo = newStereo;
			oldPan = -10.0f;
		}
		
		// ** detect pan mode change ** (and trigger recalc of panMatrix)
		PackedBytes4 newPanSig;
		newPanSig.cc4[0] = panLawStereo;
		newPanSig.cc4[1] = gInfo->panLawStereo;
		newPanSig.cc4[2] = gInfo->panLawMono;
		newPanSig.cc4[3] = 0xFF;
		if (newPanSig.cc1 != oldPanSignature.cc1) {
			oldPan = -10.0f;
			oldPanSignature.cc1 = newPanSig.cc1;
		}

		// calc ** inGain **
		inGain = inSig[0].isConnected() ? gainAdjust : 0.0f;

		// ** process linked **
		gInfo->processLinked(trackNum, paFade->getValue());
	}
	
	
	void process(float *mix, bool eco) {// track		
		if (eco) {
			// calc ** fadeGain, fadeGainX, fadeGainScaled **
			if (*fadeRate >= GlobalInfo::minFadeRate) {// if we are in fade mode
				float newTarget = calcFadeGain();
				if (newTarget != target) {
					if (!gInfo->symmetricalFade) {
						fadeGainX = 0.0f;
					}
					gInfo->fadeOtherLinkedTracks(trackNum, newTarget);
					target = newTarget;
				}
				if (fadeGain != target) {
					float deltaX = (gInfo->sampleTime / *fadeRate) * (1 + (gInfo->ecoMode & 0x3));// last value is sub refresh
					fadeGain = updateFadeGain(fadeGain, target, &fadeGainX, deltaX, fadeProfile, gInfo->symmetricalFade);
					fadeGainScaled = std::pow(fadeGain, GlobalInfo::trkAndGrpFaderScalingExponent);
				}
			}
			else {// we are in mute mode
				fadeGain = calcFadeGain();
				fadeGainX = gInfo->symmetricalFade ? fadeGain : 0.0f;
				fadeGainScaled = fadeGain;// no pow needed here since 0.0f or 1.0f
			}
			fadeGainScaled *= calcSoloGain();

			// calc ** fader, paramWithCV, volCv **
			fader = paFade->getValue();
			if (inVol->isConnected()) {
				volCv = clamp(inVol->getVoltage() * 0.1f, 0.0f, 1.0f);//(multiplying, pre-scaling)
				fader *= volCv;
				paramWithCV = fader;
			}
			else {
				volCv = 1.0f;
				paramWithCV = -1.0f;
			}

			// calc ** pan, panWithCV **
			pan = paPan->getValue();
			if (inPan->isConnected()) {
				pan += inPan->getVoltage() * 0.1f;// CV is a -5V to +5V input
				pan = clamp(pan, 0.0f, 1.0f);
				panWithCV = pan;
			}
			else {
				panWithCV = -1.0f;
			}
		}


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
				oldInUse = false;
			}
			return;
		}
		oldInUse = true;
			
		// Calc inGainSlewed
		if (inGain != inGainSlewer.out) {
			inGainSlewer.process(gInfo->sampleTime, inGain);
		}
		
		// Tap[0],[1]: pre-insert (Inputs with gain adjust)
		taps[0] = (inSig[0].getVoltageSum() * inGainSlewer.out);
		taps[1] = stereo ? (inSig[1].getVoltageSum() * inGainSlewer.out) : taps[0];
		
		int insertPortIndex = trackNum >> 3;
		
		if (gInfo->filterPos == 1 || (gInfo->filterPos == 2 && filterPos == 1)) {// if filters post insert
			// Insert outputs
			insertOuts[0] = taps[0];
			insertOuts[1] = stereo ? taps[1] : 0.0f;// don't send to R of insert outs when mono
			
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
		
		if (eco) {
			// calc ** panMatrix **
			if (pan != oldPan) {
				panMatrix = simd::float_4::zero();// L, R, RinL, LinR (used for fader-pan block)
				if (pan == 0.5f) {
					if (!stereo) panMatrix[3] = 1.0f;
					else panMatrix[1] = 1.0f;
					panMatrix[0] = 1.0f;
				}
				else {		
					if (!stereo) {// mono
						if (gInfo->panLawMono == 3) {
							// Linear panning law (+6dB boost)
							panMatrix[3] = pan * 2.0f;
							panMatrix[0] = 2.0f - panMatrix[3];
						}
						else if (gInfo->panLawMono == 0) {
							// No compensation (+0dB boost)
							panMatrix[3] = std::min(1.0f, pan * 2.0f);
							panMatrix[0] = std::min(1.0f, 2.0f - pan * 2.0f);
						}
						else if (gInfo->panLawMono == 1) {
							// Equal power panning law (+3dB boost)
							// panMatrix[3] = std::sin(pan * M_PI_2) * M_SQRT2;
							// panMatrix[0] = std::cos(pan * M_PI_2) * M_SQRT2;
							sinCosSqrt2(&panMatrix[3], &panMatrix[0], pan * M_PI_2);
						}
						else {//if (gInfo->panLawMono == 2) {
							// Compromise (+4.5dB boost)
							// panMatrix[3] = std::sqrt( std::abs( std::sin(pan * M_PI_2) * M_SQRT2   *   (pan * 2.0f) ) );
							// panMatrix[0] = std::sqrt( std::abs( std::cos(pan * M_PI_2) * M_SQRT2   *   (2.0f - pan * 2.0f) ) );
							sinCosSqrt2(&panMatrix[3], &panMatrix[0], pan * M_PI_2);
							panMatrix[3] = std::sqrt( std::abs( panMatrix[3] * (pan * 2.0f) ) );
							panMatrix[0] = std::sqrt( std::abs( panMatrix[0] * (2.0f - pan * 2.0f) ) );
						}
					}
					else {// stereo
						int stereoPanMode = (gInfo->panLawStereo < 3 ? gInfo->panLawStereo : panLawStereo);			
						if (stereoPanMode == 0) {
							// Stereo balance linear, (+0 dB), same as mono No compensation
							panMatrix[1] = std::min(1.0f, pan * 2.0f);
							panMatrix[0] = std::min(1.0f, 2.0f - pan * 2.0f);
						}
						else if (stereoPanMode == 1) {
							// Stereo balance equal power (+3dB), same as mono Equal power
							// panMatrix[1] = std::sin(pan * M_PI_2) * M_SQRT2;
							// panMatrix[0] = std::cos(pan * M_PI_2) * M_SQRT2;
							sinCosSqrt2(&panMatrix[1], &panMatrix[0], pan * M_PI_2);
						}
						else {
							// True panning, equal power
							// panMatrix[1] = pan >= 0.5f ? (1.0f) : (std::sin(pan * M_PI));
							// panMatrix[2] = pan >= 0.5f ? (0.0f) : (std::cos(pan * M_PI));
							// panMatrix[0] = pan <= 0.5f ? (1.0f) : (std::cos((pan - 0.5f) * M_PI));
							// panMatrix[3] = pan <= 0.5f ? (0.0f) : (std::sin((pan - 0.5f) * M_PI));
							if (pan > 0.5f) {
								panMatrix[1] = 1.0f;
								panMatrix[2] = 0.0f;
								sinCos(&panMatrix[3], &panMatrix[0], (pan - 0.5f) * M_PI);
							}
							else {// must be < (not <= since = 0.5 is caught at above)
								sinCos(&panMatrix[1], &panMatrix[2], pan * M_PI);
								panMatrix[0] = 1.0f;
								panMatrix[3] = 0.0f;
							}
						}
					}
				}
			}
			// calc ** faderGain **
			if (fader != oldFader) {
				faderGain = std::pow(fader, GlobalInfo::trkAndGrpFaderScalingExponent);// scaling
			}
			// calc ** gainMatrix **
			if (fader != oldFader || pan != oldPan) {
				oldFader = fader;
				oldPan = pan;	
				gainMatrix = panMatrix * faderGain;
			}
		}
		
		// Calc gainMatrixSlewed
		if (movemask(gainMatrix == gainMatrixSlewers.out) != 0xF) {// movemask returns 0xF when 4 floats are equal
			gainMatrixSlewers.process(gInfo->sampleTime, gainMatrix);
		}
		
		// Tap[64],[65]: post-fader

		// Apply gainMatrixSlewed
		simd::float_4 sigs(taps[32], taps[33], taps[33], taps[32]);
		sigs *= gainMatrixSlewers.out;
		
		taps[64] = sigs[0] + sigs[2];
		taps[65] = sigs[1] + sigs[3];


		// Tap[96],[97]: post-mute-solo
		
		// Calc muteSoloGainSlewed
		if (fadeGainScaled != muteSoloGainSlewer.out) {
			muteSoloGainSlewer.process(gInfo->sampleTime, fadeGainScaled);
		}

		taps[96] = taps[64] * muteSoloGainSlewer.out;
		taps[97] = taps[65] * muteSoloGainSlewer.out;
			
		// Add to mix since gainMatrixSlewed can be non zero
		if (paGroup->getValue() < 0.5f) {
			mix[0] += taps[96];
			mix[1] += taps[97];
		}
		else {
			int groupIndex = (int)(paGroup->getValue() - 0.5f);
			groupTaps[(groupIndex << 1) + 0] += taps[96];
			groupTaps[(groupIndex << 1) + 1] += taps[97];
		}
		
		// VUs
		if (gInfo->colorAndCloak.cc4[cloakedMode] != 0) {
			vu.reset();
		}
		else if (eco) {
			vu.process(gInfo->sampleTime * (1 + (gInfo->ecoMode & 0x3)), &taps[96]);
		}
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
	simd::float_4 panMatrix;
	float faderGain;
	simd::float_4 gainMatrix;	
	dsp::TSlewLimiter<simd::float_4> gainMatrixSlewers;
	dsp::SlewLimiter muteSoloGainSlewer;
	float muteSoloGain; // value of the mute/fade * solo_enable
	float oldPan;
	float oldFader;
	PackedBytes4 oldPanSignature;// [0] is pan stereo local, [1] is pan stereo global, [2] is pan mono global
	public:

	// no need to save, no reset
	int auxNum;// 0 to 3
	std::string ids;
	GlobalInfo *gInfo;
	Input *inInsert;
	float *flMute;
	float *flSolo0;
	float *flGroup;
	float *taps;
	int8_t* panLawStereoLocal;

	int getAuxGroup() {return (int)(*flGroup + 0.5f);}


	void construct(int _auxNum, GlobalInfo *_gInfo, Input *_inputs, float* _values12, float* _taps, int8_t* _panLawStereoLocal);
	void onReset();
	void resetNonJson();
	void dataToJson(json_t *rootJ) {}
	void dataFromJson(json_t *rootJ) {}
	
	
	void updateSlowValues() {
		// calc ** muteSoloGain **
		float soloGain = (gInfo->returnSoloBitMask == 0 || (gInfo->returnSoloBitMask & (1 << auxNum)) != 0) ? 1.0f : 0.0f;
		muteSoloGain = (*flMute > 0.5f ? 0.0f : 1.0f) * soloGain;
		// Handle "Mute aux returns when soloing track"
		// i.e. add aux returns to mix when no solo, or when solo and don't want mutes aux returns
		if (gInfo->soloBitMask != 0 && gInfo->auxReturnsMutedWhenMainSolo) {
			muteSoloGain = 0.0f;
		}
		
		// ** detect pan mode change ** (and trigger recalc of panMatrix)
		PackedBytes4 newPanSig;
		newPanSig.cc4[0] = *panLawStereoLocal;
		newPanSig.cc4[1] = gInfo->panLawStereo;
		newPanSig.cc4[2] = gInfo->panLawMono;
		newPanSig.cc4[3] = 0xFF;
		if (newPanSig.cc1 != oldPanSignature.cc1) {
			oldPan = -10.0f;
			oldPanSignature.cc1 = newPanSig.cc1;
		}
		
	}
	
	void process(float *mix, float *auxRetFadePan, bool eco) {// mixer aux
		// auxRetFadePan[0] points fader value, auxRetFadePan[4] points pan value, all indexed for a given aux
		
		// Tap[0],[1]: pre-insert (aux inputs)
		// nothing to do, already set up by the auxspander
		
		// Tap[8],[9]: pre-fader (post insert)
		if (inInsert->isConnected()) {
			taps[8] = inInsert->getVoltage((auxNum << 1) + 8);
			taps[9] = inInsert->getVoltage((auxNum << 1) + 9);
		}
		else {
			taps[8] = taps[0];
			taps[9] = taps[1];
		}
		
		if (eco) {
			// calc ** panMatrix **
			float pan = auxRetFadePan[4];// cv input and clamping already done in auxspander
			if (pan != oldPan) {
				panMatrix = simd::float_4::zero();// L, R, RinL, LinR (used for fader-pan block)
				if (pan == 0.5f) {
					panMatrix[1] = 1.0f;
					panMatrix[0] = 1.0f;
				}
				else {	
					// implicitly stereo for aux
					int stereoPanMode = (gInfo->panLawStereo < 3 ? gInfo->panLawStereo : *panLawStereoLocal);
					if (stereoPanMode == 0) {
						// Stereo balance linear, (+0 dB), same as mono No compensation
						panMatrix[1] = std::min(1.0f, pan * 2.0f);
						panMatrix[0] = std::min(1.0f, 2.0f - pan * 2.0f);
					}
					else if (stereoPanMode == 1) {
						// Stereo balance equal power (+3dB), same as mono Equal power
						// panMatrix[1] = std::sin(pan * M_PI_2) * M_SQRT2;
						// panMatrix[0] = std::cos(pan * M_PI_2) * M_SQRT2;
						sinCosSqrt2(&panMatrix[1], &panMatrix[0], pan * M_PI_2);
					}
					else {
						// True panning, equal power
						// panMatrix[1] = pan >= 0.5f ? (1.0f) : (std::sin(pan * M_PI));
						// panMatrix[2] = pan >= 0.5f ? (0.0f) : (std::cos(pan * M_PI));
						// panMatrix[0] = pan <= 0.5f ? (1.0f) : (std::cos((pan - 0.5f) * M_PI));
						// panMatrix[3] = pan <= 0.5f ? (0.0f) : (std::sin((pan - 0.5f) * M_PI));
						if (pan > 0.5f) {
							panMatrix[1] = 1.0f;
							panMatrix[2] = 0.0f;
							sinCos(&panMatrix[3], &panMatrix[0], (pan - 0.5f) * M_PI);
						}
						else {// must be < (not <= since = 0.5 is caught at above)
							sinCos(&panMatrix[1], &panMatrix[2], pan * M_PI);
							panMatrix[0] = 1.0f;
							panMatrix[3] = 0.0f;
						}
					}
				}
			}
			// calc ** faderGain **
			if (auxRetFadePan[0] != oldFader) {
				// fader and fader cv input (multiplying and pre-scaling) both done in auxspander
				faderGain = std::pow(auxRetFadePan[0], GlobalInfo::globalAuxReturnScalingExponent);// scaling
			}
			// calc ** gainMatrix **
			if (auxRetFadePan[0] != oldFader || pan != oldPan) {
				oldFader = auxRetFadePan[0];
				oldPan = pan;	
				gainMatrix = panMatrix * faderGain;
			}			
		}
		
		// Calc gainMatrixSlewed
		if (movemask(gainMatrix == gainMatrixSlewers.out) != 0xF) {// movemask returns 0xF when 4 floats are equal
			gainMatrixSlewers.process(gInfo->sampleTime, gainMatrix);
		}
		
		// Apply gainMatrixSlewed
		simd::float_4 sigs(taps[8], taps[9], taps[9], taps[8]);
		sigs = sigs * gainMatrixSlewers.out;
		
		taps[16] = sigs[0] + sigs[2];
		taps[17] = sigs[1] + sigs[3];
		
		// Calc muteSoloGainSlewed
		if (muteSoloGain != muteSoloGainSlewer.out) {
			muteSoloGainSlewer.process(gInfo->sampleTime, muteSoloGain);
		}
		
		taps[24] = taps[16] * muteSoloGainSlewer.out;
		taps[25] = taps[17] * muteSoloGainSlewer.out;
			
		mix[0] += taps[24];
		mix[1] += taps[25];
	}
};// struct MixerAux



//*****************************************************************************


struct AuxspanderAux {
	// Constants
	static constexpr float minHPFCutoffFreq = 20.0f;
	static constexpr float maxLPFCutoffFreq = 20000.0f;
	static constexpr float hpfBiquadQ = 1.0f;// 1.0 Q since preceeeded by a one pole filter to get 18dB/oct
	
	// need to save, no reset
	// none
	
	// need to save, with reset
	private:
	float hpfCutoffFreq;// always use getter and setter since tied to Biquad
	float lpfCutoffFreq;// always use getter and setter since tied to Biquad
	public:

	// no need to save, with reset
	private:
	OnePoleFilter hpPreFilter[2];// 6dB/oct
	dsp::BiquadFilter hpFilter[2];// 12dB/oct
	dsp::BiquadFilter lpFilter[2];// 12db/oct
	public:

	// no need to save, no reset
	int auxNum;
	std::string ids;
	Input *inSig;


	void construct(int _auxNum, Input *_inputs);
	void onReset();
	void resetNonJson() {}
	void dataToJson(json_t *rootJ);
	void dataFromJson(json_t *rootJ);
	

	void setHPFCutoffFreq(float fc) {// always use this instead of directly accessing hpfCutoffFreq
		hpfCutoffFreq = fc;
		fc *= APP->engine->getSampleTime();// fc is in normalized freq for rest of method
		for (int i = 0; i < 2; i++) {
			hpPreFilter[i].setCutoff(fc);
			hpFilter[i].setParameters(dsp::BiquadFilter::HIGHPASS, fc, hpfBiquadQ, 0.0);
		}
	}
	float getHPFCutoffFreq() {return hpfCutoffFreq;}
	
	void setLPFCutoffFreq(float fc) {// always use this instead of directly accessing lpfCutoffFreq
		lpfCutoffFreq = fc;
		fc *= APP->engine->getSampleTime();// fc is in normalized freq for rest of method
		lpFilter[0].setParameters(dsp::BiquadFilter::LOWPASS, fc, 0.707, 0.0);
		lpFilter[1].setParameters(dsp::BiquadFilter::LOWPASS, fc, 0.707, 0.0);
	}
	float getLPFCutoffFreq() {return lpfCutoffFreq;}


	void onSampleRateChange() {
		setHPFCutoffFreq(hpfCutoffFreq);
		setLPFCutoffFreq(lpfCutoffFreq);
	}


	void process(float *mix) {// auxspander aux	
		// optimize unused aux
		if (!inSig[0].isConnected()) {
			mix[0] = mix[1] = 0.0f;
			return;
		}
		mix[0] = inSig[0].getVoltage();
		mix[1] = inSig[1].isConnected() ? inSig[1].getVoltage() : mix[0];
		// Filters
		// HPF
		if (getHPFCutoffFreq() >= minHPFCutoffFreq) {
			mix[0] = hpFilter[0].process(hpPreFilter[0].processHP(mix[0]));
			mix[1] = inSig[1].isConnected() ? hpFilter[1].process(hpPreFilter[1].processHP(mix[1])) : mix[0];
		}
		// LPF
		if (getLPFCutoffFreq() <= maxLPFCutoffFreq) {
			mix[0] = lpFilter[0].process(mix[0]);
			mix[1] = inSig[1].isConnected() ? lpFilter[1].process(mix[1]) : mix[0];
		}
	}
};// struct AuxspanderAux


#endif