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
	
	// Need to save, with reset
	// none
	
	// No need to save, with reset
	// none

	// No need to save, no reset
	RefreshCounter refresh;	
	
	
	Unmeld() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		panelTheme = 0;
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
	
	UnmeldWidget(Unmeld *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/unmeld-1-8.svg")));
		
		// poly in/thru
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(6.84, 18.35)), true, module, Unmeld::POLY_INPUT, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(23.64, 18.35)), false, module, Unmeld::THRU_OUTPUT, module ? &module->panelTheme : NULL));
		
		// leds
		for (int i = 0; i < 8; i++) {
			addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(14.3, 10.7 + 2 * i)), module, Unmeld::CHAN_LIGHTS + 2 * i + 0));
			addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(16.18, 10.7 + 2 * i)), module, Unmeld::CHAN_LIGHTS + 2 * i + 1));
		}
		
		// split signals
		for (int i = 0; i < 8; i++) {
			addOutput(createDynamicPortCentered<DynPort>(mm2px(Vec(10.33, 34.5 + 10.85 * i)), false, module, Unmeld::SPLIT_OUTPUTS + 2 * i + 0, module ? &module->panelTheme : NULL));
			addOutput(createDynamicPortCentered<DynPort>(mm2px(Vec(20.15, 34.5 + 10.85 * i)), false, module, Unmeld::SPLIT_OUTPUTS + 2 * i + 1, module ? &module->panelTheme : NULL));
		}

	}
};


Model *modelUnmeld = createModel<Unmeld, UnmeldWidget>("Unmeld");
