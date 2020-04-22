//***********************************************************************************************
//Test module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "MindMeldModular.hpp"
#include "dsp/Bessel.hpp"


struct Test : Module {
	
	enum ParamIds {
		NUM_PARAMS
	};
	
	enum InputIds {
		ENUMS(IN_INPUTS, 2),
		NUM_INPUTS
	};
	
	enum OutputIds {
		ENUMS(OUT_OUTPUTS, 2),
		NUM_OUTPUTS
	};
	
	enum LightIds {
		NUM_LIGHTS
	};
	

	// Constants
	// none

	// Need to save, no reset
	int panelTheme;
	
	// Need to save, with reset
	// none
	
	// No need to save, with reset
	Bessel3 iir1;
	Bessel4 iir2;
	
	// No need to save, no reset
	RefreshCounter refresh;	
	
	
	Test() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		// configParam(BYPASS_PARAMS + i, 0.0f, 1.0f, 0.0f, string::f("Bypass %i", i + 1));
		
		iir1.init();
		iir2.init();
		
		onReset();
		
		panelTheme = 0;
	}
  
	void onReset() override {
		iir1.reset();
		iir2.reset();
		resetNonJson(false);
	}
	void resetNonJson(bool recurseNonJson) {
	}


	void onRandomize() override {
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		
		return rootJ;
	}


	void dataFromJson(json_t *rootJ) override {

		resetNonJson(true);
	}


	void onSampleRateChange() override {
	}
	

	void process(const ProcessArgs &args) override {
		outputs[OUT_OUTPUTS + 0].setVoltage(iir1.process(inputs[IN_INPUTS + 0].getVoltage()));
		outputs[OUT_OUTPUTS + 1].setVoltage(iir2.process(inputs[IN_INPUTS + 1].getVoltage()));
	}// process()
};


//-----------------------------------------------------------------------------


struct TestWidget : ModuleWidget {
	
	TestWidget(Test *module) {
		setModule(module);

		// Main panel from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/meld-1-8.svg")));
 
		// inputs
		addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(10.33, 34.5 + 10.85 * 6)), true, module, Test::IN_INPUTS  + 0, module ? &module->panelTheme : NULL));
		addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(20.15, 34.5 + 10.85 * 6)), true, module, Test::IN_INPUTS + 1, module ? &module->panelTheme : NULL));
			
		// outputs
		addOutput(createDynamicPortCentered<DynPort>(mm2px(Vec(10.33, 34.5 + 10.85 * 7)), false, module, Test::OUT_OUTPUTS  + 0, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPortCentered<DynPort>(mm2px(Vec(20.15, 34.5 + 10.85 * 7)), false, module, Test::OUT_OUTPUTS + 1, module ? &module->panelTheme : NULL));
			
	}
};


Model *modelTest = createModel<Test, TestWidget>("Test");
