//***********************************************************************************************
//EQ module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "EqWidgets.hpp"


struct EqMaster : Module {
	
	enum ParamIds {
		TRACK_PARAM,
		ACTIVE_PARAM,
		ENUMS(FREQ_ACTIVE_PARAMS, 4),
		ENUMS(FREQ_PARAMS, 4),
		ENUMS(GAIN_PARAMS, 4),
		ENUMS(Q_PARAMS, 4),
		LOW_BP_PARAM,
		HIGH_BP_PARAM,
		NUM_PARAMS
	};
	
	enum InputIds {
		ENUMS(SIG_INPUTS, 3),
		NUM_INPUTS
	};
	
	enum OutputIds {
		ENUMS(SIG_OUTPUTS, 3),
		NUM_OUTPUTS
	};
	
	enum LightIds {
		NUM_LIGHTS
	};
	

	// Constants
	int numChannels16 = 16;// avoids warning that happens when hardcode 16 (static const or directly use 16 in code below)

	// Need to save, no reset
	int panelTheme;
	
	// Need to save, with reset
	int mappedId;// 0 means manual
	char trackLabels[24 * 4 + 1];// needs to be saved in case we are detached
	TrackEq trackEqs[24];
	
	// No need to save, with reset
	int updateTrackLabelRequest;// 0 when nothing to do, 1 for read names in widget

	// No need to save, no reset
	RefreshCounter refresh;
	
	
	int getSelectedTrack() {
		return (int)(params[TRACK_PARAM].getValue() + 0.5f);
	}

	void initTrackLabels() {
		for (int trk = 0; trk < 24; trk++) {
			snprintf(&trackLabels[trk << 2], 5, "-%02i-", trk + 1);
		}
	}
	
		
	EqMaster() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		// Track knob
		configParam(TRACK_PARAM, 0.0f, 23.0f, 0.0f, "Track", "", 0.0f, 1.0f, 1.0f);//min, max, default, label = "", unit = "", displayBase = 0.f, displayMultiplier = 1.f, displayOffset = 0.f
		
		// Track settings
		configParam(ACTIVE_PARAM, 0.0f, 1.0f, DEFAULT_active ? 1.0f : 0.0f, "Active");
		for (int i = 0; i < 4; i++) {
			configParam(FREQ_ACTIVE_PARAMS + i, 0.0f, 1.0f, DEFAULT_freqActive ? 1.0f : 0.0f, "FreqActive");
			configParam(FREQ_PARAMS + i, 0.0f, 1.0f, DEFAULT_freq, "Freq");
			configParam(GAIN_PARAMS + i, 0.0f, 1.0f, DEFAULT_gain, "Gain");
			configParam(Q_PARAMS + i, 0.0f, 1.0f, DEFAULT_q, "Q");
		}
		configParam(LOW_BP_PARAM, 0.0f, 1.0f, DEFAULT_lowBP ? 1.0f : 0.0f, "Low is band pass");
		configParam(HIGH_BP_PARAM, 0.0f, 1.0f, DEFAULT_highBP ? 1.0f : 0.0f, "High is band pass");
		
		onReset();
		
