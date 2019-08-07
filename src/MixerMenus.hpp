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

struct VuColorItem : MenuItem {
	GlobalInfo *gInfo;

	struct VuColorItemSubItem : MenuItem {
		GlobalInfo *gInfo;
		int setVal = 0;
		void onAction(const event::Action &e) override {
			gInfo->vuColor = setVal;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		VuColorItemSubItem *col0Item = createMenuItem<VuColorItemSubItem>("Green (default)", CHECKMARK(gInfo->vuColor == 0));
		col0Item->gInfo = gInfo;
		menu->addChild(col0Item);

		VuColorItemSubItem *col1Item = createMenuItem<VuColorItemSubItem>("Blue", CHECKMARK(gInfo->vuColor == 1));
		col1Item->gInfo = gInfo;
		col1Item->setVal = 1;
		menu->addChild(col1Item);

		VuColorItemSubItem *col2Item = createMenuItem<VuColorItemSubItem>("Purple", CHECKMARK(gInfo->vuColor == 2));
		col2Item->gInfo = gInfo;
		col2Item->setVal = 2;
		menu->addChild(col2Item);

		return menu;
	}
};



// Track context menu
// --------------------

// Gain adjust menu item

struct GainAdjustQuantity : Quantity {
	MixerTrack *srcTrack = NULL;
	float gainInDB = 0.0f;
	  
	GainAdjustQuantity(MixerTrack *_srcTrack) {
		srcTrack = _srcTrack;
	}
	void setValue(float value) override {
		gainInDB = math::clamp(value, getMinValue(), getMaxValue());
		srcTrack->gainAdjust = std::pow(10.0f, gainInDB / 20.0f);
	}
	float getValue() override {
		gainInDB = 20.0f * std::log10(srcTrack->gainAdjust);
		return gainInDB;
	}
	float getMinValue() override {return -20.0f;}
	float getMaxValue() override {return 20.0f;}
	float getDefaultValue() override {return 0.0f;}
	float getDisplayValue() override {return getValue();}
	std::string getDisplayValueString() override {
		float valGain = getDisplayValue();
		valGain =  std::round(valGain * 100.0f);
		return string::f("%g", math::normalizeZero(valGain / 100.0f));
	}
	void setDisplayValue(float displayValue) override {setValue(displayValue);}
	std::string getLabel() override {return "Gain adjust";}
	std::string getUnit() override {return " dB";}
};

struct GainAdjustSlider : ui::Slider {
	GainAdjustSlider(MixerTrack *srcTrack) {
		quantity = new GainAdjustQuantity(srcTrack);
	}
	~GainAdjustSlider() {
		delete quantity;
	}
};

// Fade-rate menu item

struct FadeRateQuantity : Quantity {
	MixerTrack *srcTrack = NULL;
	  
	FadeRateQuantity(MixerTrack *_srcTrack) {
		srcTrack = _srcTrack;
	}
	void setValue(float value) override {
		srcTrack->fadeRate = math::clamp(value, getMinValue(), getMaxValue());
	}
	float getValue() override {
		return srcTrack->fadeRate;
	}
	float getMinValue() override {return 0.0f;}
	float getMaxValue() override {return 10.0f;}
	float getDefaultValue() override {return 0.0f;}
	float getDisplayValue() override {return getValue();}
	std::string getDisplayValueString() override {
		float valCut = getDisplayValue();
		if (valCut >= MixerTrack::minFadeRate) {
			return string::f("%.1f", valCut);
		}
		else {
			return "OFF";
		}
	}
	void setDisplayValue(float displayValue) override {setValue(displayValue);}
	std::string getLabel() override {return "Fade";}
	std::string getUnit() override {
		if (getDisplayValue() >= MixerTrack::minFadeRate) {
			return " s";
		}
		else {
			return "";
		}
	}};

struct FadeRateSlider : ui::Slider {
	FadeRateSlider(MixerTrack *srcTrack) {
		quantity = new FadeRateQuantity(srcTrack);
	}
	~FadeRateSlider() {
		delete quantity;
	}
};


// HPF filter cutoff menu item

struct HPFCutoffQuantity : Quantity {
	MixerTrack *srcTrack = NULL;
	
