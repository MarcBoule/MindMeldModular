//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.txt for all licenses
//***********************************************************************************************

#ifndef MMM_MIXERMENUS_HPP
#define MMM_MIXERMENUS_HPP


#include "Mixer.hpp"


// Module's context menu
// --------------------

struct PanLawMonoItem : MenuItem {
	GlobalInfo *gInfo;
	
	struct PanLawMonoSubItem : MenuItem {
		GlobalInfo *gInfo;
		int setVal = 0;
		void onAction(const event::Action &e) override {
			gInfo->panLawMono = setVal;
		}
	};
	
	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		PanLawMonoSubItem *law0Item = createMenuItem<PanLawMonoSubItem>("+0 dB (no compensation)", CHECKMARK(gInfo->panLawMono == 0));
		law0Item->gInfo = gInfo;
		menu->addChild(law0Item);

		PanLawMonoSubItem *law1Item = createMenuItem<PanLawMonoSubItem>("+3 dB boost (equal power, default)", CHECKMARK(gInfo->panLawMono == 1));
		law1Item->gInfo = gInfo;
		law1Item->setVal = 1;
		menu->addChild(law1Item);

		PanLawMonoSubItem *law2Item = createMenuItem<PanLawMonoSubItem>("+4.5 dB boost (compromise)", CHECKMARK(gInfo->panLawMono == 2));
		law2Item->gInfo = gInfo;
		law2Item->setVal = 2;
		menu->addChild(law2Item);

		PanLawMonoSubItem *law3Item = createMenuItem<PanLawMonoSubItem>("+6 dB boost (linear)", CHECKMARK(gInfo->panLawMono == 3));
		law3Item->gInfo = gInfo;
		law3Item->setVal = 3;
		menu->addChild(law3Item);

		return menu;
	}
};


struct PanLawStereoItem : MenuItem {
	GlobalInfo *gInfo;

	struct PanLawStereoSubItem : MenuItem {
		GlobalInfo *gInfo;
		int setVal = 0;
		void onAction(const event::Action &e) override {
			gInfo->panLawStereo = setVal;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		PanLawStereoSubItem *law0Item = createMenuItem<PanLawStereoSubItem>("Stereo balance (default)", CHECKMARK(gInfo->panLawStereo == 0));
		law0Item->gInfo = gInfo;
		menu->addChild(law0Item);

		PanLawStereoSubItem *law1Item = createMenuItem<PanLawStereoSubItem>("True panning", CHECKMARK(gInfo->panLawStereo == 1));
		law1Item->gInfo = gInfo;
		law1Item->setVal = 1;
		menu->addChild(law1Item);

		return menu;
	}
};


struct DirectOutsItem : MenuItem {
	GlobalInfo *gInfo;

	struct DirectOutsSubItem : MenuItem {
		GlobalInfo *gInfo;
		int setVal = 0;
		void onAction(const event::Action &e) override {
			gInfo->directOutsMode = setVal;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		DirectOutsSubItem *law0Item = createMenuItem<DirectOutsSubItem>("Pre-fader", CHECKMARK(gInfo->directOutsMode == 0));
		law0Item->gInfo = gInfo;
		menu->addChild(law0Item);

		DirectOutsSubItem *law1Item = createMenuItem<DirectOutsSubItem>("Post-fader", CHECKMARK(gInfo->directOutsMode == 1));
		law1Item->gInfo = gInfo;
		law1Item->setVal = 1;
		menu->addChild(law1Item);

		return menu;
	}
};

struct NightModeItem : MenuItem {
	GlobalInfo *gInfo;
	void onAction(const event::Action &e) override {
		gInfo->nightMode = !gInfo->nightMode;
	}
};


#endif