		panelTheme = 0;
	}
  
	void onReset() override {
		mappedId = 0;
		initTrackLabels();
		for (int t = 0; t < 24; t++) {
			trackEqs[t].init();
		}
		resetNonJson();
	}
	void resetNonJson() {
		updateTrackLabelRequest = 1;
	}


	void onRandomize() override {
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));
				
		// mappedId
		json_object_set_new(rootJ, "mappedId", json_integer(mappedId));
				
		// trackLabels
		json_object_set_new(rootJ, "trackLabels", json_string(trackLabels));
		
		// trackEqs
		// -------------
		
		// active
		json_t *activeJ = json_array();
		for (int t = 0; t < 24; t++) {
			json_array_insert_new(activeJ, t, json_boolean(trackEqs[t].getActive()));
		}
		json_object_set_new(rootJ, "active", activeJ);		
		
		// freqActive
		json_t *freqActiveJ = json_array();
		for (int t = 0; t < 24; t++) {
			for (int f = 0; f < 4; f++) {
				json_array_insert_new(freqActiveJ, (t << 2) | f, json_boolean(trackEqs[t].getFreqActive(f)));
			}
		}
		json_object_set_new(rootJ, "freqActive", freqActiveJ);		
		
		// freq
		json_t *freqJ = json_array();
		for (int t = 0; t < 24; t++) {
			for (int f = 0; f < 4; f++) {
				json_array_insert_new(freqJ, (t << 2) | f, json_real(trackEqs[t].getFreq(f)));
			}
		}
		json_object_set_new(rootJ, "freq", freqJ);		
		
		// gain
		json_t *gainJ = json_array();
		for (int t = 0; t < 24; t++) {
			for (int f = 0; f < 4; f++) {
				json_array_insert_new(gainJ, (t << 2) | f, json_real(trackEqs[t].getGain(f)));
			}
		}
		json_object_set_new(rootJ, "gain", gainJ);		
		
		// q
		json_t *qJ = json_array();
		for (int t = 0; t < 24; t++) {
			for (int f = 0; f < 4; f++) {
				json_array_insert_new(qJ, (t << 2) | f, json_real(trackEqs[t].getQ(f)));
			}
		}
		json_object_set_new(rootJ, "q", qJ);		
		
		// lowBP
		json_t *lowBPJ = json_array();
		for (int t = 0; t < 24; t++) {
			json_array_insert_new(lowBPJ, t, json_boolean(trackEqs[t].getLowBP()));
		}
		json_object_set_new(rootJ, "lowBP", lowBPJ);		
		
		// highBP
		json_t *highBPJ = json_array();
		for (int t = 0; t < 24; t++) {
			json_array_insert_new(highBPJ, t, json_boolean(trackEqs[t].getHighBP()));
		}
		json_object_set_new(rootJ, "highBP", highBPJ);		
				
		// -------------
				
		return rootJ;
	}


	void dataFromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// mappedId
		json_t *mappedIdJ = json_object_get(rootJ, "mappedId");
		if (mappedIdJ)
			mappedId = json_integer_value(mappedIdJ);

		// trackLabels
		json_t *textJ = json_object_get(rootJ, "trackLabels");
		if (textJ)
			snprintf(trackLabels, 24 * 4 + 1, "%s", json_string_value(textJ));

		// trackEqs
		// -------------

		// active
		json_t *activeJ = json_object_get(rootJ, "active");
		if (activeJ) {
			for (int t = 0; t < 24; t++) {
				json_t *activeArrayJ = json_array_get(activeJ, t);
				if (activeArrayJ)
					trackEqs[t].setActive(json_is_true(activeArrayJ));
			}
		}

		// freqActive
		json_t *freqActiveJ = json_object_get(rootJ, "freqActive");
		if (freqActiveJ) {
			for (int t = 0; t < 24; t++) {
				for (int f = 0; f < 4; f++) {
					json_t *freqActiveArrayJ = json_array_get(freqActiveJ, (t << 2) | f);
					if (freqActiveArrayJ)
						trackEqs[t].setFreqActive(f, json_is_true(freqActiveArrayJ));
				}
			}
		}

		// freq
		json_t *freqJ = json_object_get(rootJ, "freq");
		if (freqJ) {
			for (int t = 0; t < 24; t++) {
				for (int f = 0; f < 4; f++) {
					json_t *freqArrayJ = json_array_get(freqJ, (t << 2) | f);
					if (freqArrayJ)
						trackEqs[t].setFreq(f, json_number_value(freqArrayJ));
				}
			}
		}

		// gain
		json_t *gainJ = json_object_get(rootJ, "gain");
		if (gainJ) {
			for (int t = 0; t < 24; t++) {
				for (int f = 0; f < 4; f++) {
					json_t *gainArrayJ = json_array_get(gainJ, (t << 2) | f);
					if (gainArrayJ)
						trackEqs[t].setGain(f, json_number_value(gainArrayJ));
				}
			}
		}

		// q
		json_t *qJ = json_object_get(rootJ, "q");
		if (qJ) {
			for (int t = 0; t < 24; t++) {
				for (int f = 0; f < 4; f++) {
					json_t *qArrayJ = json_array_get(qJ, (t << 2) | f);
					if (qArrayJ)
						trackEqs[t].setQ(f, json_number_value(qArrayJ));
				}
			}
		}

		// lowBP
		json_t *lowBPJ = json_object_get(rootJ, "lowBP");
		if (lowBPJ) {
			for (int t = 0; t < 24; t++) {
				json_t *lowBPArrayJ = json_array_get(lowBPJ, t);
				if (lowBPArrayJ)
					trackEqs[t].setLowBP(json_is_true(lowBPArrayJ));
			}
		}

		// highBP
		json_t *highBPJ = json_object_get(rootJ, "highBP");
		if (highBPJ) {
			for (int t = 0; t < 24; t++) {
				json_t *highBPArrayJ = json_array_get(highBPJ, t);
				if (highBPArrayJ)
					trackEqs[t].setHighBP(json_is_true(highBPArrayJ));
			}
		}

		// -------------

		resetNonJson();
	}


	void onSampleRateChange() override {
	}
	

	void process(const ProcessArgs &args) override {
		//int selectedTrack = getSelectedTrack();
		
		//********** Inputs **********
		
		if (refresh.processInputs()) {
			outputs[SIG_OUTPUTS + 0].setChannels(numChannels16);
			outputs[SIG_OUTPUTS + 1].setChannels(numChannels16);
			outputs[SIG_OUTPUTS + 2].setChannels(numChannels16);
		}// userInputs refresh
		
		
		//********** Outputs **********

		for (int i = 0; i < 3; i++) {
			for (int t = 0; t < 8; t++) {
				float inL = inputs[SIG_INPUTS + i].getVoltage((t << 1) + 0);
				float inR = inputs[SIG_INPUTS + i].getVoltage((t << 1) + 1);
				float outL = trackEqs[(i << 3) + t].processL(inL);
				float outR = trackEqs[(i << 3) + t].processR(inR);
				outputs[SIG_OUTPUTS + i].setVoltage(outL, (t << 1) + 0);
				outputs[SIG_OUTPUTS + i].setVoltage(outR, (t << 1) + 1);
			}
		}
		
		
		//********** Lights **********
		
		if (refresh.processLights()) {
		}

	}// process()
};


