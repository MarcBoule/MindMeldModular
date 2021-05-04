//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#pragma once

#include "../MindMeldModular.hpp"
#include "Channel.hpp"



// Display background menu
// --------------------

void createBackgroundMenu(ui::Menu* menu, Shape* shape, Vec normPos);



// Normal point menus 
// --------------------

void createPointMenu(ui::Menu* menu, Channel* channel, int pt);



// Control point menus 
// --------------------

void createCtrlMenu(ui::Menu* menu, Shape* shape, int pt);



// Channel menu
// --------------------

struct InvShadowItem : MenuItem {
	int8_t *srcInvShadow;
	bool isGlobal = false;// true when this is in the context menu of module, false when it is in a track/group/master context menu

	struct InvShadowSubItem : MenuItem {
		int8_t *srcInvShadow;
		int setVal;
		void onAction(const event::Action &e) override {
			*srcInvShadow = setVal;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		
		InvShadowSubItem *invNormItem = createMenuItem<InvShadowSubItem>("Normal (default)", CHECKMARK(*srcInvShadow == 0));
		invNormItem->srcInvShadow = srcInvShadow;
		invNormItem->setVal = 0;
		menu->addChild(invNormItem);

		InvShadowSubItem *invInvItem = createMenuItem<InvShadowSubItem>("Inverted (ducking)", CHECKMARK(*srcInvShadow == 1));
		invInvItem->srcInvShadow = srcInvShadow;
		invInvItem->setVal = 1;
		menu->addChild(invInvItem);

		if (isGlobal) {
			InvShadowSubItem *invPerItem = createMenuItem<InvShadowSubItem>("Set per channel", CHECKMARK(*srcInvShadow == 2));
			invPerItem->srcInvShadow = srcInvShadow;
			invPerItem->setVal = 2;
			menu->addChild(invPerItem);
		}

		return menu;
	}
};

void createChannelMenu(ui::Menu* menu, Channel* channels, int chan, PackedBytes4* miscSettings2GlobalSrc, bool trigExpPresent, bool* running);



// Module's context menu
// --------------------

struct ShowChanNamesItem : MenuItem {
	int8_t *srcShowChanNames;
	void onAction(const event::Action &e) override {
		*srcShowChanNames ^= 0x1;
	}
};

struct ShowPointTooltipItem : MenuItem {
	int8_t *srcPointTooltipNames;
	void onAction(const event::Action &e) override {
		*srcPointTooltipNames ^= 0x1;
	}
};


struct RunOffSettingItem : MenuItem {
	int8_t *srcRunOffSetting;

	struct RunOffSettingSubItem : MenuItem {
		int8_t *srcRunOffSetting;
		void onAction(const event::Action &e) override {
			*srcRunOffSetting ^= 0x1;
		}
	};
	
	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		RunOffSettingSubItem *runOffItem1 = createMenuItem<RunOffSettingSubItem>("All channels stop and reset", CHECKMARK(*srcRunOffSetting  != 0x0));
		runOffItem1->srcRunOffSetting = srcRunOffSetting;
		menu->addChild(runOffItem1);
		
		RunOffSettingSubItem *runOffItem0 = createMenuItem<RunOffSettingSubItem>("All channels freeze", CHECKMARK(*srcRunOffSetting  == 0x0));
		runOffItem0->srcRunOffSetting = srcRunOffSetting;
		menu->addChild(runOffItem0);
		
		return menu;
	}
};

#ifdef SM_PRO
template <typename TShapeMaster>
struct PpqnItem : MenuItem {
	TShapeMaster *srcShapeMaster;

	struct PpqnSubItem : MenuItem {
		TShapeMaster *srcShapeMaster;
		int setVal;
		void onAction(const event::Action &e) override {
			srcShapeMaster->clockDetector.setPpqn(setVal);
			srcShapeMaster->clockDetector.resetClockDetector();
		}
	};
	
	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		
		if (srcShapeMaster->running) {
			MenuLabel *disabledLabel = new MenuLabel();
			disabledLabel->text = "[Turn off run]";
			menu->addChild(disabledLabel);
		}
		
		MenuLabel *notRecLabel = new MenuLabel();
		notRecLabel->text = "Not recommended:";
		menu->addChild(notRecLabel);

		
		for (int i = 0; i < NUM_PPQN_CHOICES; i++) {	
			int ppqnValue = ppqnValues[i];
			if (ppqnValue == 48) {
				menu->addChild(new MenuSeparator());		
				MenuLabel *recLabel = new MenuLabel();
				recLabel->text = "Recommended:";
				menu->addChild(recLabel);
			}
			std::string ppqnText = string::f("%i", ppqnValue);
			PpqnSubItem *ppqnItem0 = createMenuItem<PpqnSubItem>(ppqnText, CHECKMARK(srcShapeMaster->clockDetector.getPpqn() == ppqnValue));
			ppqnItem0->srcShapeMaster = srcShapeMaster;
			ppqnItem0->setVal = ppqnValue;
			ppqnItem0->disabled = srcShapeMaster->running;
			menu->addChild(ppqnItem0);
		}	
		
		return menu;
	}
};


template <typename TShapeMaster>
struct PpqnAvgItem : MenuItem {
	TShapeMaster *srcShapeMaster;

