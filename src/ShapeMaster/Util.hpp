//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#pragma once

#include "../MindMeldModular.hpp"
#include "History.hpp"

static const int NUM_KNOB_PARAMS = 15;
static const int NUM_BUTTON_PARAMS = 6;
static const int NUM_ARROW_PARAMS = 4;
static const int NUM_CHAN_PARAMS = NUM_KNOB_PARAMS + NUM_BUTTON_PARAMS + NUM_ARROW_PARAMS;
static const float NODE_TRIG_DURATION = 0.01f;

enum SmParamIds {
	// channel-specific params
	LENGTH_SYNC_PARAM,// these NUM_CHAN_PARAMS params must be sequential and must start at 0
	LENGTH_UNSYNC_PARAM,
	REPETITIONS_PARAM,
	OFFSET_PARAM,
	SWING_PARAM,
	PHASE_PARAM,
	RESPONSE_PARAM,
	WARP_PARAM,
	AMOUNT_PARAM,
	SLEW_PARAM,
	SMOOTH_PARAM,
	CROSSOVER_PARAM,
	HIGH_PARAM,
	LOW_PARAM,
	TRIGLEV_PARAM,// knobs this and above, buttons below
	PLAY_PARAM,
	FREEZE_PARAM,
	LOOP_PARAM,
	SYNC_PARAM,
	LOCK_PARAM,
	AUDITION_PARAM,
	ENUMS(PREV_NEXT_PRE_SHA, 4), // ch0 prev preset, ch0 next preset, ch0 prev shape, ch0 next shape also aka NUM_CHAN_PARAMS - 1
	ENUMS(CHAN2_TO_8, NUM_CHAN_PARAMS * 7),
	// everything below this is not channel-specific
	RESET_PARAM,
	RUN_PARAM,
	GEAR_PARAM,// contains channel-specific data in its menu though
	// any arrow buttons for preset and channel should go here, and they will access channel-specific data directly
	NUM_SM_PARAMS
};

enum SmInputIds {
	ENUMS(IN_INPUTS, 8),
	ENUMS(TRIG_INPUTS, 8),
	CLOCK_INPUT,
	RESET_INPUT,
	RUN_INPUT,
	SIDECHAIN_INPUT,
	NUM_SM_INPUTS
};


enum SmOutputIds {
	ENUMS(OUT_OUTPUTS, 8),
	ENUMS(CV_OUTPUTS, 8),
	NUM_SM_OUTPUTS
};



// Expanders
// --------

struct TrigExpInterface {
	// start of sustain, end of sustain, end of cycle
	uint32_t sosEosEoc;// 31:24 is exclude eoc from OR [7..0]; 23:0 is [SOS_7..SOS_0; EOS_7..EOS_0; EOC_7..EOC_0] 
	// exlusion must be valid anytime an EOC bit is set
};


struct ChanCvs {
	simd::float_4 warpPhasRespAmnt;// [0]: warp
	simd::float_4 xoverSlew;// [0]: xfreq, [1]: xhigh, [2]: xlow, [3]: slew
	simd::float_4 offsetSwingLoops;// [0]: offset (trig delay), [1]: swing, [2]: loop start, [3]: loop end
	float lengthUnsync;
	float lengthSync;
	
	uint16_t trigs;// [0]: chan reset, [1-2]: prev-next preset, [3-4]: prev-next shape, [5-6]: play rise-fall, [7-8] freeze rise-fall [9-11]: rev inv rand
	uint16_t info;// [0]: hasWarpPhasRespAmnt, [1]: hasXoverSlew, [2]: hasOffsetSwingLoops, [3]: has length unsync, [4]: has length sync
	
	
	// Setters
	// ----------
	
	// warpPhasRespAmnt
	void writeWarp(float val) {
		warpPhasRespAmnt[0] = val;
		info |= 0x1;
	}
	void writePhase(float val) {
		warpPhasRespAmnt[1] = val;
		info |= 0x1;
	}
	void writeResponse(float val) {
		warpPhasRespAmnt[2] = val;
		info |= 0x1;
	}
	void writeAmount(float val) {
		warpPhasRespAmnt[3] = val;
		info |= 0x1;
	}
	
	
	// xoverSlew
	void writeXfreq(float val) {
		xoverSlew[0] = val;
		info |= 0x2;
	}
	void writeXhigh(float val) {
		xoverSlew[1] = val;
		info |= 0x2;
	}
	void writeXlow(float val) {
		xoverSlew[2] = val;
		info |= 0x2;
	}
	void writeSlew(float val) {
		xoverSlew[3] = val;
		info |= 0x2;
	}
	
	
	// offsetSwingLoops
	void writeOffset(float val) {
		offsetSwingLoops[0] = val;
		info |= 0x4;
	}
	void writeSwing(float val) {
		offsetSwingLoops[1] = val;
		info |= 0x4;
	}
	void writeLoopStart(float val) {
		offsetSwingLoops[2] = val;
		info |= 0x4;
	}
	void writeLoopEnd(float val) {
		offsetSwingLoops[3] = val;
		info |= 0x4;
	}
	
	
	// voct and length
	void addtoLengthUnsync(float val) {
		lengthUnsync += val;
		info |= 0x8;
	}
	void writeLengthSync(float val) {
		lengthSync = val;
		info |= 0x10;
	}


