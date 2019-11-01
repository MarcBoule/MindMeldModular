//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include <time.h>
#include "MixerCommon.hpp"
#include "MixerWidgets.hpp"


template<int N_TRK>
struct MixMaster : Module {

	enum ParamIds {
		ENUMS(TRACK_FADER_PARAMS, 16),
		ENUMS(GROUP_FADER_PARAMS, 4),// must follow TRACK_FADER_PARAMS since code assumes contiguous
		ENUMS(TRACK_PAN_PARAMS, 16),
		ENUMS(GROUP_PAN_PARAMS, 4),
		ENUMS(TRACK_MUTE_PARAMS, 16),
		ENUMS(GROUP_MUTE_PARAMS, 4),// must follow TRACK_MUTE_PARAMS since code assumes contiguous
		ENUMS(TRACK_SOLO_PARAMS, 16),// must follow GROUP_MUTE_PARAMS since code assumes contiguous
		ENUMS(GROUP_SOLO_PARAMS, 4),// must follow TRACK_SOLO_PARAMS since code assumes contiguous
		MAIN_MUTE_PARAM,// must follow GROUP_SOLO_PARAMS since code assumes contiguous
		MAIN_DIM_PARAM,// must follow MAIN_MUTE_PARAM since code assumes contiguous
		MAIN_MONO_PARAM,// must follow MAIN_DIM_PARAM since code assumes contiguous
		MAIN_FADER_PARAM,
		ENUMS(GROUP_SELECT_PARAMS, 16),
		NUM_PARAMS
	}; 


	enum InputIds {
		ENUMS(TRACK_SIGNAL_INPUTS, 16 * 2), // Track 0: 0 = L, 1 = R, Track 1: 2 = L, 3 = R, etc...
		ENUMS(TRACK_VOL_INPUTS, 16),
		ENUMS(GROUP_VOL_INPUTS, 4),
		ENUMS(TRACK_PAN_INPUTS, 16), 
		ENUMS(GROUP_PAN_INPUTS, 4), 
		ENUMS(CHAIN_INPUTS, 2),
		ENUMS(INSERT_TRACK_INPUTS, 2),
		INSERT_GRP_AUX_INPUT,
		TRACK_MUTE_INPUT,
		TRACK_SOLO_INPUT,
		GRPM_MUTESOLO_INPUT,// 1-4 Group mutes, 5-8 Group solos, 9 Master Mute, 10 Master Dim, 11 Master Mono, 12 Master VOL
		NUM_INPUTS
	};


	enum OutputIds {
		ENUMS(DIRECT_OUTPUTS, 3), // Track 1-8, Track 9-16, Groups and Aux
		ENUMS(MAIN_OUTPUTS, 2),
		ENUMS(INSERT_TRACK_OUTPUTS, 2),
		INSERT_GRP_AUX_OUTPUT,
		FADE_CV_OUTPUT,
		NUM_OUTPUTS
	};


	enum LightIds {
		ENUMS(TRACK_HPF_LIGHTS, 16),
		ENUMS(TRACK_LPF_LIGHTS, 16),
		NUM_LIGHTS
	};


	#include "Mixer.hpp"
	#include "MixerMenus.hpp"
	


// Master display editable label with menu
// --------------------

struct MasterDisplay : EditableDisplayBase {
	MixerMaster *srcMaster;
	
	MasterDisplay() {
		numChars = 6;
		textSize = 13;
		box.size.x = mm2px(18.3f);
		textOffset.x = 1.4f;// 2.4f
		text = "-0000-";
	}
	
	void onButton(const event::Button &e) override {
		if (e.button == GLFW_MOUSE_BUTTON_RIGHT && e.action == GLFW_PRESS) {
			ui::Menu *menu = createMenu();

			MenuLabel *mastSetLabel = new MenuLabel();
			mastSetLabel->text = "Master settings: ";
			menu->addChild(mastSetLabel);
			
			FadeRateSlider *fadeSlider = new FadeRateSlider(&(srcMaster->fadeRate));
			fadeSlider->box.size.x = 200.0f;
			menu->addChild(fadeSlider);
			
			FadeProfileSlider *fadeProfSlider = new FadeProfileSlider(&(srcMaster->fadeProfile));
			fadeProfSlider->box.size.x = 200.0f;
			menu->addChild(fadeProfSlider);
			
			DimGainSlider *dimSliderItem = new DimGainSlider(srcMaster);
			dimSliderItem->box.size.x = 200.0f;
			menu->addChild(dimSliderItem);
			
			DcBlockItem *dcItem = createMenuItem<DcBlockItem>("DC blocker", CHECKMARK(srcMaster->dcBlock));
			dcItem->dcBlockSrc = &(srcMaster->dcBlock);
			menu->addChild(dcItem);
			
			ClippingItem *clipItem = createMenuItem<ClippingItem>("Clipping", RIGHT_ARROW);
			clipItem->clippingSrc = &(srcMaster->clipping);
			menu->addChild(clipItem);
				
			if (srcMaster->gInfo->colorAndCloak.cc4[vuColorGlobal] >= numThemes) {	
				VuColorItem *vuColItem = createMenuItem<VuColorItem>("VU Colour", RIGHT_ARROW);
				vuColItem->srcColor = &(srcMaster->vuColorThemeLocal);
				vuColItem->isGlobal = false;
				menu->addChild(vuColItem);
			}
			
			if (srcMaster->gInfo->colorAndCloak.cc4[dispColor] >= 7) {
				DispColorItem *dispColItem = createMenuItem<DispColorItem>("Display colour", RIGHT_ARROW);
				dispColItem->srcColor = &(srcMaster->dispColorLocal);
				dispColItem->isGlobal = false;
				menu->addChild(dispColItem);
			}
			
			e.consume(this);
			return;
		}
		EditableDisplayBase::onButton(e);		
	}
	void onChange(const event::Change &e) override {
		snprintf(srcMaster->masterLabel, 7, "%s", text.c_str());
		EditableDisplayBase::onChange(e);
	};
};



// Track display editable label with menu
// --------------------

struct TrackDisplay : EditableDisplayBase {
	MixerTrack *tracks = NULL;
	int trackNumSrc;
	int *updateTrackLabelRequestPtr;
	int *trackMoveInAuxRequestPtr;
	PortWidget **inputWidgets;
	bool *auxExpanderPresentPtr;

	void onButton(const event::Button &e) override {
		if (e.button == GLFW_MOUSE_BUTTON_RIGHT && e.action == GLFW_PRESS) {
			ui::Menu *menu = createMenu();
			MixerTrack *srcTrack = &(tracks[trackNumSrc]);

			MenuLabel *trkSetLabel = new MenuLabel();
			trkSetLabel->text = "Track settings: " + std::string(srcTrack->trackName, 4);
			menu->addChild(trkSetLabel);
			
			GainAdjustSlider *trackGainAdjustSlider = new GainAdjustSlider(srcTrack);
			trackGainAdjustSlider->box.size.x = 200.0f;
			menu->addChild(trackGainAdjustSlider);
			
			HPFCutoffSlider<MixerTrack> *trackHPFAdjustSlider = new HPFCutoffSlider<MixerTrack>(srcTrack);
			trackHPFAdjustSlider->box.size.x = 200.0f;
			menu->addChild(trackHPFAdjustSlider);
			
			LPFCutoffSlider<MixerTrack> *trackLPFAdjustSlider = new LPFCutoffSlider<MixerTrack>(srcTrack);
			trackLPFAdjustSlider->box.size.x = 200.0f;
			menu->addChild(trackLPFAdjustSlider);
			
			PanCvLevelSlider *panCvSlider = new PanCvLevelSlider(&(srcTrack->panCvLevel));
			panCvSlider->box.size.x = 200.0f;
			menu->addChild(panCvSlider);
			
			FadeRateSlider *fadeSlider = new FadeRateSlider(srcTrack->fadeRate);
			fadeSlider->box.size.x = 200.0f;
			menu->addChild(fadeSlider);
			
			FadeProfileSlider *fadeProfSlider = new FadeProfileSlider(&(srcTrack->fadeProfile));
			fadeProfSlider->box.size.x = 200.0f;
			menu->addChild(fadeProfSlider);
			
			LinkFaderItem<MixerTrack> *linkFadItem = createMenuItem<LinkFaderItem<MixerTrack>>("Link fader and fade", CHECKMARK(srcTrack->isLinked()));
			linkFadItem->srcTrkGrp = srcTrack;
			menu->addChild(linkFadItem);
			
			if (srcTrack->gInfo->directOutsMode >= 4) {
				TapModeItem *directOutsItem = createMenuItem<TapModeItem>("Direct outs", RIGHT_ARROW);
				directOutsItem->tapModePtr = &(srcTrack->directOutsMode);
				directOutsItem->isGlobal = false;
				menu->addChild(directOutsItem);
			}

			if (srcTrack->gInfo->filterPos >= 2) {
				FilterPosItem *filterPosItem = createMenuItem<FilterPosItem>("Filters", RIGHT_ARROW);
				filterPosItem->filterPosSrc = &(srcTrack->filterPos);
				filterPosItem->isGlobal = false;
				menu->addChild(filterPosItem);
			}

			if (srcTrack->gInfo->auxSendsMode >= 4 && *auxExpanderPresentPtr) {
				TapModeItem *auxSendsItem = createMenuItem<TapModeItem>("Aux sends", RIGHT_ARROW);
				auxSendsItem->tapModePtr = &(srcTrack->auxSendsMode);
				auxSendsItem->isGlobal = false;
				menu->addChild(auxSendsItem);
			}

			if (srcTrack->gInfo->panLawStereo >= 3) {
				PanLawStereoItem *panLawStereoItem = createMenuItem<PanLawStereoItem>("Stereo pan mode", RIGHT_ARROW);
				panLawStereoItem->panLawStereoSrc = &(srcTrack->panLawStereo);
				panLawStereoItem->isGlobal = false;
				menu->addChild(panLawStereoItem);
			}

			if (srcTrack->gInfo->colorAndCloak.cc4[vuColorGlobal] >= numThemes) {	
				VuColorItem *vuColItem = createMenuItem<VuColorItem>("VU Colour", RIGHT_ARROW);
				vuColItem->srcColor = &(srcTrack->vuColorThemeLocal);
				vuColItem->isGlobal = false;
				menu->addChild(vuColItem);
			}

			if (srcTrack->gInfo->colorAndCloak.cc4[dispColor] >= 7) {
				DispColorItem *dispColItem = createMenuItem<DispColorItem>("Display colour", RIGHT_ARROW);
				dispColItem->srcColor = &(srcTrack->dispColorLocal);
				dispColItem->isGlobal = false;
				menu->addChild(dispColItem);
			}
			
			menu->addChild(new MenuSeparator());

			MenuLabel *settingsALabel = new MenuLabel();
			settingsALabel->text = "Actions: " + std::string(srcTrack->trackName, 4);
			menu->addChild(settingsALabel);

			CopyTrackSettingsItem *copyItem = createMenuItem<CopyTrackSettingsItem>("Copy track menu settings to:", RIGHT_ARROW);
			copyItem->tracks = tracks;
			copyItem->trackNumSrc = trackNumSrc;
			menu->addChild(copyItem);
			
			TrackReorderItem *reodrerItem = createMenuItem<TrackReorderItem>("Move to:", RIGHT_ARROW);
			reodrerItem->tracks = tracks;
			reodrerItem->trackNumSrc = trackNumSrc;
			reodrerItem->updateTrackLabelRequestPtr = updateTrackLabelRequestPtr;
			reodrerItem->trackMoveInAuxRequestPtr = trackMoveInAuxRequestPtr;
			reodrerItem->inputWidgets = inputWidgets;
			menu->addChild(reodrerItem);
			
			e.consume(this);
			return;
		}
		EditableDisplayBase::onButton(e);
	}
	void onChange(const event::Change &e) override {
		(*((uint32_t*)(tracks[trackNumSrc].trackName))) = 0x20202020;
		for (int i = 0; i < std::min(4, (int)text.length()); i++) {
			tracks[trackNumSrc].trackName[i] = text[i];
		}
		EditableDisplayBase::onChange(e);
	};
};


// Group display editable label with menu
// --------------------

struct GroupDisplay : EditableDisplayBase {
	MixerGroup *srcGroup = NULL;
	bool *auxExpanderPresentPtr;

