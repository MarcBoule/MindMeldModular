//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc BoulĂ© 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************

#pragma once

#include "MixerCommon.hpp"
#include "../comp/VuMeters.hpp"


// Module's context menu
// --------------------



struct VuColorItem : MenuItem {
	int8_t *srcColor;
	bool isGlobal = false;// true when this is in the context menu of module, false when it is in a track/group/master context menu
	GlobalToLocalOp* localOp;// must always set this up when isGlobal==true

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		for (int i = 0; i < (numVuThemes + (isGlobal ? 1 : 0)); i++) {
			menu->addChild(createCheckMenuItem(vuColorNames[i], "",
				[=]() {return *srcColor == i;},
				[=]() {if (i == numVuThemes) localOp->setOp(GTOL_VUCOL, *srcColor);
					   *srcColor = i;}
			));
		}
		return menu;
	}
};


struct DispColorItem : MenuItem {
	int8_t *srcColor;
	bool isGlobal = false;// true when this is in the context menu of module, false when it is in a track/group/master context menu
	GlobalToLocalOp* localOp;// must always set this up when isGlobal==true

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		for (int i = 0; i < (numDispThemes + (isGlobal ? 1 : 0)); i++) {		
			menu->addChild(createCheckMenuItem(dispColorNames[i], "",
				[=]() {return *srcColor == i;},
				[=]() {if (i == numDispThemes) localOp->setOp(GTOL_LABELCOL, *srcColor);
					   *srcColor = i;}
			));
		}
		return menu;
	}
};


struct PanLawMonoItem : MenuItem {
	int *panLawMonoSrc;
	const std::string panLawMonoNames[4] = {
		"+0 dB (no compensation)", 
		"+3 dB boost (equal power, default)", 
		"+4.5 dB boost (compromise)", 
		"+6 dB boost (linear)"
	};
	
	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		for (int i = 0; i < 4; i++) {
			menu->addChild(createCheckMenuItem(panLawMonoNames[i], "",
				[=]() {return *panLawMonoSrc == i;},
				[=]() {*panLawMonoSrc = i;}
			));
		}
		return menu;
	}
};


struct PanLawStereoItem : MenuItem {
	int8_t *panLawStereoSrc;
	bool isGlobal;// true when this is in the context menu of module, false when it is in a track/group context menu
	GlobalToLocalOp* localOp;// must always set this up when isGlobal==true
	const std::string panLawStereoNames[4] = {
		"Stereo balance linear",
		"Stereo balance equal power (default)",
		"True panning",
		"Set per track"
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;	
		for (int i = 0; i < (isGlobal ? 4 : 3); i++) {
			menu->addChild(createCheckMenuItem(panLawStereoNames[i], "",
				[=]() {return *panLawStereoSrc == i;},
				[=]() {if (i == 3) localOp->setOp(GTOL_STEREOPAN, *panLawStereoSrc);
					   *panLawStereoSrc = i;}
			));
		}
		return menu;
	}
};


struct DirectOutsModeItem : MenuItem {
	int8_t* tapModePtr;
	bool isGlobal;
	GlobalToLocalOp* localOp;// must always set this up when isGlobal==true
	int8_t* directOutsSkipGroupedTracksPtr;// invalid value when isGlobal is false
	const std::string directOutModeNames[6] = {
		"Pre-insert",
		"Pre-fader",
		"Post-fader",
		"Post-mute/solo (default)",
		"Set per track",
		"Don't send tracks when grouped"
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		
		for (int i = 0; i < (isGlobal ? 5 : 4); i++) {
			menu->addChild(createCheckMenuItem(directOutModeNames[i], "",
				[=]() {return *tapModePtr == i;},
				[=]() {if (i == 4) localOp->setOp(GTOL_DIRECTOUTS, *tapModePtr);
					   *tapModePtr = i;}
			));
		}
		if (isGlobal) {
			menu->addChild(new MenuSeparator());
			menu->addChild(createCheckMenuItem(directOutModeNames[5], "",
				[=]() {return *directOutsSkipGroupedTracksPtr != 0;},
				[=]() {*directOutsSkipGroupedTracksPtr ^= 0x1;}
			));
		}

		return menu;
	}
};

