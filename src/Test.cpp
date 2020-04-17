//***********************************************************************************************
//Test module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "MindMeldModular.hpp"


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
	dsp::IIRFilter<5, 5, float> iir;
	
	// No need to save, no reset
	RefreshCounter refresh;	
	
	
	Test() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		// configParam(BYPASS_PARAMS + i, 0.0f, 1.0f, 0.0f, string::f("Bypass %i", i + 1));

		// Butterworth N=2, fc=1kHz, fs=44.1kHz
		// float b[3] = {0.004604f, 2.0f * 0.004604f, 0.004604f};
		// float a[2] = {-1.7990964095f, +0.8175124034f};
		
		// Bessel N=2, fc=1kHz, fs=44.1kHz, u=2 (1 sample delay)
		// float b[3] = {0.0155982532f, 2.0f * 0.0155982532f, 0.0155982532f};
		// float a[2] = {-1.500428132f, +0.562821145f};
		
		// Bessel N=4, fc=1kHz, fs=44.1kHz, u=4 (2 sample delay)
		float b[5] = {0.002431236f, 4.0f * 0.002431236f, 6.0f * 0.002431236f, 4.0f * 0.002431236f, 0.002431236f};
		float a[4] = {-2.22357546f, 1.854107934f, -0.6871248171f, 0.095492117561f};
		
		iir.setCoefficients(b, a);
		
		onReset();
		
		panelTheme = 0;
	}
  
	void onReset() override {
		iir.reset();
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
		outputs[OUT_OUTPUTS + 0].setVoltage(iir.process(inputs[IN_INPUTS + 0].getVoltage()));

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
