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
		SPLIT_PARAM,
		BASS_WIDTH_PARAM,// 0 to 1.0f; 0 is mono, 1 is stereo
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
	float splitPoint;
	dsp::BiquadFilter butter[8];// L low 1, low 2, R low 1, low 2, L high 1, high 2, R high 1, high 2
	dsp::SlewLimiter bassWidthSlewer;
	
	// No need to save, no reset
	RefreshCounter refresh;	
	
	
	void setFilterCutoffs(float nfc) {
		for (int i = 0; i < 4; i++) { 
			butter[i].setParameters(dsp::BiquadFilter::LOWPASS, nfc, 1.0f / std::sqrt(2.0f), 1.f);
			butter[i + 4].setParameters(dsp::BiquadFilter::HIGHPASS, nfc, 1.0f / std::sqrt(2.0f), 1.f);
		}
	}
	
	
	
	Test() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(SPLIT_PARAM, 50.0f, 500.0f, 120.0f, "Split point", " Hz");
		configParam(BASS_WIDTH_PARAM, 0.0f, 1.0f, 0.0f, "Bass width", "%", 0.0f, 100.0f);// diplay params are: base, mult, offset
					
					
		onReset();
		
		panelTheme = 0;
		bassWidthSlewer.setRiseFall(125.0f, 125.0f); // slew rate is in input-units per second (ex: V/s)
	}
  
	void onReset() override {
		resetNonJson(false);
	}
	void resetNonJson(bool recurseNonJson) {
		splitPoint = params[SPLIT_PARAM].getValue();
		float nfc = splitPoint / APP->engine->getSampleRate();
		setFilterCutoffs(nfc);
		
		for (int i = 0; i < 8; i++) { 
			butter[i].reset();
		}
		bassWidthSlewer.reset();
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
		float newSplitPoint = params[SPLIT_PARAM].getValue();
		if (splitPoint != newSplitPoint) {
			splitPoint = newSplitPoint;
			float nfc = splitPoint / args.sampleRate;
			setFilterCutoffs(nfc);
		}
		
		float leftLow = butter[0].process(butter[1].process(inputs[IN_INPUTS + 0].getVoltage()));
		float leftHigh = butter[4].process(butter[5].process(inputs[IN_INPUTS + 0].getVoltage()));
		float rightLow = butter[2].process(butter[3].process(inputs[IN_INPUTS + 1].getVoltage()));
		float rightHigh = butter[6].process(butter[7].process(inputs[IN_INPUTS + 1].getVoltage()));
		
		
		// Bass width (apply to leftLow and rightLow
		float newBassWidth = params[BASS_WIDTH_PARAM].getValue();
		if (newBassWidth != bassWidthSlewer.out) {
			bassWidthSlewer.process(args.sampleTime, newBassWidth);
		}
		float wdiv2 = bassWidthSlewer.out * 0.5f;
		float up = 0.5f + wdiv2;
		float down = 0.5f - wdiv2;
		float leftSig = leftLow * up + rightLow * down;
		float rightSig = rightLow * up + leftLow * down;
		leftLow = leftSig;
		rightLow = rightSig;
		
		
		outputs[OUT_OUTPUTS + 0].setVoltage(leftLow + leftHigh);
		outputs[OUT_OUTPUTS + 1].setVoltage(rightLow + rightHigh);
	}// process()
};


//-----------------------------------------------------------------------------


struct TestWidget : ModuleWidget {
	
	TestWidget(Test *module) {
		setModule(module);

		// Main panel from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/meld-1-8.svg")));
 
		// splitpoint knob
		addParam(createParamCentered<Davies1900hLargeWhiteKnob>(mm2px(Vec(15.22, 34.5 + 10.85 * 2)), module, Test::SPLIT_PARAM));
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
