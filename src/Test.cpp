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

	// LPF:
	// ----

	// Butterworth N=2, fc=1kHz, fs=44.1kHz
	// float b[3] = {0.004604f, 2.0f * 0.004604f, 0.004604f};
	// float a[2] = {-1.7990964095f, +0.8175124034f};
	
	// Bessel N=2, fc=1kHz, fs=44.1kHz, u=2 (1 sample delay, mesured ?)
	// float b[3] = {0.0155982532f, 2.0f * 0.0155982532f, 0.0155982532f};
	// float a[2] = {-1.500428132f, +0.562821145f};
	
	// Bessel N=3, fc=1kHz, fs=44.1kHz, u=3 (1.5 sample delay, measured 8)
	// static const int N = 3;
	// static constexpr float acst = 0.0054594483186731f;
	// float b[N + 1] = {acst, 3.0f * acst, 3.0f * acst, acst};
	// float a[N] = {-1.9435048603831f, 1.2590703807778f, -0.27188993384517f};
	
	// Bessel N=4, fc=1kHz, fs=44.1kHz, u=4 (2 sample delay, measured 8)
	// static const int N = 4;
	// static constexpr float acst = 0.0024312359468706f;
	// float b[N + 1] = {acst, 4.0f * acst, 6.0f * acst, 4.0f * acst, acst};
	// float a[N] = {-2.2235754596839f, 1.8541079343412f, -0.68712481706786f, 0.095492117560765f};


	// HPF:
	// ----

	// Bessel N=3, fc=1kHz, fs=44.1kHz, u=3 (1.5 sample delay, measured undoable)
	static const int N = 3;
	static constexpr float acst = 0.93201601848498f;
	float b[N + 1] = {acst, -3.0f * acst, 3.0f * acst, -acst};
	float a[N] = {-2.8608288964104f, 2.7281139915122f, -0.86718525995771f};

	// Bessel N=4, fc=1kHz, fs=44.1kHz, u=4 (2 sample delay, measured ?)
	// static const int N = 4;
	// static constexpr float acst = 0.93182429583953f;
	// float b[N + 1] = {acst, -4.0f * acst, 6.0f * acst, -4.0f * acst, acst};
	// float a[N] = {-3.8600171645062f, 5.5873996913559f, -3.5945764522652f, 0.86719542530458f};


	// Need to save, no reset
	int panelTheme;
	
	// Need to save, with reset
	// none
	
	// No need to save, with reset
	dsp::IIRFilter<N + 1, N + 1, float> iir;
	
	// No need to save, no reset
	RefreshCounter refresh;	
	
	
	Test() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		// configParam(BYPASS_PARAMS + i, 0.0f, 1.0f, 0.0f, string::f("Bypass %i", i + 1));
		
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