	// Getters
	// ----------
	bool hasWarpPhasRespAmnt() {
		return (info & 0x1) != 0;
	}
	bool hasXoverSlew() {
		return (info & 0x2) != 0;
	}
	bool hasOffsetSwingLoops() {
		return (info & 0x4) != 0;
	}
	bool hasLengthUnsync() {
		return (info & 0x8) != 0;
	}
	bool hasLengthSync() {
		return (info & 0x10) != 0;
	}
	
	bool hasChannelReset() {
		return (trigs & 0x1) != 0;
	}
	bool hasPrevPreset() {
		return (trigs & 0x2) != 0;
	}
	bool hasNextPreset() {
		return (trigs & 0x4) != 0;
	}
	bool hasPrevShape() {
		return (trigs & 0x8) != 0;
	}
	bool hasNextShape() {
		return (trigs & 0x10) != 0;
	}
	bool hasPlay() {
		return (trigs & 0x20) != 0;
	}
	bool hasFreeze() {
		return (trigs & 0x80) != 0;
	}
	bool hasReverse() {
		return (trigs & 0x200) != 0;
	}
	bool hasInvert() {
		return (trigs & 0x400) != 0;
	}
	bool hasRandom() {
		return (trigs & 0x800) != 0;
	}

};



struct CvExpInterface {
	ChanCvs chanCvs[8];
};



// Mutl/div ratios for sync'ed length
// --------

struct MutlDivRatio {
	std::string shortName;
	std::string longName;
	double ratio;// this is in quarter notes
};

static const int NUM_RATIOS = 32;
extern MutlDivRatio multDivTable[NUM_RATIOS];
static const int NUM_SECTIONS = 6;
extern int numRatiosPerSection[NUM_SECTIONS];
extern std::string sectionNames[NUM_SECTIONS];

inline float voctToUnsuncedLengthParam(float voct) {
	// LENGTH_UNSYNC_MULT was assumed to be 1 in these calculations
	// c4:=261.6256
	// length as function of param is T(p):= 1800^p
	// freq as a function of param is f(p):= 1/T(p)  =>  f(p):= 1800^-p
	// p(f):=?log10(f)/log10(1800)    // 1800 is LENGTH_UNSYNC_BASE
	// p(v):=a*v+b   // v is voltage of voct input
	// solve( p(c4/2)=a*?1+b and p(c4)=a*0+b, {a,b} )  // c4/2 is -1 (V) and c4 is 0 (V)
	// ans: a=?0.09247459166386 and b=?0.74269672770821
	return voct * -0.092475f - 0.742697f;
}
inline float unsuncedLengthParamToVoct(float p) {
	return (p + 0.742697f) / -0.092475f;
}


// Trig mode and play mode
// --------

enum TridModeIds {TM_AUTO, TM_TRIG_GATE, TM_GATE_CTRL, TM_SC, TM_CV, NUM_TRIG_MODES};
extern std::string trigModeNames[NUM_TRIG_MODES];
extern std::string trigModeNamesLong[NUM_TRIG_MODES];

enum PlayModeIds {PM_FWD, PM_REV, PM_PNG, NUM_PLAY_MODES};
extern std::string playModeNames[NUM_PLAY_MODES];
extern std::string playModeNamesLong[NUM_PLAY_MODES];



// Grid
// --------

static const int NUM_SNAP_OPTIONS = 19; // 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 24, 32, 64, 128
extern int snapValues[NUM_SNAP_OPTIONS];

static const int NUM_RANGE_OPTIONS = 9; // 0-10V, 0-5V, 0-3V, 0-2V, 0-1V, +/-5V, +/-3V, +/-2V, +/-1V
extern int rangeValues[NUM_RANGE_OPTIONS];// negative values are bipolar, positive mean unipolar



// Ppqn
// --------

static const int NUM_PPQN_CHOICES = 5; 
extern int ppqnValues[NUM_PPQN_CHOICES];


// Poly sum
// --------

enum PolyModeIds {POLY_NONE, POLY_STEREO, POLY_MONO, NUM_POLY_MODES};
static const int polyModeChanOut[NUM_POLY_MODES] = {16, 2, 1};
extern std::string polyModeNames[NUM_POLY_MODES];


// Other
// --------

static const bool WITH_PARAMS = true;
static const bool WITHOUT_PARAMS = false;

static const bool IS_DIRTY_CACHE_LOAD = true;
static const bool ISNOT_DIRTY_CACHE_LOAD = false;

static const bool WITH_PRO_UNSYNC_MATCH = true;
static const bool WITHOUT_PRO_UNSYNC_MATCH = false;

static const bool WITH_FULL_SETTINGS = true;
static const bool WITHOUT_FULL_SETTINGS = false;

