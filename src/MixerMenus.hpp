//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc BoulĂ© 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************

#pragma once

#include "MixerCommon.hpp"
#include "VuMeters.hpp"


// Module's context menu
// --------------------


struct VuColorItem : MenuItem {
	int8_t *srcColor;
	bool isGlobal = false;// true when this is in the context menu of module, false when it is in a track/group/master context menu

	struct VuColorSubItem : MenuItem {
		int8_t *srcColor;
		int setVal;
		void onAction(const event::Action &e) override {
			*srcColor = setVal;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		
		for (int i = 0; i < (numVuThemes + (isGlobal ? 1 : 0)); i++) {
			VuColorSubItem *vuColItem = createMenuItem<VuColorSubItem>(vuColorNames[i], CHECKMARK(*srcColor == i));
			vuColItem->srcColor = srcColor;
			vuColItem->setVal = i;
			menu->addChild(vuColItem);
		}

		return menu;
	}
};


struct DispColorItem : MenuItem {
	int8_t *srcColor;
	bool isGlobal = false;// true when this is in the context menu of module, false when it is in a track/group/master context menu

	struct DispColorSubItem : MenuItem {
		int8_t *srcColor;
		int setVal;
		void onAction(const event::Action &e) override {
			*srcColor = setVal;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		
		for (int i = 0; i < (numDispThemes + (isGlobal ? 1 : 0)); i++) {		
			DispColorSubItem *dispColItem = createMenuItem<DispColorSubItem>(dispColorNames[i], CHECKMARK(*srcColor == i));
			dispColItem->srcColor = srcColor;
			dispColItem->setVal = i;
			menu->addChild(dispColItem);
		}
		
		return menu;
	}
};


struct PanLawMonoItem : MenuItem {
	int *panLawMonoSrc;
	
	struct PanLawMonoSubItem : MenuItem {
		int *panLawMonoSrc;
		int setVal;
		void onAction(const event::Action &e) override {
			*panLawMonoSrc = setVal;
		}
	};
	
	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		std::string panLawMonoNames[4] = {
			"+0 dB (no compensation)", 
			"+3 dB boost (equal power, default)", 
			"+4.5 dB boost (compromise)", 
			"+6 dB boost (linear)"
		};
			
		for (int i = 0; i < 4; i++) {
			PanLawMonoSubItem *lawMonoItem = createMenuItem<PanLawMonoSubItem>(panLawMonoNames[i], CHECKMARK(*panLawMonoSrc == i));
			lawMonoItem->panLawMonoSrc = panLawMonoSrc;
			lawMonoItem->setVal = i;
			menu->addChild(lawMonoItem);
		}
		
		return menu;
	}
};


struct PanLawStereoItem : MenuItem {
	int8_t *panLawStereoSrc;
	bool isGlobal;// true when this is in the context menu of module, false when it is in a track/group context menu

	struct PanLawStereoSubItem : MenuItem {
		int8_t *panLawStereoSrc;
		int8_t setVal;
		void onAction(const event::Action &e) override {
			*panLawStereoSrc = setVal;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		std::string panLawStereoNames[4] = {
			"Stereo balance linear",
			"Stereo balance equal power (default)",
			"True panning",
			"Set per track"
		};
		
		for (int i = 0; i < (isGlobal ? 4 : 3); i++) {
			PanLawStereoSubItem *lawStereoItem = createMenuItem<PanLawStereoSubItem>(panLawStereoNames[i], CHECKMARK(*panLawStereoSrc == i));
			lawStereoItem->panLawStereoSrc = panLawStereoSrc;
			lawStereoItem->setVal = i;
			menu->addChild(lawStereoItem);
		}

		return menu;
	}
};


// used for direct outs mode and aux send mode (tap choice 1-4)
struct TapModeItem : MenuItem {
	int8_t* tapModePtr;
	bool isGlobal;
	bool isGlobalDirectOuts = false;
	int8_t* directOutsSkipGroupedTracksPtr;// invalid value when isGlobalDirectOuts is false

	struct TapModeSubItem : MenuItem {
		int8_t* tapModePtr;
		int8_t setVal;
		void onAction(const event::Action &e) override {
			*tapModePtr = setVal;
		}
	};