	struct PpqnAvgSubItem : MenuItem {
		TShapeMaster *srcShapeMaster;
		int setVal;
		void onAction(const event::Action &e) override {
			srcShapeMaster->clockDetector.setPpqnAvg(setVal);
			srcShapeMaster->clockDetector.resetClockDetector();
		}
	};
	
	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		
		if (srcShapeMaster->running) {
			MenuLabel *disabledLabel = new MenuLabel();
			disabledLabel->text = "[Turn off run]";
			menu->addChild(disabledLabel);
		}
		
		PpqnAvgSubItem *ppqnAvgItem1 = createMenuItem<PpqnAvgSubItem>("None", CHECKMARK(srcShapeMaster->clockDetector.getPpqnAvg() == 1));
		ppqnAvgItem1->srcShapeMaster = srcShapeMaster;
		ppqnAvgItem1->setVal = 1;
		ppqnAvgItem1->disabled = srcShapeMaster->running;
		menu->addChild(ppqnAvgItem1);
		
		PpqnAvgSubItem *ppqnAvgItem2 = createMenuItem<PpqnAvgSubItem>("2 pulses", CHECKMARK(srcShapeMaster->clockDetector.getPpqnAvg() == 2));
		ppqnAvgItem2->srcShapeMaster = srcShapeMaster;
		ppqnAvgItem2->setVal = 2;
		ppqnAvgItem2->disabled = srcShapeMaster->running;
		menu->addChild(ppqnAvgItem2);
		
		PpqnAvgSubItem *ppqnAvgItem4 = createMenuItem<PpqnAvgSubItem>("4 pulses", CHECKMARK(srcShapeMaster->clockDetector.getPpqnAvg() == 4));
		ppqnAvgItem4->srcShapeMaster = srcShapeMaster;
		ppqnAvgItem4->setVal = 4;
		ppqnAvgItem4->disabled = srcShapeMaster->running;
		menu->addChild(ppqnAvgItem4);
		
		PpqnAvgSubItem *ppqnAvgItem8 = createMenuItem<PpqnAvgSubItem>("8 pulses", CHECKMARK(srcShapeMaster->clockDetector.getPpqnAvg() == 8));
		ppqnAvgItem8->srcShapeMaster = srcShapeMaster;
		ppqnAvgItem8->setVal = 8;
		ppqnAvgItem8->disabled = srcShapeMaster->running;
		menu->addChild(ppqnAvgItem8);
		
		return menu;
	}
};
#endif

struct KnobDispColorItem : MenuItem {
	int8_t *srcColor;

	struct DispColorSubItem : MenuItem {
		int8_t *srcColor;
		int setVal;
		void onAction(const event::Action &e) override {
			*srcColor = setVal;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		
		for (int i = 0; i < 2; i++) {// only yellow and light gray in loop, use chan color below
			DispColorSubItem *dispColItem = createMenuItem<DispColorSubItem>(dispColorNames[i], CHECKMARK(*srcColor == i));
			dispColItem->srcColor = srcColor;
			dispColItem->setVal = i;
			menu->addChild(dispColItem);
		}
		// use chan color
		DispColorSubItem *dispColItem = createMenuItem<DispColorSubItem>("Use channel colour", CHECKMARK(*srcColor == 2));
		dispColItem->srcColor = srcColor;
		dispColItem->setVal = 2;
		menu->addChild(dispColItem);
		
		return menu;
	}
};

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


// Line width menu item

struct LineWidthQuantity : Quantity {
	float *srcLineWidth = NULL;
	  
	LineWidthQuantity(float *_srcLineWidth) {
		srcLineWidth = _srcLineWidth;
	}
	void setValue(float value) override {
		*srcLineWidth = math::clamp(value, getMinValue(), getMaxValue());
	}
	float getValue() override {
		return *srcLineWidth;
	}
	float getMinValue() override {return 0.3f;}
	float getMaxValue() override {return 2.0f;}
	float getDefaultValue() override {return 1.0f;}
	float getDisplayValue() override {return getValue();}
	std::string getDisplayValueString() override {
		return string::f("%.1f", getDisplayValue());
	}
	void setDisplayValue(float displayValue) override {setValue(displayValue);}
	std::string getLabel() override {return "Line width";}
	std::string getUnit() override {
		return " pt";
	}};

struct LineWidthSlider : ui::Slider {
	LineWidthSlider(float *_srcLineWidth) {
		quantity = new LineWidthQuantity(_srcLineWidth);
	}
	~LineWidthSlider() {
		delete quantity;
	}
};


struct ShowPointsItem : MenuItem {
	int8_t* setValSrc;

	void onAction(const event::Action &e) override {
		*setValSrc ^= 0x1;
	}
};



// Sidechain settings menu
// --------------------

void createSidechainSettingsMenu(Channel* channel);



// Display menus (Randon, Snap and Range)
// --------------------

void addRandomMenu(Menu* menu, Channel* channel);

void addGridXMenu(Menu* menu, Channel* channel);

void addRangeMenu(Menu* menu, Channel* channel);



// Block 1 menus
// --------------------

// Trig mode sub item

void addTrigModeMenu(Menu* menu, Channel* channel);


// Play mode menu item

void addPlayModeMenu(Menu* menu, Channel* channel);



// Length menu

#ifdef SM_PRO
void addSyncRatioMenu(Menu* menu, Param* lengthSyncParamSrc, Channel* channel);
void addSyncRatioMenuTwoLevel(Menu* menu, Param* lengthSyncParamSrc, Channel* channel);
#endif

void addUnsyncRatioMenu(Menu* menu, Param* lengthUnsyncParamSrc, Channel* channel);