struct AuxSendsItem : MenuItem {
	int8_t* tapModePtr;
	bool isGlobal;
	GlobalToLocalOp* localOp;// must always set this up when isGlobal==true
	int* groupsControlTrackSendLevelsSrc;// invalid value when isGlobal is false
	const std::string auxModeNames[6] = {
		"Pre-insert",
		"Pre-fader",
		"Post-fader",
		"Post-mute/solo (default)",
		"Set per track",
		"Groups control track send levels"
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		
		for (int i = 0; i < (isGlobal ? 5 : 4); i++) {
			menu->addChild(createCheckMenuItem(auxModeNames[i], "",
				[=]() {return *tapModePtr == i;},
				[=]() {if (i == 4) localOp->setOp(GTOL_AUXSENDS, *tapModePtr);
					   *tapModePtr = i;}
			));
		}
		if (isGlobal) { 
			menu->addChild(new MenuSeparator());
			menu->addChild(createCheckMenuItem(auxModeNames[5], "",
				[=]() {return *groupsControlTrackSendLevelsSrc != 0;},
				[=]() {*groupsControlTrackSendLevelsSrc ^= 0x1;}
			));
		}
		return menu;
	}
};


struct FilterPosItem : MenuItem {
	int8_t *filterPosSrc;
	bool isGlobal;// true when this is in the context menu of module, false when it is in a track/group context menu
	GlobalToLocalOp* localOp;// must always set this up when isGlobal==true
	const std::string filterPosNames[3] = {
		"Pre-insert",
		"Post-insert (default)",
		"Set per track"
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		for (int i = 0; i < (isGlobal ? 3 : 2); i++) {
			menu->addChild(createCheckMenuItem(filterPosNames[i], "",
				[=]() {return *filterPosSrc == i;},
				[=]() {if (i == 2) localOp->setOp(GTOL_FILTERPOS, *filterPosSrc);
					   *filterPosSrc = i;}
			));
		}
		return menu;
	}
};


struct MomentaryCvModeItem : MenuItem {
	int8_t *momentaryCvButtonsSrc;
	bool isGlobal;// true when this is in the context menu of module, false when it is in a track/group context menu
	GlobalToLocalOp* localOp;// must always set this up when isGlobal==true
	const std::string momentaryCvNames[3] = {
		"Gate high/low",
		"Trigger toggle (default)",
		"Set per track"
	};
	int ordering[3] = {1, 0, 2};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		for (int k = 0; k < (isGlobal ? 3 : 2); k++) {
			int i = ordering[k];
			menu->addChild(createCheckMenuItem(momentaryCvNames[i], "",
				[=]() {return *momentaryCvButtonsSrc == i;},
				[=]() {if (i == 2) localOp->setOp(GTOL_MOMENTCV, *momentaryCvButtonsSrc);
					   *momentaryCvButtonsSrc = i;}
			));
		}
		return menu;
	}
};


struct AuxReturnItem : MenuItem {
	int* auxReturnsMutedWhenMainSoloPtr;
	int* auxReturnsSolosMuteDryPtr;

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		menu->addChild(createCheckMenuItem("Mute aux returns when soloing tracks", "",
			[=]() {return *auxReturnsMutedWhenMainSoloPtr != 0;},
			[=]() {*auxReturnsMutedWhenMainSoloPtr ^= 0x1;}
		));
		menu->addChild(createCheckMenuItem("Mute tracks when soloing aux returns", "",
			[=]() {return *auxReturnsSolosMuteDryPtr != 0;},
			[=]() {*auxReturnsSolosMuteDryPtr ^= 0x1;}
		));
		return menu;
	}
};


struct ChainItem : MenuItem {
	int *chainModeSrc;

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		menu->addChild(createCheckMenuItem("Pre-master (default)", "",
			[=]() {return *chainModeSrc == 0;},
			[=]() {*chainModeSrc = 0;}
		));
		menu->addChild(createCheckMenuItem("Post-master", "",
			[=]() {return *chainModeSrc == 1;},
			[=]() {*chainModeSrc = 1;}
		));
		return menu;
	}
};


