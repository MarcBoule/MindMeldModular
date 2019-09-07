//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.txt for all licenses
//***********************************************************************************************

#include "MindMeldModular.hpp"
//#include "MixerWidgets.hpp"


struct AuxExpander : Module {
	enum ParamIds {
		ENUMS(TRACK_SEND_PARAMS, 16 * 4), // 0 = aux A trk 1, 1 = aux B trk 1, ... 4 = aux A trk 2
		ENUMS(GROUP_SEND_PARAMS, 4 * 4),
		ENUMS(TRACK_MUTE_PARAMS, 16),
		ENUMS(GROUP_MUTE_PARAMS, 4),
		ENUMS(GLOBAL_SEND_PARAMS, 4),
		ENUMS(GLOBAL_PAN_PARAMS, 4),
		ENUMS(GLOBAL_RETURN_PARAMS, 4),
		ENUMS(GLOBAL_MUTE_PARAMS, 4),
		ENUMS(GLOBAL_SOLO_PARAMS, 4),
		
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
	float leftMessages[2][23] = {};// messages from mother (first index is page)
	// [0-19]: 20 track/group names, each float has 4 chars
	// [20]: update slow (track/group labels, panelTheme, dispColor)
	// [21]: panelTheme
	// [22]: dispColor


	// Constants
	// none


	// Need to save, no reset
	int panelTheme;
	
	
	// Need to save, with reset
	// none
	
	
	// No need to save, with reset
	
	
	// No need to save, no reset
	bool motherPresent = false;// can't be local to process() since widget must know in order to properly draw border
	char trackLabels[4 * 20 + 1] = "-01--02--03--04--05--06--07--08--09--10--11--12--13--14--15--16-GRP1GRP2GRP3GRP4";// 4 chars per label, 16 tracks and 4 groups means 20 labels, null terminate the end the whole array only
	int dispColor = 0;
	int resetTrackLabelRequest = 0;// 0 when nothing to do, 1 for read names in widget
	
		
	AuxExpander() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);		
		
		leftExpander.producerMessage = leftMessages[0];
		leftExpander.consumerMessage = leftMessages[1];
		
		onReset();

		panelTheme = 0;//(loadDarkAsDefault() ? 1 : 0);

	}
  
	void onReset() override {
		resetNonJson(false);
	}
	void resetNonJson(bool recurseNonJson) {
	}


	void onRandomize() override {
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		return rootJ;
	}


	void dataFromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		resetNonJson(true);
	}



	void process(const ProcessArgs &args) override {
		 motherPresent = (leftExpander.module && leftExpander.module->model == modelMixMaster);
		 
		if (motherPresent) {
			// To Mother
			// float *messagesToMother = (float*)leftExpander.module->rightExpander.producerMessage;
			// for (int i = 0; i < 8; i++) {
				// messagesToMother[i] = inputs[i].getVoltage();
			// }
			// leftExpander.module->rightExpander.messageFlipRequested = true;
			
			// From Mother
			float *messagesFromMother = (float*)leftExpander.consumerMessage;
			uint32_t* updateSlow = (uint32_t*)(&messagesFromMother[20]);
			if (*updateSlow != 0) {
				memcpy(trackLabels, &messagesFromMother[0], 4 * 20);
				resetTrackLabelRequest = 1;
				int32_t tmp;
				memcpy(&tmp, &messagesFromMother[21], 4);
				panelTheme = tmp;
				memcpy(&tmp, &messagesFromMother[22], 4);
				dispColor = tmp;
			}
		}		
		 
		 
	}// process()
};


static const NVGcolor DISP_COLORS[] = {nvgRGB(0xff, 0xd7, 0x14), nvgRGB(102, 183, 245), nvgRGB(140, 235, 107), nvgRGB(240, 240, 240)};// yellow, blue, green, light-gray

struct TrackAndGroupLabel : LedDisplayChoice {
	 int* dispColor = NULL;// TODO make int32_t (here and all over the place where values are passed through floats for exp)
	
	TrackAndGroupLabel() {
		box.size = Vec(38, 16);
		textOffset = Vec(7.5, 12);
		text = "-00-";
	};
	
	void draw(const DrawArgs &args) override {
		if (dispColor) {
			color = DISP_COLORS[*dispColor];
		}	
		LedDisplayChoice::draw(args);
	}
};

struct AuxExpanderWidget : ModuleWidget {
	PanelBorder* panelBorder;
	bool oldMotherPresent = false;
	TrackAndGroupLabel* trackAndGroupLabels[20];

