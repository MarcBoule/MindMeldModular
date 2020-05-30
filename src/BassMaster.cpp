//***********************************************************************************************
//Bass mono module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "MindMeldModular.hpp"
#include "VuMeters.hpp"
#include "dsp/LinkwitzRileyCrossover.hpp"


template<bool IS_JR>
struct BassMaster : Module {
	
	enum ParamIds {
		CROSSOVER_PARAM,
		SLOPE_PARAM,
		LOW_WIDTH_PARAM,// 0 to 1.0f; 0 = mono, 1 = stereo
		HIGH_WIDTH_PARAM,// 0 to 2.0f; 0 = mono, 1 = stereo, 2 = 200% wide
		LOW_SOLO_PARAM,
		HIGH_SOLO_PARAM,
		LOW_GAIN_PARAM,// -20 to +20 dB
		HIGH_GAIN_PARAM,// -20 to +20 dB
		BYPASS_PARAM,
		GAIN_PARAM,
		MIX_PARAM,
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
	static constexpr float DEFAULT_SLOPE = 0.0f;

	// Need to save, no reset
	int panelTheme;
	
	// Need to save, with reset
	PackedBytes4 miscSettings;// cc4[0] is display label colours, cc4[1] is polyStereo, cc4[2] is VU color, cc4[3] is unused
	
	// No need to save, with reset
	float crossover;
	bool is24db;
	bool lowSolo;
	bool highSolo;
	LinkwitzRileyCrossover xover;
	dsp::TSlewLimiter<simd::float_4> widthAndGainSlewers;// [0] = low width, high width, low gain, [3] = high gain
	dsp::TSlewLimiter<simd::float_4> solosAndBypassSlewers;// [0] = low solo, high solo, bypass, [3] = master gain
	dsp::SlewLimiter mixSlewer;
	float linearLowGain;
	float linearHighGain;
	float linearMasterGain;
	VuMeterAllDual trackVu;
	
	// No need to save, no reset
	RefreshCounter refresh;
	
	
	BassMaster() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(CROSSOVER_PARAM, 50.0f, 500.0f, 120.0f, "Crossover", " Hz");
		configParam(SLOPE_PARAM, 0.0f, 1.0f, DEFAULT_SLOPE, "Slope 24 dB/oct");
		configParam(LOW_WIDTH_PARAM, 0.0f, 1.0f, 1.0f, "Low width", "%", 0.0f, 100.0f);// diplay params are: base, mult, offset
		configParam(HIGH_WIDTH_PARAM, 0.0f, 2.0f, 1.0f, "High width", "%", 0.0f, 100.0f);// diplay params are: base, mult, offset
		configParam(LOW_SOLO_PARAM, 0.0f, 1.0f, 0.0f, "Low solo");
		configParam(HIGH_SOLO_PARAM, 0.0f, 1.0f, 0.0f, "High solo");
		configParam(LOW_GAIN_PARAM, -1.0f, 1.0f, 0.0f, "Low gain", " dB", 0.0f, 20.0f);// diplay params are: base, mult, offset
		configParam(HIGH_GAIN_PARAM, -1.0f, 1.0f, 0.0f, "High gain", " dB", 0.0f, 20.0f);// diplay params are: base, mult, offset
		configParam(BYPASS_PARAM, 0.0f, 1.0f, 0.0f, "Bypass");
		configParam(GAIN_PARAM, -1.0f, 1.0f, 0.0f, "Master gain", " dB", 0.0f, 20.0f);// diplay params are: base, mult, offset
		configParam(MIX_PARAM, 0.0f, 1.0f, 1.0f, "Mix", "%", 0.0f, 100.0f);// diplay params are: base, mult, offset
					
		widthAndGainSlewers.setRiseFall(simd::float_4(25.0f), simd::float_4(25.0f)); // slew rate is in input-units per second (ex: V/s)		
		solosAndBypassSlewers.setRiseFall(simd::float_4(25.0f), simd::float_4(25.0f)); // slew rate is in input-units per second (ex: V/s)	
		mixSlewer.setRiseFall(25.0f, 25.0f);

		onReset();
		
		panelTheme = 0;
	}
  
