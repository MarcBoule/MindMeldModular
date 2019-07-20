//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.txt for all licenses
//***********************************************************************************************


#include "MindMeldModular.hpp"


// managed by Mixer, not by tracks (tracks read only)
struct GlobalInfo {
	// need to save, no reset
	// none
	
	// need to save, with reset
	int panLawMono;// +0dB (no compensation),  +3 (equal power, default),  +4.5 (compromize),  +6dB (linear)
	int panLawStereo;// Stereo balance (+3dB boost since one channel lost, default),  True pan (linear redistribution but is not equal power)
	
	// no need to save, with reset
	bool soloAllOff;// when true, nothing to do, when false, a track must check its solo to see it should play

	// no need to save, no reset
	// none
	

	void onReset() {
		panLawMono = 1;
		panLawStereo = 0;
		// extern must call resetNonJson()
	}
	
	void resetNonJson(Param *soloParams) {
		updateSoloAllOff(soloParams);
	}
	
	void updateSoloAllOff(Param *soloParams) {
		bool newSoloAllOff = true;// separate variable so no glitch generated
		for (int i = 0; i < 16; i++) {
			if (soloParams[i].getValue() > 0.5) {
				newSoloAllOff = false;
				break;
			}
		}
		soloAllOff = newSoloAllOff;	
	}
	
	void onRandomize(bool editingSequence) {
		
	}
	

	void dataToJson(json_t *rootJ) {
		// panLawMono 
		json_object_set_new(rootJ, "panLawMono", json_integer(panLawMono));

		// panLawStereo
		json_object_set_new(rootJ, "panLawStereo", json_integer(panLawStereo));
	}
	
	void dataFromJson(json_t *rootJ) {
		// panLawMono
		json_t *panLawMonoJ = json_object_get(rootJ, "panLawMono");
		if (panLawMonoJ)
			panLawMono = json_integer_value(panLawMonoJ);
		
		// panLawStereo
		json_t *panLawStereoJ = json_object_get(rootJ, "panLawStereo");
		if (panLawStereoJ)
			panLawStereo = json_integer_value(panLawStereoJ);
		
		// extern must call resetNonJson()
	}
};// struct GlobalInfo


//*****************************************************************************


struct MixerTrack {
	// Constants
	static constexpr float trackFaderScalingExponent = 3.0f; // for example, 3.0f is x^3 scaling (seems to be what Console uses) (must be integer for best performance)
	static constexpr float trackFaderMaxLinearGain = 2.0f; // for example, 2.0f is +6 dB
	
	// need to save, no reset
	// none
	
	// need to save, with reset
	int group;// -1 is no group, 0 to 3 is group

	// no need to save, with reset
	bool stereo;// pan coefficients use this, so set up first
	float panLcoeff;
	float panRcoeff;
	float panLinRcoeff;// used only for True stereo panning
	float panRinLcoeff;// used only for True stereo panning

	// no need to save, no reset
	int trackNum;
	std::string ids;
	GlobalInfo *gInfo;
	Input *inL;
	Input *inR;
	Input *inVol;
	Input *inPan;
	Param *paFade;
	Param *paMute;
	Param *paSolo;
	Param *paPan;


	void construct(int _trackNum, GlobalInfo *_gInfo, Input *_inL, Input *_inR, Input *_inVol, Input *_inPan, Param *_paFade, Param *_paMute, Param *_paSolo, Param *_paPan) {
		trackNum = _trackNum;
		gInfo = _gInfo;
		ids = "id" + std::to_string(trackNum) + "_";
		inL = _inL;
		inR = _inR;
		inVol = _inVol;
		inPan = _inPan;
		paFade = _paFade;
		paMute = _paMute;
		paSolo = _paSolo;
		paPan = _paPan;
	}
	
	
	void onReset() {
		group = -1;
		// extern must call resetNonJson()
	}
	void resetNonJson() {
		updatePanCoeff();
	}
	