	HPFCutoffQuantity(MixerTrack *_srcTrack) {
		srcTrack = _srcTrack;
	}
	void setValue(float value) override {
		srcTrack->setHPFCutoffFreq(math::clamp(value, getMinValue(), getMaxValue()));
	}
	float getValue() override {
		return srcTrack->getHPFCutoffFreq();
	}
	float getMinValue() override {return 13.0f;}
	float getMaxValue() override {return 350.0f;}
	float getDefaultValue() override {return 13.0f;}
	float getDisplayValue() override {return getValue();}
	std::string getDisplayValueString() override {
		float valCut = getDisplayValue();
		if (valCut >= MixerTrack::minHPFCutoffFreq) {
			return string::f("%i", (int)(math::normalizeZero(valCut) + 0.5f));
		}
		else {
			return "OFF";
		}
	}
	void setDisplayValue(float displayValue) override {setValue(displayValue);}
	std::string getLabel() override {return "HPF Cutoff";}
	std::string getUnit() override {
		if (getDisplayValue() >= MixerTrack::minHPFCutoffFreq) {
			return " Hz";
		}
		else {
			return "";
		}
	}
};

struct HPFCutoffSlider : ui::Slider {
	HPFCutoffSlider(MixerTrack *srcTrack) {
		quantity = new HPFCutoffQuantity(srcTrack);
	}
	~HPFCutoffSlider() {
		delete quantity;
	}
};



// LPF filter cutoff menu item

struct LPFCutoffQuantity : Quantity {
	MixerTrack *srcTrack = NULL;
	
	LPFCutoffQuantity(MixerTrack *_srcTrack) {
		srcTrack = _srcTrack;
	}
	void setValue(float value) override {
		srcTrack->setLPFCutoffFreq(math::clamp(value, getMinValue(), getMaxValue()));
	}
	float getValue() override {
		return srcTrack->getLPFCutoffFreq();
	}
	float getMinValue() override {return 3000.0f;}
	float getMaxValue() override {return 21000.0f;}
	float getDefaultValue() override {return 20010.0f;}
	float getDisplayValue() override {return getValue();}
	std::string getDisplayValueString() override {
		float valCut = getDisplayValue();
		if (valCut <= MixerTrack::maxLPFCutoffFreq) {
			valCut =  std::round(valCut / 100.0f);
			return string::f("%g", math::normalizeZero(valCut / 10.0f));
		}
		else {
			return "OFF";
		}
	}
	void setDisplayValue(float displayValue) override {setValue(displayValue);}
	std::string getLabel() override {return "LPF Cutoff";}
	std::string getUnit() override {
		if (getDisplayValue() <= MixerTrack::maxLPFCutoffFreq) {
			return " kHz";
		}
		else {
			return "";
		}
	}
};

struct LPFCutoffSlider : ui::Slider {
	LPFCutoffSlider(MixerTrack *srcTrack) {
		quantity = new LPFCutoffQuantity(srcTrack);
	}
	~LPFCutoffSlider() {
		delete quantity;
	}
};



// Master context menu
// --------------------

// dcBlocker
struct DcBlockItem : MenuItem {
	MixerMaster *srcMaster;
	void onAction(const event::Action &e) override {
		srcMaster->dcBlock = !srcMaster->dcBlock;
	}
};
		
		
// voltageLimiter
struct VoltLimitItem : MenuItem {
	MixerMaster *srcMaster;

	struct VoltLimitSubItem : MenuItem {
		MixerMaster *srcMaster;
		float setVal = 10.0f;
		void onAction(const event::Action &e) override {
			srcMaster->voltageLimiter = setVal;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		VoltLimitSubItem *lim0Item = createMenuItem<VoltLimitSubItem>("±10 V (default)", CHECKMARK(srcMaster->voltageLimiter < 11.0f));
		lim0Item->srcMaster = srcMaster;
		menu->addChild(lim0Item);

		VoltLimitSubItem *lim1Item = createMenuItem<VoltLimitSubItem>("±20 V", CHECKMARK(srcMaster->voltageLimiter >= 11.0f));
		lim1Item->srcMaster = srcMaster;
		lim1Item->setVal = 20.0f;
		menu->addChild(lim1Item);

		return menu;
	}
};


#endif
