//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.txt for all licenses
//***********************************************************************************************


#include "MindMeldModular.hpp"

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

	// no need to save, no reset
	int trackNum;
	std::string ids;
	int *solo;
	Input *inL;
	Input *inR;
	Input *inVol;
	Input *inPan;
	Param *paFade;
	Param *paMute;
	Param *paPan;


	void construct(int _trackNum, int *_solo, Input *_inL, Input *_inR, Input *_inVol, Input *_inPan, Param *_paFade, Param *_paMute, Param *_paPan) {
		trackNum = _trackNum;
		ids = "id" + std::to_string(trackNum) + "_";
		solo = _solo;
		inL = _inL;
		inR = _inR;
		inVol = _inVol;
		inPan = _inPan;
		paFade = _paFade;
		paMute = _paMute;
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
			pan += inPan->getVoltage() * 0.2f;// this is a -5V to +5V input
		}
		if (stereo) {
			// Balance approach
			panRcoeff = std::min(1.0f, pan + 1.0f);
			panLcoeff = std::min(1.0f, 1.0f - pan);
		}
		else {
			// TODO SB wants boost panning laws, with three choices: +3 (equal power), +3.5 (console) and +4.5
			// Linear panning law
			panRcoeff = std::max(0.0f, (1.0f + pan)*0.5f);
			panLcoeff = 1.0f - panRcoeff;
		}
	}
	
	
	void onRandomize(bool editingSequence) {
		
	}
	
	
	
	void dataToJson(json_t *rootJ) {
		// mute
		// json_object_set_new(rootJ, (ids + "mute").c_str(), json_boolean(mute));

		// group
		json_object_set_new(rootJ, (ids + "group").c_str(), json_integer(group));
	}
	
	void dataFromJson(json_t *rootJ) {
		// mute
		// json_t *muteJ = json_object_get(rootJ, (ids + "mute").c_str());
		// if (muteJ)
			// mute = json_is_true(muteJ);

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
		if (*solo != -1 && *solo != trackNum) {// TODO
			return;
		}

		// Calc track fader gain
		float gain = std::pow(paFade->getValue(), trackFaderScalingExponent);
		
		// Get inputs and apply pan
		float sigL = inL->getVoltage();
		float sigR;
		if (stereo) {
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
		else {
			// Apply fader gain
			sigL *= gain;

			// Apply CV gain
			if (inVol->isConnected()) {
				float cv = clamp(inVol->getVoltage() / 10.f, 0.f, 1.f);
				sigL *= cv;
			}

			sigR = sigL;
		}

		// Apply pan
		sigL *= panLcoeff;
		sigR *= panRcoeff;

		// Add to mix
		*mixL += sigL;
		*mixR += sigR;
	}
	
	
	
};// struct MixerTrack


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
	int solo;// -1 is no solo, 0 to 16 is solo track number
	MixerTrack tracks[16];
	int panLaw;// 0 is linear, 1 is +3.5dB sides
	
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
			configParam(TRACK_PAN_PARAMS + i, -1.0f, 1.0f, 0.0f, strBuf, "%", 0.0f, 100.0f);
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
			configParam(GROUP_PAN_PARAMS + i, -1.0f, 1.0f, 0.0f, strBuf, "%", 0.0f, 100.0f);
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
			tracks[i].construct(i, &solo, &inputs[TRACK_SIGNAL_INPUTS + 2 * i + 0], &inputs[TRACK_SIGNAL_INPUTS + 2 * i + 1],
				&inputs[TRACK_VOL_INPUTS + i], &inputs[TRACK_PAN_INPUTS + i], &params[TRACK_FADER_PARAMS + i], &params[TRACK_MUTE_PARAMS + i], &params[TRACK_PAN_PARAMS + i]);
		}
		onReset();

		panelTheme = 0;//(loadDarkAsDefault() ? 1 : 0);
	}

	
	void onReset() override {
		snprintf(trackLabels, 4 * 20 + 1, "-01--02--03--04--05--06--07--08--09--10--11--12--13--14--15--16-GRP1GRP2GRP3GRP4");		
		solo = -1;
		for (int i = 0; i < 16; i++) {
			tracks[i].onReset();
		}
		panLaw = 0;
		resetNonJson();
	}
	void resetNonJson() {
		resetTrackLabelRequest = 0;// setting to 0 will trigger 1, 2, 3 etc on each video frame afterwards
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

		// solo
		json_object_set_new(rootJ, "solo", json_integer(solo));

		// tracks
		for (int i = 0; i < 16; i++) {
			tracks[i].dataToJson(rootJ);
		}

		// panLaw
		json_object_set_new(rootJ, "panLaw", json_integer(panLaw));

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
		
		// solo
		json_t *soloJ = json_object_get(rootJ, "solo");
		if (soloJ)
			solo = json_integer_value(soloJ);

		// tracks
		for (int i = 0; i < 16; i++) {
			tracks[i].dataFromJson(rootJ);
		}

		// panLaw
		json_t *panLawJ = json_object_get(rootJ, "panLaw");
		if (panLawJ)
			panLaw = json_integer_value(panLawJ);

		resetNonJson();
	}

	
	void process(const ProcessArgs &args) override {
		if (refresh.processInputs()) {
			int trackToProcess = refresh.refreshCounter & (16 - 1);
			
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

	struct PanLawItem : MenuItem {
		struct PanLawSubItem : MenuItem {
			Mixer *module;
			int setVal = 0;
			void onAction(const event::Action &e) override {
				module->panLaw = setVal;
			}
		};
		Mixer *module;
		Menu *createChildMenu() override {
			Menu *menu = new Menu;

			PanLawSubItem *law0Item = createMenuItem<PanLawSubItem>("-6 dB center (linear)", CHECKMARK(module->panLaw == 0));
			law0Item->module = this->module;
			menu->addChild(law0Item);

			PanLawSubItem *law1Item = createMenuItem<PanLawSubItem>("+3.5 dB sides", CHECKMARK(module->panLaw == 1));
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
		
		PanLawItem *panlawItem = createMenuItem<PanLawItem>("Mono panning law", RIGHT_ARROW);
		panlawItem->module = module;
		menu->addChild(panlawItem);
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
