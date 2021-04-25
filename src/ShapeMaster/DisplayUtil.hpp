//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//Based on code from the Fundamental plugin by Andrew Belt
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#pragma once

#include "../MindMeldModular.hpp"
#include "Channel.hpp"


enum ScopeMasks {SCOPE_MASK_ON = 0x2, SCOPE_MASK_VCA_nSC = 0x1};

struct ScopeBuffers {
	static const int SCOPE_PTS = 767;// scope memories, divide into this many segments, other code must be adapted if chaged (drawPoint binary stuff depends on this)
	
	// no need to reset/init the points during run, we will use the drawPoint flags for that
	float scpFrontYmin[SCOPE_PTS + 1];// points of the main (front) scope curve, with an extra element for last
	float scpFrontYmax[SCOPE_PTS + 1];// points of the main (front) scope curve, with an extra element for last
	float scpBackYmin[SCOPE_PTS + 1];// points of the alt (back) scope curve, with an extra element for last
	float scpBackYmax[SCOPE_PTS + 1];// points of the alt (back) scope curve, with an extra element for last
	bool scopeOn;// takes channelActive into account (not the case in ScopeSettingsButtons())
	bool scopeVca;
	int lastState;
	int8_t lastTrigMode;
	Channel* lastChannel;
	
	int lastScpI;
	uint64_t drawPoint[12];// 12 is (767 + 1) / 64
	
	
	void reset() {
		memset(scpFrontYmin, 0, sizeof(scpFrontYmin));
		memset(scpFrontYmax, 0, sizeof(scpFrontYmax));
		memset(scpBackYmin, 0, sizeof(scpBackYmin));
		memset(scpBackYmax, 0, sizeof(scpBackYmax));
		scopeOn = false;
		scopeVca = false;
		lastState = PlayHead::STOPPED;
		lastTrigMode = -1;
		lastChannel = NULL;
		clear();
	}
	void clear() {
		lastScpI = -1;
		memset(drawPoint, 0, sizeof(drawPoint));
	}
	
	void setPoint(uint64_t i) {
		drawPoint[i >> 6] |= ((uint64_t)0x1 << (i & (uint64_t)0x3F));
	}
	bool isDrawPoint(uint64_t i) {
		return ( drawPoint[i >> 6] & ((uint64_t)0x1 << (i & (uint64_t)0x3F)) ) != 0;
	}
	
	void populate(Channel* channel, int8_t scopeSettings);
};


struct DisplayInfo {
	int lastMovedKnobId = -1;// this is a param index to indicate a param knob type and is only in [0; NUM_KNOB_PARAMS - 1]
	time_t lastMovedKnobTime = 0;
	
	std::string displayMessage = "";	
	time_t displayMessageTimeOff = 0;// value of time(0) at which the message can stop being displayed
};


inline int calcNumHLinesAndWithOct(int _rangeIndex, int* _numHLines) {
	*_numHLines = rangeValues[_rangeIndex];
	if (*_numHLines < 0) {
		*_numHLines *= -2;
	}
	int numHLinesWithOct = *_numHLines;
	if (numHLinesWithOct < 5) {
		numHLinesWithOct *= 12;
	}	
	return numHLinesWithOct;
}


