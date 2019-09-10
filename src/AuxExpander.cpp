//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.txt for all licenses
//***********************************************************************************************


#include "MixerWidgets.hpp"


struct AuxExpander : Module {
	enum ParamIds {
		ENUMS(TRACK_AUXSEND_PARAMS, 16 * 4), // trk 1 aux A, trk 1 aux B, ... 
		ENUMS(GROUP_AUXSEND_PARAMS, 4 * 4),
		ENUMS(TRACK_AUXMUTE_PARAMS, 16),
		ENUMS(GROUP_AUXMUTE_PARAMS, 4),
		ENUMS(GLOBAL_AUXSEND_PARAMS, 4),
		ENUMS(GLOBAL_AUXPAN_PARAMS, 4),
		ENUMS(GLOBAL_AUXRETURN_PARAMS, 4),
		ENUMS(GLOBAL_AUXMUTE_PARAMS, 4),
		ENUMS(GLOBAL_AUXSOLO_PARAMS, 4),
		ENUMS(GLOBAL_AUXGROUP_PARAMS, 4),
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(RETURN_INPUTS, 2 * 4),
		ENUMS(POLY_AUX_AD_CV_INPUTS, 4),
		POLY_AUX_M_CV_INPUT,
		POLY_GRPS_CV_INPUT,
		POLY_EXTRA_CV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(SEND_OUTPUTS, 2 * 4), // aux A is 0=L and 4=R, aux B is 1=L and 5=R, etc...
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	// Expander
	float leftMessages[2][AFM_NUM_VALUES] = {};// messages from mother (first index is page)
	// [0-19]: 20 track/group names, each float has 4 chars
	// [20]: update slow (track/group labels, panelTheme, colorAndCloak)
	// [21]: panelTheme
	// [22]: colorAndCloak


	// Constants
	// none


	// Need to save, no reset
	int panelTheme;
	
	
	// Need to save, with reset
	VuMeterAll vu[8];// use post[]. aux A left, aux A right, aux B left, ...
	
	
	// No need to save, with reset
	
	
	// No need to save, no reset
	bool motherPresent = false;// can't be local to process() since widget must know in order to properly draw border
	alignas(4) char trackLabels[4 * 20 + 1] = "-01--02--03--04--05--06--07--08--09--10--11--12--13--14--15--16-GRP1GRP2GRP3GRP4";// 4 chars per label, 16 tracks and 4 groups means 20 labels, null terminate the end the whole array only
	ColorAndCloak colorAndCloak;
	int resetTrackLabelRequest = 0;// 0 when nothing to do, 1 for read names in widget
	
		
	AuxExpander() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);		
		
		leftExpander.producerMessage = leftMessages[0];
		leftExpander.consumerMessage = leftMessages[1];
		
		char strBuf[32];

