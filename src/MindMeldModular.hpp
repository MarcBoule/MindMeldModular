//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//Based on code from the Fundamental plugins by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************

#ifndef MINDMELDMODULAR_HPP
#define MINDMELDMODULAR_HPP


#include "rack.hpp"
#include "comp/DynamicComponents.hpp"
#include "comp/GenericComponents.hpp"
#include "MessageBus.hpp"

using namespace rack;


extern Plugin *pluginInstance;


// struct Payload {
  // float values[8];
// };

// extern MessageBus<Payload> *messages;


// All modules that are part of pluginInstance go here
extern Model *modelMixMaster;
extern Model *modelAuxExpander;



// General constants
// none



// General objects

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



// General functions

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



#endif