	void onButton(const event::Button &e) override {
		if (e.button == GLFW_MOUSE_BUTTON_RIGHT && e.action == GLFW_PRESS) {
			ui::Menu *menu = createMenu();

			MenuLabel *grpSetLabel = new MenuLabel();
			grpSetLabel->text = "Group settings: " + std::string(srcGroup->groupName, 4);
			menu->addChild(grpSetLabel);
			
			PanCvLevelSlider *panCvSlider = new PanCvLevelSlider(&(srcGroup->panCvLevel));
			panCvSlider->box.size.x = 200.0f;
			menu->addChild(panCvSlider);
			
			FadeRateSlider *fadeSlider = new FadeRateSlider(srcGroup->fadeRate);
			fadeSlider->box.size.x = 200.0f;
			menu->addChild(fadeSlider);
			
			FadeProfileSlider *fadeProfSlider = new FadeProfileSlider(&(srcGroup->fadeProfile));
			fadeProfSlider->box.size.x = 200.0f;
			menu->addChild(fadeProfSlider);
			
			LinkFaderItem<MixerGroup> *linkFadItem = createMenuItem<LinkFaderItem<MixerGroup>>("Link fader and fade", CHECKMARK(srcGroup->isLinked()));
			linkFadItem->srcTrkGrp = srcGroup;
			menu->addChild(linkFadItem);
			
			if (srcGroup->gInfo->directOutsMode >= 4) {
				TapModeItem *directOutsItem = createMenuItem<TapModeItem>("Direct outs", RIGHT_ARROW);
				directOutsItem->tapModePtr = &(srcGroup->directOutsMode);
				directOutsItem->isGlobal = false;
				menu->addChild(directOutsItem);
			}

			if (srcGroup->gInfo->auxSendsMode >= 4 && *auxExpanderPresentPtr) {
				TapModeItem *auxSendsItem = createMenuItem<TapModeItem>("Aux sends", RIGHT_ARROW);
				auxSendsItem->tapModePtr = &(srcGroup->auxSendsMode);
				auxSendsItem->isGlobal = false;
				menu->addChild(auxSendsItem);
			}

			if (srcGroup->gInfo->panLawStereo >= 3) {
				PanLawStereoItem *panLawStereoItem = createMenuItem<PanLawStereoItem>("Stereo pan mode", RIGHT_ARROW);
				panLawStereoItem->panLawStereoSrc = &(srcGroup->panLawStereo);
				panLawStereoItem->isGlobal = false;
				menu->addChild(panLawStereoItem);
			}

			if (srcGroup->gInfo->colorAndCloak.cc4[vuColorGlobal] >= numThemes) {	
				VuColorItem *vuColItem = createMenuItem<VuColorItem>("VU Colour", RIGHT_ARROW);
				vuColItem->srcColor = &(srcGroup->vuColorThemeLocal);
				vuColItem->isGlobal = false;
				menu->addChild(vuColItem);
			}
			
			if (srcGroup->gInfo->colorAndCloak.cc4[dispColor] >= 7) {
				DispColorItem *dispColItem = createMenuItem<DispColorItem>("Display colour", RIGHT_ARROW);
				dispColItem->srcColor = &(srcGroup->dispColorLocal);
				dispColItem->isGlobal = false;
				menu->addChild(dispColItem);
			}
			
			e.consume(this);
			return;
		}
		EditableDisplayBase::onButton(e);
	}
	void onChange(const event::Change &e) override {
		(*((uint32_t*)(srcGroup->groupName))) = 0x20202020;
		for (int i = 0; i < std::min(4, (int)text.length()); i++) {
			srcGroup->groupName[i] = text[i];
		}
		EditableDisplayBase::onChange(e);
	};
};



// Special solo button with mutex feature (ctrl-click)

struct DynSoloButtonMutex : DynSoloButton {
	Param *soloParams;// 19 (or 15) params in here must be cleared when mutex solo performed on a group (track)
	unsigned long soloMutexUnclickMemory;// for ctrl-unclick. Invalid when soloMutexUnclickMemory == -1
	int soloMutexUnclickMemorySize;// -1 when nothing stored

	void onButton(const event::Button &e) override {
		if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS) {
			if (((APP->window->getMods() & RACK_MOD_MASK) == RACK_MOD_CTRL)) {
				int soloParamId = paramQuantity->paramId - TRACK_SOLO_PARAMS;
				bool isTrack = (soloParamId < 16);
				int end = 16 + (isTrack ? 0 : 4);
				bool turningOnSolo = soloParams[soloParamId].getValue() < 0.5f;
				
				
				if (turningOnSolo) {// ctrl turning on solo: memorize solo states and clear all other solos 
					// memorize solo states in case ctrl-unclick happens
					soloMutexUnclickMemorySize = end;
					soloMutexUnclickMemory = 0;
					for (int i = 0; i < end; i++) {
						if (soloParams[i].getValue() > 0.5f) {
							soloMutexUnclickMemory |= (1 << i);
						}
					}
					
					// clear 19 (or 15) solos 
					for (int i = 0; i < end; i++) {
						if (soloParamId != i) {
							soloParams[i].setValue(0.0f);
						}
					}
					
				}
				else {// ctrl turning off solo: recall stored solo states
					// reinstate 19 (or 15) solos 
					if (soloMutexUnclickMemorySize >= 0) {
						for (int i = 0; i < soloMutexUnclickMemorySize; i++) {
							if (soloParamId != i) {
								soloParams[i].setValue((soloMutexUnclickMemory & (1 << i)) != 0 ? 1.0f : 0.0f);
							}
						}
						soloMutexUnclickMemorySize = -1;// nothing in soloMutexUnclickMemory
					}
				}
				e.consume(this);
				return;
			}
			else {
				soloMutexUnclickMemorySize = -1;// nothing in soloMutexUnclickMemory
				if ((APP->window->getMods() & RACK_MOD_MASK) == (RACK_MOD_CTRL | GLFW_MOD_SHIFT)) {
					for (int i = 0; i < 16 + 4; i++) {
						if (i != paramQuantity->paramId - TRACK_SOLO_PARAMS) {
							soloParams[i].setValue(0.0f);
						}
					}
					e.consume(this);
					return;
				}
			}
		}
		DynSoloButton::onButton(e);		
	}
};



struct DynMuteFadeButtonWithClear : DynMuteFadeButton {
	Param *muteParams;// 19 (or 15) params in here must be cleared when mutex mute performed on a group (track)

	void onButton(const event::Button &e) override {
		if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS) {
			if ((APP->window->getMods() & RACK_MOD_MASK) == (RACK_MOD_CTRL | GLFW_MOD_SHIFT)) {
				for (int i = 0; i < 16 + 4; i++) {
					if (i != paramQuantity->paramId - TRACK_MUTE_PARAMS) {
						muteParams[i].setValue(0.0f);
					}
				}
				e.consume(this);
				return;
			}
		}
		DynMuteFadeButton::onButton(e);		
	}	
};



// linked faders
// --------------------

struct DynSmallFaderWithLink : DynSmallFader {
	GlobalInfo *gInfo;
	Param *faderParams = NULL;
	float lastValue = -1.0f;
	
