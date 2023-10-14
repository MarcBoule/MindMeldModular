//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************

#pragma once

#include "../MindMeldModular.hpp"
#include "../dsp/FirstOrderFilter.hpp"
#include "../dsp/ButterworthFilters.hpp"


enum GTOL_IDS {
	GTOL_NOP,
	GTOL_VUCOL,
	GTOL_LABELCOL,
	GTOL_STEREOPAN,
	GTOL_AUXSENDS,
	GTOL_DIRECTOUTS,
	GTOL_FILTERPOS,
	GTOL_MOMENTCV
};

struct GlobalToLocalOp {
	int8_t opCodeMixer = GTOL_NOP;;// see GTOL_IDS
	int8_t opCodeExpander = GTOL_NOP;// see GTOL_IDS
	int8_t operand = 0;// the global value that we want to be set in all the local values
		
	void setOp(int8_t _opCode, int8_t _operand) {
		operand = _operand;// set this first for safe thread behavior
		opCodeMixer = _opCode;
		opCodeExpander = _opCode;
	}
};


//*****************************************************************************
// Communications between mixer and auxspander

template <int N_TRK, int N_GRP>
struct TAfmExpInterface {// messages to expander from mother (data is in expander, mother writes into expander)
	// Fast (sample-rate)	
	bool updateSlow = false;
	float auxSends[(N_TRK + N_GRP) * 2] = {};
	int vuIndex = 0;
	float vuValues[4] = {};
	
	// Slow (sample-rate / 256), no need to init
	PackedBytes4 colorAndCloak;
	PackedBytes4 directOutPanStereoMomentCvLinearVol;
	uint32_t muteAuxSendWhenReturnGrouped = 0;
	uint16_t ecoMode = 0;// all 1's means yes, 0 means no
	int32_t trackMoveInAuxRequest = 0;// 0 when nothing to do, {dest,src} packed when a move is requested
	int8_t trackOrGroupResetInAux = 0;// -1 when nothing to do, 0 to N_TRK-1 for track reset, N_TRK to N_TRK+N_GRP-1 for group reset 
	alignas(4) char trackLabels[4 * (N_TRK + N_GRP)] = {};
	PackedBytes4 trackDispColsLocal[N_TRK / 4 + 1];// only valid when colorAndCloak.cc4[dispColorGlobal] >= numDispThemes
	float auxRetFadeGains[4] = {};
	float srcMuteGhost[4] = {};
	GlobalToLocalOp globalToLocalOp;
};


struct MfaExpInterface {// messages to mother from expander (data is in mother, expander writes into mother)
	// Fast (sample-rate)	
	bool updateSlow = false;
	float auxReturns[8] = {};
	float auxRetFaderPanFadercv[12] = {};
	
	// Slow (sample-rate / 256), no need to init
	PackedBytes4 directOutsModeLocalAux;
	PackedBytes4 stereoPanModeLocalAux;
	PackedBytes4 auxVuColors;
	PackedBytes4 auxDispColors;
	float values20[20] = {};// Aux mute, solo, group, fade rate, fade profile; 4 consective floats for each (one per aux)
	alignas(4) char auxLabels[4 * 4] = {};
};



//*****************************************************************************
// Global constants

struct GlobalConst {
	static const int 		masterFaderScalingExponent = 3; // for example, 3 is x^3 scaling
	static constexpr float 	masterFaderMaxLinearGain = 2.0f; // for example, 2.0f is +6 dB
	static const int 		trkAndGrpFaderScalingExponent = 3; 
	static constexpr float 	trkAndGrpFaderMaxLinearGain = 2.0f; 
	static const int 		globalAuxReturnScalingExponent = 3; 
	static constexpr float 	globalAuxReturnMaxLinearGain = 2.0f; 
	static const int 		individualAuxSendScalingExponent = 2;
	static constexpr float 	individualAuxSendMaxLinearGain = 1.0f;
	static const int 		globalAuxSendScalingExponent = 2; 
	static constexpr float 	globalAuxSendMaxLinearGain = 4.0f; 
	
