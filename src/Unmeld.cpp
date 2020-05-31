//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "MindMeldModular.hpp"


struct Unmeld : Module {
	
	enum ParamIds {
		NUM_PARAMS
	};
	
	enum InputIds {
		POLY_INPUT,
		NUM_INPUTS
	};
	
	enum OutputIds {
		THRU_OUTPUT,
		ENUMS(SPLIT_OUTPUTS, 16),
		NUM_OUTPUTS
	};
	
	enum LightIds {
		ENUMS(CHAN_LIGHTS, 16),
		NUM_LIGHTS
	};
	

	// Constants
	// none


	// Need to save, no reset
	int panelTheme;
	int facePlate;
	
	// Need to save, with reset
	// none
	
	// No need to save, with reset
	// none

	// No need to save, no reset
	RefreshCounter refresh;	
	
	
	Unmeld() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		onReset();
		
		panelTheme = 0;
		facePlate = 0;
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
		
		// facePlate
		json_object_set_new(rootJ, "facePlate", json_integer(facePlate));
		
		return rootJ;
	}


	void dataFromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// facePlate
		json_t *facePlateJ = json_object_get(rootJ, "facePlate");
		if (facePlateJ)
			facePlate = json_integer_value(facePlateJ);

		resetNonJson(true);	}


	void onSampleRateChange() override {
	}
	

	void process(const ProcessArgs &args) override {
		// Controls
		if (refresh.processInputs()) {
			// none
		}// userInputs refresh
		
		
		// Thru
		int numChan = inputs[POLY_INPUT].getChannels();
		outputs[THRU_OUTPUT].setChannels(numChan);
		for (int c = 0; c < numChan; c++) {
			float v = inputs[POLY_INPUT].getVoltage(c);
			outputs[THRU_OUTPUT].setVoltage(v, c);
		}
		
		// Split
		int c = 0;
		for (; c < numChan; c++) {
			float v = inputs[POLY_INPUT].getVoltage(c);
			outputs[SPLIT_OUTPUTS + c].setVoltage(v);
		}		
		for (; c < 16; c++) {
			outputs[SPLIT_OUTPUTS + c].setVoltage(0.0f);
		}
		
		// Lights
		if (refresh.processLights()) {
			int i = 0;
			for (; i < numChan; i++) {
				lights[CHAN_LIGHTS + i].setBrightness(1.0f);
			}
			for (; i < 16; i++) {
				lights[CHAN_LIGHTS + i].setBrightness(0.0f);
			}
		}
	}// process()
};


//-----------------------------------------------------------------------------


struct UnmeldWidget : ModuleWidget {
	SvgPanel* facePlates[3];
	int lastFacePlate = 0;
		
	struct PanelThemeItem : MenuItem {
		Unmeld *module;
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
		Unmeld *module = (Unmeld*)(this->module);
		assert(module);

		menu->addChild(new MenuSeparator());
		
		MenuLabel *themeLabel = new MenuLabel();
		themeLabel->text = "Panel";
		menu->addChild(themeLabel);

		for (int i = 0; i < 3; i++) {
			PanelThemeItem *aItem = new PanelThemeItem();
			aItem->text = facePlateNames[i];
			aItem->rightText = CHECKMARK(module->facePlate == i);
			aItem->module = module;
			aItem->plate = i;
			menu->addChild(aItem);
		}
	}	
	
	
	UnmeldWidget(Unmeld *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/unmeld-1-8.svg")));
        if (module) {
			facePlates[0] = (SvgPanel*)panel;
			facePlates[1] = new SvgPanel();
			facePlates[1]->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/unmeld-9-16.svg")));
			facePlates[1]->visible = false;
			addChild(facePlates[1]);
			facePlates[2] = new SvgPanel();
			facePlates[2]->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/unmeld-grp-aux.svg")));
			facePlates[2]->visible = false;
			addChild(facePlates[2]);
		}		
		
		// poly in/thru
		addInput(createInputCentered<MmPortGold>(mm2px(Vec(6.84, 18.35)), module, Unmeld::POLY_INPUT));
		addOutput(createOutputCentered<MmPortGold>(mm2px(Vec(23.64, 18.35)), module, Unmeld::THRU_OUTPUT));
		
		// leds
		for (int i = 0; i < 8; i++) {
			addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(14.3, 10.7 + 2 * i)), module, Unmeld::CHAN_LIGHTS + 2 * i + 0));
			addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(16.18, 10.7 + 2 * i)), module, Unmeld::CHAN_LIGHTS + 2 * i + 1));
		}
		
		// split signals
		for (int i = 0; i < 8; i++) {
			addOutput(createOutputCentered<MmPort>(mm2px(Vec(10.33, 34.5 + 10.85 * i)), module, Unmeld::SPLIT_OUTPUTS + 2 * i + 0));
			addOutput(createOutputCentered<MmPort>(mm2px(Vec(20.15, 34.5 + 10.85 * i)), module, Unmeld::SPLIT_OUTPUTS + 2 * i + 1));
		}
	}
	
	void step() override {
		if (module) {
			int facePlate = (((Unmeld*)module)->facePlate);
			if (facePlate != lastFacePlate) {
				facePlates[lastFacePlate]->visible = false;
				facePlates[facePlate]->visible = true;
				lastFacePlate = facePlate;
			}
		}
		Widget::step();
	}
};


Model *modelUnmeld = createModel<Unmeld, UnmeldWidget>("Unmeld");
