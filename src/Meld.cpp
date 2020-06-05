//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "MindMeldModular.hpp"


struct Meld : Module {
	
	enum ParamIds {
		ENUMS(BYPASS_PARAMS, 8),
		NUM_PARAMS
	};
	
	enum InputIds {
		POLY_INPUT,
		ENUMS(MERGE_INPUTS, 16),
		NUM_INPUTS
	};
	
	enum OutputIds {
		OUT_OUTPUT,
		NUM_OUTPUTS
	};
	
	enum LightIds {
		ENUMS(CHAN_LIGHTS, 16 * 2),// room for white-blue lights
		ENUMS(BYPASS_LIGHTS, 8 * 2),// room for red-green lights
		NUM_LIGHTS
	};
	

	// Constants
	// none


	// Need to save, no reset
	int facePlate;
	
	// Need to save, with reset
	int bypassState[8];// 0 = pass, 1 = bypass
	
	// No need to save, with reset
	int lastMergeInputIndex;// can be -1 when nothing connected
	TSlewLimiterSingle<simd::float_4> bypassSlewersVect[4];
	
	// No need to save, no reset
	RefreshCounter refresh;	
	Trigger bypassTriggers[8];
	
	
	void calcLastMergeInputIndex() {// can set to -1 when nothing connected
		int i = 15;
		for (; i >= 0; i--) {
			if (inputs[MERGE_INPUTS + i].isConnected()) {
				break;
			}
		}
		lastMergeInputIndex = i;
		return;
	}
	
	bool trackInUse(int trk) {// trk is 0 to 7
		return inputs[MERGE_INPUTS + trk * 2 + 0].isConnected() || inputs[MERGE_INPUTS + trk * 2 + 1].isConnected();
	}
	
	
	Meld() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		facePlate = 0;
		
		for (int i = 0; i < 8; i++) {
			// bypass
			configParam(BYPASS_PARAMS + i, 0.0f, 1.0f, 0.0f, string::f("Bypass %i", i + 1));
		}
		
		for (int i = 0; i < 4; i++) {
			bypassSlewersVect[i].setRiseFall(simd::float_4(100.0f)); // slew rate is in input-units per second (ex: V/s)			
		}

		onReset();
	}
  
	void onReset() override {
		for (int trk = 0; trk < 8; trk++) {
			bypassState[trk] = 0;
		}
		resetNonJson(false);
	}
	void resetNonJson(bool recurseNonJson) {
		calcLastMergeInputIndex();
		for (int i = 0; i < 16; i++) {
			bypassSlewersVect[i >> 2].out[i & 0x3] = (float)bypassState[i >> 1];
		}
	}


	void onRandomize() override {
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// facePlate
		json_object_set_new(rootJ, "facePlate", json_integer(facePlate));
		
		// bypassState
		json_t *bypassStateJ = json_array();
		for (int i = 0; i < 8; i++) {
			json_array_insert_new(bypassStateJ, i, json_integer(bypassState[i]));
		}
		json_object_set_new(rootJ, "bypassState2", bypassStateJ);// 2 to avoid loading corrupt values in 1.1.3's bug
		
		return rootJ;
	}


	void dataFromJson(json_t *rootJ) override {
		// facePlate
		json_t *facePlateJ = json_object_get(rootJ, "facePlate");
		if (facePlateJ)
			facePlate = json_integer_value(facePlateJ);

		// bypassState
		json_t *bypassStateJ = json_object_get(rootJ, "bypassState2");// 2 to avoid loading corrupt values in 1.1.3's bug
		if (bypassStateJ) {
			for (int i = 0; i < 8; i++) {
				json_t *bypassStateArrayJ = json_array_get(bypassStateJ, i);
				if (bypassStateArrayJ) {
					bypassState[i] = json_integer_value(bypassStateArrayJ);
				}
			}			
		}

		resetNonJson(true);
	}


	void onSampleRateChange() override {
	}
	

	void process(const ProcessArgs &args) override {
		// Controls
		if (refresh.processInputs()) {
			calcLastMergeInputIndex();
			
			for (int trk = 0; trk < 8; trk++) {
				if (bypassTriggers[trk].process(params[BYPASS_PARAMS + trk].getValue())) {
					if (trackInUse(trk)) {
						bypassState[trk] ^= 0x1;
					}
				}
			}
		}// userInputs refresh
		
		
		// Meld poly output
		int numChan = inputs[POLY_INPUT].isConnected() ? inputs[POLY_INPUT].getChannels() : 0;
		numChan = std::max(numChan, lastMergeInputIndex + 1);
		// here numChan can be 0		
		outputs[OUT_OUTPUT].setChannels(numChan);// clears all and sets num chan to 1 when numChan == 0

		// simd version
		simd::float_4 bypassVect[4];
		for (int c = 0; c < 16; c++) {
			bypassVect[c >> 2][c & 0x3] = (!inputs[MERGE_INPUTS + c].isConnected() || (bypassState[c >> 1] == 1)) ? 1.0f : 0.0f;
		}
		for (int i = 0; i < 4; i++) {
			bypassSlewersVect[i].process(args.sampleTime, bypassVect[i]);
			simd::float_4 vVect{inputs[MERGE_INPUTS + i * 4 + 0].getVoltage(),
				inputs[MERGE_INPUTS + i * 4 + 1].getVoltage(),
				inputs[MERGE_INPUTS + i * 4 + 2].getVoltage(),
				inputs[MERGE_INPUTS + i * 4 + 3].getVoltage()};
			vVect = crossfade(vVect, inputs[POLY_INPUT].getVoltageSimd<simd::float_4>(i << 2), bypassSlewersVect[i].out);
			outputs[OUT_OUTPUT].setVoltageSimd(vVect, i << 2);
		}
		
		// Lights
		if (refresh.processLights()) {
			// channel lights
			for (int i = 0; i < 16; i++) {
				float greenBright = 0.0f;
				float blueBright = 0.0f;
				if (i <= lastMergeInputIndex && inputs[MERGE_INPUTS + i].isConnected() && (bypassState[i >> 1] == 0)) {
					greenBright = 1.0f;
				}
				else if (i < numChan) {
					blueBright = 1.0f;
				}
				lights[CHAN_LIGHTS + 2 * i + 0].setBrightness(greenBright);
				lights[CHAN_LIGHTS + 2 * i + 1].setBrightness(blueBright);
			}
			// bypass lights
			for (int trk = 0; trk < 8; trk++) {
				bool ledEnable = trackInUse(trk);
				lights[BYPASS_LIGHTS + 2 * trk + 0].setBrightness(ledEnable && bypassState[trk] == 0 ? 1.0f : 0.0f);// green
				lights[BYPASS_LIGHTS + 2 * trk + 1].setBrightness(ledEnable && bypassState[trk] != 0 ? 1.0f : 0.0f);// red
			}
		}
	}// process()
};