	void updatePanCoeff() {
		stereo = inR->isConnected();
		
		float pan = paPan->getValue();
		if (inPan->isConnected()) {
			pan += inPan->getVoltage() * 0.1f;// this is a -5V to +5V input
		}
		
		panLinRcoeff = 0.0f;
		panRinLcoeff = 0.0f;
		if (!stereo) {// mono
			if (gInfo->panLawMono == 1) {
				// Equal power panning law (+3dB boost)
				panRcoeff = std::sin(pan * M_PI_2) * M_SQRT2;
				panLcoeff = std::cos(pan * M_PI_2) * M_SQRT2;
			}
			else if (gInfo->panLawMono == 0) {
				// No compensation (+0dB boost)
				panRcoeff = std::min(1.0f, pan * 2.0f);
				panLcoeff = std::min(1.0f, 2.0f - pan * 2.0f);
			}
			else if (gInfo->panLawMono == 2) {
				// Compromise (+4.5dB boost)
				panRcoeff = std::sqrt( std::abs( std::sin(pan * M_PI_2) * M_SQRT2   *   (pan * 2.0f) ) );
				panLcoeff = std::sqrt( std::abs( std::cos(pan * M_PI_2) * M_SQRT2   *   (2.0f - pan * 2.0f) ) );
			}
			else {
				// Linear panning law (+6dB boost)
				panRcoeff = pan * 2.0f;
				panLcoeff = 2.0f - panRcoeff;
			}
		}
		else {// stereo
			if (gInfo->panLawStereo == 0) {
				// Stereo balance (+3dB), same as mono equal power
				panRcoeff = std::sin(pan * M_PI_2) * M_SQRT2;
				panLcoeff = std::cos(pan * M_PI_2) * M_SQRT2;
			}
			else {
				// True panning, equal power
				panRcoeff = pan >= 0.5f ? (1.0f) : (std::sin(pan * M_PI));
				panRinLcoeff = pan >= 0.5f ? (0.0f) : (std::cos(pan * M_PI));
				panLcoeff = pan <= 0.5f ? (1.0f) : (std::cos((pan - 0.5f) * M_PI));
				panLinRcoeff = pan <= 0.5f ? (0.0f) : (std::sin((pan - 0.5f) * M_PI));
			}
		}
	}
	
	
	void onRandomize(bool editingSequence) {
		
	}
	
	
	
	void dataToJson(json_t *rootJ) {
		// group
		json_object_set_new(rootJ, (ids + "group").c_str(), json_integer(group));
	}
	
	void dataFromJson(json_t *rootJ) {
		// group
		json_t *groupJ = json_object_get(rootJ, (ids + "group").c_str());
		if (groupJ)
			group = json_integer_value(groupJ);
		
		// extern must call resetNonJson()
	}
	
	
	
	void process(float *mixL, float *mixR) {
		if (!inL->isConnected() || paMute->getValue() > 0.5f) {
			return;
		}
		if (!gInfo->soloAllOff && paSolo->getValue() < 0.5f) {
			return;
		}

		// Calc track fader gain
		float gain = std::pow(paFade->getValue(), trackFaderScalingExponent);
		
		// Get inputs and apply pan
		float sigL = inL->getVoltage();
		float sigR;
		if (!stereo) {// mono
			// Apply fader gain
			sigL *= gain;

			// Apply CV gain
			if (inVol->isConnected()) {
				float cv = clamp(inVol->getVoltage() / 10.f, 0.f, 1.f);
				sigL *= cv;
			}

			sigR = sigL;
		}
		else {// stereo
			sigR = inR->getVoltage();
		
			// Apply fader gain
			sigL *= gain;
			sigR *= gain;

			// Apply CV gain
			if (inVol->isConnected()) {
				float cv = clamp(inVol->getVoltage() / 10.f, 0.f, 1.f);
				sigL *= cv;
				sigR *= cv;
			}
		}

		// Apply pan
		float pannedSigL = sigL * panLcoeff;
		float pannedSigR = sigR * panRcoeff;
		if (stereo && gInfo->panLawStereo != 0) {
			pannedSigL += sigR * panRinLcoeff;
			pannedSigR += sigL * panLinRcoeff;
		}

		// Add to mix
		*mixL += pannedSigL;
		*mixR += pannedSigR;
	}
	
	
	
};// struct MixerTrack


