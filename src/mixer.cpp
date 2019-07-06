//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.txt for all licenses
//***********************************************************************************************


#include "MBSB.hpp"


struct Mixer : Module {
	enum ParamIds {
		KNOB_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		NUM_INPUTS
	};
	enum OutputIds {
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};
	
	// Need to save, no reset
	// none
	
	// Need to save, with reset
	// none
	
	// No need to save, with reset
	// noone
	
	// No need to save, no reset
	RefreshCounter refresh;	

		
	Mixer() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);		
		
		configParam(KNOB_PARAM, 0.0f, 1.0f, 0.5f, "Knob");
		
		onReset();
	}

	
	void onReset() override {
		resetNonJson();
	}
	void resetNonJson() {
	}


	void onRandomize() override {
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		return rootJ;
	}


	void dataFromJson(json_t *rootJ) override {
		
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


struct MixerWidget : ModuleWidget {

	MixerWidget(Mixer *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Mixer.svg")));
		
		

		addParam(createDynamicParamCentered<DynSmallKnob>(Vec(100, 100), module, Mixer::KNOB_PARAM, NULL));

	}
};

Model *modelMixer = createModel<Mixer, MixerWidget>("Mixer");
