//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc BoulĂ© 
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
		int setVal;
		void onAction(const event::Action &e) override {
			gInfo->panLawMono = setVal;
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
			PanLawMonoSubItem *lawMonoItem = createMenuItem<PanLawMonoSubItem>(panLawMonoNames[i], CHECKMARK(gInfo->panLawMono == i));
			lawMonoItem->gInfo = gInfo;
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

	struct TapModeSubItem : MenuItem {
		int8_t* tapModePtr;
		int8_t setVal;
		void onAction(const event::Action &e) override {
			*tapModePtr = setVal;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		std::string tapModeNames[5] = {
			"Pre-insert",
			"Pre-fader",
			"Post-fader",
			"Post-mute/solo",
			"Set per track"
		};
		
		for (int i = 0; i < (isGlobal ? 5 : 4); i++) {
			TapModeSubItem *tapModeItem = createMenuItem<TapModeSubItem>(tapModeNames[i], CHECKMARK(*tapModePtr == i));
			tapModeItem->tapModePtr = tapModePtr;
			tapModeItem->setVal = i;
			menu->addChild(tapModeItem);
		}

		return menu;
	}
};

struct TapModePlusItem : TapModeItem {
	GlobalInfo* gInfo;

	struct MuteTrkSendsWhenGrpMutedItem : MenuItem {
		int* muteTrkSendsWhenGrpMutedPtr;
		void onAction(const event::Action &e) override {
			*muteTrkSendsWhenGrpMutedPtr ^= 0x1;
		}
	};


	Menu *createChildMenu() override {
		Menu *menu = TapModeItem::createChildMenu();

		menu->addChild(new MenuLabel());// empty line	
		MuteTrkSendsWhenGrpMutedItem *muteTwhenGItem = createMenuItem<MuteTrkSendsWhenGrpMutedItem>("Mute track sends when group is muted", CHECKMARK(gInfo->muteTrkSendsWhenGrpMuted != 0));
		muteTwhenGItem->muteTrkSendsWhenGrpMutedPtr = &(gInfo->muteTrkSendsWhenGrpMuted);
		menu->addChild(muteTwhenGItem);

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
	GlobalInfo *gInfo;

	struct ChainSubItem : MenuItem {
		GlobalInfo *gInfo;
		int setVal = 0;
		void onAction(const event::Action &e) override {
			gInfo->chainMode = setVal;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		ChainSubItem *ch0Item = createMenuItem<ChainSubItem>("Pre-master", CHECKMARK(gInfo->chainMode == 0));
		ch0Item->gInfo = gInfo;
		menu->addChild(ch0Item);

		ChainSubItem *ch1Item = createMenuItem<ChainSubItem>("Post-master", CHECKMARK(gInfo->chainMode == 1));
		ch1Item->gInfo = gInfo;
		ch1Item->setVal = 1;
		menu->addChild(ch1Item);

		return menu;
	}
};

struct AuxRetFbProtItem : MenuItem {
	GlobalInfo *gInfo;

	struct AuxRetFbProtSubItem : MenuItem {
		GlobalInfo *gInfo;
		void onAction(const event::Action &e) override {
			gInfo->groupedAuxReturnFeedbackProtection ^= 0x1;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		AuxRetFbProtSubItem *fb1Item = createMenuItem<AuxRetFbProtSubItem>("Feedback protection ON (default)", CHECKMARK(gInfo->groupedAuxReturnFeedbackProtection == 1));
		fb1Item->gInfo = gInfo;
		menu->addChild(fb1Item);

		AuxRetFbProtSubItem *fb0Item = createMenuItem<AuxRetFbProtSubItem>("Feedback protection OFF (Warning RTFM!)", CHECKMARK(gInfo->groupedAuxReturnFeedbackProtection == 0));
		fb0Item->gInfo = gInfo;
		menu->addChild(fb0Item);

		return menu;
	}
};

struct CloakedModeItem : MenuItem {
	GlobalInfo *gInfo;
	void onAction(const event::Action &e) override {
		gInfo->colorAndCloak.cc4[cloakedMode] ^= 0xFF;
	}
};

struct ExpansionItem : MenuItem {
	int *expansionPtr;
	void onAction(const event::Action &e) override {
		*expansionPtr ^= 0x1;
	}
};

struct VuColorItem : MenuItem {
	int8_t *srcColor;
	bool isGlobal;// true when this is in the context menu of module, false when it is in a track/group/master context menu

	struct VuColorSubItem : MenuItem {
		int8_t *srcColor;
		int setVal;
		void onAction(const event::Action &e) override {
			*srcColor = setVal;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		std::string vuColorNames[6] = {
			"Green (default)",
			"Aqua",
			"Cyan",
			"Blue",
			"Purple",
			"Set per track"
		};
		
		for (int i = 0; i < (isGlobal ? 6 : 5); i++) {
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
	bool isGlobal;// true when this is in the context menu of module, false when it is in a track/group/master context menu

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
	GlobalInfo *gInfo;

	struct SymmetricalFadeItem : MenuItem {
		GlobalInfo *gInfo;
		void onAction(const event::Action &e) override {
			gInfo->symmetricalFade = !gInfo->symmetricalFade;
		}
	};

	struct FadeCvOutItem : MenuItem {
		GlobalInfo *gInfo;
		void onAction(const event::Action &e) override {
			gInfo->fadeCvOutsWithVolCv = !gInfo->fadeCvOutsWithVolCv;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		SymmetricalFadeItem *symItem = createMenuItem<SymmetricalFadeItem>("Symmetrical", CHECKMARK(gInfo->symmetricalFade));
		symItem->gInfo = gInfo;
		menu->addChild(symItem);
		
		FadeCvOutItem *fadeCvItem = createMenuItem<FadeCvOutItem>("Include vol CV in fade CV out", CHECKMARK(gInfo->fadeCvOutsWithVolCv));
		fadeCvItem->gInfo = gInfo;
		menu->addChild(fadeCvItem);
		
		return menu;
	}
};

struct EcoItem : MenuItem {
	GlobalInfo *gInfo;
	void onAction(const event::Action &e) override {
		gInfo->ecoMode = ~gInfo->ecoMode;
	}
};


// Track context menu
// --------------------

// Gain adjust menu item

struct GainAdjustQuantity : Quantity {
	MixerTrack *srcTrack = NULL;
	  
	GainAdjustQuantity(MixerTrack *_srcTrack) {
		srcTrack = _srcTrack;
	}
	void setValue(float value) override {
		float gainInDB = math::clamp(value, getMinValue(), getMaxValue());
		srcTrack->gainAdjust = std::pow(10.0f, gainInDB / 20.0f);
	}
	float getValue() override {
		return 20.0f * std::log10(srcTrack->gainAdjust);
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
		if (valCut >= TrackOrAux::minHPFCutoffFreq) {
			return string::f("%i", (int)(math::normalizeZero(valCut) + 0.5f));
		}
		else {
			return "OFF";
		}
	}
	void setDisplayValue(float displayValue) override {setValue(displayValue);}
	std::string getLabel() override {return "HPF Cutoff";}
	std::string getUnit() override {
		if (getDisplayValue() >= TrackOrAux::minHPFCutoffFreq) {
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



// LPF filter cutoff menu item

template <typename TrackOrAux>
struct LPFCutoffQuantity : Quantity {
	TrackOrAux *srcTrack = NULL;
	
	LPFCutoffQuantity(TrackOrAux *_srcTrack) {
		srcTrack = _srcTrack;
	}
	void setValue(float value) override {
		srcTrack->setLPFCutoffFreq(math::clamp(value, getMinValue(), getMaxValue()));
	}
	float getValue() override {
		return srcTrack->getLPFCutoffFreq();
	}
	float getMinValue() override {return 1000.0f;}
	float getMaxValue() override {return 21000.0f;}
	float getDefaultValue() override {return 20010.0f;}
	float getDisplayValue() override {return getValue();}
	std::string getDisplayValueString() override {
		float valCut = getDisplayValue();
		if (valCut <= TrackOrAux::maxLPFCutoffFreq) {
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
		if (getDisplayValue() <= TrackOrAux::maxLPFCutoffFreq) {
			return " kHz";
		}
		else {
			return "";
		}
	}
};

template <typename TrackOrAux>
struct LPFCutoffSlider : ui::Slider {
	LPFCutoffSlider(TrackOrAux *srcTrack) {
		quantity = new LPFCutoffQuantity<TrackOrAux>(srcTrack);
	}
	~LPFCutoffSlider() {
		delete quantity;
	}
};



// Fade-rate menu item

struct FadeRateQuantity : Quantity {
	float *srcFadeRate = NULL;
	float minFadeRate;
	  
	FadeRateQuantity(float *_srcFadeRate, float _minFadeRate) {
		srcFadeRate = _srcFadeRate;
		minFadeRate = _minFadeRate;
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
		if (valCut >= minFadeRate) {
			return string::f("%.1f", valCut);
		}
		else {
			return "OFF";
		}
	}
	void setDisplayValue(float displayValue) override {setValue(displayValue);}
	std::string getLabel() override {return "Fade";}
	std::string getUnit() override {
		if (getDisplayValue() >= minFadeRate) {
			return " s";
		}
		else {
			return "";
		}
	}};

struct FadeRateSlider : ui::Slider {
	FadeRateSlider(float *_srcFadeRate, float _minFadeRate) {
		quantity = new FadeRateQuantity(_srcFadeRate, _minFadeRate);
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
template <typename TrackOrGroup>
struct LinkFaderItem : MenuItem {
	TrackOrGroup *srcTrkGrp = NULL;
	void onAction(const event::Action &e) override {
		srcTrkGrp->toggleLinked();
	}
};


// copy track menu settings to
struct CopyTrackSettingsItem : MenuItem {
	MixerTrack *tracks = NULL;
	int trackNumSrc;	

	struct CopyTrackSettingsSubItem : MenuItem {
		MixerTrack *tracks = NULL;
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

		for (int trk = 0; trk < 16; trk++) {
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
struct TrackReorderItem : MenuItem {
	MixerTrack *tracks = NULL;
	int trackNumSrc;	
	int *updateTrackLabelRequestPtr;
	int32_t *trackMoveInAuxRequestPtr;
	PortWidget **inputWidgets;

	struct TrackReorderSubItem : MenuItem {
		MixerTrack *tracks = NULL;
		int trackNumSrc;	
		int trackNumDest;
		int *updateTrackLabelRequestPtr;
		int32_t *trackMoveInAuxRequestPtr;
		PortWidget **inputWidgets;
		
		CableWidget* cwClr[4];
		
		void transferTrackInputs(int srcTrk, int destTrk) {
			// use same strategy as in PortWidget::onDragStart/onDragEnd to make sure it's safely implemented (simulate manual dragging of the cables)
			for (int i = 0; i < 4; i++) {// scan Left, Right, Volume, Pan
				CableWidget* cwRip = APP->scene->rack->getTopCable(inputWidgets[srcTrk + i * 16]);// only top needed since inputs have at most one cable
				if (cwRip != NULL) {
					APP->scene->rack->removeCable(cwRip);
					cwRip->setInput(inputWidgets[destTrk + i * 16]);
					APP->scene->rack->addCable(cwRip);
				}
			}
		}
		void clearTrackInputs(int trk) {
			for (int i = 0; i < 4; i++) {// scan Left, Right, Volume, Pan
				cwClr[i] = APP->scene->rack->getTopCable(inputWidgets[trk + i * 16]);// only top needed since inputs have at most one cable
				if (cwClr[i] != NULL) {
					APP->scene->rack->removeCable(cwClr[i]);
				}
			}
		}
		void reconnectTrackInputs(int trk) {
			for (int i = 0; i < 4; i++) {// scan Left, Right, Volume, Pan
				if (cwClr[i] != NULL) {
					cwClr[i]->setInput(inputWidgets[trk + i * 16]);
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

		for (int trk = 0; trk < 16; trk++) {
			bool onSource = (trk == trackNumSrc);
			TrackReorderSubItem *reo0Item = createMenuItem<TrackReorderSubItem>(std::string(tracks[trk].trackName, 4), CHECKMARK(onSource));
			reo0Item->tracks = tracks;
			reo0Item->trackNumSrc = trackNumSrc;
			reo0Item->trackNumDest = trk;
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
	MixerMaster *srcMaster;
	void onAction(const event::Action &e) override {
		srcMaster->dcBlock = !srcMaster->dcBlock;
	}
};
		
		
// clipper
struct ClippingItem : MenuItem {
	MixerMaster *srcMaster;

	struct ClippingSubItem : MenuItem {
		MixerMaster *srcMaster;
		int setVal = 0;
		void onAction(const event::Action &e) override {
			srcMaster->clipping = setVal;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		ClippingSubItem *lim0Item = createMenuItem<ClippingSubItem>("Soft (default)", CHECKMARK(srcMaster->clipping == 0));
		lim0Item->srcMaster = srcMaster;
		menu->addChild(lim0Item);

		ClippingSubItem *lim1Item = createMenuItem<ClippingSubItem>("Hard", CHECKMARK(srcMaster->clipping == 1));
		lim1Item->srcMaster = srcMaster;
		lim1Item->setVal = 1;
		menu->addChild(lim1Item);

		return menu;
	}
};


// dim gain menu item

struct DimGainQuantity : Quantity {
	MixerMaster *srcMaster = NULL;
	  
	DimGainQuantity(MixerMaster *_srcMaster) {
		srcMaster = _srcMaster;
	}
	void setValue(float value) override {
		float gainInDB = math::clamp(value, getMinValue(), getMaxValue());
		srcMaster->dimGain = std::pow(10.0f, gainInDB / 20.0f);
		srcMaster->updateDimGainIntegerDB();
	}
	float getValue() override {
		return 20.0f * std::log10(srcMaster->dimGain);
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
	DimGainSlider(MixerMaster *srcMaster) {
		quantity = new DimGainQuantity(srcMaster);
	}
	~DimGainSlider() {
		delete quantity;
	}
};


#endif