//*****************************************************************************


struct Mixer : Module {
	enum ParamIds {
		ENUMS(TRACK_FADER_PARAMS, 16),
		ENUMS(GROUP_FADER_PARAMS, 4),
		ENUMS(TRACK_PAN_PARAMS, 16),
		ENUMS(GROUP_PAN_PARAMS, 4),
		ENUMS(TRACK_MUTE_PARAMS, 16),
		ENUMS(GROUP_MUTE_PARAMS, 4),
		ENUMS(TRACK_SOLO_PARAMS, 16),
		ENUMS(GROUP_SOLO_PARAMS, 4),
		MAIN_FADER_PARAM,
		ENUMS(GRP_INC_PARAMS, 16),
		ENUMS(GRP_DEC_PARAMS, 16),
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(TRACK_SIGNAL_INPUTS, 16 * 2), // Track 0: 0 = L, 1 = R, Track 1: 2 = L, 3 = R, etc...
		ENUMS(TRACK_VOL_INPUTS, 16),
		ENUMS(GROUP_VOL_INPUTS, 4),
		ENUMS(TRACK_PAN_INPUTS, 16), 
		ENUMS(GROUP_PAN_INPUTS, 4), 
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(MONITOR_OUTPUTS, 4), // Track 1-8, Track 9-16, Groups and Aux
		ENUMS(MAIN_OUTPUTS, 2),
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};
	
	// Expander
	// float rightMessages[2][5] = {};// messages from expander


	// Constants
	static constexpr float masterFaderScalingExponent = 3.0f; 
	static constexpr float masterFaderMaxLinearGain = 2.0f;

	// Need to save, no reset
	int panelTheme;
	
	// Need to save, with reset
	char trackLabels[4 * 20 + 1];// 4 chars per label, 16 tracks and 4 groups means 20 labels, null terminate the end the whole array only
	GlobalInfo gInfo;
	MixerTrack tracks[16];
	
	// No need to save, with reset
	int resetTrackLabelRequest;// -1 when nothing to do, 0 to 15 for incremental read in widget
	