	void onButton(const event::Button &e) override {
		int faderIndex = paramQuantity->paramId - TRACK_FADER_PARAMS;
		if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS) {
			if ((APP->window->getMods() & RACK_MOD_MASK) == GLFW_MOD_ALT) {
				gInfo->toggleLinked(faderIndex);
				e.consume(this);
				return;
			}
			else if ((APP->window->getMods() & RACK_MOD_MASK) == (GLFW_MOD_ALT | GLFW_MOD_SHIFT)) {
				gInfo->linkBitMask = 0;
				e.consume(this);
				return;
			}
		}
		DynSmallFader::onButton(e);		
	}

	void draw(const DrawArgs &args) override {
		DynSmallFader::draw(args);
		if (paramQuantity) {
			int faderIndex = paramQuantity->paramId - TRACK_FADER_PARAMS;
			if (gInfo->isLinked(faderIndex)) {
				float v = paramQuantity->getScaledValue();
				float offsetY = handle->box.size.y / 2.0f;
				float ypos = math::rescale(v, 0.f, 1.f, minHandlePos.y, maxHandlePos.y) + offsetY;
				
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg, 0, ypos);
				nvgLineTo(args.vg, box.size.x, ypos);
				nvgClosePath(args.vg);
				nvgStrokeColor(args.vg, SCHEME_RED);
				nvgStrokeWidth(args.vg, mm2px(0.4f));
				nvgStroke(args.vg);
			}
		}
	}
};



	// Expander
	float rightMessages[2][MFA_NUM_VALUES] = {};// messages from aux-expander (first index is page), see enum called MotherFromAuxIds in Mixer.hpp

	// Constants
	int numChannels16 = 16;// avoids warning that happens when hardcode 16 (static const or directly use 16 in code below)

	// Need to save, no reset
	int panelTheme;
	
	// Need to save, with reset
	alignas(4) char trackLabels[4 * (16 + 4) + 1];// 4 chars per label, 16 tracks and 4 groups means 20 labels, null terminate the end the whole array only
	GlobalInfo gInfo;
	MixerTrack tracks[16];
	MixerGroup groups[4];
	MixerAux aux[4];
	MixerMaster master;
	
	// No need to save, with reset
	int updateTrackLabelRequest;// 0 when nothing to do, 1 for read names in widget
	int32_t trackMoveInAuxRequest;// 0 when nothing to do, {dest,src} packed when a move is requested
	float values20[20];
	dsp::SlewLimiter muteTrackWhenSoloAuxRetSlewer;

	// No need to save, no reset
	RefreshCounter refresh;	
	bool auxExpanderPresent = false;// can't be local to process() since widget must know in order to properly draw border
	float trackTaps[16 * 2 * 4];// room for 4 taps for each of the 16 stereo tracks. Trk0-tap0, Trk1-tap0 ... Trk15-tap0,  Trk0-tap1
	float trackInsertOuts[16 * 2];// room for 16 stereo track insert outs
	float groupTaps[4 * 2 * 4];// room for 4 taps for each of the 4 stereo groups
	float auxTaps[4 * 2 * 4];// room for 4 taps for each of the 4 stereo aux
	float *auxSends;// index into correct page of messages from expander (avoid having separate buffers)
	float *auxReturns;// index into correct page of messages from expander (avoid having separate buffers)
	float *auxRetFadePan;// index into correct page of messages from expander (avoid having separate buffers)
	uint32_t muteAuxSendWhenReturnGrouped;// { ... g2-B, g2-A, g1-D, g1-C, g1-B, g1-A}
	PackedBytes4 directOutsModeLocalAux;
	PackedBytes4 stereoPanModeLocalAux;
	TriggerRiseFall muteSoloCvTriggers[43];// 16 trk mute, 16 trk solo, 4 grp mute, 4 grp solo, 3 mast (mute, dim, mono)
	// std::string busId;
	
		
	MixMaster() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);		
		
		rightExpander.producerMessage = rightMessages[0];
		rightExpander.consumerMessage = rightMessages[1];

		char strBuf[32];
		// Track
		float maxTGFader = std::pow(GlobalConst::trkAndGrpFaderMaxLinearGain, 1.0f / GlobalConst::trkAndGrpFaderScalingExponent);
		for (int i = 0; i < 16; i++) {
			// Pan
			snprintf(strBuf, 32, "Track #%i pan", i + 1);
			configParam(TRACK_PAN_PARAMS + i, 0.0f, 1.0f, 0.5f, strBuf, "%", 0.0f, 200.0f, -100.0f);
			// Fader
			snprintf(strBuf, 32, "Track #%i level", i + 1);
			configParam(TRACK_FADER_PARAMS + i, 0.0f, maxTGFader, 1.0f, strBuf, " dB", -10, 20.0f * GlobalConst::trkAndGrpFaderScalingExponent);
			// Mute
			snprintf(strBuf, 32, "Track #%i mute", i + 1);
			configParam(TRACK_MUTE_PARAMS + i, 0.0f, 1.0f, 0.0f, strBuf);
			// Solo
			snprintf(strBuf, 32, "Track #%i solo", i + 1);
			configParam(TRACK_SOLO_PARAMS + i, 0.0f, 1.0f, 0.0f, strBuf);
			// Group select
			snprintf(strBuf, 32, "Track #%i group", i + 1);
			configParam(GROUP_SELECT_PARAMS + i, 0.0f, 4.0f, 0.0f, strBuf);
		}
		// Group
		for (int i = 0; i < 4; i++) {
			// Pan
			snprintf(strBuf, 32, "Group #%i pan", i + 1);
			configParam(GROUP_PAN_PARAMS + i, 0.0f, 1.0f, 0.5f, strBuf, "%", 0.0f, 200.0f, -100.0f);
			// Fader
			snprintf(strBuf, 32, "Group #%i level", i + 1);
			configParam(GROUP_FADER_PARAMS + i, 0.0f, maxTGFader, 1.0f, strBuf, " dB", -10, 20.0f * GlobalConst::trkAndGrpFaderScalingExponent);
			// Mute
			snprintf(strBuf, 32, "Group #%i mute", i + 1);
			configParam(GROUP_MUTE_PARAMS + i, 0.0f, 1.0f, 0.0f, strBuf);
			// Solo
			snprintf(strBuf, 32, "Group #%i solo", i + 1);
			configParam(GROUP_SOLO_PARAMS + i, 0.0f, 1.0f, 0.0f, strBuf);
		}
		float maxMFader = std::pow(GlobalConst::masterFaderMaxLinearGain, 1.0f / GlobalConst::masterFaderScalingExponent);
		configParam(MAIN_FADER_PARAM, 0.0f, maxMFader, 1.0f, "Master level", " dB", -10, 20.0f * GlobalConst::masterFaderScalingExponent);
		// Mute
		configParam(MAIN_MUTE_PARAM, 0.0f, 1.0f, 0.0f, "Master mute");
		// Dim
		configParam(MAIN_DIM_PARAM, 0.0f, 1.0f, 0.0f, "Master dim");
		// Mono
		configParam(MAIN_MONO_PARAM, 0.0f, 1.0f, 0.0f, "Master mono");
		

		gInfo.construct(&params[0], values20);
		for (int i = 0; i < 16; i++) {
			tracks[i].construct(i, &gInfo, &inputs[0], &params[0], &(trackLabels[4 * i]), &trackTaps[i << 1], groupTaps, &trackInsertOuts[i << 1]);
		}
		for (int i = 0; i < 4; i++) {
			groups[i].construct(i, &gInfo, &inputs[0], &params[0], &(trackLabels[4 * (16 + i)]), &groupTaps[i << 1]);
			aux[i].construct(i, &gInfo, &inputs[0], values20, &auxTaps[i << 1], &stereoPanModeLocalAux.cc4[i]);
		}
		master.construct(&gInfo, &params[0], &inputs[0]);
		muteTrackWhenSoloAuxRetSlewer.setRiseFall(GlobalConst::antipopSlewFast, GlobalConst::antipopSlewFast); // slew rate is in input-units per second 
		onReset();

		panelTheme = 0;//(loadDarkAsDefault() ? 1 : 0);
		
		// busId = messages->registerMember();
	}
  
	// ~MixMaster() {
		// messages->deregisterMember(busId);
	// }

	
	void onReset() override {
		snprintf(trackLabels, 4 * (16 + 4) + 1, "-01--02--03--04--05--06--07--08--09--10--11--12--13--14--15--16-GRP1GRP2GRP3GRP4");	
		gInfo.onReset();
		for (int i = 0; i < 16; i++) {
			tracks[i].onReset();
		}
		for (int i = 0; i < 4; i++) {
			groups[i].onReset();
			aux[i].onReset();
		}
		master.onReset();
		resetNonJson(false);
	}
	void resetNonJson(bool recurseNonJson) {
		updateTrackLabelRequest = 1;
		trackMoveInAuxRequest = 0;
		if (recurseNonJson) {
			gInfo.resetNonJson();
			for (int i = 0; i < 16; i++) {
				tracks[i].resetNonJson();
			}
			for (int i = 0; i < 4; i++) {
				groups[i].resetNonJson();
				aux[i].resetNonJson();
			}
			master.resetNonJson();
		}
		for (int i = 0; i < 20; i++) {
			values20[i] = 0.0f;
		}
		muteTrackWhenSoloAuxRetSlewer.reset();
	}


	void onRandomize() override {
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// trackLabels
		json_object_set_new(rootJ, "trackLabels", json_string(trackLabels));
		
		// gInfo
		gInfo.dataToJson(rootJ);

		// tracks
		for (int i = 0; i < 16; i++) {
			tracks[i].dataToJson(rootJ);
		}
		// groups/aux
		for (int i = 0; i < 4; i++) {
			groups[i].dataToJson(rootJ);
			aux[i].dataToJson(rootJ);
		}
		// master
		master.dataToJson(rootJ);
		
		return rootJ;
	}


	void dataFromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// trackLabels
		json_t *textJ = json_object_get(rootJ, "trackLabels");
		if (textJ)
			snprintf(trackLabels, 4 * (16 + 4) + 1, "%s", json_string_value(textJ));
		
		// gInfo
		gInfo.dataFromJson(rootJ);

		// tracks
		for (int i = 0; i < 16; i++) {
			tracks[i].dataFromJson(rootJ);
		}
		// groups/aux
		for (int i = 0; i < 4; i++) {
			groups[i].dataFromJson(rootJ);
			aux[i].dataFromJson(rootJ);
		}
		// master
		master.dataFromJson(rootJ);
		
		resetNonJson(true);
	}


	void onSampleRateChange() override {
		gInfo.sampleTime = APP->engine->getSampleTime();
		for (int trk = 0; trk < 16; trk++) {
			tracks[trk].onSampleRateChange();
		}
		master.onSampleRateChange();
	}
	

	void process(const ProcessArgs &args) override {
		
		auxExpanderPresent = (rightExpander.module && rightExpander.module->model == modelAuxExpander);
		
		
		//********** Inputs **********
		
		// From Aux-Expander
		if (auxExpanderPresent) {
			float *messagesFromExpander = (float*)rightExpander.consumerMessage;// could be invalid pointer when !expanderPresent, so read it only when expanderPresent
			
			auxReturns = &messagesFromExpander[MFA_AUX_RETURNS]; // contains 8 values of the returns from the aux panel
			auxRetFadePan = &messagesFromExpander[MFA_AUX_RET_FADER]; // contains 8 values of the return faders and pan knobs
						
			int value20i = clamp((int)(messagesFromExpander[MFA_VALUE20_INDEX]), 0, 19);// mute, solo, group, fadeRate, fadeProfile for aux returns
			values20[value20i] = messagesFromExpander[MFA_VALUE20];
			
			// Direct outs and Stereo pan for each aux (could be SLOW but not worth setting up for just two floats)
			memcpy(&directOutsModeLocalAux, &messagesFromExpander[MFA_AUX_DIR_OUTS], 4);
			memcpy(&stereoPanModeLocalAux, &messagesFromExpander[MFA_AUX_STEREO_PANS], 4);
		}
		else {
			muteTrackWhenSoloAuxRetSlewer.reset();
		}

		if (refresh.processInputs()) {
			int trackToProcess = refresh.refreshCounter >> 4;// Corresponds to 172Hz refreshing of each track, at 44.1 kHz
			
			// Tracks
			gInfo.updateSoloBit(trackToProcess);
			tracks[trackToProcess].updateSlowValues();// a track is updated once every 16 passes in input proceesing
			// Groups/Aux
			if ( (trackToProcess & 0x3) == 0) {// a group is updated once every 16 passes in input proceesing
				gInfo.updateSoloBit(16 + (trackToProcess >> 2));
				groups[trackToProcess >> 2].updateSlowValues();
				if (auxExpanderPresent) {
					gInfo.updateReturnSoloBits();
					aux[trackToProcess >> 2].updateSlowValues();
				}
			}
			// Master
			if ((trackToProcess & 0x3) == 1) {// master and groupUsage updated once every 4 passes in input proceesing
				master.updateSlowValues();
				gInfo.updateGroupUsage();
			}
			
			processMuteSoloCvTriggers();
			
			// Message bus test
			// Message<Payload> *message = messages->receive("1");	
			// if (message != NULL) {
				// params[TRACK_PAN_PARAMS + 0].setValue(message->value.values[0]);
				// delete message;
			// }
		}// userInputs refresh
		
		
		// ecoCode: cycles from 0 to 3 in eco mode, stuck at 0 when full power mode
		uint16_t ecoCode = (refresh.refreshCounter & 0x3 & gInfo.ecoMode);
		bool ecoStagger4 = (gInfo.ecoMode == 0 || ecoCode == 3);
				
	
		//********** Outputs **********

		float mix[2] = {0.0f};// room for main (groups will automatically be stored into groups taps 0 by tracks)
		for (int i = 0; i < 8; i++) {
			groupTaps[i] = 0.0f;
		}
		
		// GlobalInfo
		gInfo.process();
		
		// Tracks
		for (int trk = 0; trk < 16; trk++) {
			tracks[trk].process(mix, ecoCode == 0);// stagger 1
		}
		// Aux return when group
		if (auxExpanderPresent) {
			muteAuxSendWhenReturnGrouped = 0;
			bool ecoStagger3 = (gInfo.ecoMode == 0 || ecoCode == 2);
			for (int auxi = 0; auxi < 4; auxi++) {
				int auxGroup = aux[auxi].getAuxGroup();
				if (auxGroup != 0) {
					auxGroup--;
					aux[auxi].process(&groupTaps[auxGroup << 1], &auxRetFadePan[auxi], ecoStagger3);// stagger 3
					if (gInfo.groupedAuxReturnFeedbackProtection != 0) {
						muteAuxSendWhenReturnGrouped |= (0x1 << ((auxGroup << 2) + auxi));
					}
				}
			}
		}
		
		// Groups (at this point, all groups's tap0 are setup and ready)
		bool ecoStagger2 = (gInfo.ecoMode == 0 || ecoCode == 1);
		for (int i = 0; i < 4; i++) {
			groups[i].process(mix, ecoStagger2);// stagger 2
		}
		
		// Aux
		if (auxExpanderPresent) {
			memcpy(auxTaps, auxReturns, 8 * 4);		
			
			// Mute tracks/groups when soloing aux returns
			float newMuteTrackWhenSoloAuxRet = (gInfo.returnSoloBitMask != 0 && gInfo.auxReturnsSolosMuteDry != 0) ? 0.0f : 1.0f;
			muteTrackWhenSoloAuxRetSlewer.process(args.sampleTime, newMuteTrackWhenSoloAuxRet);
			mix[0] *= muteTrackWhenSoloAuxRetSlewer.out;
			mix[1] *= muteTrackWhenSoloAuxRetSlewer.out;
			
			// Aux returns when no group
			bool ecoStagger3 = (gInfo.ecoMode == 0 || ecoCode == 2);
			for (int auxi = 0; auxi < 4; auxi++) {
				if (aux[auxi].getAuxGroup() == 0) {
					aux[auxi].process(mix, &auxRetFadePan[auxi], ecoStagger3);// stagger 3
				}
			}
		}
		// Master
		master.process(mix, ecoStagger4);// stagger 4
		
		// Set master outputs
		outputs[MAIN_OUTPUTS + 0].setVoltage(mix[0]);
		outputs[MAIN_OUTPUTS + 1].setVoltage(mix[1]);
		
		// Direct outs (uses trackTaps, groupTaps and auxTaps)
		SetDirectTrackOuts(0);// 1-8
		SetDirectTrackOuts(8);// 9-16
		SetDirectGroupAuxOuts();
				
		// Insert outs (uses trackInsertOuts and group tap0 and aux tap0)
		SetInsertTrackOuts(0);// 1-8
		SetInsertTrackOuts(8);// 9-16
		SetInsertGroupAuxOuts();	

		setFadeCvOuts();


		//********** Lights **********
		
		bool slowExpander = false;		
		if (refresh.processLights()) {
			slowExpander = true;
			for (int i = 0; i < 16; i++) {
				lights[TRACK_HPF_LIGHTS + i].setBrightness(tracks[i].getHPFCutoffFreq() >= MixerTrack::minHPFCutoffFreq ? 1.0f : 0.0f);
				lights[TRACK_LPF_LIGHTS + i].setBrightness(tracks[i].getLPFCutoffFreq() <= MixerTrack::maxLPFCutoffFreq ? 1.0f : 0.0f);
			}
		}
		
		
		//********** Expander **********
		
		// To Aux-Expander
		if (auxExpanderPresent) {
			float *messageToExpander = (float*)(rightExpander.module->leftExpander.producerMessage);
			
			// Slow
			
			uint32_t* updateSlow = (uint32_t*)(&messageToExpander[AFM_UPDATE_SLOW]);
			*updateSlow = 0;
			if (slowExpander) {
				// Track names
				*updateSlow = 1;
				memcpy(&messageToExpander[AFM_TRACK_GROUP_NAMES], trackLabels, 4 * (16 + 4));
				// Panel theme
				int32_t tmp = panelTheme;
				memcpy(&messageToExpander[AFM_PANEL_THEME], &tmp, 4);
				// Color theme
				memcpy(&messageToExpander[AFM_COLOR_AND_CLOAK], &gInfo.colorAndCloak.cc1, 4);
				// Direct outs mode global and Stereo pan mode global
				PackedBytes4 directAndPan;
				directAndPan.cc4[0] = gInfo.directOutsMode;
				directAndPan.cc4[1] = gInfo.panLawStereo;
				memcpy(&messageToExpander[AFM_DIRECT_AND_PAN_MODES], &directAndPan.cc1, 4);
				// Track move
				memcpy(&messageToExpander[AFM_TRACK_MOVE], &trackMoveInAuxRequest, 4);
				trackMoveInAuxRequest = 0;
				// Aux send mute when grouped return lights
				messageToExpander[AFM_AUXSENDMUTE_GROUPED_RETURN] = (float)(muteAuxSendWhenReturnGrouped);
				// Display colors (when per track)
				PackedBytes4 tmpDispCols[5];
				if (gInfo.colorAndCloak.cc4[dispColor] < 7) {
					for (int i = 0; i < 5; i++) {
						tmpDispCols[i].cc1 = 0;
					}
				}
				else {
					for (int i = 0; i < 4; i++) {
						for (int j = 0; j < 4; j++) {
							tmpDispCols[i].cc4[j] = tracks[ (i << 2) + j ].dispColorLocal;
						}	
					}
					for (int j = 0; j < 4; j++) {
						tmpDispCols[4].cc4[j] = groups[ j ].dispColorLocal;
					}
				}	
				memcpy(&messageToExpander[AFM_TRK_DISP_COL], tmpDispCols, 5 * 4);
				// Eco mode
				tmp = gInfo.ecoMode;
				memcpy(&messageToExpander[AFM_ECO_MODE], &tmp, 4);
				// auxFadeGains
				for (int auxi = 0; auxi < 4; auxi++) {
					messageToExpander[AFM_FADE_GAINS + auxi] = aux[auxi].fadeGain;
				}
				// momentaryCvButtons
				tmp = gInfo.momentaryCvButtons;
				memcpy(&messageToExpander[AFM_MOMENTARY_CVBUTTONS], &tmp, 4);			
			}
			
			// Fast
			
			// 16+4 stereo signals to be used to make sends in aux expander
			writeAuxSends(&messageToExpander[AFM_AUX_SENDS]);						
			// Aux VUs
			for (int i = 0; i < 4; i++) {
				// send tap 4 of the aux return signal flows
				messageToExpander[AFM_AUX_VUS + (i << 1) + 0] = auxTaps[24 + (i << 1) + 0];
				messageToExpander[AFM_AUX_VUS + (i << 1) + 1] = auxTaps[24 + (i << 1) + 1];
			}
			
			rightExpander.module->leftExpander.messageFlipRequested = true;
		}// if (auxExpanderPresent)

		
	}// process()
	
	
	void setFadeCvOuts() {
		if (outputs[FADE_CV_OUTPUT].isConnected()) {
			outputs[FADE_CV_OUTPUT].setChannels(numChannels16);
			for (int trk = 0; trk < 16; trk++) {
				float outV = tracks[trk].fadeGain * 10.0f;
				if (gInfo.fadeCvOutsWithVolCv) {
					outV *= tracks[trk].volCv;
				}
				outputs[FADE_CV_OUTPUT].setVoltage(outV, trk);
			}
		}
	}
	
	void writeAuxSends(float* auxSends) {
		// Aux sends (send track and group audio (16+4 stereo signals) to auxspander
		// auxSends[] has room for 16+4 stereo values of the sends to the aux panel (Trk1L, Trk1R, Trk2L, Trk2R ... Trk16L, Trk16R, Grp1L, Grp1R ... Grp4L, Grp4R)
		// populate auxSends[0..39]: Take the trackTaps/groupTaps indicated by the Aux sends mode (with per-track option)
		
		// tracks
		if ( gInfo.auxSendsMode < 4 && (gInfo.groupsControlTrackSendLevels == 0 || gInfo.groupUsage[4] == 0) ) {
			memcpy(auxSends, &trackTaps[gInfo.auxSendsMode << 5], 32 * 4);
		}
		else {
			int trkGroup;
			for (int trk = 0; trk < 16; trk++) {
				// tracks should have aux sends even when grouped
				int tapIndex = (tracks[trk].auxSendsMode << 5);
				float trackL = trackTaps[tapIndex + (trk << 1) + 0];
				float trackR = trackTaps[tapIndex + (trk << 1) + 1];
				if (gInfo.groupsControlTrackSendLevels != 0 && (trkGroup = (int)(tracks[trk].paGroup->getValue() + 0.5f)) != 0) {
					trkGroup--;
					simd::float_4 sigs(trackL, trackR, trackR, trackL);
					sigs = sigs * groups[trkGroup].gainMatrixSlewers.out;
					trackL = sigs[0] + sigs[2];
					trackR = sigs[1] + sigs[3];
					trackL *= groups[trkGroup].muteSoloGainSlewer.out;
					trackR *= groups[trkGroup].muteSoloGainSlewer.out;
				}					
				auxSends[(trk << 1) + 0] = trackL;
				auxSends[(trk << 1) + 1] = trackR;
			}
		}
		
		// groups
		if (gInfo.auxSendsMode < 4) {
			memcpy(&auxSends[32], &groupTaps[gInfo.auxSendsMode << 3], 8 * 4);
		}
		else {
			for (int grp = 0; grp < 4; grp++) {
				int tapIndex = (groups[grp].auxSendsMode << 3);
				auxSends[(grp << 1) + 32] = groupTaps[tapIndex + (grp << 1) + 0];
				auxSends[(grp << 1) + 33] = groupTaps[tapIndex + (grp << 1) + 1];
			}
		}

	}
	
	
	void SetDirectTrackOuts(const int base) {// base is 0 or 8
		int outi = base >> 3;
		if (outputs[DIRECT_OUTPUTS + outi].isConnected()) {
			outputs[DIRECT_OUTPUTS + outi].setChannels(numChannels16);

			int tapIndex = gInfo.directOutsMode;		
			if (tapIndex < 4) {// global direct outs
				if (auxExpanderPresent && gInfo.auxReturnsSolosMuteDry != 0 && tapIndex == 3) {
					for (unsigned int i = 0; i < 8; i++) {
						int offset = (tapIndex << 5) + ((base + i) << 1);
						outputs[DIRECT_OUTPUTS + outi].setVoltage(trackTaps[offset + 0] * muteTrackWhenSoloAuxRetSlewer.out, 2 * i);
						outputs[DIRECT_OUTPUTS + outi].setVoltage(trackTaps[offset + 1] * muteTrackWhenSoloAuxRetSlewer.out, 2 * i + 1);
					}
				}
				else {
					memcpy(outputs[DIRECT_OUTPUTS + outi].getVoltages(), &trackTaps[(tapIndex << 5) + (base << 1)], 4 * 16);
				}
			}
			else {// per track direct outs
				for (unsigned int i = 0; i < 8; i++) {
					tapIndex = tracks[base + i].directOutsMode;
					int offset = (tapIndex << 5) + ((base + i) << 1);
					if (auxExpanderPresent && gInfo.auxReturnsSolosMuteDry != 0 && tapIndex == 3) {
						outputs[DIRECT_OUTPUTS + outi].setVoltage(trackTaps[offset + 0] * muteTrackWhenSoloAuxRetSlewer.out, 2 * i);
						outputs[DIRECT_OUTPUTS + outi].setVoltage(trackTaps[offset + 1] * muteTrackWhenSoloAuxRetSlewer.out, 2 * i + 1);
					}
					else {
						outputs[DIRECT_OUTPUTS + outi].setVoltage(trackTaps[offset + 0], 2 * i);
						outputs[DIRECT_OUTPUTS + outi].setVoltage(trackTaps[offset + 1], 2 * i + 1);
					}
				}
			}
		}
	}
	
	
	void SetDirectGroupAuxOuts() {
		if (outputs[DIRECT_OUTPUTS + 2].isConnected()) {
			if (auxExpanderPresent) {
				outputs[DIRECT_OUTPUTS + 2].setChannels(numChannels16);
			}
			else {
				outputs[DIRECT_OUTPUTS + 2].setChannels(8);
			}

			// Groups
			int tapIndex = gInfo.directOutsMode;			
			if (gInfo.directOutsMode < 4) {// global direct outs
				if (auxExpanderPresent && gInfo.auxReturnsSolosMuteDry != 0 && tapIndex == 3) {
					for (unsigned int i = 0; i < 4; i++) {
						int offset = (tapIndex << 3) + (i << 1);
						outputs[DIRECT_OUTPUTS + 2].setVoltage(groupTaps[offset + 0] * muteTrackWhenSoloAuxRetSlewer.out, 2 * i);
						outputs[DIRECT_OUTPUTS + 2].setVoltage(groupTaps[offset + 1] * muteTrackWhenSoloAuxRetSlewer.out, 2 * i + 1);
					}
				}
				else {
					memcpy(outputs[DIRECT_OUTPUTS + 2].getVoltages(), &groupTaps[(tapIndex << 3)], 4 * 8);
				}
			}
			else {// per group direct outs
				for (unsigned int i = 0; i < 4; i++) {
					tapIndex = groups[i].directOutsMode;
					int offset = (tapIndex << 3) + (i << 1);
					if (auxExpanderPresent && gInfo.auxReturnsSolosMuteDry != 0 && tapIndex == 3) {
						outputs[DIRECT_OUTPUTS + 2].setVoltage(groupTaps[offset + 0] * muteTrackWhenSoloAuxRetSlewer.out, 2 * i);
						outputs[DIRECT_OUTPUTS + 2].setVoltage(groupTaps[offset + 1] * muteTrackWhenSoloAuxRetSlewer.out, 2 * i + 1);
					}
					else {
						outputs[DIRECT_OUTPUTS + 2].setVoltage(groupTaps[offset + 0], 2 * i);
						outputs[DIRECT_OUTPUTS + 2].setVoltage(groupTaps[offset + 1], 2 * i + 1);
					}
				}
			}
			
			// Aux
			// this uses one of the taps in the aux return signal flow (same signal flow as a group), and choice of tap is same as other diretct outs
			if (auxExpanderPresent) {
				if (gInfo.directOutsMode < 4) {// global direct outs
					int tapIndex = gInfo.directOutsMode;
					memcpy(outputs[DIRECT_OUTPUTS + 2].getVoltages(8), &auxTaps[(tapIndex << 3)], 4 * 8);
				}
				else {// per aux direct outs
					for (unsigned int i = 0; i < 4; i++) {
						int tapIndex = directOutsModeLocalAux.cc4[i];
						int offset = (tapIndex << 3) + (i << 1);
						outputs[DIRECT_OUTPUTS + 2].setVoltage(auxTaps[offset + 0], 8 + 2 * i);
						outputs[DIRECT_OUTPUTS + 2].setVoltage(auxTaps[offset + 1], 8 + 2 * i + 1);
					}
				}		
			}
		}
	}
	
	
	void SetInsertTrackOuts(const int base) {// base is 0 or 8
		int outi = base >> 3;
		if (outputs[INSERT_TRACK_OUTPUTS + outi].isConnected()) {
			outputs[INSERT_TRACK_OUTPUTS + outi].setChannels(numChannels16);
			memcpy(outputs[INSERT_TRACK_OUTPUTS + outi].getVoltages(), &trackInsertOuts[(base << 1)], 4 * 16);
		}
	}


	void SetInsertGroupAuxOuts() {
		if (outputs[INSERT_GRP_AUX_OUTPUT].isConnected()) {
			outputs[INSERT_GRP_AUX_OUTPUT].setChannels(auxExpanderPresent ? numChannels16 : 8);
			memcpy(outputs[INSERT_GRP_AUX_OUTPUT].getVoltages(), groupTaps, 4 * 8);// insert out for groups is directly tap0
			if (auxExpanderPresent) {
				memcpy(outputs[INSERT_GRP_AUX_OUTPUT].getVoltages(8), auxTaps, 4 * 8);// insert out for aux is directly tap0
			}
		}
	}


	void processMuteSoloCvTriggers() {
		int state;
		if (inputs[TRACK_MUTE_INPUT].isConnected()) {
			for (int trk = 0; trk < 16; trk++) {
				// track mutes
				state = muteSoloCvTriggers[trk].process(inputs[TRACK_MUTE_INPUT].getVoltage(trk));
				if (state != 0) {
					if (gInfo.momentaryCvButtons) {
						if (state == 1) {
							float newParam = 1.0f - params[TRACK_MUTE_PARAMS + trk].getValue();// toggle
							params[TRACK_MUTE_PARAMS + trk].setValue(newParam);
						};
					}
					else {
						params[TRACK_MUTE_PARAMS + trk].setValue(state == 1 ? 1.0f : 0.0f);// gate level
					}
				}
			}
		}
		if (inputs[TRACK_SOLO_INPUT].isConnected()) {
			for (int trk = 0; trk < 16; trk++) {
				// track solos
				state = muteSoloCvTriggers[trk + 16].process(inputs[TRACK_SOLO_INPUT].getVoltage(trk));
				if (state != 0) {
					if (gInfo.momentaryCvButtons) {
						if (state == 1) {
							float newParam = 1.0f - params[TRACK_SOLO_PARAMS + trk].getValue();// toggle
							params[TRACK_SOLO_PARAMS + trk].setValue(newParam);
						};
					}
					else {
						params[TRACK_SOLO_PARAMS + trk].setValue(state == 1 ? 1.0f : 0.0f);// gate level
					}
				}
			}
		}
		if (inputs[GRPM_MUTESOLO_INPUT].isConnected()) {
			for (int grp = 0; grp < 4; grp++) {
				// group mutes
				state = muteSoloCvTriggers[grp + 32].process(inputs[GRPM_MUTESOLO_INPUT].getVoltage(grp));
				if (state != 0) {
					if (gInfo.momentaryCvButtons) {
						if (state == 1) {
							float newParam = 1.0f - params[GROUP_MUTE_PARAMS + grp].getValue();// toggle
							params[GROUP_MUTE_PARAMS + grp].setValue(newParam);
						};
					}
					else {
						params[GROUP_MUTE_PARAMS + grp].setValue(state == 1 ? 1.0f : 0.0f);// gate level
					}
				}
				// group solos
				state = muteSoloCvTriggers[grp + 32 + 4].process(inputs[GRPM_MUTESOLO_INPUT].getVoltage(grp + 4));
				if (state != 0) {
					if (gInfo.momentaryCvButtons) {
						if (state == 1) {
							float newParam = 1.0f - params[GROUP_SOLO_PARAMS + grp].getValue();// toggle
							params[GROUP_SOLO_PARAMS + grp].setValue(newParam);
						};
					}
					else {
						params[GROUP_SOLO_PARAMS + grp].setValue(state == 1 ? 1.0f : 0.0f);// gate level
					}
				}
			}	
			// master mute
			state = muteSoloCvTriggers[40].process(inputs[GRPM_MUTESOLO_INPUT].getVoltage(8));
			if (state != 0) {
				if (gInfo.momentaryCvButtons) {
					if (state == 1) {
						float newParam = 1.0f - params[MAIN_MUTE_PARAM].getValue();// toggle
						params[MAIN_MUTE_PARAM].setValue(newParam);
					};
				}
				else {
					params[MAIN_MUTE_PARAM].setValue(state == 1 ? 1.0f : 0.0f);// gate level
				}
			}
			// master dim
			state = muteSoloCvTriggers[40 + 1].process(inputs[GRPM_MUTESOLO_INPUT].getVoltage(9));
			if (state != 0) {
				if (gInfo.momentaryCvButtons) {
					if (state == 1) {
						float newParam = 1.0f - params[MAIN_DIM_PARAM].getValue();// toggle
						params[MAIN_DIM_PARAM].setValue(newParam);
					};
				}
				else {
					params[MAIN_DIM_PARAM].setValue(state == 1 ? 1.0f : 0.0f);// gate level
				}
			}
			// master mono
			state = muteSoloCvTriggers[40 + 2].process(inputs[GRPM_MUTESOLO_INPUT].getVoltage(10));
			if (state != 0) {
				if (gInfo.momentaryCvButtons) {
					if (state == 1) {
						float newParam = 1.0f - params[MAIN_MONO_PARAM].getValue();// toggle
						params[MAIN_MONO_PARAM].setValue(newParam);
					};
				}
				else {
					params[MAIN_MONO_PARAM].setValue(state == 1 ? 1.0f : 0.0f);// gate level
				}
			}
		}
	}

};