	void onReset() override {
		params[SLOPE_PARAM].setValue(DEFAULT_SLOPE);// need this since no wigdet exists
		miscSettings.cc4[0] = 0;// display label colours
		miscSettings.cc4[1] = 0;// polyStereo
		miscSettings.cc4[2] = 0;// default color
		miscSettings.cc4[3] = 0;// unused
		resetNonJson(false);
	}
	void resetNonJson(bool recurseNonJson) {
		crossover = params[CROSSOVER_PARAM].getValue();
		is24db = params[SLOPE_PARAM].getValue() >= 0.5f;
		lowSolo = params[LOW_SOLO_PARAM].getValue() >= 0.5f;
		highSolo = params[HIGH_SOLO_PARAM].getValue() >= 0.5f;
		xover.setFilterCutoffs(crossover / APP->engine->getSampleRate(), is24db);
		xover.reset();
		widthAndGainSlewers.reset();
		solosAndBypassSlewers.reset();
		mixSlewer.reset();
		linearLowGain = 1.0f;
		linearHighGain = 1.0f;
		linearMasterGain = 1.0f;
		trackVu.reset();
	}


	void onRandomize() override {
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		
		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));
				
		// miscSettings
		json_object_set_new(rootJ, "miscSettings", json_integer(miscSettings.cc1));
				
		return rootJ;
	}


	void dataFromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// miscSettings
		json_t *miscSettingsJ = json_object_get(rootJ, "miscSettings");
		if (miscSettingsJ)
			miscSettings.cc1 = json_integer_value(miscSettingsJ);

		resetNonJson(true);
	}


	void onSampleRateChange() override {
		xover.setFilterCutoffs(crossover / APP->engine->getSampleRate(), is24db);
	}
	

	void process(const ProcessArgs &args) override {
		// crossover knob and is24dB refreshes
		float newCrossover = params[CROSSOVER_PARAM].getValue();
		bool newIs24db = params[SLOPE_PARAM].getValue() >= 0.5f;
		if (crossover != newCrossover || is24db != newIs24db) {
			crossover = newCrossover;
			is24db = newIs24db;
			xover.setFilterCutoffs(crossover / args.sampleRate, is24db);
		}
	
		// solo mutex mechanism and solo refreshes
		bool newLowSolo = params[LOW_SOLO_PARAM].getValue() >= 0.5f;
		if (lowSolo != newLowSolo) {
			if (newLowSolo) {
				params[HIGH_SOLO_PARAM].setValue(0.0f);
				highSolo = 0.0f;
			}
			lowSolo = newLowSolo;
		}	
		bool newHighSolo = params[HIGH_SOLO_PARAM].getValue() >= 0.5f;
		if (highSolo != newHighSolo) {
			if (newHighSolo) {
				params[LOW_SOLO_PARAM].setValue(0.0f);
				lowSolo = 0.0f;
			}
			highSolo = newHighSolo;	
		}
		
		float inLeft;
		float inRight;
		bool polyStereo = miscSettings.cc4[1] != 0 && !inputs[IN_INPUTS + 1].isConnected() && inputs[IN_INPUTS + 0].isPolyphonic();
		if (polyStereo) {
			// here were are in polyStero mode, so take all odd numbered into L, even numbered into R (1-indexed)
			inLeft = 0.0f;
			inRight = 0.0f;
			for (int c = 0; c < inputs[IN_INPUTS + 0].getChannels(); c++) {
				if ((c & 0x1) == 0) {// if L channels (odd channels when 1-indexed)
					inLeft += inputs[IN_INPUTS + 0].getVoltage(c);
				}
				else {
					inRight += inputs[IN_INPUTS + 0].getVoltage(c);
				}
			}
		}
		else {
			inLeft = inputs[IN_INPUTS + 0].getVoltageSum();
			inRight = inputs[IN_INPUTS + 1].getVoltageSum();
		}
		
		simd::float_4 outs = xover.process(clamp20V(inLeft), clamp20V(inRight));
		// outs: [0] = left low, left high, right low, [3] = right high
		float dryLeft;
		float dryRight;
		if (!IS_JR) {
			dryLeft = outs[0] + outs[1];
			dryRight = outs[2] + outs[3];
		}
		
		// Width and gain slewers
		simd::float_4 widthAndGain = simd::float_4(params[LOW_WIDTH_PARAM].getValue(), params[HIGH_WIDTH_PARAM].getValue(),
												   params[LOW_GAIN_PARAM].getValue(), params[HIGH_GAIN_PARAM].getValue());
		if (movemask(widthAndGain == widthAndGainSlewers.out) != 0xF) {// movemask returns 0xF when 4 floats are equal
			widthAndGainSlewers.process(args.sampleTime, widthAndGain);
			linearLowGain = std::pow(10.0f, widthAndGainSlewers.out[2]);
			linearHighGain = std::pow(10.0f, widthAndGainSlewers.out[3]);
		}
		
		// Widths (low and high)
		applyStereoWidth(widthAndGainSlewers.out[0], &outs[0], &outs[2]);// bass width (apply to left low and right low)	
		applyStereoWidth(widthAndGainSlewers.out[1], &outs[1], &outs[3]);// high width (apply to left high and right high)

		// Solos and bypass slewers
		simd::float_4 solosAndBypass = simd::float_4(lowSolo ? 0.0f : 1.0f, highSolo ? 0.0f : 1.0f, 
													 params[BYPASS_PARAM].getValue() >= 0.5f ? 0.0f : 1.0f, 
													 params[GAIN_PARAM].getValue());// last is master gain
		if (movemask(solosAndBypass == solosAndBypassSlewers.out) != 0xF) {// movemask returns 0xF when 4 floats are equal
			solosAndBypassSlewers.process(args.sampleTime, solosAndBypass);
			linearMasterGain = std::pow(10.0f, solosAndBypassSlewers.out[3]);
		}

		// Gains (low and high)
		float gLow = linearLowGain * solosAndBypassSlewers.out[1];
		float gHigh = linearHighGain * solosAndBypassSlewers.out[0];
		outs *= simd::float_4(gLow, gHigh, gLow, gHigh);
		
		// master gain (doesn't apply to Jr)
		if (!IS_JR) {
			outs *= linearMasterGain;
		}
		
		// convert to stereo
		float outStereo[2] = {outs[0] + outs[1], outs[2] + outs[3]};// [0] is left, [1] is right
		
		// mix knob (doesn't apply to Jr)
		if (!IS_JR) {
			mixSlewer.process(args.sampleTime, params[MIX_PARAM].getValue());
			outStereo[0] = crossfade(dryLeft, outStereo[0], mixSlewer.out);// 0.0 is first arg, 1.0 is second
			outStereo[1] = crossfade(dryRight, outStereo[1], mixSlewer.out);// 0.0 is first arg, 1.0 is second
		}
		
		// bypass
		outStereo[0] = crossfade(inputs[IN_INPUTS + 0].getVoltage(), outStereo[0], solosAndBypassSlewers.out[2]);// 0.0 is first arg, 1.0 is second
		outStereo[1] = crossfade(inputs[IN_INPUTS + 1].getVoltage(), outStereo[1], solosAndBypassSlewers.out[2]);// 0.0 is first arg, 1.0 is second

		// VU meter (doesn't apply to Jr)
		if (!IS_JR) {
			trackVu.process(args.sampleTime, outStereo);
		}

		outputs[OUT_OUTPUTS + 0].setVoltage(outStereo[0]);
		outputs[OUT_OUTPUTS + 1].setVoltage(outStereo[1]);
	}// process()
};


