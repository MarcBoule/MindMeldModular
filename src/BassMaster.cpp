//***********************************************************************************************
//Bass mono module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************




#include "MindMeldModular.hpp"
#include "dsp/LinkwitzRileyCrossover.hpp"


struct BassMaster : Module {
	
	enum ParamIds {
		CROSSOVER_PARAM,
		LOW_WIDTH_PARAM,// 0 to 1.0f; 0 = mono, 1 = stereo
		HIGH_WIDTH_PARAM,// 0 to 2.0f; 0 = mono, 1 = stereo, 2 = 200% wide
		LOW_SOLO_PARAM,
		HIGH_SOLO_PARAM,
		LOW_GAIN_PARAM,// -20 to +20 dB
		HIGH_GAIN_PARAM,// -20 to +20 dB
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
	bool is24db = false;

	// Need to save, no reset
	int panelTheme;
	
	// Need to save, with reset
	// none
	
	// No need to save, with reset
	float crossover;
	dsp::IIRFilter<2 + 1, 2 + 1, float> iirs[8];// Left low 1, low 2, Right low 1, low 2, Left high 1, high 2, Right high 1, high 2
	dsp::SlewLimiter lowWidthSlewer;
	dsp::SlewLimiter highWidthSlewer;
	
	// No need to save, no reset
	RefreshCounter refresh;
	
	
	void setFilterCutoffs(float nfc, bool secondOrder) {
		// nfc: normalized cutoff frequency (cutoff frequency / sample rate), must be > 0
		// freq pre-warping with inclusion of M_PI factor; 
		//   avoid tan() if fc is low (< 1102.5 Hz @ 44.1 kHz, since error at this freq is 2 Hz)
		float nfcw = nfc < 0.025f ? M_PI * nfc : std::tan(M_PI * std::min(0.499f, nfc));
		
		if (secondOrder) {
			// butterworth LPF and HPF filters (for 4th order Linkwitz-Riley crossover)
			// denominator coefficients (same for both LPF and HPF)
			float acst = nfcw * nfcw + nfcw * (float)M_SQRT2 + 1.0f;
			float a[2] = {2.0f * (nfcw * nfcw - 1.0f) / acst, (nfcw * nfcw - nfcw * (float)M_SQRT2 + 1.0f) / acst};
			// numerator coefficients
			float hbcst = 1.0f / acst;
			float hb[2 + 1] = {hbcst, -2.0f * hbcst, hbcst};
			float lbcst = hbcst * nfcw * nfcw;
			float lb[2 + 1] = {lbcst, 2.0f * lbcst, lbcst};
			for (int i = 0; i < 4; i++) { 
				iirs[i].setCoefficients(lb, a);
				iirs[i + 4].setCoefficients(hb, a);
			}
		}
		else {
			// first order LPF and HPF filters (for 2nd order Linkwitz-Riley crossover)
			// denominator coefficients (same for both LPF and HPF)
			float acst = (nfcw - 1.0f) / (nfcw + 1.0f);
			float a[2] = {acst, 0.0f};
			// numerator coefficients
			float hbcst = 1.0f / (1.0f + nfcw);
			float hb[2 + 1] = {hbcst, -hbcst, 0.0f};
			float lbcst = 1.0f - hbcst;// equivalent to: hbcst * nfcw;
			float lb[2 + 1] = {lbcst, lbcst, 0.0f};
			for (int i = 0; i < 4; i++) { 
				iirs[i].setCoefficients(lb, a);
				iirs[i + 4].setCoefficients(hb, a);
			}
		}
	}
	
	
	BassMaster() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(CROSSOVER_PARAM, 50.0f, 500.0f, 120.0f, "Crossover", " Hz");
		configParam(LOW_WIDTH_PARAM, 0.0f, 1.0f, 1.0f, "Low width", "%", 0.0f, 100.0f);// diplay params are: base, mult, offset
		configParam(HIGH_WIDTH_PARAM, 0.0f, 2.0f, 1.0f, "High width", "%", 0.0f, 100.0f);// diplay params are: base, mult, offset
		configParam(LOW_SOLO_PARAM, 0.0f, 1.0f, 0.0f, "Low solo");
		configParam(HIGH_SOLO_PARAM, 0.0f, 1.0f, 0.0f, "High solo");
		configParam(LOW_GAIN_PARAM, -20.0f, 20.0f, 0.0f, "Low gain", " dB");
		configParam(HIGH_GAIN_PARAM, -20.0f, 20.0f, 0.0f, "High gain", " dB");
					
		onReset();
		
		panelTheme = 0;
		lowWidthSlewer.setRiseFall(125.0f, 125.0f); // slew rate is in input-units per second (ex: V/s)
		highWidthSlewer.setRiseFall(125.0f, 125.0f); // slew rate is in input-units per second (ex: V/s)
	}
  
	void onReset() override {
		resetNonJson(false);
	}
	void resetNonJson(bool recurseNonJson) {
		crossover = params[CROSSOVER_PARAM].getValue();
		setFilterCutoffs(crossover / APP->engine->getSampleRate(), is24db);
		
		for (int i = 0; i < 8; i++) { 
			iirs[i].reset();
		}
		lowWidthSlewer.reset();
		highWidthSlewer.reset();
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
		setFilterCutoffs(crossover / APP->engine->getSampleRate(), is24db);
	}
	