struct MixMasterWidget : ModuleWidget {
	static const int N_TRK = 16;

	MixMaster<N_TRK>::MasterDisplay* masterDisplay;
	MixMaster<N_TRK>::TrackDisplay* trackDisplays[16];
	MixMaster<N_TRK>::GroupDisplay* groupDisplays[4];
	PortWidget* inputWidgets[16 * 4];// Left, Right, Volume, Pan
	PanelBorder* panelBorder;
	bool oldAuxExpanderPresent = false;
	time_t oldTime = 0;


	// Module's context menu
	// --------------------

	void appendContextMenu(Menu *menu) override {
		MixMaster<N_TRK> *module = dynamic_cast<MixMaster<N_TRK>*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator());
		
		MenuLabel *settingsALabel = new MenuLabel();
		settingsALabel->text = "Settings (audio)";
		menu->addChild(settingsALabel);
		
		FilterPosItem *filterPosItem = createMenuItem<FilterPosItem>("Filters", RIGHT_ARROW);
		filterPosItem->filterPosSrc = &(module->gInfo.filterPos);
		filterPosItem->isGlobal = true;
		menu->addChild(filterPosItem);
		
		PanLawMonoItem *panLawMonoItem = createMenuItem<PanLawMonoItem>("Mono pan law", RIGHT_ARROW);
		panLawMonoItem->panLawMonoSrc = &(module->gInfo.panLawMono);
		menu->addChild(panLawMonoItem);
		
