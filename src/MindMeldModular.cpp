//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//Based on code from the Fundamental plugins by Andrew Belt 
//See ./LICENSE.txt for all licenses
//***********************************************************************************************

#include "MindMeldModular.hpp"


Plugin *pluginInstance;


void init(rack::Plugin *p) {
	pluginInstance = p;

	p->addModel(modelMixMasterJr);
}


// General objects


bool Trigger::process(float in) {
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

bool HoldDetect::process(float paramValue) {
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


// General functions

// none
