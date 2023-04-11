//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//Based on code from the Fundamental plugins by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************

#pragma once

#include "rack.hpp"
#include "comp/GenericComponents.hpp"
#include "MixerMessageBus.hpp"

using namespace rack;


extern Plugin *pluginInstance;



extern MixerMessageBus mixerMessageBus;


// All modules that are part of pluginInstance go here
extern Model *modelPatchMaster;
extern Model *modelPatchMasterBlank;
extern Model *modelRouteMasterMono5to1;
extern Model *modelRouteMasterStereo5to1;
extern Model *modelRouteMasterMono1to5;
extern Model *modelRouteMasterStereo1to5;
extern Model *modelMasterChannel;
extern Model *modelMeld;
extern Model *modelUnmeld;
extern Model *modelMSMelder;
extern Model *modelEqMaster;
extern Model *modelEqExpander;
extern Model *modelBassMaster;
extern Model *modelBassMasterJr;
extern Model *modelShapeMaster;
extern Model *modelMixMasterJr;
extern Model *modelAuxExpanderJr;
extern Model *modelMixMaster;
extern Model *modelAuxExpander;


// General constants

// none



// Global settings

extern int8_t pmAllowMouseTileMove;// PatchMaster allow ctrl/cmd click to move tiles



// General objects

union PackedBytes4 {
	int32_t cc1;
	int8_t cc4[4];
};

struct RefreshCounter {
	// Note: because of stagger, and asyncronous dataFromJson, should not assume this processInputs() will return true on first run
	// of module::process()
	static const uint16_t displayRefreshStepSkips = 256;// 
	static const uint16_t userInputsStepSkipMask = 0xF;// sub interval of displayRefreshStepSkips, since inputs should be more responsive than lights
	// above values should make it such that inputs are sampled > 1kHz so as to not miss 1ms triggers
	// values of 256 and 0xF respectively will give input sampled at 2756 Hz and lights at 172 Hz (when 44.1 kHz)
	
	uint16_t refreshCounter = (random::u32() % displayRefreshStepSkips);// stagger start values to avoid processing peaks when many Geo, MMM and Impromptu modules in the patch
	
	bool processInputs() {
		return ((refreshCounter & userInputsStepSkipMask) == 0);
	}
	bool processLights() {// this must be called even if module has no lights, since counter is decremented here
		refreshCounter++;
		bool process = refreshCounter >= displayRefreshStepSkips;
		if (process) {
			refreshCounter = 0;
		}
		return process;
	}
};


struct Trigger {
	bool state = true;

	void reset() {
		state = true;
	}
	
	bool isHigh() {
		return state;
	}
	
	bool process(float in) {
		if (state) {
			// HIGH to LOW
			if (in <= 0.1f) {
				state = false;
			}
		}
		else {
			// LOW to HIGH
			if (in >= 1.0f) {
				state = true;
				return true;
			}
		}
		return false;
	}	
};	


struct TriggerRiseFall {
	bool state = false;

	void reset() {
		state = false;
	}

	int process(float in) {
		if (state) {
			// HIGH to LOW
			if (in <= 0.1f) {
				state = false;
				return -1;
			}
		}
		else {
			// LOW to HIGH
			if (in >= 1.0f) {
				state = true;
				return 1;
			}
		}
		return 0;
	}	
};	

// struct below adapted from code by Andrew Belt in Rack/include/dsp/filter.hpp
template <typename T = float>
struct TSlewLimiterSingle {
	T out = 0.f;
	T riseFall = 0.f;

	void reset() {
		out = 0.f;
	}

	void setRiseFall(T riseFall) {
		this->riseFall = riseFall;
	}
	T process(T deltaTime, T in) {
		out = simd::clamp(in, out - riseFall * deltaTime, out + riseFall * deltaTime);
		return out;
	}
	T process(T deltaTime, T in, T _riseFall) {
		out = simd::clamp(in, out - _riseFall * deltaTime, out + _riseFall * deltaTime);
		return out;
	}
};
typedef TSlewLimiterSingle<> SlewLimiterSingle;

struct SlewLimiterFast {
	float out = 0.0f;

	void reset() {
		out = 0.0f;
	}

	float process(float deltaTimeTimesRiseFall, float in) {
		out = clamp(in, out - deltaTimeTimesRiseFall, out + deltaTimeTimesRiseFall);
		return out;
	}
};

struct HoldDetect {
	long modeHoldDetect;// 0 when not detecting, downward counter when detecting
	
	void reset() {
		modeHoldDetect = 0l;
	}
	
	void start(long startValue) {
		modeHoldDetect = startValue;
	}

