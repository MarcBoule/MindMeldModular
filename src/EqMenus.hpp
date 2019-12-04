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


		std::vector<MessageBase>* mixerMessageSurvey = mixerMessageBus.surveyValues();
		for (MessageBase pl : *mixerMessageSurvey) {
			FetchLabelsSubItem *idItem = createMenuItem<FetchLabelsSubItem>(std::string(&(pl.name[0]), 6) + string::f(" (id %i)", pl.id), CHECKMARK(*mappedIdSrc == pl.id));
			idItem->mappedIdSrc = mappedIdSrc;
			idItem->setId = pl.id;
			menu->addChild(idItem);
		}
		delete mixerMessageSurvey;
		
		return menu;
	}
};

#endif