//-----------------------------------------------------------------------------


struct EqMasterWidget : ModuleWidget {
	time_t oldTime = 0;
	int oldMappedId = 0;
	int oldSelectedTrack = -1;
	TrackLabel* trackLabel;
	
	

	// Module's context menu
	// --------------------

	void appendContextMenu(Menu *menu) override {		
		EqMaster* module = (EqMaster*)(this->module);
		assert(module);

		menu->addChild(new MenuSeparator());
		
		FetchLabelsItem *fetchItem = createMenuItem<FetchLabelsItem>("Fetch track labels from Mixer", RIGHT_ARROW);
		fetchItem->mappedIdSrc = &(module->mappedId);
		menu->addChild(fetchItem);
	}
	
	
	EqMasterWidget(EqMaster *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/labeltester.svg")));
		
		
		// Left side controls and inputs
		// ------------------
		static const float leftX = 10.0f;
		// Track label
		addChild(trackLabel = createWidgetCentered<TrackLabel>(mm2px(Vec(leftX, 4.7))));
		if (module) {
			trackLabel->trackLabelsSrc = module->trackLabels;
			trackLabel->trackParamSrc = &(module->params[EqMaster::TRACK_PARAM]);
		}
		// Track knob
		TrackKnob* trackKnob;
		addParam(trackKnob = createDynamicParamCentered<TrackKnob>(mm2px(Vec(leftX, 20.0f)), module, EqMaster::TRACK_PARAM, module ? &module->panelTheme : NULL));
		if (module) {
			trackKnob->updateTrackLabelRequestSrc = &(module->updateTrackLabelRequest);
		}
		// Active switch
		ActiveSwitch* activeSwitch;
		addParam(activeSwitch = createParamCentered<ActiveSwitch>(mm2px(Vec(leftX, 40.0f)), module, EqMaster::ACTIVE_PARAM));
		if (module) {
			activeSwitch->trackParamSrc = &(module->params[EqMaster::TRACK_PARAM]);
			activeSwitch->trackEqsSrc = module->trackEqs;
		}
		// Signal inputs
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(leftX, 60.0f)), true, module, EqMaster::SIG_INPUTS + 0, module ? &module->panelTheme : NULL));		
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(leftX, 80.0f)), true, module, EqMaster::SIG_INPUTS + 1, module ? &module->panelTheme : NULL));		
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(leftX, 100.0f)), true, module, EqMaster::SIG_INPUTS + 2, module ? &module->panelTheme : NULL));		
		
		
		// Center part
		// Screen
		// todo
		// Controls
		static const float ctrlX = 25.0f;
		static const float ctrlDX = 12.0f;
		for (int c = 0; c < 4; c++) {
			EqFreqKnob* freqKnob;
			addParam(freqKnob = createDynamicParamCentered<EqFreqKnob>(mm2px(Vec(ctrlX + ctrlDX * c, 60.0f)), module, EqMaster::FREQ_PARAMS + c, module ? &module->panelTheme : NULL));
			addParam(createDynamicParamCentered<DynSmallKnobGrey>(mm2px(Vec(ctrlX + ctrlDX * c + 6.0f, 80.0f)), module, EqMaster::GAIN_PARAMS + c, module ? &module->panelTheme : NULL));
			addParam(createDynamicParamCentered<DynSmallKnobGrey>(mm2px(Vec(ctrlX + ctrlDX * c, 100.0f)), module, EqMaster::Q_PARAMS + c, module ? &module->panelTheme : NULL));
			if (module) {
				freqKnob->trackParamSrc = &(module->params[EqMaster::TRACK_PARAM]);
				freqKnob->trackEqsSrc = module->trackEqs;
				freqKnob->eqNum = c;
			}
		}
		
				
		// Right side VU and outputs
		// ------------------
		static const float rightX = 80.0f;
		// Signal outputs
		addOutput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(rightX, 60)), false, module, EqMaster::SIG_OUTPUTS + 0, module ? &module->panelTheme : NULL));		
		addOutput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(rightX, 80)), false, module, EqMaster::SIG_OUTPUTS + 1, module ? &module->panelTheme : NULL));		
		addOutput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(rightX, 100)), false, module, EqMaster::SIG_OUTPUTS + 2, module ? &module->panelTheme : NULL));		
		
	}
	
	void step() override {
		EqMaster* module = (EqMaster*)(this->module);
		if (module) {
			int trk = module->getSelectedTrack();
			
			// update labels from message bus at 1Hz
			time_t currentTime = time(0);
			if (currentTime != oldTime) {
				oldTime = currentTime;

				if (module->mappedId == 0) {
					if (oldMappedId != 0) {
						module->initTrackLabels();
					}
				}
				else {
					MixerMessage message;
					message.id = module->mappedId;
					mixerMessageBus.receive(&message);
					if (message.id == 0) {// if deregistered
						// do nothing! since it may be possible that the eq is loaded and step executes before the mixer modules are finished loading
						// module->initTrackLabels();
						// module->mappedId = 0;
					}
					else {
						if (message.isJr) {
							module->initTrackLabels();// need this since message.trkGrpAuxLabels is not completely filled
							memcpy(module->trackLabels, message.trkGrpAuxLabels, 8 * 4);
							memcpy(&(module->trackLabels[16 * 4]), &(message.trkGrpAuxLabels[16 * 4]), 2 * 4);
							memcpy(&(module->trackLabels[(16 + 4) * 4]), &(message.trkGrpAuxLabels[(16 + 4) * 4]), 4 * 4);
						}
						else {
							memcpy(module->trackLabels, message.trkGrpAuxLabels, 24 * 4);
						}
					}
				}
				module->updateTrackLabelRequest = 1;
				
				oldMappedId = module->mappedId;
			}

			// Track label (pull from module or this step method)
			if (module->updateTrackLabelRequest != 0) {// pull request from module
				trackLabel->text = std::string(&(module->trackLabels[trk * 4]), 4);
				module->updateTrackLabelRequest = 0;// all done pulling
			}

			if (oldSelectedTrack != trk) {// update controls when user selects another track
				module->params[EqMaster::ACTIVE_PARAM].setValue(module->trackEqs[trk].getActive() ? 1.0f : 0.0f);
				for (int c = 0; c < 4; c++) {
					module->params[EqMaster::FREQ_PARAMS + c].setValue(module->trackEqs[trk].getFreq(c));
				}				
				oldSelectedTrack = trk;
			}
		}
		Widget::step();
	}
};


Model *modelEqMaster = createModel<EqMaster, EqMasterWidget>("EqMaster");