struct AuxRetFbProtItem : MenuItem {
	int8_t *groupedAuxReturnFeedbackProtectionSrc;

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		menu->addChild(createCheckMenuItem("Feedback protection ON (default)", "",
			[=]() {return *groupedAuxReturnFeedbackProtectionSrc == 1;},
			[=]() {*groupedAuxReturnFeedbackProtectionSrc = 1;}
		));
		menu->addChild(createCheckMenuItem("Feedback protection OFF (Warning RTFM!)", "",
			[=]() {return *groupedAuxReturnFeedbackProtectionSrc == 0;},
			[=]() {*groupedAuxReturnFeedbackProtectionSrc = 0;}
		));
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

	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		menu->addChild(createCheckMenuItem("On", "",
			[=]() {return (*srcDetailsShow & 0x3) == 0x3;},
			[=]() {*srcDetailsShow &= ~0x3; *srcDetailsShow |= 0x3;}
		));
		menu->addChild(createCheckMenuItem("CV only", "",
			[=]() {return (*srcDetailsShow & 0x3) == 0x2;},
			[=]() {*srcDetailsShow &= ~0x3; *srcDetailsShow |= 0x2;}
		));
		menu->addChild(createCheckMenuItem("Off", "",
			[=]() {return (*srcDetailsShow & 0x3) == 0x0;},
			[=]() {*srcDetailsShow &= ~0x3; *srcDetailsShow |= 0x0;}
		));
		
		return menu;
	}
};

struct CvPointerShowItem : MenuItem {
	int8_t *srcDetailsShow;

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		menu->addChild(createCheckMenuItem("On", "",
			[=]() {return (*srcDetailsShow & 0x4) != 0;},
			[=]() {*srcDetailsShow |= 0x4;}
		));
		menu->addChild(createCheckMenuItem("Off", "",
			[=]() {return (*srcDetailsShow & 0x4) == 0;},
			[=]() {*srcDetailsShow &= ~0x4;}
		));
		return menu;
	}
};


struct FadeSettingsItem : MenuItem {
	bool *symmetricalFadeSrc;
	bool *fadeCvOutsWithVolCvSrc;

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		menu->addChild(createCheckMenuItem("Symmetrical", "",
			[=]() {return *symmetricalFadeSrc;},
			[=]() {*symmetricalFadeSrc = !*symmetricalFadeSrc;}
		));
		menu->addChild(createCheckMenuItem("Include vol CV in fade CV out", "",
			[=]() {return *fadeCvOutsWithVolCvSrc;},
			[=]() {*fadeCvOutsWithVolCvSrc = !*fadeCvOutsWithVolCvSrc;}
		));
		return menu;
	}
};


struct LinCvItem : MenuItem {
	int8_t *linearVolCvInputsSrc;

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		menu->addChild(createCheckMenuItem("Fader control", "",
			[=]() {return *linearVolCvInputsSrc == 0;},
			[=]() {*linearVolCvInputsSrc = 0;}
		));
		menu->addChild(createCheckMenuItem("Linear VCA", "",
			[=]() {return *linearVolCvInputsSrc == 1;},
			[=]() {*linearVolCvInputsSrc = 1;}
		));
		return menu;
	}
};


// Track context menu
// --------------------

// Gain adjust menu item
struct GainAdjustQuantity : Quantity {
	float *gainAdjustSrc;
	float minDb;
	float maxDb;
	  
