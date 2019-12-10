//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc BoulĂ© 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#ifndef MMM_EQMENUS_HPP
#define MMM_EQMENUS_HPP


#include "EqMasterCommon.hpp"


// Module's context menu
// --------------------

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


struct DispColorItem : MenuItem {
	int8_t *srcColor;
	const bool isGlobal = false;// true when this is in the context menu of module, false when it is in a track/group/master context menu

	struct DispColorSubItem : MenuItem {
		int8_t *srcColor;
		int setVal;
		void onAction(const event::Action &e) override {
			*srcColor = setVal;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		std::string dispColorNames[8] = {
			"Yellow (default)",
			"Light-grey",
			"Green",
			"Aqua",
			"Cyan",
			"Blue",
			"Purple",
			"Set per track"
		};
		
		for (int i = 0; i < (isGlobal ? 8 : 7); i++) {		
			DispColorSubItem *dispColItem = createMenuItem<DispColorSubItem>(dispColorNames[i], CHECKMARK(*srcColor == i));
			dispColItem->srcColor = srcColor;
			dispColItem->setVal = i;
			menu->addChild(dispColItem);
		}
		
		return menu;
	}
};


#endif