		float maxAGFader = std::pow(GlobalInfo::individualAuxSendMaxLinearGain, 1.0f / GlobalInfo::individualAuxSendScalingExponent);
		for (int i = 0; i < 16; i++) {
			// Track send aux A
			snprintf(strBuf, 32, "Track #%i aux send A", i + 1);
			configParam(TRACK_AUXSEND_PARAMS + i * 4 + 0, 0.0f, maxAGFader, 1.0f, strBuf, " dB", -10, 20.0f * GlobalInfo::individualAuxSendScalingExponent);
			// Track send aux B
			snprintf(strBuf, 32, "Track #%i aux send B", i + 1);
			configParam(TRACK_AUXSEND_PARAMS + i * 4 + 1, 0.0f, maxAGFader, 1.0f, strBuf, " dB", -10, 20.0f * GlobalInfo::individualAuxSendScalingExponent);
			// Track send aux C
			snprintf(strBuf, 32, "Track #%i aux send C", i + 1);
			configParam(TRACK_AUXSEND_PARAMS + i * 4 + 2, 0.0f, maxAGFader, 1.0f, strBuf, " dB", -10, 20.0f * GlobalInfo::individualAuxSendScalingExponent);
			// Track send aux D
			snprintf(strBuf, 32, "Track #%i aux send D", i + 1);
			configParam(TRACK_AUXSEND_PARAMS + i * 4 + 3, 0.0f, maxAGFader, 1.0f, strBuf, " dB", -10, 20.0f * GlobalInfo::individualAuxSendScalingExponent);
			// Mute
			snprintf(strBuf, 32, "Track #%i aux send mute", i + 1);
			configParam(TRACK_AUXMUTE_PARAMS + i, 0.0f, 1.0f, 0.0f, strBuf);
		}
		for (int i = 0; i < 4; i++) {
			// Group send aux A
			snprintf(strBuf, 32, "Group #%i aux send A", i + 1);
			configParam(GROUP_AUXSEND_PARAMS + i * 4 + 0, 0.0f, maxAGFader, 1.0f, strBuf, " dB", -10, 20.0f * GlobalInfo::individualAuxSendScalingExponent);
			// Group send aux B
			snprintf(strBuf, 32, "Group #%i aux send B", i + 1);
			configParam(GROUP_AUXSEND_PARAMS + i * 4 + 1, 0.0f, maxAGFader, 1.0f, strBuf, " dB", -10, 20.0f * GlobalInfo::individualAuxSendScalingExponent);
			// Group send aux C
			snprintf(strBuf, 32, "Group #%i aux send C", i + 1);
			configParam(GROUP_AUXSEND_PARAMS + i * 4 + 2, 0.0f, maxAGFader, 1.0f, strBuf, " dB", -10, 20.0f * GlobalInfo::individualAuxSendScalingExponent);
			// Group send aux D
			snprintf(strBuf, 32, "Group #%i aux send D", i + 1);
			configParam(GROUP_AUXSEND_PARAMS + i * 4 + 3, 0.0f, maxAGFader, 1.0f, strBuf, " dB", -10, 20.0f * GlobalInfo::individualAuxSendScalingExponent);
			// Mute
			snprintf(strBuf, 32, "Group #%i aux send mute", i + 1);
			configParam(GROUP_AUXMUTE_PARAMS + i, 0.0f, 1.0f, 0.0f, strBuf);		
		}

		// Global send aux A-D
		maxAGFader = std::pow(GlobalInfo::globalAuxSendMaxLinearGain, 1.0f / GlobalInfo::globalAuxSendScalingExponent);
		configParam(GLOBAL_AUXSEND_PARAMS + 0, 0.0f, maxAGFader, 1.0f, "Global aux send A", " dB", -10, 20.0f * GlobalInfo::globalAuxSendScalingExponent);
		configParam(GLOBAL_AUXSEND_PARAMS + 1, 0.0f, maxAGFader, 1.0f, "Global aux send B", " dB", -10, 20.0f * GlobalInfo::globalAuxSendScalingExponent);
		configParam(GLOBAL_AUXSEND_PARAMS + 2, 0.0f, maxAGFader, 1.0f, "Global aux send C", " dB", -10, 20.0f * GlobalInfo::globalAuxSendScalingExponent);
		configParam(GLOBAL_AUXSEND_PARAMS + 3, 0.0f, maxAGFader, 1.0f, "Global aux send D", " dB", -10, 20.0f * GlobalInfo::globalAuxSendScalingExponent);

		// Global pan return aux A-D
		configParam(GLOBAL_AUXPAN_PARAMS + 0, 0.0f, 1.0f, 0.5f, "Global aux return pan A", "%", 0.0f, 200.0f, -100.0f);
		configParam(GLOBAL_AUXPAN_PARAMS + 1, 0.0f, 1.0f, 0.5f, "Global aux return pan B", "%", 0.0f, 200.0f, -100.0f);
		configParam(GLOBAL_AUXPAN_PARAMS + 2, 0.0f, 1.0f, 0.5f, "Global aux return pan C", "%", 0.0f, 200.0f, -100.0f);
		configParam(GLOBAL_AUXPAN_PARAMS + 3, 0.0f, 1.0f, 0.5f, "Global aux return pan D", "%", 0.0f, 200.0f, -100.0f);