	// No need to save, no reset
	RefreshCounter refresh;	

		
	Mixer() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);		
		
		char strBuf[32];
		// Track
		float maxTFader = std::pow(MixerTrack::trackFaderMaxLinearGain, 1.0f / MixerTrack::trackFaderScalingExponent);
		for (int i = 0; i < 16; i++) {
			// Pan
			snprintf(strBuf, 32, "Track #%i pan", i + 1);
			configParam(TRACK_PAN_PARAMS + i, 0.0f, 1.0f, 0.5f, strBuf, "%", 0.0f, 200.0f, -100.0f);
			// Fader
			snprintf(strBuf, 32, "Track #%i level", i + 1);
			configParam(TRACK_FADER_PARAMS + i, 0.0f, maxTFader, 1.0f, strBuf, " dB", -10, 20.0f * MixerTrack::trackFaderScalingExponent);
			// Mute
			snprintf(strBuf, 32, "Track #%i mute", i + 1);
			configParam(TRACK_MUTE_PARAMS + i, 0.0f, 1.0f, 0.0f, strBuf);
			// Solo
			snprintf(strBuf, 32, "Track #%i solo", i + 1);
			configParam(TRACK_SOLO_PARAMS + i, 0.0f, 1.0f, 0.0f, strBuf);
			// Group decrement
			snprintf(strBuf, 32, "Track #%i group -", i + 1);
			configParam(GRP_DEC_PARAMS + i, 0.0f, 1.0f, 0.0f, strBuf);
			// Group increment
			snprintf(strBuf, 32, "Track #%i group +", i + 1);
			configParam(GRP_INC_PARAMS + i, 0.0f, 1.0f, 0.0f, strBuf);
		}
		// Group
		for (int i = 0; i < 4; i++) {
			// Pan
			snprintf(strBuf, 32, "Group #%i pan", i + 1);
			configParam(GROUP_PAN_PARAMS + i, 0.0f, 1.0f, 0.5f, strBuf, "%", 0.0f, 200.0f, -100.0f);
			// Fader
			snprintf(strBuf, 32, "Group #%i level", i + 1);
			configParam(GROUP_FADER_PARAMS + i, 0.0f, maxTFader, 1.0f, strBuf, " dB", -10, 20.0f * MixerTrack::trackFaderScalingExponent);
			// Mute
			snprintf(strBuf, 32, "Group #%i mute", i + 1);
			configParam(GROUP_MUTE_PARAMS + i, 0.0f, 1.0f, 0.0f, strBuf);
			// Solo
			snprintf(strBuf, 32, "Group #%i solo", i + 1);
			configParam(GROUP_SOLO_PARAMS + i, 0.0f, 1.0f, 0.0f, strBuf);
		}
		float maxMFader = std::pow(masterFaderMaxLinearGain, 1.0f / masterFaderScalingExponent);
		configParam(MAIN_FADER_PARAM, 0.0f, maxMFader, 1.0f, "Main level", " dB", -10, 20.0f * masterFaderScalingExponent);
		
		for (int i = 0; i < 16; i++) {
			tracks[i].construct(i, &gInfo, &inputs[TRACK_SIGNAL_INPUTS + 2 * i + 0], &inputs[TRACK_SIGNAL_INPUTS + 2 * i + 1],
				&inputs[TRACK_VOL_INPUTS + i], &inputs[TRACK_PAN_INPUTS + i], &params[TRACK_FADER_PARAMS + i], &params[TRACK_MUTE_PARAMS + i], &params[TRACK_SOLO_PARAMS + i], &params[TRACK_PAN_PARAMS + i]);
		}
		onReset();

		panelTheme = 0;//(loadDarkAsDefault() ? 1 : 0);
	}

	
	void onReset() override {
		snprintf(trackLabels, 4 * 20 + 1, "-01--02--03--04--05--06--07--08--09--10--11--12--13--14--15--16-GRP1GRP2GRP3GRP4");		
		gInfo.onReset();
		for (int i = 0; i < 16; i++) {
			tracks[i].onReset();
		}
		resetNonJson();
	}
	void resetNonJson() {
		resetTrackLabelRequest = 0;// setting to 0 will trigger 1, 2, 3 etc on each video frame afterwards
		gInfo.resetNonJson(&params[TRACK_SOLO_PARAMS]);
		for (int i = 0; i < 16; i++) {
			tracks[i].resetNonJson();
		}
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
			snprintf(trackLabels, 4 * 20 + 1, "%s", json_string_value(textJ));
		
		// gInfo
		gInfo.dataFromJson(rootJ);

		// tracks
		for (int i = 0; i < 16; i++) {
			tracks[i].dataFromJson(rootJ);
		}

		resetNonJson();
	}

	
	void process(const ProcessArgs &args) override {
		if (refresh.processInputs()) {
			int trackToProcess = refresh.refreshCounter >> 4;// Corresponds to 172Hz refreshing of each track, at 44.1 kHz
			
			gInfo.updateSoloAllOff(&params[TRACK_SOLO_PARAMS]);// TODO optimize this (spread in 16 passes?) 
			tracks[trackToProcess].updatePanCoeff();
			
		}// userInputs refresh
		
		
		
		//********** Outputs and lights **********
		float mix[2] = {};

		// Tracks
		for (int i = 0; i < 16; i++) {
			tracks[i].process(&mix[0], &mix[1]);
		}

		// Main output

		// Apply mastet fader gain
		float gain = std::pow(params[MAIN_FADER_PARAM].getValue(), masterFaderScalingExponent);
		mix[0] *= gain;
		mix[1] *= gain;

		// Apply mix CV gain (UNUSED)
		// if (inputs[MAIN_FADER_CV_INPUT].isConnected()) {
			// float cv = clamp(inputs[MAIN_FADER_CV_INPUT].getVoltage() / 10.f, 0.f, 1.f);
			// mix[0] *= cv;
			// mix[1] *= cv;
		// }

		// Set mix output
		outputs[MAIN_OUTPUTS + 0].setVoltage(mix[0]);
		outputs[MAIN_OUTPUTS + 1].setVoltage(mix[1]);
		
		
		// lights
		if (refresh.processLights()) {

		}
		
	}// process()

};




