//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc BoulĂ© 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#pragma once

#include "EqMasterCommon.hpp"
#include "../comp/VuMeters.hpp"


// Freq, gain, q display labels right-click menu
// --------------------

// CV level items (freq, gain, q)
struct CvLevelQuantity : Quantity {
	float *srcCvLevel = NULL;
	  
	CvLevelQuantity(float *_srcCvLevel) {
		srcCvLevel = _srcCvLevel;
	}
	void setValue(float value) override {
		*srcCvLevel = math::clamp(value, getMinValue(), getMaxValue());
	}
	float getValue() override {
		return *srcCvLevel;
	}
	float getMinValue() override {return 0.0f;}
	float getMaxValue() override {return 1.0f;}
	float getDefaultValue() override {return 1.0f;}
	float getDisplayValue() override {return getValue();}
	std::string getDisplayValueString() override {
		return string::f("%i", (int)std::round(getDisplayValue() * 100.0f));
	}
	void setDisplayValue(float displayValue) override {setValue(displayValue);}
	std::string getLabel() override {return "CV input level";}
	std::string getUnit() override {return " %";}
};

struct CvLevelSlider : ui::Slider {
	CvLevelSlider(float *_srcCvLevel) {
		quantity = new CvLevelQuantity(_srcCvLevel);
	}
	~CvLevelSlider() {
		delete quantity;
	}
};
		

// Module's context menu
// --------------------

struct KnobArcShowItem : MenuItem {
	int8_t *srcDetailsShow;

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		menu->addChild(createCheckMenuItem("On", "",
			[=]() {return (*srcDetailsShow & 0x3) == 0x3;},
			[=]() {*srcDetailsShow &= ~0x3;   *srcDetailsShow |= 0x3;}
		));	
		menu->addChild(createCheckMenuItem("CV only", "",
			[=]() {return (*srcDetailsShow & 0x3) == 0x2;},
			[=]() {*srcDetailsShow &= ~0x3;   *srcDetailsShow |= 0x2;}
		));	
		menu->addChild(createCheckMenuItem("Off", "",
			[=]() {return (*srcDetailsShow & 0x3) == 0x0;},
			[=]() {*srcDetailsShow &= ~0x3;   *srcDetailsShow |= 0x0;}
		));	
		return menu;
	}
};

struct MomentaryCvItem : MenuItem {
	int8_t *momentaryCvButtonsSrc;

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		menu->addChild(createCheckMenuItem("Trigger toggle", "",
			[=]() {return *momentaryCvButtonsSrc == 1;},
			[=]() {*momentaryCvButtonsSrc ^= 0x1;}
		));	
		menu->addChild(createCheckMenuItem("Gate high/low", "",
			[=]() {return *momentaryCvButtonsSrc == 0;},
			[=]() {*momentaryCvButtonsSrc ^= 0x1;}
		));	
		return menu;
	}
};



struct MixerLinkItem : MenuItem {
	int64_t *mappedIdSrc;
	
	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		menu->addChild(createCheckMenuItem("None (default labels)", "",
			[=]() {return *mappedIdSrc == 0;},
			[=]() {*mappedIdSrc = 0;}
		));	

		bool sawMappedId = *mappedIdSrc == 0;
		
		std::vector<MessageBase>* mixerMessageSurvey = mixerMessageBus.surveyValues();
		for (MessageBase pl : *mixerMessageSurvey) {
			if (*mappedIdSrc == pl.id) {
				sawMappedId = true;
			}
			std::string mixerName = std::string(pl.name) + string::f("  (id %" PRId64 ")", pl.id);
			menu->addChild(createCheckMenuItem(mixerName, "",
				[=]() {return *mappedIdSrc == pl.id;},
				[=]() {*mappedIdSrc = pl.id;}
			));	
		}
		delete mixerMessageSurvey;
		
