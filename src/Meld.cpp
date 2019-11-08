//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "MindMeldModular.hpp"


struct Meld : Module {
	
	enum ParamIds {
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
		ENUMS(CHAN_LIGHTS, 32),
		NUM_LIGHTS
	};
	

	// Constants
	// none


	// Need to save, no reset
	int panelTheme;
	
	// Need to save, with reset
	// none
	
	// No need to save, with reset
	int lastMergeInputIndex;
	
	// No need to save, no reset
	RefreshCounter refresh;	
	
	
	void calcLastMergeInputIndex() {
		for (int i = 15; i >= 0; i--) {
			if (inputs[MERGE_INPUTS + i].isConnected()) {
				lastMergeInputIndex = i;
				return;
			}
		}
		lastMergeInputIndex = 0;
	}
	
	
	Meld() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		panelTheme = 0;
	}
  
	void onReset() override {
		resetNonJson(false);
	}
	void resetNonJson(bool recurseNonJson) {
		calcLastMergeInputIndex();
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
			calcLastMergeInputIndex();
		}// userInputs refresh
		
		
		// Merge out
		int numChan = std::max(inputs[POLY_INPUT].getChannels(), lastMergeInputIndex + 1);
		outputs[OUT_OUTPUT].setChannels(numChan);
		for (int c = 0; c < numChan; c++) {
			float v;
			if (inputs[MERGE_INPUTS + c].isConnected()) {
				v = inputs[MERGE_INPUTS + c].getVoltage();
			}
			else {
				v = inputs[POLY_INPUT].getVoltage(c);
			}
			outputs[OUT_OUTPUT].setVoltage(v, c);
		}
		
		// Lights
		if (refresh.processLights()) {
			for (int i = 0; i < 16; i++) {
				float greenBright = 0.0f;
				float blueBright = 0.0f;
				if (i <= lastMergeInputIndex && inputs[MERGE_INPUTS + i].isConnected()) {
					greenBright = 1.0f;
				}
				else if (i < numChan) {
					blueBright = 1.0f;
				}
				lights[CHAN_LIGHTS + 2 * i + 0].setBrightness(greenBright);
				lights[CHAN_LIGHTS + 2 * i + 1].setBrightness(blueBright);
			}
		}
	}// process()
};


//-----------------------------------------------------------------------------


struct MeldWidget : ModuleWidget {
	
	MeldWidget(Meld *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/meld-1-8.svg")));

		// poly in/thru
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(6.84, 18.35)), true, module, Meld::POLY_INPUT, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(23.64, 18.35)), false, module, Meld::OUT_OUTPUT, module ? &module->panelTheme : NULL));
		
		// leds
		for (int i = 0; i < 8; i++) {
			addChild(createLightCentered<TinyLight<MMGreenBlueLight>>(mm2px(Vec(14.3, 10.7 + 2 * i)), module, Meld::CHAN_LIGHTS + 2 * (2 * i + 0)));
			addChild(createLightCentered<TinyLight<MMGreenBlueLight>>(mm2px(Vec(16.18, 10.7 + 2 * i)), module, Meld::CHAN_LIGHTS + 2 * (2 * i + 1)));
		}
				
		// merge signals
		for (int i = 0; i < 8; i++) {
			addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(10.33, 34.5 + 10.85 * i)), true, module, Meld::MERGE_INPUTS + 2 * i + 0, module ? &module->panelTheme : NULL));
			addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(20.15, 34.5 + 10.85 * i)), true, module, Meld::MERGE_INPUTS + 2 * i + 1, module ? &module->panelTheme : NULL));
		}
		

	}
};


Model *modelMeld = createModel<Meld, MeldWidget>("Meld");
