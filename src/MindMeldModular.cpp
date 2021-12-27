//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//Based on code from the Fundamental plugins by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************

#include "MindMeldModular.hpp"


Plugin *pluginInstance;
MixerMessageBus mixerMessageBus;

void init(Plugin *p) {
	pluginInstance = p;

	p->addModel(modelMixMasterJr);
	p->addModel(modelAuxExpanderJr);
	p->addModel(modelMixMaster);
	p->addModel(modelAuxExpander);
	p->addModel(modelMeld);
	p->addModel(modelUnmeld);
	p->addModel(modelMSMelder);
	p->addModel(modelEqMaster);
	p->addModel(modelEqExpander);
	p->addModel(modelBassMaster);
	p->addModel(modelBassMasterJr);
	p->addModel(modelShapeMaster);
}


// General objects
// none


// General functions

char noteLettersSharp[12] = {'C', 'C', 'D', 'D', 'E', 'F', 'F', 'G', 'G', 'A', 'A', 'B'};
char noteLettersFlat [12] = {'C', 'D', 'D', 'E', 'E', 'F', 'G', 'G', 'A', 'A', 'B', 'B'};
char isBlackKey      [12] = { 0,   1,   0,   1,   0,   0,   1,   0,   1,   0,   1,   0 };

void printNote(float cvVal, char* text, bool sharp) {// text must be at least 5 chars long (4 displayed chars plus end of string)
	static const float offByTolerance = 0.15f;
	
	float offsetScaled = (cvVal + 20.0f) * 12.0f;// +20.0f is to properly handle negative note voltages
	int offsetScaledRounded = ((int)( offsetScaled + 0.5f ));
	int indexNote =  offsetScaledRounded % 12;
	
	// note letter
	char noteLetter = sharp ? noteLettersSharp[indexNote] : noteLettersFlat[indexNote];
	snprintf(text, 5, "%c", noteLetter);
		
	// sharp/flat
	if (isBlackKey[indexNote] == 1) {
		strcat(text, sharp ? "#" : "b");
	}
	
	// octave number
	int octave = offsetScaledRounded / 12  -20 + 4;
	if (octave >= 0 && octave <= 9) {
		char octChar[2] = {((char)(octave % 10 + 0x30)), 0};
		strcat(text, octChar);
	}
	
	// off by
	float offBy = offsetScaled - (float)offsetScaledRounded; 
	if (offBy < -offByTolerance) {
		strcat(text, "-");
	}
	else if (offBy > offByTolerance) {
		strcat(text, "+");
	}
}


std::string timeToString(float timeVal, bool lowPrecision) {
	long res;
	int resi;
	float lp = lowPrecision ? 1.0f : 10.0f;
	if (timeVal < 1e-4 * lp) {
		res = 10000000l;
		resi = 7;
	}
	else if (timeVal < 1e-3 * lp) {
		res = 1000000l;
		resi = 6;
	}
	else if (timeVal < 0.01f * lp) {
		res = 100000l;
		resi = 5;
	}
	else if (timeVal < 0.1f * lp) {
		res = 10000l;
		resi = 4;
	}
	else if (timeVal < 1.0f * lp) {
		res = 1000l;
		resi = 3;
	}
	else if (timeVal < 10.0f * lp) {
		res = 100l;
		resi = 2;
	}
	else {
		res = 10l;
		resi = 1;
	}
	
	// note: variable names are for max resolution (usecs) for simplicity
	
	long timeValUs = (long)(timeVal * ((float)res) + 0.5f);
	
	long timeMins = (long)(timeValUs / (60l * res));
	long timeSecsInUs = (long)(timeValUs % (60l * res));
	
	long timeSecs = (long)(timeSecsInUs / res);
	long timeUsecs = (long)(timeSecsInUs % res);
	
	std::string timeText("");
	if (timeMins != 0) {
		timeText.append(string::f("%li:", timeMins));
	}
	timeText.append(string::f("%li", timeSecs));
	if (timeUsecs != 0) {
		timeText.append(string::f(".%.*li", resi, timeUsecs));
		if (timeText[timeText.size() - 1] == '0') {
			timeText.erase(timeText.size() - 1);
			if (timeText[timeText.size() - 1] == '0') {
				timeText.erase(timeText.size() - 1);
			}
		}
	}
	return timeText;
}


float stringToVoct(std::string* noteText) {
	// returns -100.0f if parsing error

	static const float whiteKeysVocts[7] = {9.0f/12.0f, 11.0f/12.0f, 0.0f, 2.0f/12.0f, 4.0f/12.0f, 5.0f/12.0f, 7.0f/12.0f};// A, B, C, D, E, F, G

	float newVolts = -100.0f;
	
	char newNote[3] = {};
	for (size_t i = 0; i < noteText->size(); i++) {
		newNote[i] = noteText->c_str()[i];
		if (i == 0) {
			newNote[i] &= ~0x20;// force capitalization for note letter
		}
	}
	bool hasAccidental = (newNote[1] == '#' || newNote[1] == 'b');
	int octIndex = hasAccidental ? 2 : 1;
	if (newNote[0] >= 'A' && newNote[0] <= 'G' && newNote[octIndex] >= '0' && newNote[octIndex] <= '9') {
		newVolts = (float)(newNote[octIndex] - '0') - 4.0f + whiteKeysVocts[newNote[0] - 'A'];
		if (hasAccidental) {
			if (newNote[1] == '#') {
				newVolts += 1.0f/12.0f;
			}
			else {
				newVolts -= 1.0f/12.0f;
			}
		}
	}			

	return newVolts;
}