struct TrackDisplay : LedDisplayTextField {
	TrackDisplay() {
		box.size = Vec(38, 16);
		textOffset = Vec(2.6f, -2.2f);
	};
	void draw(const DrawArgs &args) override {// override and do not call LedDisplayTextField.draw() since draw manually here
		if (cursor > 4) {
			text.resize(4);
			cursor = 4;
			selection = 4;
		}
		
		// the code below is from LedDisplayTextField.draw()
		nvgScissor(args.vg, RECT_ARGS(args.clipBox));
		if (font->handle >= 0) {
			bndSetFont(font->handle);

			NVGcolor highlightColor = color;
			highlightColor.a = 0.5;
			int begin = std::min(cursor, selection);
			int end = (this == APP->event->selectedWidget) ? std::max(cursor, selection) : -1;
			bndIconLabelCaret(args.vg, textOffset.x, textOffset.y,
				box.size.x - 2*textOffset.x, box.size.y - 2*textOffset.y,
				-1, color, 12, text.c_str(), highlightColor, begin, end);

			bndSetFont(APP->window->uiFont->handle);
		}
		nvgResetScissor(args.vg);
	}
};


struct MixerWidget : ModuleWidget {
	TrackDisplay* trackDisplays[20];
	unsigned int trackLabelIndexToPush = 0;


	// Module's context menu
	// --------------------

	struct PanLawMonoItem : MenuItem {
		struct PanLawMonoSubItem : MenuItem {
			Mixer *module;
			int setVal = 0;
			void onAction(const event::Action &e) override {
				module->gInfo.panLawMono = setVal;
			}
		};
		Mixer *module;
		Menu *createChildMenu() override {
			Menu *menu = new Menu;

			PanLawMonoSubItem *law0Item = createMenuItem<PanLawMonoSubItem>("+0 dB (no compensation)", CHECKMARK(module->gInfo.panLawMono == 0));
			law0Item->module = this->module;
			menu->addChild(law0Item);

			PanLawMonoSubItem *law1Item = createMenuItem<PanLawMonoSubItem>("+3 dB boost (equal power, default)", CHECKMARK(module->gInfo.panLawMono == 1));
			law1Item->module = this->module;
			law1Item->setVal = 1;
			menu->addChild(law1Item);

			PanLawMonoSubItem *law2Item = createMenuItem<PanLawMonoSubItem>("+4.5 dB boost (compromise)", CHECKMARK(module->gInfo.panLawMono == 2));
			law2Item->module = this->module;
			law2Item->setVal = 2;
			menu->addChild(law2Item);

			PanLawMonoSubItem *law3Item = createMenuItem<PanLawMonoSubItem>("+6 dB boost (linear)", CHECKMARK(module->gInfo.panLawMono == 3));
			law3Item->module = this->module;
			law3Item->setVal = 3;
			menu->addChild(law3Item);

			return menu;
		}
	};