	struct SkipGroupedSubItem : MenuItem {
		int8_t* directOutsSkipGroupedTracksPtr;
		void onAction(const event::Action &e) override {
			*directOutsSkipGroupedTracksPtr ^= 0x1;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		std::string tapModeNames[6] = {
			"Pre-insert",
			"Pre-fader",
			"Post-fader",
			"Post-mute/solo (default)",
			"Set per track",
			"Don't send tracks when grouped"
		};
		
		for (int i = 0; i < (isGlobal ? 5 : 4); i++) {
			TapModeSubItem *tapModeItem = createMenuItem<TapModeSubItem>(tapModeNames[i], CHECKMARK(*tapModePtr == i));
			tapModeItem->tapModePtr = tapModePtr;
			tapModeItem->setVal = i;
			menu->addChild(tapModeItem);
		}

		menu->addChild(new MenuSeparator());
		
		if (isGlobalDirectOuts) {
			SkipGroupedSubItem *skipGrpItem = createMenuItem<SkipGroupedSubItem>(tapModeNames[5], CHECKMARK(*directOutsSkipGroupedTracksPtr != 0));
			skipGrpItem->directOutsSkipGroupedTracksPtr = directOutsSkipGroupedTracksPtr;
			menu->addChild(skipGrpItem);
		}

		return menu;
	}
};

struct TapModePlusItem : TapModeItem {
	int* groupsControlTrackSendLevelsSrc;

	struct GroupsControlTrackSendLevelsItem : MenuItem {
		int* groupsControlTrackSendLevelsSrc;
		void onAction(const event::Action &e) override {
			*groupsControlTrackSendLevelsSrc ^= 0x1;
		}
	};


	Menu *createChildMenu() override {
		Menu *menu = TapModeItem::createChildMenu();

		menu->addChild(new MenuSeparator());
		
		GroupsControlTrackSendLevelsItem *levelTwhenGItem = createMenuItem<GroupsControlTrackSendLevelsItem>("Groups control track send levels", CHECKMARK(*groupsControlTrackSendLevelsSrc != 0));
		levelTwhenGItem->groupsControlTrackSendLevelsSrc = groupsControlTrackSendLevelsSrc;
		menu->addChild(levelTwhenGItem);

		return menu;
	}
};


struct FilterPosItem : MenuItem {
	int8_t *filterPosSrc;
	bool isGlobal;// true when this is in the context menu of module, false when it is in a track/group context menu

	struct FilterPosSubItem : MenuItem {
		int8_t *filterPosSrc;
		int8_t setVal;
		void onAction(const event::Action &e) override {
			*filterPosSrc = setVal;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		std::string filterPosNames[3] = {
			"Pre-insert",
			"Post-insert (default)",
			"Set per track"
		};
		
		for (int i = 0; i < (isGlobal ? 3 : 2); i++) {
			FilterPosSubItem *fpItem = createMenuItem<FilterPosSubItem>(filterPosNames[i], CHECKMARK(*filterPosSrc == i));
			fpItem->filterPosSrc = filterPosSrc;
			fpItem->setVal = i;
			menu->addChild(fpItem);
		}
		
		return menu;
	}
};

struct AuxReturnItem : MenuItem {
	int* auxReturnsMutedWhenMainSoloPtr;
	int* auxReturnsSolosMuteDryPtr;

	struct AuxReturnModeSubItem : MenuItem {
		int* modePtr;
		void onAction(const event::Action &e) override {
			*modePtr ^= 0x1;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		AuxReturnModeSubItem *ret0Item = createMenuItem<AuxReturnModeSubItem>("Mute aux returns when soloing tracks", CHECKMARK(*auxReturnsMutedWhenMainSoloPtr != 0));
		ret0Item->modePtr = auxReturnsMutedWhenMainSoloPtr;
		menu->addChild(ret0Item);

		AuxReturnModeSubItem *ret1Item = createMenuItem<AuxReturnModeSubItem>("Mute tracks when soloing aux returns", CHECKMARK(*auxReturnsSolosMuteDryPtr != 0));
		ret1Item->modePtr = auxReturnsSolosMuteDryPtr;
		menu->addChild(ret1Item);

		return menu;
	}
};


struct ChainItem : MenuItem {
	int *chainModeSrc;