		// Global return aux A-D
		maxAGFader = std::pow(GlobalInfo::globalAuxReturnMaxLinearGain, 1.0f / GlobalInfo::globalAuxReturnScalingExponent);
		configParam(GLOBAL_AUXRETURN_PARAMS + 0, 0.0f, maxAGFader, 1.0f, "Global aux return A", " dB", -10, 20.0f * GlobalInfo::globalAuxReturnScalingExponent);
		configParam(GLOBAL_AUXRETURN_PARAMS + 1, 0.0f, maxAGFader, 1.0f, "Global aux return B", " dB", -10, 20.0f * GlobalInfo::globalAuxReturnScalingExponent);
		configParam(GLOBAL_AUXRETURN_PARAMS + 2, 0.0f, maxAGFader, 1.0f, "Global aux return C", " dB", -10, 20.0f * GlobalInfo::globalAuxReturnScalingExponent);
		configParam(GLOBAL_AUXRETURN_PARAMS + 3, 0.0f, maxAGFader, 1.0f, "Global aux return D", " dB", -10, 20.0f * GlobalInfo::globalAuxReturnScalingExponent);

		// Global mute
		configParam(GLOBAL_AUXMUTE_PARAMS + 0, 0.0f, 1.0f, 0.0f, "Global aux send mute A");		
		configParam(GLOBAL_AUXMUTE_PARAMS + 1, 0.0f, 1.0f, 0.0f, "Global aux send mute B");		
		configParam(GLOBAL_AUXMUTE_PARAMS + 2, 0.0f, 1.0f, 0.0f, "Global aux send mute C");		
		configParam(GLOBAL_AUXMUTE_PARAMS + 3, 0.0f, 1.0f, 0.0f, "Global aux send mute D");		

		// Global solo
		configParam(GLOBAL_AUXSOLO_PARAMS + 0, 0.0f, 1.0f, 0.0f, "Global aux send solo A");		
		configParam(GLOBAL_AUXSOLO_PARAMS + 1, 0.0f, 1.0f, 0.0f, "Global aux send solo B");		
		configParam(GLOBAL_AUXSOLO_PARAMS + 2, 0.0f, 1.0f, 0.0f, "Global aux send solo C");		
		configParam(GLOBAL_AUXSOLO_PARAMS + 3, 0.0f, 1.0f, 0.0f, "Global aux send solo D");		


		colorAndCloak.cc1 = 0;
		
		onReset();