		PanLawStereoItem *panLawStereoItem = createMenuItem<PanLawStereoItem>("Stereo pan mode", RIGHT_ARROW);
		panLawStereoItem->panLawStereoSrc = &(module->gInfo.panLawStereo);
		panLawStereoItem->isGlobal = true;
		menu->addChild(panLawStereoItem);
		
		ChainItem *chainItem = createMenuItem<ChainItem>("Chain input", RIGHT_ARROW);
		chainItem->chainModeSrc = &(module->gInfo.chainMode);
		menu->addChild(chainItem);
		
		TapModeItem *directOutsItem = createMenuItem<TapModeItem>("Direct outs", RIGHT_ARROW);
		directOutsItem->tapModePtr = &(module->gInfo.directOutsMode);
		directOutsItem->isGlobal = true;
		menu->addChild(directOutsItem);
		
		MomentaryCvItem *momentItem = createMenuItem<MomentaryCvItem>("Mute/Solo CV", RIGHT_ARROW);
		momentItem->momentaryCvButtonsSrc = &(module->gInfo.momentaryCvButtons);
		menu->addChild(momentItem);

		FadeSettingsItem *fadItem = createMenuItem<FadeSettingsItem>("Fades", RIGHT_ARROW);
		fadItem->symmetricalFadeSrc = &(module->gInfo.symmetricalFade);
		fadItem->fadeCvOutsWithVolCvSrc = &(module->gInfo.fadeCvOutsWithVolCv);
		menu->addChild(fadItem);
		