//-----------------------------------------------------------------------------

template<bool IS_JR>
struct BassMasterWidget : ModuleWidget {	
	struct BassMasterLabel : LedDisplayChoice {
		int8_t* dispColorPtr = NULL;
		
		BassMasterLabel() {
			box.size = mm2px(Vec(10.6f, 5.0f));
			font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/RobotoCondensed-Regular.ttf"));
			color = DISP_COLORS[0];
			textOffset = Vec(4.2f, 11.3f);
			text = "---";
		};
		
		void draw(const DrawArgs &args) override {
			if (dispColorPtr) {
				color = DISP_COLORS[*dispColorPtr];
			}	
			LedDisplayChoice::draw(args);
		}
	};	
	BassMasterLabel* bassMasterLabels[5];// xover, width high, gain high, width low, gain low


	struct SlopeItem : MenuItem {
		Param* srcParam;

		struct SlopeSubItem : MenuItem {
			Param *srcParam;
			float setVal;
			void onAction(const event::Action &e) override {
				srcParam->setValue(setVal);
			}
		};

		Menu *createChildMenu() override {
			Menu *menu = new Menu;
			
			SlopeSubItem *slope0Item = createMenuItem<SlopeSubItem>("12 db/oct", CHECKMARK(srcParam->getValue() < 0.5f));
			slope0Item->srcParam = srcParam;
			slope0Item->setVal = 0.0f;
			menu->addChild(slope0Item);

			SlopeSubItem *slope1Item = createMenuItem<SlopeSubItem>("24 db/oct", CHECKMARK(srcParam->getValue() >= 0.5f));
			slope1Item->srcParam = srcParam;
			slope1Item->setVal = 1.0f;
			menu->addChild(slope1Item);

			return menu;
		}
	};	
	