		panelTheme = 0;//(loadDarkAsDefault() ? 1 : 0);

	}
  
	void onReset() override {
		for (int i = 0; i < 4; i++) {
			vu[i << 1].vuColorTheme = 0;// vu[1], vu[3], vu[5], vu[7]  color theme is not used
		}
		resetNonJson(false);
	}
	void resetNonJson(bool recurseNonJson) {
		for (int i = 0; i < 8; i++) {
			vu[i].reset();
		}
	}


	void onRandomize() override {
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// vuColorTheme
		for (int i = 0; i < 4; i++ ) {
			json_object_set_new(rootJ, ("vuColorTheme" + i), json_integer(vu[i << 1].vuColorTheme));
		}
		
		return rootJ;
	}


	void dataFromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// vuColorTheme
		for (int i = 0; i < 4; i++ ) {
			json_t *vuColorThemeJ = json_object_get(rootJ, "vuColorTheme" + i);
			if (vuColorThemeJ)
				vu[i << 1].vuColorTheme = json_integer_value(vuColorThemeJ);
		}
		
		resetNonJson(true);
	}



	void process(const ProcessArgs &args) override {
		motherPresent = (leftExpander.module && leftExpander.module->model == modelMixMaster);
		float *messagesFromMother = (float*)leftExpander.consumerMessage;
		 
		if (motherPresent) {
			// To Mother
			// float *messagesToMother = (float*)leftExpander.module->rightExpander.producerMessage;
			// for (int i = 0; i < 8; i++) {
				// messagesToMother[i] = inputs[i].getVoltage();
			// }
			// leftExpander.module->rightExpander.messageFlipRequested = true;
			
			// From Mother
			
			// Slow values from mother
			uint32_t* updateSlow = (uint32_t*)(&messagesFromMother[AFM_UPDATE_SLOW]);
			if (*updateSlow != 0) {
				memcpy(trackLabels, &messagesFromMother[AFM_TRACK_GROUP_NAMES], 4 * 20);
				resetTrackLabelRequest = 1;
				int32_t tmp;
				memcpy(&tmp, &messagesFromMother[AFM_PANEL_THEME], 4);
				panelTheme = tmp;
				memcpy(&colorAndCloak.cc1, &messagesFromMother[AFM_COLOR_AND_CLOAK], 4);
			}
			
			// Fast values from mother
			// VUS: leave is messagesFromMother, nothing to do
			
		}	


		// VUs
		if (!motherPresent || colorAndCloak.cc4[cloakedMode]) {
			for (int i = 0; i < 8; i++) {
				vu[i].reset();
			}
		}
		else {
			for (int i = 0; i < 8; i++) {
				vu[i].process(args.sampleTime, messagesFromMother[AFM_AUX_VUS + i]);
			}
		}
		
	}// process()
};


struct AuxExpanderWidget : ModuleWidget {
	PanelBorder* panelBorder;
	bool oldMotherPresent = false;
	TrackAndGroupLabel* trackAndGroupLabels[20];
	AuxDisplay* auxDisplays[4];

