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
extern Model *modelMixMasterJr;
extern Model *modelAuxExpanderJr;
extern Model *modelMixMaster;
extern Model *modelAuxExpander;
extern Model *modelMeld;
extern Model *modelUnmeld;
extern Model *modelEqMaster;
extern Model *modelEqExpander;
extern Model *modelBassMaster;
extern Model *modelBassMasterJr;



// General constants

// none

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


struct Trigger : dsp::SchmittTrigger {
	// implements a 0.1V - 1.0V SchmittTrigger (see include/dsp/digital.hpp) instead of 
	//   calling SchmittTriggerInstance.process(math::rescale(in, 0.1f, 1.f, 0.f, 1.f))
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

	struct DispColorSubItem : MenuItem {
		int8_t *srcColor;
		int setVal;
		void onAction(const event::Action &e) override {
			*srcColor = setVal;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		
		for (int i = 0; i < 2; i++) {// only yellow and light gray wanted in EqMaster
			DispColorSubItem *dispColItem = createMenuItem<DispColorSubItem>(dispColorNames[i], CHECKMARK(*srcColor == i));
			dispColItem->srcColor = srcColor;
			dispColItem->setVal = i;
			menu->addChild(dispColItem);
		}
		
		return menu;
	}
};



// poly stereo menu item
struct PolyStereoItem : MenuItem {
	int8_t *polyStereoSrc;

	struct PolyStereoSubItem : MenuItem {
		int8_t *polyStereoSrc;
		int setVal = 0;
		void onAction(const event::Action &e) override {
			*polyStereoSrc = setVal;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		PolyStereoSubItem *ps0Item = createMenuItem<PolyStereoSubItem>("Sum each input (L, R)", CHECKMARK(*polyStereoSrc == 0));
		ps0Item->polyStereoSrc = polyStereoSrc;
		menu->addChild(ps0Item);

		PolyStereoSubItem *ps1Item = createMenuItem<PolyStereoSubItem>("Sum to stereo (L only)", CHECKMARK(*polyStereoSrc == 1));
		ps1Item->polyStereoSrc = polyStereoSrc;
		ps1Item->setVal = 1;
		menu->addChild(ps1Item);

		return menu;
	}
};




// General functions

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


void printNote(float cvVal, char* text, bool sharp);

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

static inline float clamp20V(float in) {// meant to catch invalid values like -inf, +inf, strong overvoltage only.
	//return in;
	if (in >= -20.0f && in <= 20.0f) {
		return in;
	}
	return in > 20.0f ? 20.0f : -20.0f;
}