	struct ChainSubItem : MenuItem {
		int *chainModeSrc;
		int setVal = 0;
		void onAction(const event::Action &e) override {
			*chainModeSrc = setVal;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		ChainSubItem *ch0Item = createMenuItem<ChainSubItem>("Pre-master", CHECKMARK(*chainModeSrc == 0));
		ch0Item->chainModeSrc = chainModeSrc;
		menu->addChild(ch0Item);

		ChainSubItem *ch1Item = createMenuItem<ChainSubItem>("Post-master (default)", CHECKMARK(*chainModeSrc == 1));
		ch1Item->chainModeSrc = chainModeSrc;
		ch1Item->setVal = 1;
		menu->addChild(ch1Item);

		return menu;
	}
};


struct AuxRetFbProtItem : MenuItem {
	int8_t *groupedAuxReturnFeedbackProtectionSrc;

	struct AuxRetFbProtSubItem : MenuItem {
		int8_t *groupedAuxReturnFeedbackProtectionSrc;
		void onAction(const event::Action &e) override {
			*groupedAuxReturnFeedbackProtectionSrc ^= 0x1;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		AuxRetFbProtSubItem *fb1Item = createMenuItem<AuxRetFbProtSubItem>("Feedback protection ON (default)", CHECKMARK(*groupedAuxReturnFeedbackProtectionSrc == 1));
		fb1Item->groupedAuxReturnFeedbackProtectionSrc = groupedAuxReturnFeedbackProtectionSrc;
		menu->addChild(fb1Item);

		AuxRetFbProtSubItem *fb0Item = createMenuItem<AuxRetFbProtSubItem>("Feedback protection OFF (Warning RTFM!)", CHECKMARK(*groupedAuxReturnFeedbackProtectionSrc == 0));
		fb0Item->groupedAuxReturnFeedbackProtectionSrc = groupedAuxReturnFeedbackProtectionSrc;
		menu->addChild(fb0Item);

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

struct CloakedModeItem : MenuItem {
	PackedBytes4 *colorAndCloakSrc;
	void onAction(const event::Action &e) override {
		colorAndCloakSrc->cc4[cloakedMode] ^= 0xFF;
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

struct CvPointerShowItem : MenuItem {
	int8_t *srcDetailsShow;

	struct CvPointerShowSubItem : MenuItem {
		int8_t *srcDetailsShow;
		void onAction(const event::Action &e) override {
			*srcDetailsShow ^= 0x4;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		CvPointerShowSubItem *cvShowItem0 = createMenuItem<CvPointerShowSubItem>("On", CHECKMARK((*srcDetailsShow & 0x4) != 0));
		cvShowItem0->srcDetailsShow = srcDetailsShow;
		menu->addChild(cvShowItem0);
		
		CvPointerShowSubItem *cvShowItem1 = createMenuItem<CvPointerShowSubItem>("Off", CHECKMARK((*srcDetailsShow & 0x4) == 0));
		cvShowItem1->srcDetailsShow = srcDetailsShow;
		menu->addChild(cvShowItem1);
		
		return menu;
	}
};


struct FadeSettingsItem : MenuItem {
	bool *symmetricalFadeSrc;
	bool *fadeCvOutsWithVolCvSrc;

	struct SymmetricalFadeItem : MenuItem {
		bool *symmetricalFadeSrc;
		void onAction(const event::Action &e) override {
			*symmetricalFadeSrc = !*symmetricalFadeSrc;
		}
	};

	struct FadeCvOutItem : MenuItem {
		bool *fadeCvOutsWithVolCvSrc;
		void onAction(const event::Action &e) override {
			*fadeCvOutsWithVolCvSrc = !*fadeCvOutsWithVolCvSrc;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		SymmetricalFadeItem *symItem = createMenuItem<SymmetricalFadeItem>("Symmetrical", CHECKMARK(*symmetricalFadeSrc));
		symItem->symmetricalFadeSrc = symmetricalFadeSrc;
		menu->addChild(symItem);
		
		FadeCvOutItem *fadeCvItem = createMenuItem<FadeCvOutItem>("Include vol CV in fade CV out", CHECKMARK(*fadeCvOutsWithVolCvSrc));
		fadeCvItem->fadeCvOutsWithVolCvSrc = fadeCvOutsWithVolCvSrc;
		menu->addChild(fadeCvItem);
		
		return menu;
	}
};

struct EcoItem : MenuItem {
	uint16_t *ecoModeSrc;
	void onAction(const event::Action &e) override {
		*ecoModeSrc = ~*ecoModeSrc;
	}
};

struct LinCvItem : MenuItem {
	int8_t *linearVolCvInputsSrc;

	struct LinCvSubItem : MenuItem {
		int8_t *linearVolCvInputsSrc;
		void onAction(const event::Action &e) override {
			*linearVolCvInputsSrc ^= 0x1;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		LinCvSubItem *lin0Item = createMenuItem<LinCvSubItem>("Fader control", CHECKMARK(*linearVolCvInputsSrc == 0));
		lin0Item->linearVolCvInputsSrc = linearVolCvInputsSrc;
		menu->addChild(lin0Item);

		LinCvSubItem *lin1Item = createMenuItem<LinCvSubItem>("Linear VCA", CHECKMARK(*linearVolCvInputsSrc == 1));
		lin1Item->linearVolCvInputsSrc = linearVolCvInputsSrc;
		menu->addChild(lin1Item);

		return menu;
	}
};


// Track context menu
// --------------------


struct InvertInputItem : MenuItem {
	int8_t *invertInputSrc;
	void onAction(const event::Action &e) override {
		*invertInputSrc ^= 0x1;
	}
};

// Gain adjust menu item

struct GainAdjustQuantity : Quantity {
	float *gainAdjustSrc;
	  
	GainAdjustQuantity(float *_gainAdjustSrc) {
		gainAdjustSrc = _gainAdjustSrc;
	}
	void setValue(float value) override {
		float gainInDB = math::clamp(value, getMinValue(), getMaxValue());
		*gainAdjustSrc = std::pow(10.0f, gainInDB / 20.0f);
	}
	float getValue() override {
		return 20.0f * std::log10(*gainAdjustSrc);
	}
	float getMinValue() override {return -20.0f;}
	float getMaxValue() override {return 20.0f;}
	float getDefaultValue() override {return 0.0f;}
	float getDisplayValue() override {return getValue();}
	std::string getDisplayValueString() override {
		float valGain = getDisplayValue();
		valGain =  std::round(valGain * 100.0f);
		return string::f("%.1f", math::normalizeZero(valGain / 100.0f));
	}
	void setDisplayValue(float displayValue) override {setValue(displayValue);}
	std::string getLabel() override {return "Gain adjust";}
	std::string getUnit() override {return " dB";}
};

struct GainAdjustSlider : ui::Slider {
	GainAdjustSlider(float *gainAdjustSrc) {
		quantity = new GainAdjustQuantity(gainAdjustSrc);
	}
	~GainAdjustSlider() {
		delete quantity;
	}
};



// HPF filter cutoff menu item

template <typename TrackOrAux>
struct HPFCutoffQuantity : Quantity {
	TrackOrAux *srcTrackOrAux = NULL;
	
	HPFCutoffQuantity(TrackOrAux *_srcTrackOrAux) {
		srcTrackOrAux = _srcTrackOrAux;
	}
	void setValue(float value) override {
		srcTrackOrAux->setHPFCutoffFreq(math::clamp(value, getMinValue(), getMaxValue()));
	}
	float getValue() override {
		return srcTrackOrAux->getHPFCutoffFreq();
	}
	float getMinValue() override {return 13.0f;}
	float getMaxValue() override {return 1000.0f;}
	float getDefaultValue() override {return 13.0f;}
	float getDisplayValue() override {return getValue();}
	std::string getDisplayValueString() override {
		float valCut = getDisplayValue();
		if (valCut >= GlobalConst::minHPFCutoffFreq) {
			return string::f("%i", (int)(math::normalizeZero(valCut) + 0.5f));
		}
		else {
			return "OFF";
		}
	}
	void setDisplayValue(float displayValue) override {setValue(displayValue);}
	std::string getLabel() override {return "HPF Cutoff";}
	std::string getUnit() override {
		if (getDisplayValue() >= GlobalConst::minHPFCutoffFreq) {
			return " Hz";
		}
		else {
			return "";
		}
	}
};

template <typename TrackOrAux>
struct HPFCutoffSlider : ui::Slider {
	HPFCutoffSlider(TrackOrAux *srcTrackOrAux) {
		quantity = new HPFCutoffQuantity<TrackOrAux>(srcTrackOrAux);
	}
	~HPFCutoffSlider() {
		delete quantity;
	}
};
struct HPFCutoffSlider2 : ui::Slider {
	HPFCutoffSlider2(ParamQuantity *srcPQ) {
		quantity = srcPQ;
	}
};


// LPF filter cutoff menu item

template <typename TrackOrAux>
struct LPFCutoffQuantity : Quantity {
	TrackOrAux *srcTrackOrAux = NULL;
	
	LPFCutoffQuantity(TrackOrAux *_srcTrack) {
		srcTrackOrAux = _srcTrack;
	}
	void setValue(float value) override {
		srcTrackOrAux->setLPFCutoffFreq(math::clamp(value, getMinValue(), getMaxValue()));
	}
	float getValue() override {
		return srcTrackOrAux->getLPFCutoffFreq();
	}
	float getMinValue() override {return 1000.0f;}
	float getMaxValue() override {return 21000.0f;}
	float getDefaultValue() override {return 20010.0f;}
	float getDisplayValue() override {return getValue();}
	std::string getDisplayValueString() override {
		float valCut = getDisplayValue();
		if (valCut <= GlobalConst::maxLPFCutoffFreq) {
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
		if (getDisplayValue() <= GlobalConst::maxLPFCutoffFreq) {
			return " kHz";
		}
		else {
			return "";
		}
	}
};

template <typename TrackOrAux>
struct LPFCutoffSlider : ui::Slider {
	LPFCutoffSlider(TrackOrAux *srcTrackOrAux) {
		quantity = new LPFCutoffQuantity<TrackOrAux>(srcTrackOrAux);
	}
	~LPFCutoffSlider() {
		delete quantity;
	}
};
struct LPFCutoffSlider2 : ui::Slider {
	LPFCutoffSlider2(ParamQuantity *srcPQ) {
		quantity = srcPQ;
	}
};

// Stereo width item and
// Pan CV level item

struct PercentQuantity : Quantity {
	float *srcValue = NULL;
	std::string label;
	  
	PercentQuantity(float *_srcValue, std::string _label) {
		srcValue = _srcValue;
		label = _label;
	}
	void setValue(float value) override {
		*srcValue = math::clamp(value, getMinValue(), getMaxValue());
	}
	float getValue() override {
		return *srcValue;
	}
	float getMinValue() override {return 0.0f;}
	float getMaxValue() override {return 2.0f;}
	float getDefaultValue() override {return 1.0f;}
	float getDisplayValue() override {return getValue();}
	std::string getDisplayValueString() override {
		return string::f("%i", (int)std::round(getDisplayValue() * 100.0f));
	}
	void setDisplayValue(float displayValue) override {setValue(displayValue);}
	std::string getLabel() override {return label;}
	std::string getUnit() override {return " %";}
};

struct StereoWidthLevelSlider : ui::Slider {
	StereoWidthLevelSlider(float *_srcStereoWidth) {
		quantity = new PercentQuantity(_srcStereoWidth, "Stereo width");
	}
	~StereoWidthLevelSlider() {
		delete quantity;
	}
};

struct PanCvLevelSlider : ui::Slider {
	PanCvLevelSlider(float *_srcPanCvLevel) {
		quantity = new PercentQuantity(_srcPanCvLevel, "Pan CV input level");
	}
	~PanCvLevelSlider() {
		delete quantity;
	}
};


// Fade-rate menu item

struct FadeRateQuantity : Quantity {
	float *srcFadeRate = NULL;
	  
	FadeRateQuantity(float *_srcFadeRate) {
		srcFadeRate = _srcFadeRate;
	}
	void setValue(float value) override {
		*srcFadeRate = math::clamp(value, getMinValue(), getMaxValue());
	}
	float getValue() override {
		return *srcFadeRate;
	}
	float getMinValue() override {return 0.0f;}
	float getMaxValue() override {return 30.0f;}
	float getDefaultValue() override {return 0.0f;}
	float getDisplayValue() override {return getValue();}
	std::string getDisplayValueString() override {
		float valCut = getDisplayValue();
		if (valCut >= GlobalConst::minFadeRate) {
			return string::f("%.1f", valCut);
		}
		else {
			return "OFF";
		}
	}
	void setDisplayValue(float displayValue) override {setValue(displayValue);}
	std::string getLabel() override {return "Fade";}
	std::string getUnit() override {
		if (getDisplayValue() >= GlobalConst::minFadeRate) {
			return " s";
		}
		else {
			return "";
		}
	}};

struct FadeRateSlider : ui::Slider {
	FadeRateSlider(float *_srcFadeRate) {
		quantity = new FadeRateQuantity(_srcFadeRate);
	}
	~FadeRateSlider() {
		delete quantity;
	}
};



// Fade-profile menu item

struct FadeProfileQuantity : Quantity {
	float *srcFadeProfile = NULL;
	  
	FadeProfileQuantity(float *_srcFadeProfile) {
		srcFadeProfile = _srcFadeProfile;
	}
	void setValue(float value) override {
		*srcFadeProfile = math::clamp(value, getMinValue(), getMaxValue());
	}
	float getValue() override {
		return *srcFadeProfile;
	}
	float getMinValue() override {return -1.0f;}
	float getMaxValue() override {return 1.0f;}
	float getDefaultValue() override {return 0.0f;}
	float getDisplayValue() override {return getValue();}
	std::string getDisplayValueString() override {
		float valProf = getDisplayValue();
		if (valProf >= 0.005f) {
			return string::f("Exp %i", (int)std::round(valProf * 100.0f));
		}
		else if (valProf <= -0.005f) {
			return string::f("Log %i", (int)std::round(valProf * -100.0f));
		}
		else {
			return "Linear";
		}
	}
	void setDisplayValue(float displayValue) override {setValue(displayValue);}
	std::string getLabel() override {return "Fade";}
	std::string getUnit() override {
		float valProf = getDisplayValue();
		if (valProf >= 0.005f || valProf <= -0.005f) {
			return "%";
		}
		else {
			return "";
		}
	}};

struct FadeProfileSlider : ui::Slider {
	FadeProfileSlider(float *_srcProfileRate) {
		quantity = new FadeProfileQuantity(_srcProfileRate);
	}
	~FadeProfileSlider() {
		delete quantity;
	}
};



// linked fader menu item
struct LinkFaderItem : MenuItem {
	unsigned long *linkBitMaskSrc;
	int trackOrGroupNum;// must be properly indexed for groups
	
	void onAction(const event::Action &e) override {
		toggleLinked(linkBitMaskSrc, trackOrGroupNum);
	}
};

// init track settings
template <typename TMixerTrack>
struct InitializeTrackItem : MenuItem {
	TMixerTrack *srcTrack;
	int *updateTrackLabelRequestPtr;
	void onAction(const event::Action &e) override {
		srcTrack->paGroup->setValue(0.0f);
		srcTrack->paFade->setValue(1.0f);
		srcTrack->paMute->setValue(0.0f);
		srcTrack->paSolo->setValue(0.0f);
		srcTrack->paPan->setValue(0.5f);
		srcTrack->gInfo->clearLinked(srcTrack->trackNum);
		srcTrack->paHpfCutoff->setValue(GlobalConst::defHPFCutoffFreq);
		srcTrack->paLpfCutoff->setValue(GlobalConst::defLPFCutoffFreq);
		srcTrack->onReset();
		*updateTrackLabelRequestPtr = 1;
	}
};

// init group settings
template <typename TMixerGroup>
struct InitializeGroupItem : MenuItem {
	TMixerGroup *srcGroup;
	int groupNumForLink;
	int *updateTrackLabelRequestPtr;
	void onAction(const event::Action &e) override {
		srcGroup->paFade->setValue(1.0f);
		srcGroup->paMute->setValue(0.0f);
		srcGroup->paSolo->setValue(0.0f);
		srcGroup->paPan->setValue(0.5f);
		srcGroup->gInfo->clearLinked(groupNumForLink);
		srcGroup->paHpfCutoff->setValue(GlobalConst::defHPFCutoffFreq);
		srcGroup->paLpfCutoff->setValue(GlobalConst::defLPFCutoffFreq);
		srcGroup->onReset();
		*updateTrackLabelRequestPtr = 1;
	}
};

// init group settings
template <typename TAuxspanderAux>
struct InitializeAuxItem : MenuItem {
	TAuxspanderAux *srcAux;
	int numTracks;
	int numGroups;
	int *updateAuxLabelRequestPtr;
	void onAction(const event::Action &e) override {
		for (int i = 0; i < numTracks; i++) {
			srcAux->trackAuxSendParam[4 * i].setValue(0.0f);
		}
		for (int i = 0; i < numGroups; i++) {
			srcAux->groupAuxSendParam[4 * i].setValue(0.0f);
		}
		srcAux->globalAuxParam[0].setValue(0.0f);// mute
		srcAux->globalAuxParam[4].setValue(0.0f);// solo
		srcAux->globalAuxParam[8].setValue(0.0f);// group
		srcAux->globalAuxParam[12].setValue(1.0f);// global send
		srcAux->globalAuxParam[16].setValue(0.5f);// pan
		srcAux->globalAuxParam[20].setValue(1.0f);// return fader
		srcAux->onReset();
		*updateAuxLabelRequestPtr = 1;
	}
};


// copy track menu settings to
template <typename TMixerTrack>
struct CopyTrackSettingsItem : MenuItem {
	TMixerTrack *tracks = NULL;
	int trackNumSrc;
	int numTracks;

	struct CopyTrackSettingsSubItem : MenuItem {
		TMixerTrack *tracks = NULL;
		int trackNumSrc;	
		int trackNumDest;

		void onAction(const event::Action &e) override {
			TrackSettingsCpBuffer buffer;
			tracks[trackNumSrc].write(&buffer);
			tracks[trackNumDest].read(&buffer);
		}
	};
	
	
	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		for (int trk = 0; trk < numTracks; trk++) {
			bool onSource = (trk == trackNumSrc);
			CopyTrackSettingsSubItem *reo0Item = createMenuItem<CopyTrackSettingsSubItem>(std::string(tracks[trk].trackName, 4), CHECKMARK(onSource));
			reo0Item->tracks = tracks;
			reo0Item->trackNumSrc = trackNumSrc;
			reo0Item->trackNumDest = trk;
			reo0Item->disabled = onSource;
			menu->addChild(reo0Item);
		}

		return menu;
	}		
};

// move track to
template <typename TMixerTrack>
struct TrackReorderItem : MenuItem {
	TMixerTrack *tracks = NULL;
	int trackNumSrc;	
	int numTracks;
	int *updateTrackLabelRequestPtr;
	int32_t *trackMoveInAuxRequestPtr;
	PortWidget **inputWidgets;

	struct TrackReorderSubItem : MenuItem {
		TMixerTrack *tracks = NULL;
		int trackNumSrc;	
		int trackNumDest;
		int numTracks;
		int *updateTrackLabelRequestPtr;
		int32_t *trackMoveInAuxRequestPtr;
		PortWidget **inputWidgets;
		
		CableWidget* cwClr[4];
		
		void transferTrackInputs(int srcTrk, int destTrk) {
			// use same strategy as in PortWidget::onDragStart/onDragEnd to make sure it's safely implemented (simulate manual dragging of the cables)
			for (int i = 0; i < 4; i++) {// scan Left, Right, Volume, Pan
				CableWidget* cwRip = APP->scene->rack->getTopCable(inputWidgets[srcTrk + i * numTracks]);// only top needed since inputs have at most one cable
				if (cwRip != NULL) {
					APP->scene->rack->removeCable(cwRip);
					cwRip->setInput(inputWidgets[destTrk + i * numTracks]);
					APP->scene->rack->addCable(cwRip);
				}
			}
		}
		void clearTrackInputs(int trk) {
			for (int i = 0; i < 4; i++) {// scan Left, Right, Volume, Pan
				cwClr[i] = APP->scene->rack->getTopCable(inputWidgets[trk + i * numTracks]);// only top needed since inputs have at most one cable
				if (cwClr[i] != NULL) {
					APP->scene->rack->removeCable(cwClr[i]);
				}
			}
		}
		void reconnectTrackInputs(int trk) {
			for (int i = 0; i < 4; i++) {// scan Left, Right, Volume, Pan
				if (cwClr[i] != NULL) {
					cwClr[i]->setInput(inputWidgets[trk + i * numTracks]);
					APP->scene->rack->addCable(cwClr[i]);
				}
			}
		}

		void onAction(const event::Action &e) override {
			TrackSettingsCpBuffer buffer1;
			TrackSettingsCpBuffer buffer2;
			
			tracks[trackNumSrc].write2(&buffer2);
			clearTrackInputs(trackNumSrc);// dangle the wires connected to the source track while ripple re-connect
			if (trackNumDest < trackNumSrc) {
				for (int trk = trackNumSrc - 1; trk >= trackNumDest; trk--) {
					tracks[trk].write2(&buffer1);
					tracks[trk + 1].read2(&buffer1);
					transferTrackInputs(trk, trk + 1);
				}
			}
			else {// must automatically be bigger (equal is impossible)
				for (int trk = trackNumSrc; trk < trackNumDest; trk++) {
					tracks[trk + 1].write2(&buffer1);
					tracks[trk].read2(&buffer1);
					transferTrackInputs(trk + 1, trk);
				}
			}
			tracks[trackNumDest].read2(&buffer2);
			reconnectTrackInputs(trackNumDest);
			
			*updateTrackLabelRequestPtr = 1;
			*trackMoveInAuxRequestPtr = (trackNumSrc | (trackNumDest << 8));
		}
	};
	
	
	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		for (int trk = 0; trk < numTracks; trk++) {
			bool onSource = (trk == trackNumSrc);
			TrackReorderSubItem *reo0Item = createMenuItem<TrackReorderSubItem>(std::string(tracks[trk].trackName, 4), CHECKMARK(onSource));
			reo0Item->tracks = tracks;
			reo0Item->trackNumSrc = trackNumSrc;
			reo0Item->trackNumDest = trk;
			reo0Item->numTracks = numTracks;
			reo0Item->updateTrackLabelRequestPtr = updateTrackLabelRequestPtr;
			reo0Item->trackMoveInAuxRequestPtr = trackMoveInAuxRequestPtr;
			reo0Item->inputWidgets = inputWidgets;
			reo0Item->disabled = onSource;
			menu->addChild(reo0Item);
		}

		return menu;
	}		
};


// Master right-click menu
// --------------------

// dcBlocker
struct DcBlockItem : MenuItem {
	bool *dcBlockSrc;
	void onAction(const event::Action &e) override {
		*dcBlockSrc = !*dcBlockSrc;
	}
};
		
		
// clipper
struct ClippingItem : MenuItem {
	int *clippingSrc;

	struct ClippingSubItem : MenuItem {
		int *clippingSrc;
		int setVal = 0;
		void onAction(const event::Action &e) override {
			*clippingSrc = setVal;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		ClippingSubItem *lim0Item = createMenuItem<ClippingSubItem>("Soft (default)", CHECKMARK(*clippingSrc == 0));
		lim0Item->clippingSrc = clippingSrc;
		menu->addChild(lim0Item);

		ClippingSubItem *lim1Item = createMenuItem<ClippingSubItem>("Hard", CHECKMARK(*clippingSrc == 1));
		lim1Item->clippingSrc = clippingSrc;
		lim1Item->setVal = 1;
		menu->addChild(lim1Item);

		return menu;
	}
};


// masterFaderScalesSends
struct MasterFaderScalesSendsItem : MenuItem {
	int8_t *masterFaderScalesSendsSrc;
	void onAction(const event::Action &e) override {
		*masterFaderScalesSendsSrc ^= 0x1;
	}
};



// dim gain menu item

struct DimGainQuantity : Quantity {
	float *dimGainSrc;
	float *dimGainIntegerDBSrc;
	  
	DimGainQuantity(float* _dimGainSrc, float* _dimGainIntegerDBSrc) {
		dimGainSrc = _dimGainSrc;
		dimGainIntegerDBSrc = _dimGainIntegerDBSrc;
	}
	void setValue(float value) override {
		float gainInDB = math::clamp(value, getMinValue(), getMaxValue());
		float gainLin = std::pow(10.0f, gainInDB / 20.0f);
		*dimGainSrc = gainLin;
		*dimGainIntegerDBSrc = calcDimGainIntegerDB(gainLin);
	}
	float getValue() override {
		return 20.0f * std::log10(*dimGainSrc);
	}
	float getMinValue() override {return -30.0f;}
	float getMaxValue() override {return -1.0f;}
	float getDefaultValue() override {return -12.0f;}
	float getDisplayValue() override {return getValue();}
	std::string getDisplayValueString() override {
		float valGain = getDisplayValue();
		valGain =  std::round(valGain);
		return string::f("%g", math::normalizeZero(valGain));
	}
	void setDisplayValue(float displayValue) override {setValue(displayValue);}
	std::string getLabel() override {return "Dim gain";}
	std::string getUnit() override {return " dB";}
};

struct DimGainSlider : ui::Slider {
	DimGainSlider(float* dimGainSrc, float* dimGainIntegerDBSrc) {
		quantity = new DimGainQuantity(dimGainSrc, dimGainIntegerDBSrc);
	}
	~DimGainSlider() {
		delete quantity;
	}
};