	void appendContextMenu(Menu *menu) override {		
		BassMaster<IS_JR>* module = (BassMaster<IS_JR>*)(this->module);
		assert(module);

		menu->addChild(new MenuSeparator());
		
		SlopeItem *slopeItem = createMenuItem<SlopeItem>("Crossover slope", RIGHT_ARROW);
		slopeItem->srcParam = &(module->params[BassMaster<IS_JR>::SLOPE_PARAM]);
		menu->addChild(slopeItem);		

		PolyStereoItem *polySteItem = createMenuItem<PolyStereoItem>("Poly input behavior", RIGHT_ARROW);
		polySteItem->polyStereoSrc = &(module->miscSettings.cc4[1]);
		menu->addChild(polySteItem);

		menu->addChild(new MenuSeparator());

		DispTwoColorItem *dispColItem = createMenuItem<DispTwoColorItem>("Display colour", RIGHT_ARROW);
		dispColItem->srcColor = &(module->miscSettings.cc4[0]);
		menu->addChild(dispColItem);
		
		if (!IS_JR) {
			VuFiveColorItem *vuColItem = createMenuItem<VuFiveColorItem>("VU colour", RIGHT_ARROW);
			vuColItem->srcColors = &(module->miscSettings.cc4[2]);
			menu->addChild(vuColItem);
		}
	}