	AuxExpanderWidget(AuxExpander *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/auxspander.svg")));
		panelBorder = findBorder(panel);


		// Left side (globals)
		for (int i = 0; i < 4; i++) {
			// Labels
			addChild(auxDisplays[i] = createWidgetCentered<AuxDisplay>(mm2px(Vec(6.35 + 12.7 * i, 4.7))));
			if (module) {
				auxDisplays[i]->colorAndCloak = &(module->colorAndCloak);
				auxDisplays[i]->srcVus = &(module->vu[0]);
				auxDisplays[i]->auxNumber = i;
				char buf[5] = "GRP0";
				buf[3] += i;
				auxDisplays[i]->text = buf;
			}
			// Y is 4.7, same X as below
			
			// Left sends
			addOutput(createDynamicPortCentered<DynPort>(mm2px(Vec(6.35 + 12.7 * i, 12.8)), false, module, AuxExpander::SEND_OUTPUTS + i, module ? &module->panelTheme : NULL));			
			// Right sends
			addOutput(createDynamicPortCentered<DynPort>(mm2px(Vec(6.35 + 12.7 * i, 21.8)), false, module, AuxExpander::SEND_OUTPUTS + 4 + i, module ? &module->panelTheme : NULL));

			// Left returns
			addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(6.35 + 12.7 * i, 31.5)), true, module, AuxExpander::RETURN_INPUTS + i, module ? &module->panelTheme : NULL));			
			// Right returns
			addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(6.35 + 12.7 * i, 40.5)), true, module, AuxExpander::RETURN_INPUTS + 4 + i, module ? &module->panelTheme : NULL));			
			
			// Pan knobs
			addParam(createDynamicParamCentered<DynSmallKnobGrey>(mm2px(Vec(6.35 + 12.7 * i, 62.83)), module, AuxExpander::GLOBAL_AUXPAN_PARAMS + i, module ? &module->panelTheme : NULL));
			
			// Return faders
			addParam(createDynamicParamCentered<DynSmallerFader>(mm2px(Vec(6.35 + 3.67 + 12.7 * i, 87.2)), module, AuxExpander::GLOBAL_AUXRETURN_PARAMS + i, module ? &module->panelTheme : NULL));
			if (module) {
				// VU meters
				VuMeterAux *newVU = createWidgetCentered<VuMeterAux>(mm2px(Vec(6.35 + 12.7 * i, 87.2)));
				newVU->srcLevels = &(module->vu[i << 1]);
				newVU->colorThemeGlobal = &(module->colorAndCloak.cc4[vuColor]);
				addChild(newVU);
			}				
			
			// Global mute buttons
			addParam(createDynamicParamCentered<DynMuteButton>(mm2px(Vec(6.35  + 12.7 * i, 109.8)), module, AuxExpander::GLOBAL_AUXMUTE_PARAMS + i, module ? &module->panelTheme : NULL));
			
			// Global solo buttons
			addParam(createDynamicParamCentered<DynSoloButton>(mm2px(Vec(6.35  + 12.7 * i, 116.1)), module, AuxExpander::GLOBAL_AUXSOLO_PARAMS + i, module ? &module->panelTheme : NULL));		

			// Group dec
			DynGroupMinusButtonNotify *newGrpMinusButton;
			addChild(newGrpMinusButton = createDynamicWidgetCentered<DynGroupMinusButtonNotify>(mm2px(Vec(6.35 - 3.73 + 12.7 * i - 0.75, 123.1)), module ? &module->panelTheme : NULL));
			if (module) {
				newGrpMinusButton->sourceParam = &(module->params[AuxExpander::GLOBAL_AUXGROUP_PARAMS + i]);
			}
			// Group inc
			DynGroupPlusButtonNotify *newGrpPlusButton;
			addChild(newGrpPlusButton = createDynamicWidgetCentered<DynGroupPlusButtonNotify>(mm2px(Vec(6.35 + 3.77 + 12.7 * i + 0.75, 123.1)), module ? &module->panelTheme : NULL));
			if (module) {
				newGrpPlusButton->sourceParam = &(module->params[AuxExpander::GLOBAL_AUXGROUP_PARAMS + i]);
			}
			// Group select displays
			GroupSelectDisplay* groupSelectDisplay;
			addParam(groupSelectDisplay = createParamCentered<GroupSelectDisplay>(mm2px(Vec(6.35 + 12.7 * i - 0.1, 123.1)), module, AuxExpander::GLOBAL_AUXGROUP_PARAMS + i));
			if (module) {
				groupSelectDisplay->srcColor = &(module->colorAndCloak);
			}
		}

		// Global send knobs
		addParam(createDynamicParamCentered<DynSmallKnobAuxA>(mm2px(Vec(6.35 + 12.7 * 0, 51.8)), module, AuxExpander::GLOBAL_AUXSEND_PARAMS + 0, module ? &module->panelTheme : NULL));
		addParam(createDynamicParamCentered<DynSmallKnobAuxB>(mm2px(Vec(6.35 + 12.7 * 1, 51.8)), module, AuxExpander::GLOBAL_AUXSEND_PARAMS + 1, module ? &module->panelTheme : NULL));
		addParam(createDynamicParamCentered<DynSmallKnobAuxC>(mm2px(Vec(6.35 + 12.7 * 2, 51.8)), module, AuxExpander::GLOBAL_AUXSEND_PARAMS + 2, module ? &module->panelTheme : NULL));
		addParam(createDynamicParamCentered<DynSmallKnobAuxD>(mm2px(Vec(6.35 + 12.7 * 3, 51.8)), module, AuxExpander::GLOBAL_AUXSEND_PARAMS + 3, module ? &module->panelTheme : NULL));


		// Right side (individual tracks)
		for (int i = 0; i < 8; i++) {
			// Labels for tracks 1 to 8
			addChild(trackAndGroupLabels[i] = createWidgetCentered<TrackAndGroupLabel>(mm2px(Vec(67.31 + 12.7 * i, 4.7))));
			if (module) {
				trackAndGroupLabels[i]->dispColor = &(module->colorAndCloak.cc4[dispColor]);
			}
			// aux A send for tracks 1 to 8
			addParam(createDynamicParamCentered<DynSmallKnobAuxA>(mm2px(Vec(67.31 + 12.7 * i, 14)), module, AuxExpander::TRACK_AUXSEND_PARAMS + i * 4 + 0, module ? &module->panelTheme : NULL));			
			// aux B send for tracks 1 to 8
			addParam(createDynamicParamCentered<DynSmallKnobAuxB>(mm2px(Vec(67.31 + 12.7 * i, 24.85)), module, AuxExpander::TRACK_AUXSEND_PARAMS + i * 4 + 1, module ? &module->panelTheme : NULL));
			// aux C send for tracks 1 to 8
			addParam(createDynamicParamCentered<DynSmallKnobAuxC>(mm2px(Vec(67.31 + 12.7 * i, 35.7)), module, AuxExpander::TRACK_AUXSEND_PARAMS + i * 4 + 2, module ? &module->panelTheme : NULL));
			// aux D send for tracks 1 to 8
			addParam(createDynamicParamCentered<DynSmallKnobAuxD>(mm2px(Vec(67.31 + 12.7 * i, 46.55)), module, AuxExpander::TRACK_AUXSEND_PARAMS + i * 4 + 3, module ? &module->panelTheme : NULL));
			// mute for tracks 1 to 8
			addParam(createDynamicParamCentered<DynMuteButton>(mm2px(Vec(67.31  + 12.7 * i, 55.7)), module, AuxExpander::TRACK_AUXMUTE_PARAMS + i, module ? &module->panelTheme : NULL));
			
			
			// Labels for tracks 9 to 16
			addChild(trackAndGroupLabels[i + 8] = createWidgetCentered<TrackAndGroupLabel>(mm2px(Vec(67.31 + 12.7 * i, 65.08))));
			if (module) {
				trackAndGroupLabels[i + 8]->dispColor = &(module->colorAndCloak.cc4[dispColor]);
			}

			// aux A send for tracks 9 to 16
			addParam(createDynamicParamCentered<DynSmallKnobAuxA>(mm2px(Vec(67.31 + 12.7 * i, 74.5)), module, AuxExpander::TRACK_AUXSEND_PARAMS + (i + 8) * 4 + 0, module ? &module->panelTheme : NULL));			
			// aux B send for tracks 9 to 16
			addParam(createDynamicParamCentered<DynSmallKnobAuxB>(mm2px(Vec(67.31 + 12.7 * i, 85.35)), module, AuxExpander::TRACK_AUXSEND_PARAMS + (i + 8) * 4 + 1, module ? &module->panelTheme : NULL));
			// aux C send for tracks 9 to 16
			addParam(createDynamicParamCentered<DynSmallKnobAuxC>(mm2px(Vec(67.31 + 12.7 * i, 96.2)), module, AuxExpander::TRACK_AUXSEND_PARAMS + (i + 8) * 4 + 2, module ? &module->panelTheme : NULL));
			// aux D send for tracks 9 to 16
			addParam(createDynamicParamCentered<DynSmallKnobAuxD>(mm2px(Vec(67.31 + 12.7 * i, 107.05)), module, AuxExpander::TRACK_AUXSEND_PARAMS + (i + 8) * 4 + 3, module ? &module->panelTheme : NULL));
			// mute for tracks 1 to 8
			addParam(createDynamicParamCentered<DynMuteButton>(mm2px(Vec(67.31  + 12.7 * i, 116.1)), module, AuxExpander::TRACK_AUXMUTE_PARAMS + i + 8, module ? &module->panelTheme : NULL));
		}

		// Right side (individual groups)
		for (int i = 0; i < 2; i++) {
			// Labels for groups 1 to 2
			addChild(trackAndGroupLabels[i + 16] = createWidgetCentered<TrackAndGroupLabel>(mm2px(Vec(171.45 + 12.7 * i, 4.7))));
			if (module) {
				trackAndGroupLabels[i + 16]->dispColor = &(module->colorAndCloak.cc4[dispColor]);
			}

			// aux A send for groups 1 to 2
			addParam(createDynamicParamCentered<DynSmallKnobAuxA>(mm2px(Vec(171.45 + 12.7 * i, 14)), module, AuxExpander::GROUP_AUXSEND_PARAMS + i * 4 + 0, module ? &module->panelTheme : NULL));			
			// aux B send for groups 1 to 2
			addParam(createDynamicParamCentered<DynSmallKnobAuxB>(mm2px(Vec(171.45 + 12.7 * i, 24.85)), module, AuxExpander::GROUP_AUXSEND_PARAMS + i * 4 + 1, module ? &module->panelTheme : NULL));
			// aux C send for groups 1 to 2
			addParam(createDynamicParamCentered<DynSmallKnobAuxC>(mm2px(Vec(171.45 + 12.7 * i, 35.7)), module, AuxExpander::GROUP_AUXSEND_PARAMS + i * 4 + 2, module ? &module->panelTheme : NULL));
			// aux D send for groups 1 to 2
			addParam(createDynamicParamCentered<DynSmallKnobAuxD>(mm2px(Vec(171.45 + 12.7 * i, 46.55)), module, AuxExpander::GROUP_AUXSEND_PARAMS + i * 4 + 3, module ? &module->panelTheme : NULL));
			// mute for groups 1 to 2
			addParam(createDynamicParamCentered<DynMuteButton>(mm2px(Vec(171.45  + 12.7 * i, 55.7)), module, AuxExpander::GROUP_AUXMUTE_PARAMS + i, module ? &module->panelTheme : NULL));
			
			
			// Labels for groups 3 to 4
			addChild(trackAndGroupLabels[i + 18] = createWidgetCentered<TrackAndGroupLabel>(mm2px(Vec(171.45 + 12.7 * i, 65.08))));
			if (module) {
				trackAndGroupLabels[i + 18]->dispColor = &(module->colorAndCloak.cc4[dispColor]);
			}

			// aux A send for groups 3 to 4
			addParam(createDynamicParamCentered<DynSmallKnobAuxA>(mm2px(Vec(171.45 + 12.7 * i, 74.5)), module, AuxExpander::GROUP_AUXSEND_PARAMS + (i + 2) * 4 + 0, module ? &module->panelTheme : NULL));			
			// aux B send for groups 3 to 4
			addParam(createDynamicParamCentered<DynSmallKnobAuxB>(mm2px(Vec(171.45 + 12.7 * i, 85.35)), module, AuxExpander::GROUP_AUXSEND_PARAMS + (i + 2) * 4 + 1, module ? &module->panelTheme : NULL));
			// aux C send for groups 3 to 4
			addParam(createDynamicParamCentered<DynSmallKnobAuxC>(mm2px(Vec(171.45 + 12.7 * i, 96.2)), module, AuxExpander::GROUP_AUXSEND_PARAMS + (i + 2) * 4 + 2, module ? &module->panelTheme : NULL));
			// aux D send for groups 3 to 4
			addParam(createDynamicParamCentered<DynSmallKnobAuxD>(mm2px(Vec(171.45 + 12.7 * i, 107.05)), module, AuxExpander::GROUP_AUXSEND_PARAMS + (i + 2) * 4 + 3, module ? &module->panelTheme : NULL));
			// mute for groups 3 to 4
			addParam(createDynamicParamCentered<DynMuteButton>(mm2px(Vec(171.45  + 12.7 * i, 116.1)), module, AuxExpander::GROUP_AUXMUTE_PARAMS + i + 2, module ? &module->panelTheme : NULL));
		}
		
		// CV inputs A-D
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(204.62, 14)), true, module, AuxExpander::POLY_AUX_AD_CV_INPUTS + 0, module ? &module->panelTheme : NULL));			
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(204.62, 24.85)), true, module, AuxExpander::POLY_AUX_AD_CV_INPUTS + 1, module ? &module->panelTheme : NULL));			
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(204.62, 35.7)), true, module, AuxExpander::POLY_AUX_AD_CV_INPUTS + 2, module ? &module->panelTheme : NULL));			
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(204.62, 46.55)), true, module, AuxExpander::POLY_AUX_AD_CV_INPUTS + 3, module ? &module->panelTheme : NULL));	
		
		// CV input M
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(204.62, 57.2)), true, module, AuxExpander::POLY_AUX_M_CV_INPUT, module ? &module->panelTheme : NULL));	
		
		// CV input grp A-D
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(204.62, 74.5)), true, module, AuxExpander::POLY_GRPS_CV_INPUT, module ? &module->panelTheme : NULL));	
		
		// CV input extra
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(204.62, 96.2)), true, module, AuxExpander::POLY_EXTRA_CV_INPUT, module ? &module->panelTheme : NULL));	
	
	}
	
	
	json_t* toJson() override {
		json_t* rootJ = ModuleWidget::toJson();

		// aux0
		json_object_set_new(rootJ, "aux0", json_string(auxDisplays[0]->text.c_str()));
		// aux1
		json_object_set_new(rootJ, "aux1", json_string(auxDisplays[1]->text.c_str()));
		// aux2
		json_object_set_new(rootJ, "aux2", json_string(auxDisplays[2]->text.c_str()));
		// aux3
		json_object_set_new(rootJ, "aux3", json_string(auxDisplays[3]->text.c_str()));

		return rootJ;
	}

	void fromJson(json_t* rootJ) override {
		ModuleWidget::fromJson(rootJ);

		// aux0
		json_t* aux0J = json_object_get(rootJ, "aux0");
		if (aux0J)
			auxDisplays[0]->text = json_string_value(aux0J);
		
		// aux1
		json_t* aux1J = json_object_get(rootJ, "aux1");
		if (aux1J)
			auxDisplays[1]->text = json_string_value(aux1J);

		// aux2
		json_t* aux2J = json_object_get(rootJ, "aux2");
		if (aux2J)
			auxDisplays[2]->text = json_string_value(aux2J);
		
		// aux3
		json_t* aux3J = json_object_get(rootJ, "aux3");
		if (aux3J)
			auxDisplays[3]->text = json_string_value(aux3J);
	}


	void step() override {
		if (module) {
			AuxExpander* moduleA = (AuxExpander*)module;
			
			// Track labels (pull from module)
			if (moduleA->resetTrackLabelRequest >= 1) {// pull request from module
				// track and group labels
				for (int trk = 0; trk < 20; trk++) {
					trackAndGroupLabels[trk]->text = std::string(&(moduleA->trackLabels[trk * 4]), 4);
				}
				moduleA->resetTrackLabelRequest = 0;// all done pulling
			}
			
			// Borders			
			if ( moduleA->motherPresent != oldMotherPresent ) {
				oldMotherPresent = moduleA->motherPresent;
				if (oldMotherPresent) {
					panelBorder->box.pos.x = -3;
					panelBorder->box.size.x = box.size.x + 3;
				}
				else {
					panelBorder->box.pos.x = 0;
					panelBorder->box.size.x = box.size.x;
				}
				((SvgPanel*)panel)->dirty = true;// weird zoom bug: if the if/else above is commented, zoom bug when this executes
			}
		}
		Widget::step();
	}
	
};

Model *modelAuxExpander = createModel<AuxExpander, AuxExpanderWidget>("AuxExpander");
