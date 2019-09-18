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
	int8_t *panLawStereoSrc;
	bool isGlobal;// true when this is in the context menu of module, false when it is in a track/group context menu

	struct PanLawStereoSubItem : MenuItem {
		int8_t *panLawStereoSrc;
		int8_t setVal = 0;
		void onAction(const event::Action &e) override {
			*panLawStereoSrc = setVal;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		PanLawStereoSubItem *law0Item = createMenuItem<PanLawStereoSubItem>("Stereo balance (default)", CHECKMARK(*panLawStereoSrc == 0));
		law0Item->panLawStereoSrc = panLawStereoSrc;
		menu->addChild(law0Item);

		PanLawStereoSubItem *law1Item = createMenuItem<PanLawStereoSubItem>("True panning", CHECKMARK(*panLawStereoSrc == 1));
		law1Item->panLawStereoSrc = panLawStereoSrc;
		law1Item->setVal = 1;
		menu->addChild(law1Item);

		if (isGlobal) {
			PanLawStereoSubItem *law2Item = createMenuItem<PanLawStereoSubItem>("Set per track", CHECKMARK(*panLawStereoSrc == 2));
			law2Item->panLawStereoSrc = panLawStereoSrc;
			law2Item->setVal = 2;
			menu->addChild(law2Item);
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
		int8_t setVal = 0;
		void onAction(const event::Action &e) override {
			*tapModePtr = setVal;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		TapModeSubItem *pre0Item = createMenuItem<TapModeSubItem>("Pre-insert", CHECKMARK(*tapModePtr == 0));
		pre0Item->tapModePtr = tapModePtr;
		menu->addChild(pre0Item);

		TapModeSubItem *pre1Item = createMenuItem<TapModeSubItem>("Pre-fader", CHECKMARK(*tapModePtr == 1));
		pre1Item->tapModePtr = tapModePtr;
		pre1Item->setVal = 1;
		menu->addChild(pre1Item);

		TapModeSubItem *pre2Item = createMenuItem<TapModeSubItem>("Post-fader", CHECKMARK(*tapModePtr == 2));
		pre2Item->tapModePtr = tapModePtr;
		pre2Item->setVal = 2;
		menu->addChild(pre2Item);

		TapModeSubItem *pre3Item = createMenuItem<TapModeSubItem>("Post-mute/solo", CHECKMARK(*tapModePtr == 3));
		pre3Item->tapModePtr = tapModePtr;
		pre3Item->setVal = 3;
		menu->addChild(pre3Item);

		if (isGlobal) {
			TapModeSubItem *pre4Item = createMenuItem<TapModeSubItem>("Set per track", CHECKMARK(*tapModePtr == 4));
			pre4Item->tapModePtr = tapModePtr;
			pre4Item->setVal = 4;
			menu->addChild(pre4Item);
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

		AuxReturnModeSubItem *ret0Item = createMenuItem<AuxReturnModeSubItem>("Mute aux returns when soloing", CHECKMARK(*auxReturnsMutedWhenMainSoloPtr != 0));
		ret0Item->modePtr = auxReturnsMutedWhenMainSoloPtr;
		menu->addChild(ret0Item);

		AuxReturnModeSubItem *ret1Item = createMenuItem<AuxReturnModeSubItem>("Silence dry when aux return is solo'd", CHECKMARK(*auxReturnsSolosMuteDryPtr != 0));
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

struct CloakedModeItem : MenuItem {
	GlobalInfo *gInfo;
	void onAction(const event::Action &e) override {
		gInfo->colorAndCloak.cc4[cloakedMode] ^= 0x1;
	}
};

struct VuColorItem : MenuItem {
	int8_t *srcColor;
	bool isGlobal;// true when this is in the context menu of module, false when it is in a track/group/master context menu

	struct VuColorSubItem : MenuItem {
		int8_t *srcColor;
		int setVal = 0;
		void onAction(const event::Action &e) override {
			*srcColor = setVal;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		VuColorSubItem *col0Item = createMenuItem<VuColorSubItem>("Green (default)", CHECKMARK(*srcColor == 0));
		col0Item->srcColor = srcColor;
		menu->addChild(col0Item);

		VuColorSubItem *col1Item = createMenuItem<VuColorSubItem>("Aqua", CHECKMARK(*srcColor == 1));
		col1Item->srcColor = srcColor;
		col1Item->setVal = 1;
		menu->addChild(col1Item);

		VuColorSubItem *col2Item = createMenuItem<VuColorSubItem>("Cyan", CHECKMARK(*srcColor == 2));// Light blue
		col2Item->srcColor = srcColor;
		col2Item->setVal = 2;
		menu->addChild(col2Item);

		VuColorSubItem *col3Item = createMenuItem<VuColorSubItem>("Blue", CHECKMARK(*srcColor == 3));
		col3Item->srcColor = srcColor;
		col3Item->setVal = 3;
		menu->addChild(col3Item);

		VuColorSubItem *col4Item = createMenuItem<VuColorSubItem>("Purple", CHECKMARK(*srcColor == 4));
		col4Item->srcColor = srcColor;
		col4Item->setVal = 4;
		menu->addChild(col4Item);
	
		if (isGlobal) {
			VuColorSubItem *colIItem = createMenuItem<VuColorSubItem>("Set per track", CHECKMARK(*srcColor == 5));
			colIItem->srcColor = srcColor;
			colIItem->setVal = 5;
			menu->addChild(colIItem);
		}

		return menu;
	}
};


struct DispColorItem : MenuItem {
	int8_t *srcColor;

	struct DispColorSubItem : MenuItem {
		int8_t *srcColor;
		int setVal = 0;
		void onAction(const event::Action &e) override {
			*srcColor = setVal;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		DispColorSubItem *col0Item = createMenuItem<DispColorSubItem>("Yellow (default)", CHECKMARK(*srcColor == 0));
		col0Item->srcColor = srcColor;
		menu->addChild(col0Item);

		DispColorSubItem *col1Item = createMenuItem<DispColorSubItem>("Blue", CHECKMARK(*srcColor == 1));
		col1Item->srcColor = srcColor;
		col1Item->setVal = 1;
		menu->addChild(col1Item);

		DispColorSubItem *col2Item = createMenuItem<DispColorSubItem>("Green", CHECKMARK(*srcColor == 2));
		col2Item->srcColor = srcColor;
		col2Item->setVal = 2;
		menu->addChild(col2Item);

		DispColorSubItem *col3Item = createMenuItem<DispColorSubItem>("Light-grey", CHECKMARK(*srcColor == 3));
		col3Item->srcColor = srcColor;
		col3Item->setVal = 3;
		menu->addChild(col3Item);

		return menu;
	}
};

struct SymmetricalFadeItem : MenuItem {
	GlobalInfo *gInfo;
	void onAction(const event::Action &e) override {
		gInfo->symmetricalFade = !gInfo->symmetricalFade;
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
	float getMaxValue() override {return 1000.0f;}
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
	float getMinValue() override {return 1000.0f;}
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
	PortWidget **inputWidgets;

	struct TrackReorderSubItem : MenuItem {
		MixerTrack *tracks = NULL;
		int trackNumSrc;	
		int trackNumDest;
		int *updateTrackLabelRequestPtr;
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
	float getDefaultValue() override {return -20.0f;}
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