	AuxExpanderWidget(AuxExpander *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/auxspander.svg")));
		panelBorder = findBorder(panel);


		// Left side (globals)
		for (int i = 0; i < 4; i++) {
			// Labels
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
			addParam(createDynamicParamCentered<DynSmallKnobGrey>(mm2px(Vec(6.35 + 12.7 * i, 62.83)), module, AuxExpander::GLOBAL_PAN_PARAMS + i, module ? &module->panelTheme : NULL));
			
			// Return faders
			addParam(createDynamicParamCentered<DynSmallerFader>(mm2px(Vec(6.35 + 3.67 + 12.7 * i, 87.2)), module, AuxExpander::GLOBAL_RETURN_PARAMS + i, module ? &module->panelTheme : NULL));
			
			// Global mute buttons
			addParam(createDynamicParamCentered<DynMuteButton>(mm2px(Vec(6.35  + 12.7 * i, 109.8)), module, AuxExpander::GROUP_MUTE_PARAMS + i, module ? &module->panelTheme : NULL));
			
			// Global solo buttons
			addParam(createDynamicParamCentered<DynSoloButton>(mm2px(Vec(6.35  + 12.7 * i, 116.1)), module, AuxExpander::GLOBAL_SOLO_PARAMS + i, module ? &module->panelTheme : NULL));		

			// Group select
			// same Y as in mixmaster, same X as above
		}

		// Global send knobs
		addParam(createDynamicParamCentered<DynSmallKnobAuxA>(mm2px(Vec(6.35 + 12.7 * 0, 51.8)), module, AuxExpander::GLOBAL_SEND_PARAMS + 0, module ? &module->panelTheme : NULL));
		addParam(createDynamicParamCentered<DynSmallKnobAuxB>(mm2px(Vec(6.35 + 12.7 * 1, 51.8)), module, AuxExpander::GLOBAL_SEND_PARAMS + 1, module ? &module->panelTheme : NULL));
		addParam(createDynamicParamCentered<DynSmallKnobAuxC>(mm2px(Vec(6.35 + 12.7 * 2, 51.8)), module, AuxExpander::GLOBAL_SEND_PARAMS + 2, module ? &module->panelTheme : NULL));
		addParam(createDynamicParamCentered<DynSmallKnobAuxD>(mm2px(Vec(6.35 + 12.7 * 3, 51.8)), module, AuxExpander::GLOBAL_SEND_PARAMS + 3, module ? &module->panelTheme : NULL));


		// Right side (individual tracks)
		for (int i = 0; i < 8; i++) {
			// Labels for tracks 1 to 8
			addChild(trackAndGroupLabels[i] = createWidgetCentered<TrackAndGroupLabel>(mm2px(Vec(67.31 + 12.7 * i, 4.7))));
			if (module) {
				trackAndGroupLabels[i]->dispColor = &(module->dispColor);
			}
			// aux A send for tracks 1 to 8
			addParam(createDynamicParamCentered<DynSmallKnobAuxA>(mm2px(Vec(67.31 + 12.7 * i, 14)), module, AuxExpander::TRACK_SEND_PARAMS + i * 4 + 0, module ? &module->panelTheme : NULL));			
			// aux B send for tracks 1 to 8
			addParam(createDynamicParamCentered<DynSmallKnobAuxB>(mm2px(Vec(67.31 + 12.7 * i, 24.85)), module, AuxExpander::TRACK_SEND_PARAMS + i * 4 + 1, module ? &module->panelTheme : NULL));
			// aux C send for tracks 1 to 8
			addParam(createDynamicParamCentered<DynSmallKnobAuxC>(mm2px(Vec(67.31 + 12.7 * i, 35.7)), module, AuxExpander::TRACK_SEND_PARAMS + i * 4 + 2, module ? &module->panelTheme : NULL));
			// aux D send for tracks 1 to 8
			addParam(createDynamicParamCentered<DynSmallKnobAuxD>(mm2px(Vec(67.31 + 12.7 * i, 46.55)), module, AuxExpander::TRACK_SEND_PARAMS + i * 4 + 3, module ? &module->panelTheme : NULL));
			// mute for tracks 1 to 8
			addParam(createDynamicParamCentered<DynMuteButton>(mm2px(Vec(67.31  + 12.7 * i, 55.7)), module, AuxExpander::TRACK_MUTE_PARAMS + i, module ? &module->panelTheme : NULL));
			
			
			// Labels for tracks 9 to 16
			addChild(trackAndGroupLabels[i + 8] = createWidgetCentered<TrackAndGroupLabel>(mm2px(Vec(67.31 + 12.7 * i, 65.08))));
			if (module) {
				trackAndGroupLabels[i + 8]->dispColor = &(module->dispColor);
			}

			// aux A send for tracks 9 to 16
			addParam(createDynamicParamCentered<DynSmallKnobAuxA>(mm2px(Vec(67.31 + 12.7 * i, 74.5)), module, AuxExpander::TRACK_SEND_PARAMS + (i + 8) * 4 + 0, module ? &module->panelTheme : NULL));			
			// aux B send for tracks 9 to 16
			addParam(createDynamicParamCentered<DynSmallKnobAuxB>(mm2px(Vec(67.31 + 12.7 * i, 85.35)), module, AuxExpander::TRACK_SEND_PARAMS + (i + 8) * 4 + 1, module ? &module->panelTheme : NULL));
			// aux C send for tracks 9 to 16
			addParam(createDynamicParamCentered<DynSmallKnobAuxC>(mm2px(Vec(67.31 + 12.7 * i, 96.2)), module, AuxExpander::TRACK_SEND_PARAMS + (i + 8) * 4 + 2, module ? &module->panelTheme : NULL));
			// aux D send for tracks 9 to 16
			addParam(createDynamicParamCentered<DynSmallKnobAuxD>(mm2px(Vec(67.31 + 12.7 * i, 107.05)), module, AuxExpander::TRACK_SEND_PARAMS + (i + 8) * 4 + 3, module ? &module->panelTheme : NULL));
			// mute for tracks 1 to 8
			addParam(createDynamicParamCentered<DynMuteButton>(mm2px(Vec(67.31  + 12.7 * i, 116.1)), module, AuxExpander::TRACK_MUTE_PARAMS + i + 8, module ? &module->panelTheme : NULL));
		}

		// Right side (individual groups)
		for (int i = 0; i < 2; i++) {
			// Labels for groups 1 to 2
			addChild(trackAndGroupLabels[i + 16] = createWidgetCentered<TrackAndGroupLabel>(mm2px(Vec(171.45 + 12.7 * i, 4.7))));
			if (module) {
				trackAndGroupLabels[i + 16]->dispColor = &(module->dispColor);
			}

			// aux A send for groups 1 to 2
			addParam(createDynamicParamCentered<DynSmallKnobAuxA>(mm2px(Vec(171.45 + 12.7 * i, 14)), module, AuxExpander::GROUP_SEND_PARAMS + i * 4 + 0, module ? &module->panelTheme : NULL));			
			// aux B send for groups 1 to 2
			addParam(createDynamicParamCentered<DynSmallKnobAuxB>(mm2px(Vec(171.45 + 12.7 * i, 24.85)), module, AuxExpander::GROUP_SEND_PARAMS + i * 4 + 1, module ? &module->panelTheme : NULL));
			// aux C send for groups 1 to 2
			addParam(createDynamicParamCentered<DynSmallKnobAuxC>(mm2px(Vec(171.45 + 12.7 * i, 35.7)), module, AuxExpander::GROUP_SEND_PARAMS + i * 4 + 2, module ? &module->panelTheme : NULL));
			// aux D send for groups 1 to 2
			addParam(createDynamicParamCentered<DynSmallKnobAuxD>(mm2px(Vec(171.45 + 12.7 * i, 46.55)), module, AuxExpander::GROUP_SEND_PARAMS + i * 4 + 3, module ? &module->panelTheme : NULL));
			// mute for groups 1 to 2
			addParam(createDynamicParamCentered<DynMuteButton>(mm2px(Vec(171.45  + 12.7 * i, 55.7)), module, AuxExpander::GROUP_MUTE_PARAMS + i, module ? &module->panelTheme : NULL));
			
			
			// Labels for groups 3 to 4
			addChild(trackAndGroupLabels[i + 18] = createWidgetCentered<TrackAndGroupLabel>(mm2px(Vec(171.45 + 12.7 * i, 65.08))));
			if (module) {
				trackAndGroupLabels[i + 18]->dispColor = &(module->dispColor);
			}

			// aux A send for groups 3 to 4
			addParam(createDynamicParamCentered<DynSmallKnobAuxA>(mm2px(Vec(171.45 + 12.7 * i, 74.5)), module, AuxExpander::GROUP_SEND_PARAMS + (i + 2) * 4 + 0, module ? &module->panelTheme : NULL));			
			// aux B send for groups 3 to 4
			addParam(createDynamicParamCentered<DynSmallKnobAuxB>(mm2px(Vec(171.45 + 12.7 * i, 85.35)), module, AuxExpander::GROUP_SEND_PARAMS + (i + 2) * 4 + 1, module ? &module->panelTheme : NULL));
			// aux C send for groups 3 to 4
			addParam(createDynamicParamCentered<DynSmallKnobAuxC>(mm2px(Vec(171.45 + 12.7 * i, 96.2)), module, AuxExpander::GROUP_SEND_PARAMS + (i + 2) * 4 + 2, module ? &module->panelTheme : NULL));
			// aux D send for groups 3 to 4
			addParam(createDynamicParamCentered<DynSmallKnobAuxD>(mm2px(Vec(171.45 + 12.7 * i, 107.05)), module, AuxExpander::GROUP_SEND_PARAMS + (i + 2) * 4 + 3, module ? &module->panelTheme : NULL));
			// mute for groups 3 to 4
			addParam(createDynamicParamCentered<DynMuteButton>(mm2px(Vec(171.45  + 12.7 * i, 116.1)), module, AuxExpander::GROUP_MUTE_PARAMS + i + 2, module ? &module->panelTheme : NULL));
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