	static constexpr float antipopSlewFast = 125.0f;// for pan/fader when linear, and mute/solo
	static constexpr float antipopSlewSlow = 25.0f;// for pan/fader when not linear
	static constexpr float minFadeRate = 0.1f;
	static constexpr float minHPFCutoffFreq = 20.0f;
	static constexpr float defHPFCutoffFreq = 13.0f;
	static constexpr float maxLPFCutoffFreq = 20000.0f;
	static constexpr float defLPFCutoffFreq = 20010.0f;
};



//*****************************************************************************
// Math

// Calculate std::sin(theta) using MacLaurin series
// Calculate std::cos(theta) using cos(x) = sin(Pi/2 - x)
// Assumes: 0 <= theta <= Pi/2
static inline void sinCos(float *destSin, float *destCos, float theta) {
	*destSin = theta + std::pow(theta, 3) * (-0.166666667f + theta * theta * 0.00833333333f);
	theta = float(M_PI_2) - theta;
	*destCos = theta + std::pow(theta, 3) * (-0.166666667f + theta * theta * 0.00833333333f);
}
static inline void sinCosSqrt2(float *destSin, float *destCos, float theta) {
	sinCos(destSin, destCos, theta);
	*destSin *= float(M_SQRT2);
	*destCos *= float(M_SQRT2);
}


static inline float calcDimGainIntegerDB(float dimGain) {
	float integerDB = std::round(20.0f * std::log10(dimGain));
	return std::pow(10.0f, integerDB / 20.0f);
}



//*****************************************************************************
// Utility

float updateFadeGain(float fadeGain, float target, float *fadeGainX, float *fadeGainXr, float timeStepX, float shape, bool symmetricalFade);

struct TrackSettingsCpBuffer {
	// first level of copy paste (copy copy-paste of track settings)
	float gainAdjust = 0.0f;
	float fadeRate = 0.0f;
	float fadeProfile = 0.0f;
	float hpfCutoffFreq = 0.0f;// !! user must call filters' setCutoffs manually when copy pasting these
	float lpfCutoffFreq = 0.0f;// !! user must call filters' setCutoffs manually when copy pasting these
	int8_t directOutsMode = 0;
	int8_t auxSendsMode = 0;
	int8_t panLawStereo = 0;
	int8_t vuColorThemeLocal = 0;
	int8_t filterPos = 0;
	int8_t dispColorLocal = 0;
	int8_t momentCvMuteLocal = 0;
	int8_t momentCvSoloLocal = 0;
	int8_t polyStereo = 0;
	float panCvLevel = 0.0f;
	float stereoWidth = 0.0f;
	int8_t invertInput = 0;
	bool linkedFader = false;

	// second level of copy paste (for track re-ordering)
	float paGroup = 0.0f;
	float paFade = 0.0f;
	float paMute = 0.0f;
	float paSolo = 0.0f;
	float paPan = 0.0f;
	char trackName[4] = {};// track names are not null terminated in MixerTracks
	float fadeGain = 0.0f;
	float target = 0.0f;
	float fadeGainX = 0.0f;
	float fadeGainXr = 0.0f;
	float fadeGainScaled = 0.0f;
};


static inline bool isLinked(const unsigned long *linkBitMaskSrc, int index) {return (*linkBitMaskSrc & (1 << index)) != 0;}
static inline void toggleLinked(unsigned long *linkBitMaskSrc, int index) {*linkBitMaskSrc ^= (1 << index);}


//*****************************************************************************


enum ccIds {
	cloakedMode, // turn off track VUs only, keep master VUs (also called "Cloaked mode"), this has only two values, 0x0 and 0xFF so that it can be used in bit mask operations
	vuColorGlobal, // 0 is green, 1 is aqua, 2 is cyan, 3 is blue, 4 is purple, 5 is individual colors for each track/group/master (every user of vuColor must first test for != 5 before using as index into color table, or else array overflow)
	dispColorGlobal, // 0 is yellow, 1 is light-gray, 2 is green, 3 is aqua, 4 is cyan, 5 is blue, 6 is purple, 7 is per track
	detailsShow // bit 0 is knob param arc, bit 1 is knob cv arc, bit 2 is fader cv pointer
};