		EcoItem *eco0Item = createMenuItem<EcoItem>("Eco mode", CHECKMARK(module->gInfo.ecoMode));
		eco0Item->ecoModeSrc = &(module->gInfo.ecoMode);
		menu->addChild(eco0Item);
		
		if (module->auxExpanderPresent) {
			menu->addChild(new MenuSeparator());

			MenuLabel *settingsVLabel = new MenuLabel();
			settingsVLabel->text = "AuxSpander";
			menu->addChild(settingsVLabel);
			
			TapModePlusItem *auxSendsItem = createMenuItem<TapModePlusItem>("Aux sends", RIGHT_ARROW);
			auxSendsItem->tapModePtr = &(module->gInfo.auxSendsMode);
			auxSendsItem->isGlobal = true;
			auxSendsItem->groupsControlTrackSendLevelsSrc = &(module->gInfo.groupsControlTrackSendLevels);
			menu->addChild(auxSendsItem);
			
			AuxReturnItem *auxRetunsItem = createMenuItem<AuxReturnItem>("Aux returns", RIGHT_ARROW);
			auxRetunsItem->auxReturnsMutedWhenMainSoloPtr = &(module->gInfo.auxReturnsMutedWhenMainSolo);
			auxRetunsItem->auxReturnsSolosMuteDryPtr = &(module->gInfo.auxReturnsSolosMuteDry);
			menu->addChild(auxRetunsItem);
		
			AuxRetFbProtItem *fbpItem = createMenuItem<AuxRetFbProtItem>("Routing returns to groups", RIGHT_ARROW);
			fbpItem->groupedAuxReturnFeedbackProtectionSrc = &(module->gInfo.groupedAuxReturnFeedbackProtection);
			menu->addChild(fbpItem);
		}
		
		
		menu->addChild(new MenuSeparator());
		
		MenuLabel *settingsVLabel = new MenuLabel();
		settingsVLabel->text = "Settings (visual)";
		menu->addChild(settingsVLabel);
		
		DispColorItem *dispColItem = createMenuItem<DispColorItem>("Display colour", RIGHT_ARROW);
		dispColItem->srcColor = &(module->gInfo.colorAndCloak.cc4[dispColor]);
		dispColItem->isGlobal = true;
		menu->addChild(dispColItem);
		
		VuColorItem *vuColItem = createMenuItem<VuColorItem>("VU colour", RIGHT_ARROW);
		vuColItem->srcColor = &(module->gInfo.colorAndCloak.cc4[vuColorGlobal]);
		vuColItem->isGlobal = true;
		menu->addChild(vuColItem);
		
		KnobArcShowItem *knobArcShowItem = createMenuItem<KnobArcShowItem>("Knob arcs", RIGHT_ARROW);
		knobArcShowItem->srcDetailsShow = &(module->gInfo.colorAndCloak.cc4[detailsShow]);
		menu->addChild(knobArcShowItem);
		
		CvPointerShowItem *cvPointerShowItem = createMenuItem<CvPointerShowItem>("Fader CV pointers", RIGHT_ARROW);
		cvPointerShowItem->srcDetailsShow = &(module->gInfo.colorAndCloak.cc4[detailsShow]);
		menu->addChild(cvPointerShowItem);
		