	BassMasterWidget(BassMaster<IS_JR> *module) {
		setModule(module);

		// Main panel from Inkscape
        if (IS_JR) {
			setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/BassMasterJr.svg")));
		}
		else {
			setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/BassMaster.svg")));
		}
 
		// crossover knob
		addParam(createDynamicParamCentered<DynBiggerKnobWhite>(mm2px(Vec(15.24, 22.98)), module, BassMaster<IS_JR>::CROSSOVER_PARAM, module ? &module->panelTheme : NULL));// was 22.49
		
		// all labels (xover, width high, gain high, width low, gain low)
		addChild(bassMasterLabels[0] = createWidgetCentered<BassMasterLabel>(mm2px(Vec(15.24, 32.3 + 1))));
		addChild(bassMasterLabels[1] = createWidgetCentered<BassMasterLabel>(mm2px(Vec(7.5 + 0.5, 59.71 + 1))));
		addChild(bassMasterLabels[2] = createWidgetCentered<BassMasterLabel>(mm2px(Vec(22.9 + 0.5, 59.71 + 1))));
		addChild(bassMasterLabels[3] = createWidgetCentered<BassMasterLabel>(mm2px(Vec(7.5 + 0.5, 87.42 + 1))));
		addChild(bassMasterLabels[4] = createWidgetCentered<BassMasterLabel>(mm2px(Vec(22.9 + 0.5, 87.42 + 1))));
		if (module) {
			for (int i = 0; i < 5; i++) {
				bassMasterLabels[i]->dispColorPtr = &(module->miscSettings.cc4[0]);
			}
		}
		
		// high solo button
		addParam(createDynamicParamCentered<DynSoloRoundButton>(mm2px(Vec(15.24, 45.93 + 1)), module, BassMaster<IS_JR>::HIGH_SOLO_PARAM, module ? &module->panelTheme : NULL));
		// low solo button
		addParam(createDynamicParamCentered<DynSoloRoundButton>(mm2px(Vec(15.24, 73.71 + 1)), module, BassMaster<IS_JR>::LOW_SOLO_PARAM, module ? &module->panelTheme : NULL));
		// bypass button
		addParam(createDynamicParamCentered<DynBypassRoundButton>(mm2px(Vec(15.24, 95.4 + 1)), module, BassMaster<IS_JR>::BYPASS_PARAM, module ? &module->panelTheme : NULL));

		// high width and gain
		addParam(createDynamicParamCentered<DynSmallKnobGrey8mm>(mm2px(Vec(7.5, 51.68 + 1)), module, BassMaster<IS_JR>::HIGH_WIDTH_PARAM, module ? &module->panelTheme : NULL));
		addParam(createDynamicParamCentered<DynSmallKnobGrey8mm>(mm2px(Vec(22.9, 51.68 + 1)), module, BassMaster<IS_JR>::HIGH_GAIN_PARAM, module ? &module->panelTheme : NULL));
 
		// low width and gain
		addParam(createDynamicParamCentered<DynSmallKnobGrey8mm>(mm2px(Vec(7.5, 79.46 + 1)), module, BassMaster<IS_JR>::LOW_WIDTH_PARAM, module ? &module->panelTheme : NULL));
		addParam(createDynamicParamCentered<DynSmallKnobGrey8mm>(mm2px(Vec(22.9, 79.46 + 1)), module, BassMaster<IS_JR>::LOW_GAIN_PARAM, module ? &module->panelTheme : NULL));
 
		// inputs
		addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(6.81, 102.03 + 1)), true, module, BassMaster<IS_JR>::IN_INPUTS + 0, module ? &module->panelTheme : NULL));
		addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(6.81, 111.45 + 1)), true, module, BassMaster<IS_JR>::IN_INPUTS + 1, module ? &module->panelTheme : NULL));
			
		// outputs
		addOutput(createDynamicPortCentered<DynPort>(mm2px(Vec(23.52, 102.03 + 1)), false, module, BassMaster<IS_JR>::OUT_OUTPUTS  + 0, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPortCentered<DynPort>(mm2px(Vec(23.52, 111.45 + 1)), false, module, BassMaster<IS_JR>::OUT_OUTPUTS + 1, module ? &module->panelTheme : NULL));
		
		if (!IS_JR) {
			// VU meter
			if (module) {
				VuMeterBassMono *newVU = createWidgetCentered<VuMeterBassMono>(mm2px(Vec(37, 37.5f)));
				newVU->srcLevels = module->trackVu.vuValues;
				newVU->bassVuColorsSrc = &(module->miscSettings.cc4[2]);
				addChild(newVU);
			}
						
			// master gain and mix
			addParam(createDynamicParamCentered<DynSmallKnobGrey8mm>(mm2px(Vec(37, 68)), module, BassMaster<IS_JR>::GAIN_PARAM, module ? &module->panelTheme : NULL));
			addParam(createDynamicParamCentered<DynSmallKnobGrey8mm>(mm2px(Vec(37, 85)), module, BassMaster<IS_JR>::MIX_PARAM, module ? &module->panelTheme : NULL));
		}
	}
	
	void step() override {
		BassMaster<IS_JR>* module = (BassMaster<IS_JR>*)(this->module);
		if (module) {
			bassMasterLabels[0]->text = string::f("%i", (int)(module->crossover + 0.5f));
			bassMasterLabels[1]->text = string::f("%i", (int)(module->params[BassMaster<IS_JR>::HIGH_WIDTH_PARAM].getValue() * 100.0f + 0.5f));
			bassMasterLabels[2]->text = string::f("%i", (int)std::round(module->params[BassMaster<IS_JR>::HIGH_GAIN_PARAM].getValue() * 20.0f));
			bassMasterLabels[3]->text = string::f("%i", (int)(module->params[BassMaster<IS_JR>::LOW_WIDTH_PARAM].getValue() * 100.0f + 0.5f));
			bassMasterLabels[4]->text = string::f("%i", (int)std::round(module->params[BassMaster<IS_JR>::LOW_GAIN_PARAM].getValue() * 20.0f));
		}
		Widget::step();
	}
};


Model *modelBassMaster = createModel<BassMaster<false>, BassMasterWidget<false>>("BassMaster");
Model *modelBassMasterJr = createModel<BassMaster<true>, BassMasterWidget<true>>("BassMasterJr");
