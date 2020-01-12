//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc BoulĂ© 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#ifndef MMM_EQMENUS_HPP
#define MMM_EQMENUS_HPP


#include "EqMasterCommon.hpp"
#include "VuMeters.hpp"


// Freq, gain, q display labels right-click menu
// --------------------

// show notes (freq displays only)
struct ShowNotesItem : MenuItem {
	int8_t *showFreqAsNotesSrc;
	void onAction(const event::Action &e) override {
		*showFreqAsNotesSrc ^= 0x1;
	}
};

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

	struct KnobArcShowSubItem : MenuItem {
		int8_t *srcDetailsShow;
		int setVal;
		void onAction(const event::Action &e) override {
			*srcDetailsShow &= ~0x3;
			*srcDetailsShow |= setVal;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		KnobArcShowSubItem *arcShowItem0 = createMenuItem<KnobArcShowSubItem>("On", CHECKMARK((*srcDetailsShow & 0x3) == 0x3));
		arcShowItem0->srcDetailsShow = srcDetailsShow;
		arcShowItem0->setVal = 0x3;
		menu->addChild(arcShowItem0);
		
		KnobArcShowSubItem *arcShowItem1 = createMenuItem<KnobArcShowSubItem>("CV only", CHECKMARK((*srcDetailsShow & 0x3) == 0x2));
		arcShowItem1->srcDetailsShow = srcDetailsShow;
		arcShowItem1->setVal = 0x2;
		menu->addChild(arcShowItem1);
		
		KnobArcShowSubItem *arcShowItem2 = createMenuItem<KnobArcShowSubItem>("Off", CHECKMARK((*srcDetailsShow & 0x3) == 0x0));
		arcShowItem2->srcDetailsShow = srcDetailsShow;
		arcShowItem2->setVal = 0x0;
		menu->addChild(arcShowItem2);
		
		return menu;
	}
};

struct MomentaryCvItem : MenuItem {
	int8_t *momentaryCvButtonsSrc;

	struct MomentaryCvSubItem : MenuItem {
		int8_t *momentaryCvButtonsSrc;
		void onAction(const event::Action &e) override {
			*momentaryCvButtonsSrc ^= 0x1;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		MomentaryCvSubItem *mo1Item = createMenuItem<MomentaryCvSubItem>("Trigger toggle", CHECKMARK(*momentaryCvButtonsSrc == 1));
		mo1Item->momentaryCvButtonsSrc = momentaryCvButtonsSrc;
		menu->addChild(mo1Item);

		MomentaryCvSubItem *mo0Item = createMenuItem<MomentaryCvSubItem>("Gate high/low", CHECKMARK(*momentaryCvButtonsSrc == 0));
		mo0Item->momentaryCvButtonsSrc = momentaryCvButtonsSrc;
		menu->addChild(mo0Item);

		return menu;
	}
};


struct VuColorEqItem : MenuItem {
	int8_t *srcColors;

	struct VuColorSubItem : MenuItem {
		int8_t *srcColors;
		int setVal;
		void onAction(const event::Action &e) override {
			for (int t = 0; t < 24; t++) {
				srcColors[t] = setVal;
			}
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		
		for (int i = 0; i < numVuThemes; i++) {
			VuColorSubItem *vuColItem = createMenuItem<VuColorSubItem>(vuColorNames[i], CHECKMARK(*srcColors == i));
			vuColItem->srcColors = srcColors;
			vuColItem->setVal = i;
			menu->addChild(vuColItem);
		}

		return menu;
	}
};


struct DispColorEqItem : MenuItem {
	int8_t *srcColors;

	struct DispColorSubItem : MenuItem {
		int8_t *srcColors;
		int setVal;
		void onAction(const event::Action &e) override {
			for (int t = 0; t < 24; t++) {
				srcColors[t] = setVal;
			}
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		
		for (int i = 0; i < numDispThemes; i++) {		
			DispColorSubItem *dispColItem = createMenuItem<DispColorSubItem>(dispColorNames[i], CHECKMARK(*srcColors == i));
			dispColItem->srcColors = srcColors;
			dispColItem->setVal = i;
			menu->addChild(dispColItem);
		}
		
		return menu;
	}
};


struct FetchLabelsItem : MenuItem {
	int *mappedIdSrc;
	
	struct FetchLabelsSubItem : MenuItem {
		int *mappedIdSrc;
		int setId;
		void onAction(const event::Action &e) override {
			*mappedIdSrc = setId;
		}
	};
	
	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		FetchLabelsSubItem *id0Item = createMenuItem<FetchLabelsSubItem>("None (default labels)", CHECKMARK(*mappedIdSrc == 0));
		id0Item->mappedIdSrc = mappedIdSrc;
		id0Item->setId = 0;
		menu->addChild(id0Item);

		bool sawMappedId = *mappedIdSrc == 0;
		
		std::vector<MessageBase>* mixerMessageSurvey = mixerMessageBus.surveyValues();
		for (MessageBase pl : *mixerMessageSurvey) {
			if (*mappedIdSrc == pl.id) {
				sawMappedId = true;
			}
			std::string mixerName = std::string(pl.name) + string::f("  (id %d)", pl.id);
			FetchLabelsSubItem *idItem = createMenuItem<FetchLabelsSubItem>(mixerName, CHECKMARK(*mappedIdSrc == pl.id));
			idItem->mappedIdSrc = mappedIdSrc;
			idItem->setId = pl.id;
			menu->addChild(idItem);
		}
		delete mixerMessageSurvey;
		
		if (!sawMappedId) {
			std::string mixerName = std::string("[deleted]") + string::f("  (id %d)", *mappedIdSrc);
			FetchLabelsSubItem *idItem = createMenuItem<FetchLabelsSubItem>(mixerName, CHECKMARK(true));
			idItem->mappedIdSrc = mappedIdSrc;
			idItem->setId = *mappedIdSrc;
			idItem->disabled = true;
			menu->addChild(idItem);
		}
		
		return menu;
	}
};


struct CopyTrackSettingsItem : MenuItem {
	char* trackLabelsSrc;
	TrackEq *trackEqsSrc;
	int trackNumSrc;

	struct CopyTrackSettingsSubItem : MenuItem {
		TrackEq *trackEqsSrc;
		int trackNumSrc;	
		int trackNumDest;

		void onAction(const event::Action &e) override {
			trackEqsSrc[trackNumDest].copyFrom(&trackEqsSrc[trackNumSrc]);
		}
	};
	
	
	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		for (int trk = 0; trk < 24; trk++) {
			bool onSource = (trk == trackNumSrc);
			CopyTrackSettingsSubItem *reo0Item = createMenuItem<CopyTrackSettingsSubItem>(std::string(&(trackLabelsSrc[trk * 4]), 4), CHECKMARK(onSource));
			reo0Item->trackEqsSrc = trackEqsSrc;
			reo0Item->trackNumSrc = trackNumSrc;
			reo0Item->trackNumDest = trk;
			reo0Item->disabled = onSource;
			menu->addChild(reo0Item);
		}

		return menu;
	}		
};


#endif