	struct PanLawStereoItem : MenuItem {
		struct PanLawStereoSubItem : MenuItem {
			Mixer *module;
			int setVal = 0;
			void onAction(const event::Action &e) override {
				module->gInfo.panLawStereo = setVal;
			}
		};
		Mixer *module;
		Menu *createChildMenu() override {
			Menu *menu = new Menu;

			PanLawStereoSubItem *law0Item = createMenuItem<PanLawStereoSubItem>("Stereo balance (default)", CHECKMARK(module->gInfo.panLawStereo == 0));
			law0Item->module = this->module;
			menu->addChild(law0Item);

			PanLawStereoSubItem *law1Item = createMenuItem<PanLawStereoSubItem>("True panning", CHECKMARK(module->gInfo.panLawStereo == 1));
			law1Item->module = this->module;
			law1Item->setVal = 1;
			menu->addChild(law1Item);

			return menu;
		}
	};

	void appendContextMenu(Menu *menu) override {
		Mixer *module = dynamic_cast<Mixer*>(this->module);
		assert(module);

		menu->addChild(new MenuLabel());// empty line
		
		MenuLabel *settingsLabel = new MenuLabel();
		settingsLabel->text = "Settings";
		menu->addChild(settingsLabel);
		
		PanLawMonoItem *panLawMonoItem = createMenuItem<PanLawMonoItem>("Mono pan law", RIGHT_ARROW);
		panLawMonoItem->module = module;
		menu->addChild(panLawMonoItem);
		
		PanLawStereoItem *panLawStereoItem = createMenuItem<PanLawStereoItem>("Stereo pan mode", RIGHT_ARROW);
		panLawStereoItem->module = module;
		menu->addChild(panLawStereoItem);
	}

	// Module's widget
	// --------------------

