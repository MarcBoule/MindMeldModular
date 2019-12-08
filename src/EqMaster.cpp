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
		TRACK_GAIN_PARAM,
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
	int8_t vuColorGlobal = 0;
	int8_t vuColorThemeLocal = 0;

	// Need to save, no reset
	int panelTheme;
	
	// Need to save, with reset
	int mappedId;// 0 means manual
	char trackLabels[24 * 4 + 1];// needs to be saved in case we are detached
	TrackEq trackEqs[24];
	
	// No need to save, with reset
	int updateTrackLabelRequest;// 0 when nothing to do, 1 for read names in widget
	VuMeterAllDual trackVu;

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
		configParam(ACTIVE_PARAM, 0.0f, 1.0f, DEFAULT_active ? 1.0f : 0.0f, "Track EQ active");
		configParam(TRACK_GAIN_PARAM, -20.0f, 20.0f, DEFAULT_gain, "Track gain", " dB");
		configParam(FREQ_PARAMS + 0, 20.0f, 500.0f, DEFAULT_freq[0], "LF freq", " Hz");
		configParam(FREQ_PARAMS + 1, 60.0f, 2000.0f, DEFAULT_freq[1], "LMF freq", " Hz");
		configParam(FREQ_PARAMS + 2, 500.0f, 5000.0f, DEFAULT_freq[2], "HMF freq", " Hz");
		configParam(FREQ_PARAMS + 3, 1000.0f, 20000.0f, DEFAULT_freq[3], "HF freq", " Hz");
		for (int i = 0; i < 4; i++) {
			configParam(FREQ_ACTIVE_PARAMS + i, 0.0f, 1.0f, DEFAULT_bandActive ? 1.0f : 0.0f, bandNames[i] + " active");
			configParam(GAIN_PARAMS + i, -20.0f, 20.0f, DEFAULT_gain, bandNames[i] + " gain", " dB");
			configParam(Q_PARAMS + i, 0.5f, 15.0f, DEFAULT_q, bandNames[i] + " Q");
		}
		configParam(LOW_BP_PARAM, 0.0f, 1.0f, DEFAULT_lowPeak ? 1.0f : 0.0f, "Low is peak type");
		configParam(HIGH_BP_PARAM, 0.0f, 1.0f, DEFAULT_highPeak ? 1.0f : 0.0f, "High is peak type");
		
		onReset();
		
		panelTheme = 0;
	}
  
	void onReset() override {
		mappedId = 0;
		initTrackLabels();
		for (int t = 0; t < 24; t++) {
			trackEqs[t].init(APP->engine->getSampleRate());
		}
		resetNonJson();
	}
	void resetNonJson() {
		updateTrackLabelRequest = 1;
		trackVu.reset();
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
		
		// bandActive
		json_t *bandActiveJ = json_array();
		for (int t = 0; t < 24; t++) {
			for (int f = 0; f < 4; f++) {
				json_array_insert_new(bandActiveJ, (t << 2) | f, json_boolean(trackEqs[t].getBandActive(f)));
			}
		}
		json_object_set_new(rootJ, "bandActive", bandActiveJ);		
		
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
		
		// lowPeak
		json_t *lowPeakJ = json_array();
		for (int t = 0; t < 24; t++) {
			json_array_insert_new(lowPeakJ, t, json_boolean(trackEqs[t].getLowPeak()));
		}
		json_object_set_new(rootJ, "lowPeak", lowPeakJ);		
		
		// highPeak
		json_t *highPeakJ = json_array();
		for (int t = 0; t < 24; t++) {
			json_array_insert_new(highPeakJ, t, json_boolean(trackEqs[t].getHighPeak()));
		}
		json_object_set_new(rootJ, "highPeak", highPeakJ);		
				
		// trackGain
		json_t *trackGainJ = json_array();
		for (int t = 0; t < 24; t++) {
			json_array_insert_new(trackGainJ, t, json_real(trackEqs[t].getTrackGain()));
		}
		json_object_set_new(rootJ, "trackGain", trackGainJ);		
		
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

		// bandActive
		json_t *bandActiveJ = json_object_get(rootJ, "bandActive");
		if (bandActiveJ) {
			for (int t = 0; t < 24; t++) {
				for (int f = 0; f < 4; f++) {
					json_t *bandActiveArrayJ = json_array_get(bandActiveJ, (t << 2) | f);
					if (bandActiveArrayJ)
						trackEqs[t].setBandActive(f, json_is_true(bandActiveArrayJ));
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

		// lowPeak
		json_t *lowPeakJ = json_object_get(rootJ, "lowPeak");
		if (lowPeakJ) {
			for (int t = 0; t < 24; t++) {
				json_t *lowPeakArrayJ = json_array_get(lowPeakJ, t);
				if (lowPeakArrayJ)
					trackEqs[t].setLowPeak(json_is_true(lowPeakArrayJ));
			}
		}

		// highPeak
		json_t *highPeakJ = json_object_get(rootJ, "highPeak");
		if (highPeakJ) {
			for (int t = 0; t < 24; t++) {
				json_t *highPeakArrayJ = json_array_get(highPeakJ, t);
				if (highPeakArrayJ)
					trackEqs[t].setHighPeak(json_is_true(highPeakArrayJ));
			}
		}

		// trackGain
		json_t *trackGainJ = json_object_get(rootJ, "trackGain");
		if (trackGainJ) {
			for (int t = 0; t < 24; t++) {
				json_t *trackGainArrayJ = json_array_get(trackGainJ, t);
				if (trackGainArrayJ)
					trackEqs[t].setTrackGain(json_number_value(trackGainArrayJ));
			}
		}

		// -------------

		resetNonJson();
	}


	void onSampleRateChange() override {
		for (int t = 0; t < 24; t++) {
			trackEqs[t].updateSampleRate(APP->engine->getSampleRate());
		}
	}
	

	void process(const ProcessArgs &args) override {
		int selectedTrack = getSelectedTrack();
		
		//********** Inputs **********
		
		if (refresh.processInputs()) {
			outputs[SIG_OUTPUTS + 0].setChannels(numChannels16);
			outputs[SIG_OUTPUTS + 1].setChannels(numChannels16);
			outputs[SIG_OUTPUTS + 2].setChannels(numChannels16);
		}// userInputs refresh
		
		
		//********** Outputs **********

		bool vuProcessed = false;
		for (int i = 0; i < 3; i++) {
			if (inputs[SIG_INPUTS + i].isConnected()) {
				for (int t = 0; t < 8; t++) {
					float inL = inputs[SIG_INPUTS + i].getVoltage((t << 1) + 0);
					float inR = inputs[SIG_INPUTS + i].getVoltage((t << 1) + 1);
					float out[2];
					trackEqs[(i << 3) + t].process(out, inL, inR);
					outputs[SIG_OUTPUTS + i].setVoltage(out[0], (t << 1) + 0);
					outputs[SIG_OUTPUTS + i].setVoltage(out[1], (t << 1) + 1);
					if ( ((i << 3) + t) == selectedTrack ) {
						trackVu.process(args.sampleTime, out);
						vuProcessed = true;
					}
				}
			}
		}
		if (!vuProcessed) {
			trackVu.reset();
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
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/EQmaster.svg")));
		
		
		// Left side controls and inputs
		// ------------------
		static const float leftX = 8.29f;
		// Track label
		addChild(trackLabel = createWidgetCentered<TrackLabel>(mm2px(Vec(leftX, 11.5f))));
		if (module) {
			trackLabel->trackLabelsSrc = module->trackLabels;
			trackLabel->trackParamSrc = &(module->params[EqMaster::TRACK_PARAM]);
		}
		// Track knob
		TrackKnob* trackKnob;
		addParam(trackKnob = createDynamicParamCentered<TrackKnob>(mm2px(Vec(leftX, 22.7f)), module, EqMaster::TRACK_PARAM, module ? &module->panelTheme : NULL));
		if (module) {
			trackKnob->updateTrackLabelRequestSrc = &(module->updateTrackLabelRequest);
			trackKnob->trackEqsSrc = module->trackEqs;
		}
		// Active switch
		ActiveSwitch* activeSwitch;
		addParam(activeSwitch = createParamCentered<ActiveSwitch>(mm2px(Vec(leftX, 48.775f)), module, EqMaster::ACTIVE_PARAM));
		if (module) {
			activeSwitch->trackParamSrc = &(module->params[EqMaster::TRACK_PARAM]);
			activeSwitch->trackEqsSrc = module->trackEqs;
		}
		// Signal inputs
		static const float jackY = 84.35f;
		static const float jackDY = 12.8f;
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(leftX, jackY)), true, module, EqMaster::SIG_INPUTS + 0, module ? &module->panelTheme : NULL));		
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(leftX, jackY + jackDY)), true, module, EqMaster::SIG_INPUTS + 1, module ? &module->panelTheme : NULL));		
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(leftX, jackY + jackDY * 2)), true, module, EqMaster::SIG_INPUTS + 2, module ? &module->panelTheme : NULL));		
		
		
		// Center part
		// Screen
		// todo
		// Controls
		static const float ctrlX = 23.94f;
		static const float ctrlDX = 27.74f;
		BandKnob* freqKnobs[4];
		BandKnob* gainKnobs[4];
		BandKnob* qKnobs[4];
		// freq
		addParam(freqKnobs[0] = createDynamicParamCentered<EqFreqKnob<0>>(mm2px(Vec(ctrlX + ctrlDX * 0, 91.2f)), module, EqMaster::FREQ_PARAMS + 0, module ? &module->panelTheme : NULL));
		addParam(freqKnobs[1] = createDynamicParamCentered<EqFreqKnob<1>>(mm2px(Vec(ctrlX + ctrlDX * 1, 91.2f)), module, EqMaster::FREQ_PARAMS + 1, module ? &module->panelTheme : NULL));
		addParam(freqKnobs[2] = createDynamicParamCentered<EqFreqKnob<2>>(mm2px(Vec(ctrlX + ctrlDX * 2, 91.2f)), module, EqMaster::FREQ_PARAMS + 2, module ? &module->panelTheme : NULL));
		addParam(freqKnobs[3] = createDynamicParamCentered<EqFreqKnob<3>>(mm2px(Vec(ctrlX + ctrlDX * 3, 91.2f)), module, EqMaster::FREQ_PARAMS + 3, module ? &module->panelTheme : NULL));
		// gain
		addParam(gainKnobs[0] = createDynamicParamCentered<EqGainKnob<0>>(mm2px(Vec(ctrlX + ctrlDX * 0 + 11.04f, 101.78f)), module, EqMaster::GAIN_PARAMS + 0, module ? &module->panelTheme : NULL));
		addParam(gainKnobs[1] = createDynamicParamCentered<EqGainKnob<1>>(mm2px(Vec(ctrlX + ctrlDX * 1 + 11.04f, 101.78f)), module, EqMaster::GAIN_PARAMS + 1, module ? &module->panelTheme : NULL));
		addParam(gainKnobs[2] = createDynamicParamCentered<EqGainKnob<2>>(mm2px(Vec(ctrlX + ctrlDX * 2 + 11.04f, 101.78f)), module, EqMaster::GAIN_PARAMS + 2, module ? &module->panelTheme : NULL));
		addParam(gainKnobs[3] = createDynamicParamCentered<EqGainKnob<3>>(mm2px(Vec(ctrlX + ctrlDX * 3 + 11.04f, 101.78f)), module, EqMaster::GAIN_PARAMS + 3, module ? &module->panelTheme : NULL));
		// q
		addParam(qKnobs[0] = createDynamicParamCentered<EqQKnob<0>>(mm2px(Vec(ctrlX + ctrlDX * 0, 112.37f)), module, EqMaster::Q_PARAMS + 0, module ? &module->panelTheme : NULL));
		addParam(qKnobs[1] = createDynamicParamCentered<EqQKnob<1>>(mm2px(Vec(ctrlX + ctrlDX * 1, 112.37f)), module, EqMaster::Q_PARAMS + 1, module ? &module->panelTheme : NULL));
		addParam(qKnobs[2] = createDynamicParamCentered<EqQKnob<2>>(mm2px(Vec(ctrlX + ctrlDX * 2, 112.37f)), module, EqMaster::Q_PARAMS + 2, module ? &module->panelTheme : NULL));
		addParam(qKnobs[3] = createDynamicParamCentered<EqQKnob<3>>(mm2px(Vec(ctrlX + ctrlDX * 3, 112.37f)), module, EqMaster::Q_PARAMS + 3, module ? &module->panelTheme : NULL));
		
		if (module) {
			for (int c = 0; c < 4; c++) {
				freqKnobs[c]->trackParamSrc = &(module->params[EqMaster::TRACK_PARAM]);
				freqKnobs[c]->trackEqsSrc = module->trackEqs;
				gainKnobs[c]->trackParamSrc = &(module->params[EqMaster::TRACK_PARAM]);
				gainKnobs[c]->trackEqsSrc = module->trackEqs;
				qKnobs[c]->trackParamSrc = &(module->params[EqMaster::TRACK_PARAM]);
				qKnobs[c]->trackEqsSrc = module->trackEqs;
			}
		}
		
				
		// Right side VU and outputs
		// ------------------
		static const float rightX = 133.95f;
		// VU meter
		if (module) {
			VuMeterTrack *newVU = createWidgetCentered<VuMeterTrack>(mm2px(Vec(rightX, 37.5f)));
			newVU->srcLevels = &(module->trackVu);
			newVU->colorThemeGlobal = &(module->vuColorGlobal);
			newVU->colorThemeLocal = &(module->vuColorThemeLocal);
			addChild(newVU);
		}
		// Gain knob
		TrackGainKnob* trackGainKnob;
		addParam(trackGainKnob = createDynamicParamCentered<TrackGainKnob>(mm2px(Vec(rightX, 67.0f)), module, EqMaster::TRACK_GAIN_PARAM, module ? &module->panelTheme : NULL));
		if (module) {
			trackGainKnob->trackParamSrc = &(module->params[EqMaster::TRACK_PARAM]);
			trackGainKnob->trackEqsSrc = module->trackEqs;
		}		
		// Signal outputs
		addOutput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(rightX, jackY)), false, module, EqMaster::SIG_OUTPUTS + 0, module ? &module->panelTheme : NULL));		
		addOutput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(rightX, jackY + jackDY)), false, module, EqMaster::SIG_OUTPUTS + 1, module ? &module->panelTheme : NULL));		
		addOutput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(rightX, jackY + jackDY * 2)), false, module, EqMaster::SIG_OUTPUTS + 2, module ? &module->panelTheme : NULL));		
		
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
				module->params[EqMaster::TRACK_GAIN_PARAM].setValue(module->trackEqs[trk].getTrackGain());
				for (int c = 0; c < 4; c++) {
					module->params[EqMaster::FREQ_PARAMS + c].setValue(module->trackEqs[trk].getFreq(c));
					module->params[EqMaster::GAIN_PARAMS + c].setValue(module->trackEqs[trk].getGain(c));
					module->params[EqMaster::Q_PARAMS + c].setValue(module->trackEqs[trk].getQ(c));
				}				
				oldSelectedTrack = trk;
			}
		}
		Widget::step();
	}
};


Model *modelEqMaster = createModel<EqMaster, EqMasterWidget>("EqMaster");