	bool process(float paramValue) {
		bool ret = false;
		if (modeHoldDetect > 0l) {
			if (paramValue < 0.5f)
				modeHoldDetect = 0l;
			else {
				if (modeHoldDetect == 1l) {
					ret = true;
				}
				modeHoldDetect--;
			}
		}
		return ret;
	}
};

struct DispTwoColorItem : MenuItem {
	int8_t *srcColor;

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		for (int i = 0; i < 2; i++) {// only yellow and light gray wanted in EqMaster
			menu->addChild(createCheckMenuItem(dispColorNames[i], "",
				[=]() {return *srcColor == i;},
				[=]() {*srcColor = i;}
			));
		}
		return menu;
	}
};



// poly stereo menu item
struct PolyStereoItem : MenuItem {
	int8_t *polyStereoSrc;

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		menu->addChild(createCheckMenuItem("Sum each input (L, R)", "",
			[=]() {return *polyStereoSrc == 0;},
			[=]() {*polyStereoSrc = 0;}
		));
		menu->addChild(createCheckMenuItem("Sum to stereo (L only)", "",
			[=]() {return *polyStereoSrc == 1;},
			[=]() {*polyStereoSrc = 1;}
		));
		return menu;
	}
};




// General functions

void readGlobalSettings();
void writeGlobalSettings();
inline bool isPmAllowMouseTileMove() {
	return pmAllowMouseTileMove != 0;
}
inline void togglePmAllowMouseTileMove() {
	pmAllowMouseTileMove ^= 0x1;
	writeGlobalSettings();
}


// sort the 4 floats in a float_4 in ascending order starting with index 0
// adapted from https://stackoverflow.com/questions/6145364/sort-4-number-with-few-comparisons
inline simd::float_4 sortFloat4(simd::float_4 in) {
    float low1;
	float high1;
    float low2;
	float high2;
	
	if (in[0] < in[1]) {
        low1 = in[0];
        high1 = in[1];
	}
    else {
        low1 = in[1];
        high1 = in[0];
	}

    if (in[2] < in[3]) {
        low2 = in[2];
        high2 = in[3];
	}
    else {
        low2 = in[3];
        high2 = in[2];
	}

    simd::float_4 ret;
	float middle1;
	float middle2;
	
	if (low1 < low2) {
        ret[0] = low1;
        middle1 = low2;
	}
    else {
        ret[0] = low2;
        middle1 = low1;
	}

    if (high1 > high2) {
        ret[3] = high1;
        middle2 = high2;
	}
    else {
        ret[3] = high2;
        middle2 = high1;
	}

    if (middle1 < middle2) {
		ret[1] = middle1;
		ret[2] = middle2;
	}
    else {
		ret[1] = middle2;
		ret[2] = middle1;
	}
	return ret;
}


// Remove Rack's standard border in the given widget's children
// inline void removeBorder(Widget* widget) {
	// for (auto it = widget->children.begin(); it != widget->children.end(); ) {
		// PanelBorder *bwChild = dynamic_cast<PanelBorder*>(*it);
		// if (bwChild) {
			// it = widget->children.erase(it);
		// }
		// else {
			// ++it;
		// }
	// }
// }


// Find a PanelBorder instance in the given widget's children
inline PanelBorder* findBorder(Widget* widget) {
	for (auto it = widget->children.begin(); it != widget->children.end(); ) {
		PanelBorder *bwChild = dynamic_cast<PanelBorder*>(*it);
		if (bwChild) {
			return bwChild;
		}
		else {
			++it;
		}
	}
	return NULL;
}


static inline void applyStereoWidth(float width, float* left, float* right) {
	// in this algo, width can go to 2.0f to implement 200% stereo widening (1.0f stereo i.e. no change, 0.0f is mono)
	float wdiv2 = width * 0.5f;
	float up = 0.5f + wdiv2;
	float down = 0.5f - wdiv2;
	float leftSig = *left * up + *right * down;
	float rightSig = *right * up + *left * down;
	*left = leftSig;
	*right = rightSig;
}


static inline float clampNothing(float in) {// meant to catch invalid values like -inf, +inf, strong overvoltage only. Not needed anymore since Rack2 has invalid value protection on outputs
	return in;
	// if (in >= -20.0f && in <= 20.0f) {
		// return in;
	// }
	// return in > 20.0f ? 20.0f : -20.0f;
}


extern char noteLettersSharp[12];
extern char noteLettersFlat [12];
extern char isBlackKey      [12];

void printNote(float cvVal, char* text, bool sharp);

std::string timeToString(float timeVal, bool lowPrecision);

float stringToVoct(std::string* noteText);