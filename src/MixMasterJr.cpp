//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.txt for all licenses
//***********************************************************************************************


#include "Mixer.hpp"
#include "MixerWidgets.hpp"


struct MixMasterJr : Module {
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
	MixerTrack tracks[20];
	
	// No need to save, with reset
	int resetTrackLabelRequest;// -1 when nothing to do, 0 to 15 for incremental read in widget
	
	// No need to save, no reset
	RefreshCounter refresh;	

		
	MixMasterJr() {
		config(NUM_PARAMS_JR, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);		
		
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
		
		for (int i = 0; i < 20; i++) {
			tracks[i].construct(i, &gInfo, &inputs[0], &params[0]);
		}
		onReset();

		panelTheme = 0;//(loadDarkAsDefault() ? 1 : 0);
	}

	
	void onReset() override {
		snprintf(trackLabels, 4 * 20 + 1, "-01--02--03--04--05--06--07--08--09--10--11--12--13--14--15--16-GRP1GRP2GRP3GRP4");		
		gInfo.onReset();
		for (int i = 0; i < 20; i++) {
			tracks[i].onReset();
		}
		resetNonJson();
	}
	void resetNonJson() {
		resetTrackLabelRequest = 0;// setting to 0 will trigger 1, 2, 3 etc on each video frame afterwards
		gInfo.resetNonJson(&params[TRACK_SOLO_PARAMS]);
		for (int i = 0; i < 20; i++) {
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
		for (int i = 0; i < 20; i++) {
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
		for (int i = 0; i < 20; i++) {
			tracks[i].dataFromJson(rootJ);
		}

		resetNonJson();
	}

	
	void process(const ProcessArgs &args) override {
		if (refresh.processInputs()) {
			int trackToProcess = refresh.refreshCounter >> 4;// Corresponds to 172Hz refreshing of each track, at 44.1 kHz
			
			gInfo.updateSoloBit(trackToProcess, params[TRACK_SOLO_PARAMS + trackToProcess].getValue() > 0.5f);
			tracks[trackToProcess].updatePanCoeff();
			
		}// userInputs refresh
		
		
		
		//********** Outputs and lights **********
		float mix[2] = {};

		// Tracks
		for (int i = 0; i < 16; i++) {// groups not done yet so stop at 16 instead of 20
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



struct MixMasterJrWidget : ModuleWidget {
	TrackDisplay* trackDisplays[20];
	unsigned int trackLabelIndexToPush = 0;


	// Module's context menu
	// --------------------

	void appendContextMenu(Menu *menu) override {
		MixMasterJr *module = dynamic_cast<MixMasterJr*>(this->module);
		assert(module);

		menu->addChild(new MenuLabel());// empty line
		
		MenuLabel *settingsLabel = new MenuLabel();
		settingsLabel->text = "Settings";
		menu->addChild(settingsLabel);
		
		PanLawMonoItem *panLawMonoItem = createMenuItem<PanLawMonoItem>("Mono pan law", RIGHT_ARROW);
		panLawMonoItem->gInfo = &(module->gInfo);
		menu->addChild(panLawMonoItem);
		
		PanLawStereoItem *panLawStereoItem = createMenuItem<PanLawStereoItem>("Stereo pan mode", RIGHT_ARROW);
		panLawStereoItem->gInfo = &(module->gInfo);
		menu->addChild(panLawStereoItem);
	}

	// Module's widget
	// --------------------

	MixMasterJrWidget(MixMasterJr *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/mixmaster-jr.svg")));
		
		// Tracks
		for (int i = 0; i < 16; i++) {
			// Labels
			addChild(trackDisplays[i] = createWidgetCentered<TrackDisplay>(mm2px(Vec(11.43 + 12.7 * i + 0.4, 4.2))));
			// Left inputs
			addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(11.43 + 12.7 * i, 12)), true, module, TRACK_SIGNAL_INPUTS + 2 * i + 0, module ? &module->panelTheme : NULL));			
			// Right inputs
			addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(11.43 + 12.7 * i, 21)), true, module, TRACK_SIGNAL_INPUTS + 2 * i + 1, module ? &module->panelTheme : NULL));	
			// Volume inputs
			addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(11.43 + 12.7 * i, 30.7)), true, module, TRACK_VOL_INPUTS + i, module ? &module->panelTheme : NULL));			
			// Pan inputs
			addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(11.43 + 12.7 * i, 39.7)), true, module, TRACK_PAN_INPUTS + i, module ? &module->panelTheme : NULL));			
			// Pan knobs
			addParam(createDynamicParamCentered<DynSmallKnob>(mm2px(Vec(11.43 + 12.7 * i, 51)), module, TRACK_PAN_PARAMS + i, module ? &module->panelTheme : NULL));
			
			// Faders
			addParam(createDynamicParamCentered<DynSmallFader>(mm2px(Vec(15.1 + 12.7 * i, 80.4)), module, TRACK_FADER_PARAMS + i, module ? &module->panelTheme : NULL));
			
			// Mutes
			addParam(createDynamicParamCentered<DynMuteButton>(mm2px(Vec(11.43 + 12.7 * i, 109)), module, TRACK_MUTE_PARAMS + i, module ? &module->panelTheme : NULL));
			// Solos
			addParam(createDynamicParamCentered<DynSoloButton>(mm2px(Vec(11.43 + 12.7 * i, 115.3)), module, TRACK_SOLO_PARAMS + i, module ? &module->panelTheme : NULL));
			// Group dec
			addParam(createDynamicParamCentered<DynGroupMinusButton>(mm2px(Vec(7.7 + 12.7 * i, 122.6)), module, GRP_DEC_PARAMS + i, module ? &module->panelTheme : NULL));
			// Group inc
			addParam(createDynamicParamCentered<DynGroupPlusButton>(mm2px(Vec(15.2 + 12.7 * i, 122.6)), module, GRP_INC_PARAMS + i, module ? &module->panelTheme : NULL));

		}
		
		// Monitor outputs and groups
		for (int i = 0; i < 4; i++) {
			// Monitor outputs
			addOutput(createDynamicPortCentered<DynPort>(mm2px(Vec(217.17 + 12.7 * i, 12)), false, module, MONITOR_OUTPUTS + i, module ? &module->panelTheme : NULL));			

			// Labels
			addChild(trackDisplays[16 + i] = createWidgetCentered<TrackDisplay>(mm2px(Vec(217.17 + 12.7 * i + 0.4, 22.7))));
			// Volume inputs
			addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(217.17 + 12.7 * i, 30.7)), true, module, GROUP_VOL_INPUTS + i, module ? &module->panelTheme : NULL));			
			// Pan inputs
			addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(217.17 + 12.7 * i, 39.7)), true, module, GROUP_PAN_INPUTS + i, module ? &module->panelTheme : NULL));			
			// Pan knobs
			addParam(createDynamicParamCentered<DynSmallKnob>(mm2px(Vec(217.17 + 12.7 * i, 51)), module, GROUP_PAN_PARAMS + i, module ? &module->panelTheme : NULL));
			
			// Faders
			addParam(createDynamicParamCentered<DynSmallFader>(mm2px(Vec(220.84 + 12.7 * i, 80.4)), module, GROUP_FADER_PARAMS + i, module ? &module->panelTheme : NULL));		

			// Mutes
			addParam(createDynamicParamCentered<DynMuteButton>(mm2px(Vec(217.17 + 12.7 * i, 109)), module, GROUP_MUTE_PARAMS + i, module ? &module->panelTheme : NULL));
			// Solos
			addParam(createDynamicParamCentered<DynSoloButton>(mm2px(Vec(217.17 + 12.7 * i, 115.3)), module, GROUP_SOLO_PARAMS + i, module ? &module->panelTheme : NULL));
		}
		
		// Main outputs
		addOutput(createDynamicPortCentered<DynPort>(mm2px(Vec(273.8, 12)), false, module, MAIN_OUTPUTS + 0, module ? &module->panelTheme : NULL));			
		addOutput(createDynamicPortCentered<DynPort>(mm2px(Vec(273.8, 21)), false, module, MAIN_OUTPUTS + 1, module ? &module->panelTheme : NULL));			
		
		// Main fader
		addParam(createDynamicParamCentered<DynBigFader>(mm2px(Vec(277.65, 69.5)), module, MAIN_FADER_PARAM, module ? &module->panelTheme : NULL));		
		
	}
	
	void step() override {
		MixMasterJr* moduleM = (MixMasterJr*)module;
		if (moduleM) {
			// Track labels and other info (push and pull from module)
			int trackLabelIndexToPull = moduleM->resetTrackLabelRequest;
			if (trackLabelIndexToPull >= 0) {// pull request from module
				trackDisplays[trackLabelIndexToPull]->text = std::string(&(moduleM->trackLabels[trackLabelIndexToPull * 4]), 4);
				trackDisplays[trackLabelIndexToPull]->gainAdjust = 20.0f * std::log10(moduleM->tracks[trackLabelIndexToPull].gainAdjust);// convert linear to dB for slider
				moduleM->resetTrackLabelRequest++;
				if (moduleM->resetTrackLabelRequest >= 20) {
					moduleM->resetTrackLabelRequest = -1;// all done pulling
				}
			}
			else {// push to module regularly
				int unsigned i = 0;
				*((uint32_t*)(&(moduleM->trackLabels[trackLabelIndexToPush * 4]))) = 0x20202020;
				for (; i < trackDisplays[trackLabelIndexToPush]->text.length(); i++) {
					moduleM->trackLabels[trackLabelIndexToPush * 4 + i] = trackDisplays[trackLabelIndexToPush]->text[i];
				}
				moduleM->tracks[trackLabelIndexToPush].gainAdjust = std::pow(10.0f, trackDisplays[trackLabelIndexToPush]->gainAdjust / 20.0f);
				trackLabelIndexToPush++;
				if (trackLabelIndexToPush >= 20) {
					trackLabelIndexToPush = 0;
				}
			}
		}
		Widget::step();
	}
};

Model *modelMixMasterJr = createModel<MixMasterJr, MixMasterJrWidget>("MixMaster-Jr");
