//***********************************************************************************************
//A trigger counter with interval gate output VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "../MindMeldModular.hpp"


struct TrigCounter : Module {
	
	enum ParamIds {
		LOW_PARAM,
		HIGH_PARAM,
		TOTAL_PARAM,
		NUM_PARAMS
	};
	
	enum InputIds {
		TRIG_INPUT,
		RESET_INPUT,
		NUM_INPUTS
	};
	
	enum OutputIds {
		GATE_OUTPUT,
		NUM_OUTPUTS
	};
	
	enum LightIds {
		NUM_LIGHTS
	};
	

	// Constants
	// none


	// Need to save, no reset
	// none
	
	// Need to save, with reset
	int count;
	
	// No need to save, with reset
	// none

	// No need to save, no reset
	RefreshCounter refresh;	
	Trigger trigTrigger;
	Trigger resetTrigger;
	
	
	TrigCounter() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		configParam(LOW_PARAM, 1.0f, 99.0f, 2.0f, "Low bound");
		getParamQuantity(LOW_PARAM)->snapEnabled = true;
		configParam(HIGH_PARAM, 1.0f, 99.0f, 3.0f, "High bound");
		getParamQuantity(HIGH_PARAM)->snapEnabled = true;
		configParam(TOTAL_PARAM, 1.0f, 100.0f, 4.0f, "Total for reset");// 100.0f means infinity (no reset)
		getParamQuantity(TOTAL_PARAM)->snapEnabled = true;
		
		configInput(TRIG_INPUT, "Trigger/gate/clock");
		configInput(RESET_INPUT, "Reset");
		
		configOutput(GATE_OUTPUT, "Gate");
		
		onReset();
	}
  
	void onReset() override {
		count = 0;
		resetNonJson(false);
	}
	void resetNonJson(bool recurseNonJson) {
	}


	void onRandomize() override {
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		
		// count
		json_object_set_new(rootJ, "count", json_integer(count));

		return rootJ;
	}


	void dataFromJson(json_t *rootJ) override {
		// count
		json_t *countJ = json_object_get(rootJ, "count");
		if (countJ)
			count = json_integer_value(countJ);

		resetNonJson(true);	
	}


	void onSampleRateChange() override {
	}
	

	void process(const ProcessArgs &args) override {
		
		// Inputs and slow
		if (refresh.processInputs()) {
		
		}// userInputs refresh
		
		
		// Reset and trig
		if (resetTrigger.process(inputs[RESET_INPUT].getVoltage())) {
			count = 0;
		}
		if (trigTrigger.process(inputs[TRIG_INPUT].getVoltage())) {
			count++;
		}
		
		// Output
		int low = (int)(params[LOW_PARAM].getValue() + 0.5f);
		int high = (int)(params[HIGH_PARAM].getValue() + 0.5f);
		bool gate = (count >= low && count <= high);
		outputs[GATE_OUTPUT].setVoltage(gate ? 10.0f : 0.0f);
		
		// Lights
		if (refresh.processLights()) {
			// none
		}
	}// process()
};


//-----------------------------------------------------------------------------


struct TrigCounterWidget : ModuleWidget {
	TrigCounterWidget(TrigCounter *module) {
		setModule(module);

		// Main panels from Inkscape
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/meld/meld-1-8.svg")));
	}
};


Model *modelTrigCounter = createModel<TrigCounter, TrigCounterWidget>("TrigCounter");
