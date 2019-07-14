//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.txt for all licenses
//***********************************************************************************************


#include "MindMeldModular.hpp"


struct Mixer : Module {
	enum ParamIds {
		ENUMS(TRACK_FADER_PARAMS, 16),
		ENUMS(GROUP_FADER_PARAMS, 4),
		ENUMS(TRACK_PAN_PARAMS, 16),
		ENUMS(GROUP_PAN_PARAMS, 4),
		MAIN_FADER_PARAM,
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
	static constexpr float trackFaderScalingExponent = 3.0f; // for example, 3.0f is x^3 scaling 
	static constexpr float trackFaderMaxLinearGain = 2.0f; // for example, 2.0f is +6 dB
	static constexpr float masterFaderScalingExponent = 3.0f; // for example, 3.0f is x^3 scaling 
	static constexpr float masterFaderMaxLinearGain = 3.0f; // for example, 2.0f is +6 dB


	// Need to save, no reset
	int panelTheme;
	
	// Need to save, with reset
	char trackLabels[4 * 20 + 1];// 4 chars per label, 16 tracks and 4 groups means 20 labels, null terminate the end the whole array only
	
	// No need to save, with reset
	int resetTrackLabelRequest;// -1 when nothing to do, 0 to 15 for incremental read in widget
	
	// No need to save, no reset
	RefreshCounter refresh;	

		
	Mixer() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);		
		
		char strBuf[32];
		// Track
		for (int i = 0; i < 16; i++) {
			// Pan
			snprintf(strBuf, 32, "Track #%i pan", i + 1);
			configParam(TRACK_PAN_PARAMS + i, -1.0f, 1.0f, 0.0f, strBuf, "%", 0.0f, 100.0f);
			// Fader
			snprintf(strBuf, 32, "Track #%i level", i + 1);
			configParam(TRACK_FADER_PARAMS + i, 0.0f, std::pow(trackFaderMaxLinearGain, 1.0f / trackFaderScalingExponent), 1.0f, strBuf, " dB", -10, 20.0f * trackFaderScalingExponent);
		}
		// Group
		for (int i = 0; i < 4; i++) {
			// Pan
			snprintf(strBuf, 32, "Group #%i pan", i + 1);
			configParam(GROUP_PAN_PARAMS + i, -1.0f, 1.0f, 0.0f, strBuf, "%", 0.0f, 100.0f);
			// Fader
			snprintf(strBuf, 32, "Group #%i level", i + 1);
			configParam(GROUP_FADER_PARAMS + i, 0.0f, std::pow(trackFaderMaxLinearGain, 1.0f / trackFaderScalingExponent), 1.0f, strBuf, " dB", -10, 20.0f * trackFaderScalingExponent);
		}
		configParam(MAIN_FADER_PARAM, 0.0f, std::pow(masterFaderMaxLinearGain, 1.0f / masterFaderScalingExponent), 1.0f, "Main level", " dB", -10, 20.0f * masterFaderScalingExponent);
		
		onReset();

		panelTheme = 0;//(loadDarkAsDefault() ? 1 : 0);
	}

	
	void onReset() override {
		snprintf(trackLabels, 4 * 20 + 1, "-01--02--03--04--05--06--07--08--09--10--11--12--13--14--15--16-GRP1GRP2GRP3GRP4");		
		resetNonJson();
	}
	void resetNonJson() {
		resetTrackLabelRequest = 0;// setting to 0 will trigger 1, 2, 3 etc on each video frame afterwards
	}


	void onRandomize() override {
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// trackLabels
		json_object_set_new(rootJ, "trackLabels", json_string(trackLabels));

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
		
		resetNonJson();
	}

	
	void process(const ProcessArgs &args) override {
		if (refresh.processInputs()) {

		}// userInputs refresh
		
		
		
		//********** Outputs and lights **********
		
		
		// outputs
		// none
		
		
		// lights
		if (refresh.processLights()) {

		}
		
	}// process()

};


struct TrackDisplay : LedDisplayTextField {
	TrackDisplay() {
		box.size = Vec(38, 16);// 37 no good, will overflow when zoom out too much
		textOffset = Vec(2.6f, -2.2f);
	};
	void draw(const DrawArgs &args) override {// override and do not call LedDisplayTextField.draw() since draw manually here
		if (cursor > 4) {
			text.resize(4);
			cursor = 4;
			selection = 4;
		}
		
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

	MixerWidget(Mixer *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/mixmaster-jr.svg")));
		
		// Tracks
		for (int i = 0; i < 16; i++) {
			// Labels
			addChild(trackDisplays[i] = createWidgetCentered<TrackDisplay>(mm2px(Vec(11.43 + 12.7 * i, 4.2))));
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
		}
		
		// Monitor outputs and groups
		for (int i = 0; i < 4; i++) {
			// Monitor outputs
			addOutput(createDynamicPortCentered<DynPort>(mm2px(Vec(217.17 + 12.7 * i, 12)), false, module, Mixer::MONITOR_OUTPUTS + i, module ? &module->panelTheme : NULL));			

			// Labels
			addChild(trackDisplays[16 + i] = createWidgetCentered<TrackDisplay>(mm2px(Vec(217.17 + 12.7 * i, 22.7))));
			// Volume inputs
			addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(217.17 + 12.7 * i, 30.7)), true, module, Mixer::GROUP_VOL_INPUTS + i, module ? &module->panelTheme : NULL));			
			// Pan inputs
			addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(217.17 + 12.7 * i, 39.7)), true, module, Mixer::GROUP_PAN_INPUTS + i, module ? &module->panelTheme : NULL));			
			// Pan knobs
			addParam(createDynamicParamCentered<DynSmallKnob>(mm2px(Vec(217.17 + 12.7 * i, 51)), module, Mixer::GROUP_PAN_PARAMS + i, module ? &module->panelTheme : NULL));
			
			// Faders
			addParam(createDynamicParamCentered<DynSmallFader>(mm2px(Vec(220.84 + 12.7 * i, 80.4)), module, Mixer::GROUP_FADER_PARAMS + i, module ? &module->panelTheme : NULL));		
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