		if (!sawMappedId) {
			int64_t deletedMappedIdSrc = *mappedIdSrc;
			std::string mixerName = std::string("[deleted]") + string::f("  (id %" PRId64 ")", *mappedIdSrc);
			menu->addChild(createCheckMenuItem(mixerName, "",
				[=]() {return *mappedIdSrc == deletedMappedIdSrc;},
				[=]() {},
				true
			));	
		}
		
		return menu;
	}
};


struct CopyTrackSettingsItem : MenuItem {
	Param* trackParamSrc;
	TrackEq *trackEqsSrc;
	char* trackLabelsSrc;

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		for (int trk = 0; trk < 24; trk++) {
			menu->addChild(createCheckMenuItem(std::string(&(trackLabelsSrc[trk * 4]), 4), "",
				[=]() {return trk == (int)(trackParamSrc->getValue() + 0.5f);},
				[=]() {int trackNumSrc = (int)(trackParamSrc->getValue() + 0.5f);
						if (trackNumSrc != trk) trackEqsSrc[trk].copyFrom(&trackEqsSrc[trackNumSrc]);}
			));
		}
		return menu;
	}		
};


void moveTrack(TrackEq *trackEqsSrc, int trackNumSrc, int trackNumDest) {
	// does not check for trackNumSrc == trackNumDest
	TrackEq buffer1;
	
	buffer1.copyFrom(&trackEqsSrc[trackNumSrc]);
	if (trackNumDest < trackNumSrc) {
		for (int trk = trackNumSrc - 1; trk >= trackNumDest; trk--) {
			trackEqsSrc[trk + 1].copyFrom(&trackEqsSrc[trk]);
		}
	}
	else {// must automatically be bigger (equal is impossible)
		for (int trk = trackNumSrc; trk < trackNumDest; trk++) {
			trackEqsSrc[trk].copyFrom(&trackEqsSrc[trk + 1]);
		}
	}
	trackEqsSrc[trackNumDest].copyFrom(&buffer1);
}


struct MoveTrackSettingsItem : MenuItem {
	Param* trackParamSrc;
	TrackEq *trackEqsSrc;
	char* trackLabelsSrc;
	int* updateTrackLabelRequestSrc = NULL;
	
	struct MoveTrackSubItem : MenuItem {	
		TrackEq *trackEqsSrc = NULL;
		int trackNumSrc;	
		int trackNumDest;
		int* updateTrackLabelRequestSrc = NULL;
		char* trackLabelsSrc;
		
		void onAction(const event::Action &e) override {
			if (trackNumSrc != trackNumDest) {		
				moveTrack(trackEqsSrc, trackNumSrc, trackNumDest);
				*updateTrackLabelRequestSrc = 2;// force param refreshing
				
				e.consume(this);// don't allow ctrl-click to keep menu open
			}
		}
	};
	
	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		int trackNumSrc = (int)(trackParamSrc->getValue() + 0.5f);
		
		for (int trk = 0; trk < 24; trk++) {
			bool onSource = (trk == trackNumSrc);
			MoveTrackSubItem *reo0Item = createMenuItem<MoveTrackSubItem>(std::string(&(trackLabelsSrc[trk * 4]), 4), CHECKMARK(onSource));
			reo0Item->trackEqsSrc = trackEqsSrc;
			reo0Item->trackNumSrc = trackNumSrc;
			reo0Item->trackNumDest = trk;
			reo0Item->updateTrackLabelRequestSrc = updateTrackLabelRequestSrc;
			reo0Item->trackLabelsSrc = trackLabelsSrc;
			reo0Item->disabled = onSource;
			menu->addChild(reo0Item);
		}
		
		return menu;
	}		
};


struct DecayRateItem : MenuItem {
	int8_t *decayRateSrc;

	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		std::string decayRateNames[4] = {
			"Slow",
			"Medium",
			"Fast (default)",
			"Off"
		};
		
		for (int i = 0; i < 4; i++) {
			menu->addChild(createCheckMenuItem(decayRateNames[i], "",
				[=]() {return *decayRateSrc == i;},
				[=]() {*decayRateSrc = i;}
			));	
		}
		
		return menu;
	}
};