	MixerWidget(Mixer *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/mixmaster-jr.svg")));
		
		// Tracks
		for (int i = 0; i < 16; i++) {
			// Labels
			addChild(trackDisplays[i] = createWidgetCentered<TrackDisplay>(mm2px(Vec(11.43 + 12.7 * i + 0.4, 4.2))));
			// Left inputs
			addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(11.43 + 12.7 * i, 12)), true, module, Mixer::TRACK_SIGNAL_INPUTS + 2 * i + 0, module ? &module->panelTheme : NULL));			
			// Right inputs
			addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(11.43 + 12.7 * i, 21)), true, module, Mixer::TRACK_SIGNAL_INPUTS + 2 * i + 1, module ? &module->panelTheme : NULL));	
			// Volume inputs
			addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(11.43 + 12.7 * i, 30.7)), true, module, Mixer::TRACK_VOL_INPUTS + i, module ? &module->panelTheme : NULL));			
			// Pan inputs
			addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(11.43 + 12.7 * i, 39.7)), true, module, Mixer::TRACK_PAN_INPUTS + i, module ? &module->panelTheme : NULL));			
			// Pan knobs
			addParam(createDynamicParamCentered<DynSmallKnob>(mm2px(Vec(11.43 + 12.7 * i, 51)), module, Mixer::TRACK_PAN_PARAMS + i, module ? &module->panelTheme : NULL));
			
			// Faders
			addParam(createDynamicParamCentered<DynSmallFader>(mm2px(Vec(15.1 + 12.7 * i, 80.4)), module, Mixer::TRACK_FADER_PARAMS + i, module ? &module->panelTheme : NULL));
			
			// Mutes
			addParam(createDynamicParamCentered<DynMuteButton>(mm2px(Vec(11.43 + 12.7 * i, 109)), module, Mixer::TRACK_MUTE_PARAMS + i, module ? &module->panelTheme : NULL));
			// Solos
			addParam(createDynamicParamCentered<DynSoloButton>(mm2px(Vec(11.43 + 12.7 * i, 115.3)), module, Mixer::TRACK_SOLO_PARAMS + i, module ? &module->panelTheme : NULL));
			// Group dec
			addParam(createDynamicParamCentered<DynGroupMinusButton>(mm2px(Vec(7.7 + 12.7 * i, 122.6)), module, Mixer::GRP_DEC_PARAMS + i, module ? &module->panelTheme : NULL));
			// Group inc
			addParam(createDynamicParamCentered<DynGroupPlusButton>(mm2px(Vec(15.2 + 12.7 * i, 122.6)), module, Mixer::GRP_INC_PARAMS + i, module ? &module->panelTheme : NULL));

		}
		
		// Monitor outputs and groups
		for (int i = 0; i < 4; i++) {
			// Monitor outputs
			addOutput(createDynamicPortCentered<DynPort>(mm2px(Vec(217.17 + 12.7 * i, 12)), false, module, Mixer::MONITOR_OUTPUTS + i, module ? &module->panelTheme : NULL));			

			// Labels
			addChild(trackDisplays[16 + i] = createWidgetCentered<TrackDisplay>(mm2px(Vec(217.17 + 12.7 * i + 0.4, 22.7))));
			// Volume inputs
			addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(217.17 + 12.7 * i, 30.7)), true, module, Mixer::GROUP_VOL_INPUTS + i, module ? &module->panelTheme : NULL));			
			// Pan inputs
			addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(217.17 + 12.7 * i, 39.7)), true, module, Mixer::GROUP_PAN_INPUTS + i, module ? &module->panelTheme : NULL));			
			// Pan knobs
			addParam(createDynamicParamCentered<DynSmallKnob>(mm2px(Vec(217.17 + 12.7 * i, 51)), module, Mixer::GROUP_PAN_PARAMS + i, module ? &module->panelTheme : NULL));
			
			// Faders
			addParam(createDynamicParamCentered<DynSmallFader>(mm2px(Vec(220.84 + 12.7 * i, 80.4)), module, Mixer::GROUP_FADER_PARAMS + i, module ? &module->panelTheme : NULL));		

			// Mutes
			addParam(createDynamicParamCentered<DynMuteButton>(mm2px(Vec(217.17 + 12.7 * i, 109)), module, Mixer::GROUP_MUTE_PARAMS + i, module ? &module->panelTheme : NULL));
			// Solos
			addParam(createDynamicParamCentered<DynSoloButton>(mm2px(Vec(217.17 + 12.7 * i, 115.3)), module, Mixer::GROUP_SOLO_PARAMS + i, module ? &module->panelTheme : NULL));
		}
		
		// Main outputs
		addOutput(createDynamicPortCentered<DynPort>(mm2px(Vec(273.8, 12)), false, module, Mixer::MAIN_OUTPUTS + 0, module ? &module->panelTheme : NULL));			
		addOutput(createDynamicPortCentered<DynPort>(mm2px(Vec(273.8, 21)), false, module, Mixer::MAIN_OUTPUTS + 1, module ? &module->panelTheme : NULL));			
		
		// Main fader
		addParam(createDynamicParamCentered<DynBigFader>(mm2px(Vec(277.65, 69.5)), module, Mixer::MAIN_FADER_PARAM, module ? &module->panelTheme : NULL));		
		
	}
	
	void step() override {
		Mixer* moduleM = (Mixer*)module;
		if (moduleM) {
			// Track labels (push and pull from module)
			int trackLabelIndexToPull = moduleM->resetTrackLabelRequest;
			if (trackLabelIndexToPull >= 0) {// pull request from module
				trackDisplays[trackLabelIndexToPull]->text = std::string(&(moduleM->trackLabels[trackLabelIndexToPull * 4]), 4);
				moduleM->resetTrackLabelRequest++;
				if (moduleM->resetTrackLabelRequest >= 20) {
					moduleM->resetTrackLabelRequest = -1;// all done pulling
				}
			}
			else {// push to module
				int unsigned i = 0;
				*((uint32_t*)(&(moduleM->trackLabels[trackLabelIndexToPush * 4]))) = 0x20202020;
				for (; i < trackDisplays[trackLabelIndexToPush]->text.length(); i++) {
					moduleM->trackLabels[trackLabelIndexToPush * 4 + i] = trackDisplays[trackLabelIndexToPush]->text[i];
				}
				trackLabelIndexToPush++;
				if (trackLabelIndexToPush >= 20) {
					trackLabelIndexToPush = 0;
				}
			}
		}
		Widget::step();
	}
};

Model *modelMixer = createModel<Mixer, MixerWidget>("MixMaster-jr");