//-----------------------------------------------------------------------------


struct MeldWidget : ModuleWidget {
	SvgPanel* facePlates[3];
	int lastFacePlate = 0;
		
	struct FacePlateItem : MenuItem {
		Meld *module;
		int plate;
		void onAction(const event::Action &e) override {
			module->facePlate = plate;
		}
	};
	
	std::string facePlateNames[3] = {
		"1-8",
		"9-16",
		"Group/Aux"
	};

	void appendContextMenu(Menu *menu) override {
		Meld *module = (Meld*)(this->module);
		assert(module);

		menu->addChild(new MenuSeparator());

		MenuLabel *themeLabel = new MenuLabel();
		themeLabel->text = "Panel";
		menu->addChild(themeLabel);

		for (int i = 0; i < 3; i++) {
			FacePlateItem *aItem = new FacePlateItem();
			aItem->text = facePlateNames[i];
			aItem->rightText = CHECKMARK(module->facePlate == i);
			aItem->module = module;
			aItem->plate = i;
			menu->addChild(aItem);
		}
	}	
	
	
	MeldWidget(Meld *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/meld-1-8.svg")));
        if (module) {
			facePlates[0] = (SvgPanel*)panel;
			facePlates[1] = new SvgPanel();
			facePlates[1]->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/meld-9-16.svg")));
			facePlates[1]->visible = false;
			addChild(facePlates[1]);
			facePlates[2] = new SvgPanel();
			facePlates[2]->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/meld-grp-aux.svg")));
			facePlates[2]->visible = false;
			addChild(facePlates[2]);
		}

		// poly in/thru
		addInput(createInputCentered<MmPortGold>(mm2px(Vec(6.84, 18.35)), module, Meld::POLY_INPUT));
		addOutput(createOutputCentered<MmPortGold>(mm2px(Vec(23.64, 18.35)), module, Meld::OUT_OUTPUT));
		
		// leds
		for (int i = 0; i < 8; i++) {
			addChild(createLightCentered<TinyLight<MMWhiteBlueLight>>(mm2px(Vec(14.3, 10.7 + 2 * i)), module, Meld::CHAN_LIGHTS + 2 * (2 * i + 0)));
			addChild(createLightCentered<TinyLight<MMWhiteBlueLight>>(mm2px(Vec(16.18, 10.7 + 2 * i)), module, Meld::CHAN_LIGHTS + 2 * (2 * i + 1)));
		}
				
		for (int i = 0; i < 8; i++) {
			// merge signals
			addInput(createInputCentered<MmPort>(mm2px(Vec(10.33, 34.5 + 10.85 * i)), module, Meld::MERGE_INPUTS + 2 * i + 0));
			addInput(createInputCentered<MmPort>(mm2px(Vec(20.15, 34.5 + 10.85 * i)), module, Meld::MERGE_INPUTS + 2 * i + 1));
			
			// bypass led-buttons
			addParam(createParamCentered<LedButton>(mm2px(Vec(26.93, 34.5 + 10.85 * i)), module, Meld::BYPASS_PARAMS + i));
			addChild(createLightCentered<SmallLight<GreenRedLight>>(mm2px(Vec(26.93, 34.5 + 10.85 * i)), module, Meld::BYPASS_LIGHTS + 2 * i));
		}
	}
	
	void step() override {
		if (module) {
			int facePlate = (((Meld*)module)->facePlate);
			if (facePlate != lastFacePlate) {
				facePlates[lastFacePlate]->visible = false;
				facePlates[facePlate]->visible = true;
				lastFacePlate = facePlate;
				// update bypass tooltips
				for (int i = 0; i < 8; i++) {
					if (facePlate == 0) {
						module->paramQuantities[Meld::BYPASS_PARAMS + i]->label = string::f("Bypass %i", i + 1);
					}
					else if (facePlate == 1) {
						module->paramQuantities[Meld::BYPASS_PARAMS + i]->label = string::f("Bypass %i", i + 1 + 8);
					}
					else {
						if (i < 4) {
							module->paramQuantities[Meld::BYPASS_PARAMS + i]->label = string::f("Bypass G%i", i + 1);
						}
						else {
							module->paramQuantities[Meld::BYPASS_PARAMS + i]->label = string::f("Bypass A%i", i + 1 - 4);
						}
					}
				}
			}
		}
		Widget::step();
	}	
};


Model *modelMeld = createModel<Meld, MeldWidget>("Meld");
