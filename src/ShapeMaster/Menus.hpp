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

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		
		menu->addChild(createCheckMenuItem("Normal (default)", "",
			[=]() {return *srcInvShadow == 0;},
			[=]() {*srcInvShadow = 0;}
		));	
		menu->addChild(createCheckMenuItem("Inverted (ducking)", "",
			[=]() {return *srcInvShadow == 1;},
			[=]() {*srcInvShadow = 1;}
		));	
		menu->addChild(createCheckMenuItem("Set per channel", "",
			[=]() {return *srcInvShadow == 2;},
			[=]() {*srcInvShadow = 2;}
		));	

		return menu;
	}
};

void createChannelMenu(ui::Menu* menu, Channel* channels, int chan, const PackedBytes4* miscSettings2GlobalSrc, bool trigExpPresent, bool* running);



// Module's context menu
// --------------------

struct RunOffSettingItem : MenuItem {
	int8_t *srcRunOffSetting;

	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		menu->addChild(createCheckMenuItem("All channels stop and reset", "",
			[=]() {return *srcRunOffSetting != 0x0;},
			[=]() {*srcRunOffSetting ^= 0x1;}
		));	
	
		menu->addChild(createCheckMenuItem("All channels freeze", "",
			[=]() {return *srcRunOffSetting == 0x0;},
			[=]() {*srcRunOffSetting ^= 0x1;}
		));	

		return menu;
	}
};

#ifdef SM_PRO
template <typename TShapeMaster>
struct PpqnItem : MenuItem {
	TShapeMaster *srcShapeMaster;
	
	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		
		if (srcShapeMaster->running) {
			menu->addChild(createMenuLabel("[Turn off run]"));
		}
		
		menu->addChild(createMenuLabel("Not recommended:"));
		for (int i = 0; i < NUM_PPQN_CHOICES; i++) {	
			int ppqnValue = ppqnValues[i];
			if (ppqnValue == 48) {
				menu->addChild(new MenuSeparator());		
				menu->addChild(createMenuLabel("Recommended:"));
			}
			std::string ppqnText = string::f("%i", ppqnValue);
			
			menu->addChild(createCheckMenuItem(ppqnText, "",
				[=]() {return srcShapeMaster->clockDetector.getPpqn() == ppqnValue;},
				[=]() {srcShapeMaster->clockDetector.setPpqn(ppqnValue);
						srcShapeMaster->clockDetector.resetClockDetector();},
				srcShapeMaster->running
			));	
		}	
		
		return menu;
	}
};
#endif


struct KnobDispColorItem : MenuItem {
	int8_t *srcColor;

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		
		for (int i = 0; i < 2; i++) {// only yellow and light gray in loop, use chan color below
			menu->addChild(createCheckMenuItem(dispColorNames[i], "",
				[=]() {return *srcColor == i;},
				[=]() {*srcColor = i;}
			));	
		}
		// use chan color
		menu->addChild(createCheckMenuItem("Use channel colour", "",
			[=]() {return *srcColor == 2;},
			[=]() {*srcColor = 2;}
		));	
		
		return menu;
	}
};

struct KnobArcShowItem : MenuItem {
	int8_t *srcDetailsShow;

	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		menu->addChild(createCheckMenuItem("On", "",
			[=]() {return (*srcDetailsShow & 0x3) == 0x3;},
			[=]() {*srcDetailsShow &= ~0x3;  
				*srcDetailsShow |= 0x3;}
		));	
		
		menu->addChild(createCheckMenuItem("CV only", "",
			[=]() {return (*srcDetailsShow & 0x3) == 0x2;},
			[=]() {*srcDetailsShow &= ~0x3;  
				*srcDetailsShow |= 0x2;}
		));	
			
		menu->addChild(createCheckMenuItem("Off", "",
			[=]() {return (*srcDetailsShow & 0x3) == 0x0;},
			[=]() {*srcDetailsShow &= ~0x3;  
				*srcDetailsShow |= 0x0;}
		));	
		
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
void addSyncRatioMenuTwoLevel(Menu* menu, Param* lengthSyncParamSrc, Channel* channel);
#endif

void addUnsyncRatioMenu(Menu* menu, Param* lengthUnsyncParamSrc, Channel* channel);

