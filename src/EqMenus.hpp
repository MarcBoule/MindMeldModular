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

#endif