	GainAdjustQuantity(float *_gainAdjustSrc, float _minDb, float _maxDb) {
		gainAdjustSrc = _gainAdjustSrc;
		minDb = _minDb;
		maxDb = _maxDb;
	}
	void setValue(float value) override {
		float gainInDB = math::clamp(value, getMinValue(), getMaxValue());
		*gainAdjustSrc = std::pow(10.0f, gainInDB / 20.0f);
	}
	float getValue() override {
		return 20.0f * std::log10(*gainAdjustSrc);
	}
	float getMinValue() override {return minDb;}
	float getMaxValue() override {return maxDb;}
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
	GainAdjustSlider(float *gainAdjustSrc, float minDb, float maxDb) {
		quantity = new GainAdjustQuantity(gainAdjustSrc, minDb, maxDb);
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



// init track settings
template <typename TMixerTrack>
struct InitializeTrackItem : MenuItem {
	TMixerTrack *srcTrack;
	int *updateTrackLabelRequestPtr;
	int8_t *trackOrGroupResetInAuxPtr;
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
		*trackOrGroupResetInAuxPtr = srcTrack->trackNum;
	}
};

// init group settings
template <typename TMixerGroup>
struct InitializeGroupItem : MenuItem {
	TMixerGroup *srcGroup;
	int groupNumForLink;
	int *updateTrackLabelRequestPtr;
	int8_t *trackOrGroupResetInAuxPtr;
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
		*trackOrGroupResetInAuxPtr = groupNumForLink;
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

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		
		for (int trk = 0; trk < numTracks; trk++) {
			menu->addChild(createCheckMenuItem(std::string(tracks[trk].trackName, 4), "",
				[=]() {return trk == trackNumSrc;},
				[=]() {TrackSettingsCpBuffer buffer;
						tracks[trackNumSrc].write(&buffer);   
						tracks[trk].read(&buffer);},
				trk == trackNumSrc
			));
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
	int64_t *idPtr;

	struct TrackReorderSubItem : MenuItem {
		TMixerTrack *tracks = NULL;
		int trackNumSrc;	
		int trackNumDest;
		int numTracks;
		int *updateTrackLabelRequestPtr;
		int32_t *trackMoveInAuxRequestPtr;
		PortWidget **inputWidgets;
		int64_t *idPtr;
		
		CableWidget* cwClr[4];
		
		void clearTrackInputs(int trk) {
			for (int i = 0; i < 4; i++) {// scan Left, Right, Volume, Pan
				cwClr[i] = APP->scene->rack->getTopCable(inputWidgets[trk + i * numTracks]);// only top needed since inputs have at most one cable
				if (cwClr[i] != NULL) {
					// v1: 
					// APP->scene->rack->removeCable(cwClr[i]);
					// v2:
					APP->scene->rack->removeCable(cwClr[i]);
					cwClr[i]->inputPort = NULL;
					cwClr[i]->updateCable();					
				}
			}
		}
		void transferTrackInputs(int srcTrk, int destTrk) {
			for (int i = 0; i < 4; i++) {// scan Left, Right, Volume, Pan
				CableWidget* cwRip = APP->scene->rack->getTopCable(inputWidgets[srcTrk + i * numTracks]);// only top needed since inputs have at most one cable
				if (cwRip != NULL) {
					// v1:
					// APP->scene->rack->removeCable(cwRip);
					// cwRip->setInput(inputWidgets[destTrk + i * numTracks]);
					// APP->scene->rack->addCable(cwRip);
					// v2:
					APP->scene->rack->removeCable(cwRip);
					cwRip->inputPort = inputWidgets[destTrk + i * numTracks];
					cwRip->updateCable();
					APP->scene->rack->addCable(cwRip);
				}
			}
		}
		void reconnectTrackInputs(int trk) {
			for (int i = 0; i < 4; i++) {// scan Left, Right, Volume, Pan
				if (cwClr[i] != NULL) {
					// v1:
					// cwClr[i]->setInput(inputWidgets[trk + i * numTracks]);
					// APP->scene->rack->addCable(cwClr[i]);
					// v2: 
					cwClr[i]->inputPort = inputWidgets[trk + i * numTracks];
					cwClr[i]->updateCable();
					APP->scene->rack->addCable(cwClr[i]);
				}
			}
		}

		void onAction(const event::Action &e) override {
			TrackSettingsCpBuffer buffer1;
			TrackSettingsCpBuffer buffer2;
			
			// use same strategy as in PortWidget::onDragStart/onDragEnd to make sure it's safely implemented (simulate manual dragging of the cables)
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
			
			// broadcast track move to message bus
			mixerMessageBus.sendTrackMove((*idPtr) + 1, trackNumSrc, trackNumDest);
			
			e.consume(this);// don't allow ctrl-click to keep menu open
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
			reo0Item->idPtr = idPtr;
			reo0Item->disabled = onSource;
			menu->addChild(reo0Item);
		}

		return menu;
	}		
};


// Master right-click menu
// --------------------
			
// clipper
struct ClippingItem : MenuItem {
	int *clippingSrc;

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		menu->addChild(createCheckMenuItem("Soft (default)", "",
			[=]() {return *clippingSrc == 0;},
			[=]() {*clippingSrc = 0;}
		));
		menu->addChild(createCheckMenuItem("Hard", "",
			[=]() {return *clippingSrc == 1;},
			[=]() {*clippingSrc = 1;}
		));
		return menu;
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
