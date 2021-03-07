//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//Based on code from the Fundamental plugins by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************

#include "MindMeldModular.hpp"


Plugin *pluginInstance;
MixerMessageBus mixerMessageBus;

void init(rack::Plugin *p) {
	pluginInstance = p;

	p->addModel(modelMixMasterJr);
	p->addModel(modelAuxExpanderJr);
	p->addModel(modelMixMaster);
	p->addModel(modelAuxExpander);
	p->addModel(modelMeld);
	p->addModel(modelUnmeld);
	p->addModel(modelEqMaster);
	p->addModel(modelEqExpander);
	p->addModel(modelBassMaster);
	p->addModel(modelBassMasterJr);
}


// General objects
// none


// General functions

void printNote(float cvVal, char* text, bool sharp) {// text must be at least 5 chars long (4 displayed chars plus end of string)
	static const char noteLettersSharp[12] = {'C', 'C', 'D', 'D', 'E', 'F', 'F', 'G', 'G', 'A', 'A', 'B'};
	static const char noteLettersFlat [12] = {'C', 'D', 'D', 'E', 'E', 'F', 'G', 'G', 'A', 'A', 'B', 'B'};
	static const char isBlackKey      [12] = { 0,   1,   0,   1,   0,   0,   1,   0,   1,   0,   1,   0 };
	static const float offByTolerance = 0.15f;
	
	float offsetScaled = (cvVal + 20.0f) * 12.0f;// +20.0f is to properly handle negative note voltages
	int offsetScaledRounded = ((int)( offsetScaled + 0.5f ));
	int indexNote =  offsetScaledRounded % 12;
	
	// note letter
	char noteLetter = sharp ? noteLettersSharp[indexNote] : noteLettersFlat[indexNote];
	
	// octave number
	int octave = offsetScaledRounded / 12  -20 + 4;
	
	if (octave >= 0 && octave <= 9) {
		snprintf(text, 5, "%c%c", noteLetter, (char)(octave % 10) + 0x30);
	}
	else {
		snprintf(text, 5, "%c", noteLetter);
	}
	
	// sharp/flat
	if (isBlackKey[indexNote] == 1) {
		strcat(text, sharp ? "#" : "b");
	}
	
	//
	float offBy = offsetScaled - (float)offsetScaledRounded; 
	if (offBy < -offByTolerance) {
		strcat(text, "-");
	}
	else if (offBy > offByTolerance) {
		strcat(text, "+");
	}
}