	void process(const ProcessArgs &args) override {
		float newcrossover = params[CROSSOVER_PARAM].getValue();
		if (crossover != newcrossover) {
			crossover = newcrossover;
			float nfc = crossover / args.sampleRate;
			setFilterCutoffs(nfc, is24db);
		}
		
		float leftLow = inputs[IN_INPUTS + 0].getVoltage();
		float leftHigh = leftLow;
		float rightLow = inputs[IN_INPUTS + 1].getVoltage();
		float rightHigh = rightLow;
		if (!is24db) {
			leftLow *= -1.0f;
			rightLow *= -1.0f;
		}
		leftLow = iirs[0].process(iirs[1].process(leftLow));
		leftHigh = iirs[4].process(iirs[5].process(leftHigh));
		rightLow = iirs[2].process(iirs[3].process(rightLow));
		rightHigh = iirs[6].process(iirs[7].process(rightHigh));

		// Bass width (apply to leftLow and rightLow)
		float newLowWidth = params[LOW_WIDTH_PARAM].getValue();
		if (newLowWidth != lowWidthSlewer.out) {
			lowWidthSlewer.process(args.sampleTime, newLowWidth);
		}
		// lowWidthSlewer is 0 to 1.0f; 0 is mono, 1 is stereo
		float wdiv2 = lowWidthSlewer.out * 0.5f;// in this algo, width can go to 2.0f to implement 200% stereo widening
		float up = 0.5f + wdiv2;
		float down = 0.5f - wdiv2;
		float leftSig = leftLow * up + rightLow * down;
		float rightSig = rightLow * up + leftLow * down;
		leftLow = leftSig;
		rightLow = rightSig;
		
		// High width (apply to leftHigh and rightHigh)
		float newHighWidth = params[HIGH_WIDTH_PARAM].getValue();
		if (newHighWidth != highWidthSlewer.out) {
			highWidthSlewer.process(args.sampleTime, newHighWidth);
		}
		// highWidthSlewer is 0 to 2.0f; 0 is mono, 1 is stereo, 2 is 200% wide
		wdiv2 = highWidthSlewer.out * 0.5f;// in this algo, width can go to 2.0f to implement 200% stereo widening
		up = 0.5f + wdiv2;
		down = 0.5f - wdiv2;
		leftSig = leftHigh * up + rightHigh * down;
		rightSig = rightHigh * up + leftHigh * down;
		leftHigh = leftSig;
		rightHigh = rightSig;
		
		outputs[OUT_OUTPUTS + 0].setVoltage(leftLow + leftHigh);
		outputs[OUT_OUTPUTS + 1].setVoltage(rightLow + rightHigh);
	}// process()
};


//-----------------------------------------------------------------------------


struct BassMasterWidget : ModuleWidget {
	
	struct CrossoverKnob : DynKnob {
		CrossoverKnob() {
			addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/big-knob-pointer.svg")));
		}
	};	
	
	BassMasterWidget(BassMaster *module) {
		setModule(module);

		// Main panel from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/BassMaster.svg")));
 
		// crossover knob
		addParam(createDynamicParamCentered<CrossoverKnob>(mm2px(Vec(15.24, 22.49)), module, BassMaster::CROSSOVER_PARAM, module ? &module->panelTheme : NULL));
		
		// low solo button
		// addParam(createParamCentered<CKSS>(mm2px(Vec(15.24, 73.71)), module, BassMaster::LOW_SOLO_PARAM));

		// high width and gain
		addParam(createDynamicParamCentered<DynSmallKnobGrey>(mm2px(Vec(7.5, 51.68)), module, BassMaster::HIGH_WIDTH_PARAM, module ? &module->panelTheme : NULL));
		addParam(createDynamicParamCentered<DynSmallKnobGrey>(mm2px(Vec(22.9, 51.68)), module, BassMaster::HIGH_GAIN_PARAM, module ? &module->panelTheme : NULL));
 
		// low width and gain
		addParam(createDynamicParamCentered<DynSmallKnobGrey>(mm2px(Vec(7.5, 79.46)), module, BassMaster::LOW_WIDTH_PARAM, module ? &module->panelTheme : NULL));
		addParam(createDynamicParamCentered<DynSmallKnobGrey>(mm2px(Vec(22.9, 79.46)), module, BassMaster::LOW_GAIN_PARAM, module ? &module->panelTheme : NULL));
 
		// inputs
		addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(6.81, 102.03)), true, module, BassMaster::IN_INPUTS + 0, module ? &module->panelTheme : NULL));
		addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(6.81, 111.45)), true, module, BassMaster::IN_INPUTS + 1, module ? &module->panelTheme : NULL));
			
		// outputs
		addOutput(createDynamicPortCentered<DynPort>(mm2px(Vec(23.52, 102.03)), false, module, BassMaster::OUT_OUTPUTS  + 0, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPortCentered<DynPort>(mm2px(Vec(23.52, 111.45)), false, module, BassMaster::OUT_OUTPUTS + 1, module ? &module->panelTheme : NULL));
	}
};


Model *modelBassMaster = createModel<BassMaster, BassMasterWidget>("BassMaster");