		CloakedModeItem *nightItem = createMenuItem<CloakedModeItem>("Cloaked mode", CHECKMARK(module->gInfo.colorAndCloak.cc4[cloakedMode]));
		nightItem->colorAndCloakSrc = &(module->gInfo.colorAndCloak);
		menu->addChild(nightItem);
	}

	// Module's widget
	// --------------------

	MixMasterWidget(MixMaster<N_TRK> *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/mixmaster.svg")));
		panelBorder = findBorder(panel);		
		
		// Inserts and CVs
		static const float xIns = 13.8;
		// Insert outputs
		addOutput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xIns, 12.8)), false, module, INSERT_TRACK_OUTPUTS + 0, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 1)), false, module, INSERT_TRACK_OUTPUTS + 1, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 2)), false, module, INSERT_GRP_AUX_OUTPUT, module ? &module->panelTheme : NULL));
		// Insert inputs
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 3)), true, module, INSERT_TRACK_INPUTS + 0, module ? &module->panelTheme : NULL));
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 4)), true, module, INSERT_TRACK_INPUTS + 1, module ? &module->panelTheme : NULL));
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 5)), true, module, INSERT_GRP_AUX_INPUT, module ? &module->panelTheme : NULL));
		// Insert inputs
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 7)), true, module, TRACK_MUTE_INPUT, module ? &module->panelTheme : NULL));
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 8)), true, module, TRACK_SOLO_INPUT, module ? &module->panelTheme : NULL));
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 9)), true, module, GRPM_MUTESOLO_INPUT, module ? &module->panelTheme : NULL));
		
		
		// Tracks
		static const float xTrck1 = 11.43 + 20.32;
		for (int i = 0; i < 16; i++) {
			// Labels
			addChild(trackDisplays[i] = createWidgetCentered<TrackDisplay>(mm2px(Vec(xTrck1 + 12.7 * i + 0.4, 4.7))));
			if (module) {
				trackDisplays[i]->colorAndCloak = &(module->gInfo.colorAndCloak);
				trackDisplays[i]->tracks = &(module->tracks[0]);
				trackDisplays[i]->trackNumSrc = i;
				trackDisplays[i]->updateTrackLabelRequestPtr = &(module->updateTrackLabelRequest);
				trackDisplays[i]->trackMoveInAuxRequestPtr = &(module->trackMoveInAuxRequest);
				trackDisplays[i]->inputWidgets = inputWidgets;
				trackDisplays[i]->auxExpanderPresentPtr = &(module->auxExpanderPresent);
				trackDisplays[i]->dispColorLocal = &(module->tracks[i].dispColorLocal);
			}
			// HPF lights
			addChild(createLightCentered<TinyLight<GreenLight>>(mm2px(Vec(xTrck1 - 4.17 + 12.7 * i, 8.3)), module, TRACK_HPF_LIGHTS + i));	
			// LPF lights
			addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(xTrck1 + 4.17 + 12.7 * i, 8.3)), module, TRACK_LPF_LIGHTS + i));	
			// Left inputs
			addInput(inputWidgets[i + 0] = createDynamicPortCentered<DynPort>(mm2px(Vec(xTrck1 + 12.7 * i, 12.8)), true, module, TRACK_SIGNAL_INPUTS + 2 * i + 0, module ? &module->panelTheme : NULL));			
			// Right inputs
			addInput(inputWidgets[i + 16] = createDynamicPortCentered<DynPort>(mm2px(Vec(xTrck1 + 12.7 * i, 21.8)), true, module, TRACK_SIGNAL_INPUTS + 2 * i + 1, module ? &module->panelTheme : NULL));	
			// Volume inputs
			addInput(inputWidgets[i + 16 * 2] = createDynamicPortCentered<DynPort>(mm2px(Vec(xTrck1 + 12.7 * i, 31.5)), true, module, TRACK_VOL_INPUTS + i, module ? &module->panelTheme : NULL));			
			// Pan inputs
			addInput(inputWidgets[i + 16 * 3] = createDynamicPortCentered<DynPort>(mm2px(Vec(xTrck1 + 12.7 * i, 40.5)), true, module, TRACK_PAN_INPUTS + i, module ? &module->panelTheme : NULL));			
			// Pan knobs
			DynSmallKnobGreyWithArc *panKnobTrack;
			addParam(panKnobTrack = createDynamicParamCentered<DynSmallKnobGreyWithArc>(mm2px(Vec(xTrck1 + 12.7 * i, 51.8)), module, TRACK_PAN_PARAMS + i, module ? &module->panelTheme : NULL));
			if (module) {
				panKnobTrack->colorAndCloakPtr = &(module->gInfo.colorAndCloak);
				panKnobTrack->paramWithCV = &(module->tracks[i].panWithCV);
				panKnobTrack->dispColorLocal = &(module->tracks[i].dispColorLocal);
			}
			
			// Faders
			DynSmallFaderWithLink *newFader;
			addParam(newFader = createDynamicParamCentered<DynSmallFaderWithLink>(mm2px(Vec(xTrck1 + 3.67 + 12.7 * i, 81.2)), module, TRACK_FADER_PARAMS + i, module ? &module->panelTheme : NULL));
			if (module) {
				newFader->gInfo = &(module->gInfo);
				newFader->faderParams = &module->params[TRACK_FADER_PARAMS];
				// VU meters
				VuMeterTrack *newVU = createWidgetCentered<VuMeterTrack>(mm2px(Vec(xTrck1 + 12.7 * i, 81.2)));
				newVU->srcLevels = &(module->tracks[i].vu);
				newVU->colorThemeGlobal = &(module->gInfo.colorAndCloak.cc4[vuColorGlobal]);
				newVU->colorThemeLocal = &(module->tracks[i].vuColorThemeLocal);
				addChild(newVU);
				// Fade pointers
				CvAndFadePointerTrack *newFP = createWidgetCentered<CvAndFadePointerTrack>(mm2px(Vec(xTrck1 - 2.95 + 12.7 * i, 81.2)));
				newFP->srcParam = &(module->params[TRACK_FADER_PARAMS + i]);
				newFP->srcParamWithCV = &(module->tracks[i].paramWithCV);
				newFP->colorAndCloak = &(module->gInfo.colorAndCloak);
				newFP->srcFadeGain = &(module->tracks[i].fadeGain);
				newFP->srcFadeRate = module->tracks[i].fadeRate;
				newFP->dispColorLocalPtr = &(module->tracks[i].dispColorLocal);
				addChild(newFP);				
			}
			
			
			// Mutes
			DynMuteFadeButtonWithClear* newMuteFade;
			addParam(newMuteFade = createDynamicParamCentered<DynMuteFadeButtonWithClear>(mm2px(Vec(xTrck1 + 12.7 * i, 109.8)), module, TRACK_MUTE_PARAMS + i, module ? &module->panelTheme : NULL));
			if (module) {
				newMuteFade->type = module->tracks[i].fadeRate;
				newMuteFade->muteParams = &module->params[TRACK_MUTE_PARAMS];
			}
			// Solos
			DynSoloButtonMutex *newSoloButton;
			addParam(newSoloButton = createDynamicParamCentered<DynSoloButtonMutex>(mm2px(Vec(xTrck1 + 12.7 * i, 116.1)), module, TRACK_SOLO_PARAMS + i, module ? &module->panelTheme : NULL));
			newSoloButton->soloParams =  module ? &module->params[TRACK_SOLO_PARAMS] : NULL;
			// Group dec
			DynGroupMinusButtonNotify *newGrpMinusButton;
			addChild(newGrpMinusButton = createDynamicWidgetCentered<DynGroupMinusButtonNotify>(mm2px(Vec(xTrck1 - 3.73 + 12.7 * i - 0.75, 123.1)), module ? &module->panelTheme : NULL));
			if (module) {
				newGrpMinusButton->sourceParam = &(module->params[GROUP_SELECT_PARAMS + i]);
			}
			// Group inc
			DynGroupPlusButtonNotify *newGrpPlusButton;
			addChild(newGrpPlusButton = createDynamicWidgetCentered<DynGroupPlusButtonNotify>(mm2px(Vec(xTrck1 + 3.77 + 12.7 * i + 0.75, 123.1)), module ? &module->panelTheme : NULL));
			if (module) {
				newGrpPlusButton->sourceParam = &(module->params[GROUP_SELECT_PARAMS + i]);
			}
			// Group select displays
			GroupSelectDisplay* groupSelectDisplay;
			addParam(groupSelectDisplay = createParamCentered<GroupSelectDisplay>(mm2px(Vec(xTrck1 + 12.7 * i - 0.1, 123.1)), module, GROUP_SELECT_PARAMS + i));
			if (module) {
				groupSelectDisplay->srcColor = &(module->gInfo.colorAndCloak);
				groupSelectDisplay->srcColorLocal = &(module->tracks[i].dispColorLocal);
			}
		}
		
		// Monitor outputs and groups
		static const float xGrp1 = 217.17 + 20.32;
		for (int i = 0; i < 4; i++) {
			// Monitor outputs
			if (i > 0) {
				addOutput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xGrp1 + 12.7 * (i), 11.5)), false, module, DIRECT_OUTPUTS + i - 1, module ? &module->panelTheme : NULL));
			}
			else {
				addOutput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xGrp1 + 12.7 * (0), 11.5)), false, module, FADE_CV_OUTPUT, module ? &module->panelTheme : NULL));				
			}
			// Labels
			addChild(groupDisplays[i] = createWidgetCentered<GroupDisplay>(mm2px(Vec(xGrp1 + 12.7 * i + 0.4, 23.5))));
			if (module) {
				groupDisplays[i]->colorAndCloak = &(module->gInfo.colorAndCloak);
				groupDisplays[i]->srcGroup = &(module->groups[i]);
				groupDisplays[i]->auxExpanderPresentPtr = &(module->auxExpanderPresent);
				groupDisplays[i]->dispColorLocal = &(module->groups[i].dispColorLocal);
			}
			
			// Volume inputs
			addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(xGrp1 + 12.7 * i, 31.5)), true, module, GROUP_VOL_INPUTS + i, module ? &module->panelTheme : NULL));			
			// Pan inputs
			addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(xGrp1 + 12.7 * i, 40.5)), true, module, GROUP_PAN_INPUTS + i, module ? &module->panelTheme : NULL));			
			// Pan knobs
			DynSmallKnobGreyWithArc *panKnobGroup;
			addParam(panKnobGroup = createDynamicParamCentered<DynSmallKnobGreyWithArc>(mm2px(Vec(xGrp1 + 12.7 * i, 51.8)), module, GROUP_PAN_PARAMS + i, module ? &module->panelTheme : NULL));
			if (module) {
				panKnobGroup->colorAndCloakPtr = &(module->gInfo.colorAndCloak);
				panKnobGroup->paramWithCV = &(module->groups[i].panWithCV);
				panKnobGroup->dispColorLocal = &(module->groups[i].dispColorLocal);
			}
			
			// Faders
			DynSmallFaderWithLink *newFader;
			addParam(newFader = createDynamicParamCentered<DynSmallFaderWithLink>(mm2px(Vec(xGrp1 + 3.67 + 12.7 * i, 81.2)), module, GROUP_FADER_PARAMS + i, module ? &module->panelTheme : NULL));		
			if (module) {
				newFader->gInfo = &(module->gInfo);
				newFader->faderParams = &module->params[TRACK_FADER_PARAMS];
				// VU meters
				VuMeterTrack *newVU = createWidgetCentered<VuMeterTrack>(mm2px(Vec(xGrp1 + 12.7 * i, 81.2)));
				newVU->srcLevels = &(module->groups[i].vu);
				newVU->colorThemeGlobal = &(module->gInfo.colorAndCloak.cc4[vuColorGlobal]);
				newVU->colorThemeLocal = &(module->groups[i].vuColorThemeLocal);
				addChild(newVU);
				// Fade pointers
				CvAndFadePointerGroup *newFP = createWidgetCentered<CvAndFadePointerGroup>(mm2px(Vec(xGrp1 - 2.95 + 12.7 * i, 81.2)));
				newFP->srcParam = &(module->params[GROUP_FADER_PARAMS + i]);
				newFP->srcParamWithCV = &(module->groups[i].paramWithCV);
				newFP->colorAndCloak = &(module->gInfo.colorAndCloak);
				newFP->srcFadeGain = &(module->groups[i].fadeGain);
				newFP->srcFadeRate = module->groups[i].fadeRate;
				newFP->dispColorLocalPtr = &(module->groups[i].dispColorLocal);
				addChild(newFP);				
			}

			// Mutes
			DynMuteFadeButtonWithClear* newMuteFade;
			addParam(newMuteFade = createDynamicParamCentered<DynMuteFadeButtonWithClear>(mm2px(Vec(xGrp1 + 12.7 * i, 109.8)), module, GROUP_MUTE_PARAMS + i, module ? &module->panelTheme : NULL));
			if (module) {
				newMuteFade->type = module->groups[i].fadeRate;
				newMuteFade->muteParams = &module->params[TRACK_MUTE_PARAMS];
			}
			// Solos
			DynSoloButtonMutex* newSoloButton;
			addParam(newSoloButton = createDynamicParamCentered<DynSoloButtonMutex>(mm2px(Vec(xGrp1 + 12.7 * i, 116.1)), module, GROUP_SOLO_PARAMS + i, module ? &module->panelTheme : NULL));
			newSoloButton->soloParams =  module ? &module->params[TRACK_SOLO_PARAMS] : NULL;
		}
		
		// Master inputs
		addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(289.62, 12.8)), true, module, CHAIN_INPUTS + 0, module ? &module->panelTheme : NULL));			
		addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(289.62, 21.8)), true, module, CHAIN_INPUTS + 1, module ? &module->panelTheme : NULL));			
		
		// Master outputs
		addOutput(createDynamicPortCentered<DynPort>(mm2px(Vec(300.12, 12.8)), false, module, MAIN_OUTPUTS + 0, module ? &module->panelTheme : NULL));			
		addOutput(createDynamicPortCentered<DynPort>(mm2px(Vec(300.12, 21.8)), false, module, MAIN_OUTPUTS + 1, module ? &module->panelTheme : NULL));			
		
		// Master label
		addChild(masterDisplay = createWidgetCentered<MasterDisplay>(mm2px(Vec(294.81 + 1.2, 128.5 - 97.0))));
		if (module) {
			masterDisplay->srcMaster = &(module->master);
			masterDisplay->colorAndCloak = &(module->gInfo.colorAndCloak);
			masterDisplay->dispColorLocal = &(module->master.dispColorLocal);
		}
		
		// Master fader
		addParam(createDynamicParamCentered<DynBigFader>(mm2px(Vec(300.17, 70.3)), module, MAIN_FADER_PARAM, module ? &module->panelTheme : NULL));
		if (module) {
			// VU meter
			VuMeterMaster *newVU = createWidgetCentered<VuMeterMaster>(mm2px(Vec(294.82, 70.3)));
			newVU->srcLevels = &(module->master.vu);
			newVU->colorThemeGlobal = &(module->gInfo.colorAndCloak.cc4[vuColorGlobal]);
			newVU->colorThemeLocal = &(module->master.vuColorThemeLocal);
			newVU->clippingPtr = &(module->master.clipping);
			addChild(newVU);
			// Fade pointer
			CvAndFadePointerMaster *newFP = createWidgetCentered<CvAndFadePointerMaster>(mm2px(Vec(294.82 - 3.4, 70.3)));
			newFP->srcParam = &(module->params[MAIN_FADER_PARAM]);
			newFP->srcParamWithCV = &(module->master.paramWithCV);
			newFP->colorAndCloak = &(module->gInfo.colorAndCloak);
			newFP->srcFadeGain = &(module->master.fadeGain);
			newFP->srcFadeRate = &(module->master.fadeRate);
			addChild(newFP);				
		}
		
		// Master mute
		DynMuteFadeButton* newMuteFade;
		addParam(newMuteFade = createDynamicParamCentered<DynMuteFadeButton>(mm2px(Vec(294.82, 109.8)), module, MAIN_MUTE_PARAM, module ? &module->panelTheme : NULL));
		if (module) {
			newMuteFade->type = &(module->master.fadeRate);
		}
		
		// Master dim
		addParam(createDynamicParamCentered<DynDimButton>(mm2px(Vec(289.42, 116.1)), module, MAIN_DIM_PARAM, module ? &module->panelTheme : NULL));
		
		// Master mono
		addParam(createDynamicParamCentered<DynMonoButton>(mm2px(Vec(300.22, 116.1)), module, MAIN_MONO_PARAM, module ? &module->panelTheme : NULL));
	}
	
	void step() override {
		MixMaster* moduleM = (MixMaster*)module;
		if (moduleM) {
			// Track labels (pull from module)
			if (moduleM->updateTrackLabelRequest != 0) {// pull request from module
				// master display
				masterDisplay->text = std::string(&(moduleM->master.masterLabel[0]), 6);
				// track displays
				for (int trk = 0; trk < 16; trk++) {
					trackDisplays[trk]->text = std::string(&(moduleM->trackLabels[trk * 4]), 4);
				}
				// group displays
				for (int grp = 0; grp < 4; grp++) {
					groupDisplays[grp]->text = std::string(&(moduleM->trackLabels[(16 + grp) * 4]), 4);
				}
				moduleM->updateTrackLabelRequest = 0;// all done pulling
			}
			
			// Borders
			if ( moduleM->auxExpanderPresent != oldAuxExpanderPresent ) {
				oldAuxExpanderPresent = moduleM->auxExpanderPresent;
			
				if (oldAuxExpanderPresent) {
					//panelBorder->box.pos.x = 0;
					panelBorder->box.size.x = box.size.x + 3;
				}
				else {
					//panelBorder->box.pos.x = 0;
					panelBorder->box.size.x = box.size.x;
				}
				((SvgPanel*)panel)->dirty = true;// weird zoom bug: if the if/else above is commented, zoom bug when this executes
			}
			
			// Update param tooltips at 1Hz
			time_t currentTime = time(0);
			if (currentTime != oldTime) {
				oldTime = currentTime;
				char strBuf[32];
				for (int i = 0; i < 16; i++) {
					std::string trackLabel = std::string(&(moduleM->trackLabels[i * 4]), 4);
					// Pan
					snprintf(strBuf, 32, "%s: pan", trackLabel.c_str());
					moduleM->paramQuantities[TRACK_PAN_PARAMS + i]->label = strBuf;
					// Fader
					snprintf(strBuf, 32, "%s: level", trackLabel.c_str());
					moduleM->paramQuantities[TRACK_FADER_PARAMS + i]->label = strBuf;
					// Mute/fade
					if (moduleM->tracks[i].isFadeMode()) {
						snprintf(strBuf, 32, "%s: fade", trackLabel.c_str());
					}
					else {
						snprintf(strBuf, 32, "%s: mute", trackLabel.c_str());
					}
					moduleM->paramQuantities[TRACK_MUTE_PARAMS + i]->label = strBuf;
					// Solo
					snprintf(strBuf, 32, "%s: solo", trackLabel.c_str());
					moduleM->paramQuantities[TRACK_SOLO_PARAMS + i]->label = strBuf;
					// Group select
					snprintf(strBuf, 32, "%s: group", trackLabel.c_str());
					moduleM->paramQuantities[GROUP_SELECT_PARAMS + i]->label = strBuf;
				}
				// Group
				for (int i = 0; i < 4; i++) {
					std::string groupLabel = std::string(&(moduleM->trackLabels[(16 + i) * 4]), 4);
					// Pan
					snprintf(strBuf, 32, "%s: pan", groupLabel.c_str());
					moduleM->paramQuantities[GROUP_PAN_PARAMS + i]->label = strBuf;
					// Fader
					snprintf(strBuf, 32, "%s: level", groupLabel.c_str());
					moduleM->paramQuantities[GROUP_FADER_PARAMS + i]->label = strBuf;
					// Mute/fade
					if (moduleM->groups[i].isFadeMode()) {
						snprintf(strBuf, 32, "%s: fade", groupLabel.c_str());
					}
					else {
						snprintf(strBuf, 32, "%s: mute", groupLabel.c_str());
					}
					moduleM->paramQuantities[GROUP_MUTE_PARAMS + i]->label = strBuf;
					// Solo
					snprintf(strBuf, 32, "%s: solo", groupLabel.c_str());
					moduleM->paramQuantities[GROUP_SOLO_PARAMS + i]->label = strBuf;
				}
				std::string masterLabel = std::string(moduleM->master.masterLabel, 6);
				// Fader
				snprintf(strBuf, 32, "%s: level", masterLabel.c_str());
				moduleM->paramQuantities[MAIN_FADER_PARAM]->label = strBuf;
				// Mute/fade
				if (moduleM->master.isFadeMode()) {
					snprintf(strBuf, 32, "%s: fade", masterLabel.c_str());
				}
				else {
					snprintf(strBuf, 32, "%s: mute", masterLabel.c_str());
				}
				moduleM->paramQuantities[MAIN_MUTE_PARAM]->label = strBuf;
				// Dim
				snprintf(strBuf, 32, "%s: dim", masterLabel.c_str());
				moduleM->paramQuantities[MAIN_DIM_PARAM]->label = strBuf;
				// Mono
				snprintf(strBuf, 32, "%s: mono", masterLabel.c_str());
				moduleM->paramQuantities[MAIN_MONO_PARAM]->label = strBuf;
			}
		}			
		
			
		Widget::step();
	}// void step()
};

Model *modelMixMaster = createModel<MixMaster<16>, MixMasterWidget>("MixMaster");
