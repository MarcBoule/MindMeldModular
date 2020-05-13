//***********************************************************************************************
//Test module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


/* 		{
			"slug": "Test",
			"name": "Test",
			"description": "Test module",
			"manualUrl": "https://github.com/MarcBoule/MindMeldModular/tree/master/doc/MindMeld-MixMaster-Manual-V1_1_4.pdf",
			"tags": ["Utility"]
		}
*/

#include "MindMeldModular.hpp"
#include "dsp/Bessel.hpp"


struct Test : Module {
	
	enum ParamIds {
		CROSSOVER_PARAM,
		BASS_WIDTH_PARAM,// 0 to 1.0f; 0 is stereo, 1 is mono
		TWENTYFOURDB_PARAM,// 24 dB/oct when true, else 12 dB
		SOLO_PARAM,
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
	float crossover;
	dsp::BiquadFilter butter[8];// L low 1, low 2, R low 1, low 2, L high 1, high 2, R high 1, high 2
	dsp::IIRFilter<2 + 1, 2 + 1, float> onepole[8];
	dsp::SlewLimiter bassWetSlewer;
	
	// No need to save, no reset
	RefreshCounter refresh;	
	
	
	void setFilterCutoffs(float nfc) {
		// butter
		for (int i = 0; i < 4; i++) { 
			butter[i].setParameters(dsp::BiquadFilter::LOWPASS, nfc, 1.0f / std::sqrt(2.0f), 1.f);
			butter[i + 4].setParameters(dsp::BiquadFilter::HIGHPASS, nfc, 1.0f / std::sqrt(2.0f), 1.f);
		}
		// one pole
		float hbcst = 1.0f / (1.0f + M_PI * nfc);
		float hb[2 + 1] = {hbcst, -hbcst, 0.0f};
		float acst = (M_PI * nfc - 1.0f) / (M_PI * nfc + 1.0f);
		float a[2] = {acst, 0.0f};
		float lbcst = 1.0f - hbcst;
		float lb[2 + 1] = {lbcst, lbcst, 0.0f};
		for (int i = 0; i < 4; i++) { 
			onepole[i].setCoefficients(lb, a);
			onepole[i + 4].setCoefficients(hb, a);
		}
	}
	
	
	Test() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(CROSSOVER_PARAM, 50.0f, 500.0f, 120.0f, "Crossover", " Hz");
		configParam(BASS_WIDTH_PARAM, 0.0f, 1.0f, 1.0f, "Dry/Wet", "%", 0.0f, 100.0f);// diplay params are: base, mult, offset
		configParam(TWENTYFOURDB_PARAM, 0.0f, 1.0f, 1.0f, "24 dB/oct");
		configParam(SOLO_PARAM, 0.0f, 1.0f, 0.0f, "Solo");
					
		onReset();
		
		panelTheme = 0;
		bassWetSlewer.setRiseFall(125.0f, 125.0f); // slew rate is in input-units per second (ex: V/s)
	}
  
	void onReset() override {
		resetNonJson(false);
	}
	void resetNonJson(bool recurseNonJson) {
		crossover = params[CROSSOVER_PARAM].getValue();
		setFilterCutoffs(crossover / APP->engine->getSampleRate());
		
		for (int i = 0; i < 8; i++) { 
			butter[i].reset();
			onepole[i].reset();
		}
		bassWetSlewer.reset();
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
		setFilterCutoffs(crossover / APP->engine->getSampleRate());
	}
	

	void process(const ProcessArgs &args) override {
		float newcrossover = params[CROSSOVER_PARAM].getValue();
		if (crossover != newcrossover) {
			crossover = newcrossover;
			float nfc = crossover / args.sampleRate;
			setFilterCutoffs(nfc);
		}
		
		float leftLow;
		float leftHigh;
		float rightLow;
		float rightHigh;
		if (params[TWENTYFOURDB_PARAM].getValue() >= 0.5f) {
			leftLow = butter[0].process(butter[1].process(inputs[IN_INPUTS + 0].getVoltage()));
			leftHigh = butter[4].process(butter[5].process(inputs[IN_INPUTS + 0].getVoltage()));
			rightLow = butter[2].process(butter[3].process(inputs[IN_INPUTS + 1].getVoltage()));
			rightHigh = butter[6].process(butter[7].process(inputs[IN_INPUTS + 1].getVoltage()));
		}
		else {
			leftLow = onepole[0].process(onepole[1].process(-inputs[IN_INPUTS + 0].getVoltage()));
			leftHigh = onepole[4].process(onepole[5].process(inputs[IN_INPUTS + 0].getVoltage()));
			rightLow = onepole[2].process(onepole[3].process(-inputs[IN_INPUTS + 1].getVoltage()));
			rightHigh = onepole[6].process(onepole[7].process(inputs[IN_INPUTS + 1].getVoltage()));
		}

		// Bass width (apply to leftLow and rightLow
		float newBassWidth = params[BASS_WIDTH_PARAM].getValue();
		if (newBassWidth != bassWetSlewer.out) {
			bassWetSlewer.process(args.sampleTime, newBassWidth);
		}
		float wdiv2 = bassWetSlewer.out * 0.5f;
		float up = 1.0f - wdiv2;
		float down = wdiv2;
		float leftSig = leftLow * up + rightLow * down;
		float rightSig = rightLow * up + leftLow * down;
		leftLow = leftSig;
		rightLow = rightSig;
		
		bool solo = params[SOLO_PARAM].getValue() >= 0.5f;
		if (!solo) {
			leftLow += leftHigh;
			rightLow += rightHigh;
		}
		outputs[OUT_OUTPUTS + 0].setVoltage(leftLow);
		outputs[OUT_OUTPUTS + 1].setVoltage(rightLow);
	}// process()
};


//-----------------------------------------------------------------------------


struct TestWidget : ModuleWidget {
	
	TestWidget(Test *module) {
		setModule(module);

		// Main panel from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/meld-1-8.svg")));
 
		// 24 db switch and solo button
		addParam(createParamCentered<CKSS>(mm2px(Vec(10.13, 34.5 + 10.85 * 0 - 6)), module, Test::TWENTYFOURDB_PARAM));
		addParam(createParamCentered<CKSS>(mm2px(Vec(20.15, 34.5 + 10.85 * 0 - 6)), module, Test::SOLO_PARAM));

		// crossover knob
		addParam(createParamCentered<Davies1900hLargeWhiteKnob>(mm2px(Vec(15.22, 34.5 + 10.85 * 2)), module, Test::CROSSOVER_PARAM));
		addParam(createParamCentered<Davies1900hWhiteKnob>(mm2px(Vec(15.22, 34.5 + 10.85 * 4)), module, Test::BASS_WIDTH_PARAM));
 
		// inputs
		addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(10.33, 34.5 + 10.85 * 6)), true, module, Test::IN_INPUTS  + 0, module ? &module->panelTheme : NULL));
		addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(20.15, 34.5 + 10.85 * 6)), true, module, Test::IN_INPUTS + 1, module ? &module->panelTheme : NULL));
			
		// outputs
		addOutput(createDynamicPortCentered<DynPort>(mm2px(Vec(10.33, 34.5 + 10.85 * 7)), false, module, Test::OUT_OUTPUTS  + 0, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPortCentered<DynPort>(mm2px(Vec(20.15, 34.5 + 10.85 * 7)), false, module, Test::OUT_OUTPUTS + 1, module ? &module->panelTheme : NULL));
				
			
	}
};


Model *modelTest = createModel<Test, TestWidget>("Test");
