//***********************************************************************************************
//Bass mono module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "../MindMeldModular.hpp"
#include "../comp/VuMeters.hpp"
#include "../dsp/LinkwitzRileyCrossover.hpp"


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
		LOW_WIDTH_INPUT,
		HIGH_WIDTH_INPUT,
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
	static constexpr float SLEW_RATE = 25.0f;
	int8_t cloakedMode = 0x0;
	int8_t detailsShow = 0x3;


	// Need to save, no reset
	// none
	
	// Need to save, with reset
	PackedBytes4 miscSettings;// cc4[0] is display label colours, cc4[1] is polyStereo, cc4[2] is VU color, cc4[3] is isMasterTrack
	
	// No need to save, with reset
	float crossover;
	bool is24db;
	bool lowSolo;
	bool highSolo;
	LinkwitzRileyStereoCrossover xover;
	TSlewLimiterSingle<simd::float_4> widthAndGainSlewers;// [0] = low width, high width, low gain, [3] = high gain
	TSlewLimiterSingle<simd::float_4> solosAndBypassSlewers;// [0] = low solo, high solo, bypass, [3] = master gain
	SlewLimiterSingle mixSlewer;
	float linearLowGain;
	float linearHighGain;
	float linearMasterGain;
	VuMeterAllDual trackVu;
	
	// No need to save, no reset
	RefreshCounter refresh;
	bool paramCvLowWidthConnected = false;
	bool paramCvHighWidthConnected = false;
	float lowWidth = 1.0f;
	float highWidth = 1.0f;
	
	
	BassMaster() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(CROSSOVER_PARAM, 50.0f, 500.0f, 120.0f, "Crossover", " Hz");
		configParam(SLOPE_PARAM, 0.0f, 1.0f, DEFAULT_SLOPE, "Slope 24 dB/oct");
		configParam(LOW_WIDTH_PARAM, 0.0f, 2.0f, 1.0f, "Low width", "%", 0.0f, 100.0f);// diplay params are: base, mult, offset
		configParam(HIGH_WIDTH_PARAM, 0.0f, 2.0f, 1.0f, "High width", "%", 0.0f, 100.0f);// diplay params are: base, mult, offset
		configParam(LOW_SOLO_PARAM, 0.0f, 1.0f, 0.0f, "Low solo");
		configParam(HIGH_SOLO_PARAM, 0.0f, 1.0f, 0.0f, "High solo");
		configParam(LOW_GAIN_PARAM, -1.0f, 1.0f, 0.0f, "Low gain", " dB", 0.0f, 20.0f);// diplay params are: base, mult, offset
		configParam(HIGH_GAIN_PARAM, -1.0f, 1.0f, 0.0f, "High gain", " dB", 0.0f, 20.0f);// diplay params are: base, mult, offset
		configParam(BYPASS_PARAM, 0.0f, 1.0f, 0.0f, "Bypass");
		configParam(GAIN_PARAM, -1.0f, 1.0f, 0.0f, "Master gain", " dB", 0.0f, 20.0f);// diplay params are: base, mult, offset
		configParam(MIX_PARAM, 0.0f, 1.0f, 1.0f, "Mix", "%", 0.0f, 100.0f);// diplay params are: base, mult, offset
				
		configInput(IN_INPUTS + 0, "Left");
		configInput(IN_INPUTS + 1, "Right");
		configInput(LOW_WIDTH_INPUT, "Low width");
		configInput(HIGH_WIDTH_INPUT, "High width");

		configOutput(OUT_OUTPUTS + 0, "Left");
		configOutput(OUT_OUTPUTS + 1, "Right");

		configBypass(IN_INPUTS + 0, OUT_OUTPUTS + 0);
		configBypass(IN_INPUTS + 1, OUT_OUTPUTS + 1);
				
		widthAndGainSlewers.setRiseFall(simd::float_4(SLEW_RATE)); // slew rate is in input-units per second (ex: V/s)		
		solosAndBypassSlewers.setRiseFall(simd::float_4(SLEW_RATE)); // slew rate is in input-units per second (ex: V/s)	
		mixSlewer.setRiseFall(SLEW_RATE);

		onReset();
	}
  
	void onReset() override {
		params[SLOPE_PARAM].setValue(DEFAULT_SLOPE);// need this since no wigdet exists
		miscSettings.cc4[0] = 0;// display label colours
		miscSettings.cc4[1] = 0;// polyStereo
		miscSettings.cc4[2] = 0;// default color
		miscSettings.cc4[3] = 0;// isMasterTrack
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
		
		// miscSettings
		json_object_set_new(rootJ, "miscSettings", json_integer(miscSettings.cc1));
				
		return rootJ;
	}


	void dataFromJson(json_t *rootJ) override {
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
		
		simd::float_4 outs = xover.process(clampNothing(inLeft), clampNothing(inRight));
		// outs: [0] = left low, left high, right low, [3] = right high
		float dryLeft;
		float dryRight;
		if (!IS_JR) {
			dryLeft = outs[0] + outs[1];
			dryRight = outs[2] + outs[3];
		}
		
		// Width and gain slewers
		lowWidth = params[LOW_WIDTH_PARAM].getValue();
		highWidth = params[HIGH_WIDTH_PARAM].getValue();
		if (!IS_JR) {
			if (inputs[LOW_WIDTH_INPUT].isConnected()) {
				lowWidth = clamp(lowWidth + inputs[LOW_WIDTH_INPUT].getVoltage() * 0.2f, 0.0f, 2.0f);
				paramCvLowWidthConnected = true;
			}
			else {
				paramCvLowWidthConnected = false;
			}
			if (inputs[HIGH_WIDTH_INPUT].isConnected()) {
				highWidth = clamp(highWidth + inputs[HIGH_WIDTH_INPUT].getVoltage() * 0.2f, 0.0f, 2.0f);
				paramCvHighWidthConnected = true;
			}
			else {
				paramCvHighWidthConnected = false;
			}
		}
		
		simd::float_4 widthAndGain = simd::float_4(lowWidth, highWidth,
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
		outStereo[0] = crossfade(inLeft, outStereo[0], solosAndBypassSlewers.out[2]);
		outStereo[1] = crossfade(inRight, outStereo[1], solosAndBypassSlewers.out[2]);

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
			color = DISP_COLORS[0];
			textOffset = Vec(4.2f, 11.3f);
			text = "---";
			fontPath = std::string(asset::plugin(pluginInstance, "res/fonts/RobotoCondensed-Regular.ttf"));
		};
		
		void draw(const DrawArgs &args) override {}	// don't want background, which is in draw, actual text is in drawLayer
		
		void drawLayer(const DrawArgs &args, int layer) override {
			if (layer == 1) {
				if (dispColorPtr) {
					color = DISP_COLORS[*dispColorPtr];
				}
			}
			LedDisplayChoice::drawLayer(args, layer);
		}
	};	
	BassMasterLabel* bassMasterLabels[5];// xover, width high, gain high, width low, gain low


	struct SlopeItem : MenuItem {
		Param* srcParam;

		Menu *createChildMenu() override {
			Menu *menu = new Menu;			
			menu->addChild(createCheckMenuItem("12 db/oct", "",
				[=]() {return srcParam->getValue() < 0.5f;},
				[=]() {srcParam->setValue(0.0f);}
			));			
			menu->addChild(createCheckMenuItem("24 db/oct", "",
				[=]() {return srcParam->getValue() >= 0.5f;},
				[=]() {srcParam->setValue(1.0f);}
			));	
			return menu;
		}
	};	
	
	struct VuTypeItem : MenuItem {
		int8_t* isMasterTypeSrc;

		Menu *createChildMenu() override {
			Menu *menu = new Menu;
			menu->addChild(createCheckMenuItem("Scale as track", "",
				[=]() {return *isMasterTypeSrc == 0;},
				[=]() {*isMasterTypeSrc = 0;}
			));	
			menu->addChild(createCheckMenuItem("Scale as master", "",
				[=]() {return *isMasterTypeSrc != 0;},
				[=]() {*isMasterTypeSrc = 1;}
			));	
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
			VuTypeItem *vutItem = createMenuItem<VuTypeItem>("VU scaling", RIGHT_ARROW);
			vutItem->isMasterTypeSrc = &(module->miscSettings.cc4[3]);
			menu->addChild(vutItem);
			
			VuFiveColorItem *vuColItem = createMenuItem<VuFiveColorItem>("VU colour", RIGHT_ARROW);
			vuColItem->srcColors = &(module->miscSettings.cc4[2]);
			menu->addChild(vuColItem);
		}
	}

	BassMasterWidget(BassMaster<IS_JR> *module) {
		setModule(module);

		// Main panel from Inkscape
        if (IS_JR) {
			setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/BassMaster.svg")));
		}
		else {
			setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/BassMasterSnr.svg")));
		}
 
		// crossover knob
		addParam(createParamCentered<MmBiggerKnobWhite>(mm2px(Vec(15.24, 22.98)), module, BassMaster<IS_JR>::CROSSOVER_PARAM));
		
		// all labels (xover, width high, gain high, width low, gain low)
		addChild(bassMasterLabels[0] = createWidgetCentered<BassMasterLabel>(mm2px(Vec(14.74+0.0, 33.3))));
		addChild(bassMasterLabels[1] = createWidgetCentered<BassMasterLabel>(mm2px(Vec(7.5 + 0.0, 60.71))));
		addChild(bassMasterLabels[2] = createWidgetCentered<BassMasterLabel>(mm2px(Vec(22.9 + 0.0, 60.71))));
		addChild(bassMasterLabels[3] = createWidgetCentered<BassMasterLabel>(mm2px(Vec(7.5 + 0.0, 88.42))));
		addChild(bassMasterLabels[4] = createWidgetCentered<BassMasterLabel>(mm2px(Vec(22.9 + 0.0, 88.42))));
		if (module) {
			for (int i = 0; i < 5; i++) {
				bassMasterLabels[i]->dispColorPtr = &(module->miscSettings.cc4[0]);
			}
		}
		
		// high solo button
		addParam(createParamCentered<MmSoloRoundButton>(mm2px(Vec(15.24, 45.93 + 1)), module, BassMaster<IS_JR>::HIGH_SOLO_PARAM));
		// low solo button
		addParam(createParamCentered<MmSoloRoundButton>(mm2px(Vec(15.24, 73.71 + 1)), module, BassMaster<IS_JR>::LOW_SOLO_PARAM));
		// bypass button
		addParam(createParamCentered<MmBypassRoundButton>(mm2px(Vec(15.24, 95.4 + 1)), module, BassMaster<IS_JR>::BYPASS_PARAM));

		// high width
		MmKnobWithArc *highWidthKnob;
		addParam(highWidthKnob = createParamCentered<Mm8mmKnobGrayWithArcTopCentered>(mm2px(Vec(7.5, 51.68 + 1)), module, BassMaster<IS_JR>::HIGH_WIDTH_PARAM));
		if (module) {
			highWidthKnob->paramWithCV = &(module->highWidth);
			highWidthKnob->paramCvConnected = &(module->paramCvHighWidthConnected);
			highWidthKnob->detailsShowSrc = &(module->detailsShow);
			highWidthKnob->cloakedModeSrc = &(module->cloakedMode);
		}
		
		// high gain
		MmKnobWithArc *highGainKnob;
		addParam(highGainKnob = createParamCentered<Mm8mmKnobGrayWithArcTopCentered>(mm2px(Vec(22.9, 51.68 + 1)), module, BassMaster<IS_JR>::HIGH_GAIN_PARAM));
		if (module) {
			highGainKnob->detailsShowSrc = &(module->detailsShow);
			highGainKnob->cloakedModeSrc = &(module->cloakedMode);
		}
 
		// low width
		MmKnobWithArc *lowWidthKnob;
		addParam(lowWidthKnob = createParamCentered<Mm8mmKnobGrayWithArcTopCentered>(mm2px(Vec(7.5, 79.40 + 1)), module, BassMaster<IS_JR>::LOW_WIDTH_PARAM));
		if (module) {
			lowWidthKnob->paramWithCV = &(module->lowWidth);
			lowWidthKnob->paramCvConnected = &(module->paramCvLowWidthConnected);
			lowWidthKnob->detailsShowSrc = &(module->detailsShow);
			lowWidthKnob->cloakedModeSrc = &(module->cloakedMode);
		}
		
		// low gain
		MmKnobWithArc *lowGainKnob;
		addParam(lowGainKnob = createParamCentered<Mm8mmKnobGrayWithArcTopCentered>(mm2px(Vec(22.9, 79.40 + 1)), module, BassMaster<IS_JR>::LOW_GAIN_PARAM));
		if (module) {
			lowGainKnob->detailsShowSrc = &(module->detailsShow);
			lowGainKnob->cloakedModeSrc = &(module->cloakedMode);
		}

		// inputs
		addInput(createInputCentered<MmPort>(mm2px(Vec(6.81, 102.03 + 1)), module, BassMaster<IS_JR>::IN_INPUTS + 0));
		addInput(createInputCentered<MmPort>(mm2px(Vec(6.81, 111.45 + 1)), module, BassMaster<IS_JR>::IN_INPUTS + 1));
			
		// outputs
		addOutput(createOutputCentered<MmPort>(mm2px(Vec(23.52, 102.03 + 1)), module, BassMaster<IS_JR>::OUT_OUTPUTS  + 0));
		addOutput(createOutputCentered<MmPort>(mm2px(Vec(23.52, 111.45 + 1)), module, BassMaster<IS_JR>::OUT_OUTPUTS + 1));
		
		if (!IS_JR) {
			// VU meter
			if (module) {
				VuMeterBassMono *newVU = createWidgetCentered<VuMeterBassMono>(mm2px(Vec(37.2, 37.5f)));
				newVU->srcLevels = module->trackVu.vuValues;
				newVU->bassVuColorsSrc = &(module->miscSettings.cc4[2]);
				newVU->isMasterTypeSrc = &(module->miscSettings.cc4[3]);
				addChild(newVU);
			}
						
			// master gain
			MmKnobWithArc *masterGainKnob;
			addParam(masterGainKnob = createParamCentered<Mm8mmKnobGrayWithArcTopCentered>(mm2px(Vec(37.2, 66.09)), module, BassMaster<IS_JR>::GAIN_PARAM));
			if (module) {
				masterGainKnob->detailsShowSrc = &(module->detailsShow);
				masterGainKnob->cloakedModeSrc = &(module->cloakedMode);
			}
			
			// mix knob
			MmKnobWithArc *mixKnob;
			addParam(mixKnob = createParamCentered<Mm8mmKnobGrayWithArc>(mm2px(Vec(37.2, 82.35)), module, BassMaster<IS_JR>::MIX_PARAM));
			if (module) {
				mixKnob->detailsShowSrc = &(module->detailsShow);
				mixKnob->cloakedModeSrc = &(module->cloakedMode);
			}
			
			// width CV inputs
			addInput(createInputCentered<MmPort>(mm2px(Vec(36.4, 102.03 + 1)), module, BassMaster<IS_JR>::HIGH_WIDTH_INPUT));
			addInput(createInputCentered<MmPort>(mm2px(Vec(36.4, 111.45 + 1)), module, BassMaster<IS_JR>::LOW_WIDTH_INPUT));
		}
	}
	
	void step() override {
		BassMaster<IS_JR>* module = (BassMaster<IS_JR>*)(this->module);
		if (module) {
			bassMasterLabels[0]->text = string::f("%.1f",   math::normalizeZero(module->crossover));
			bassMasterLabels[1]->text = string::f("%.1f",   math::normalizeZero(module->params[BassMaster<IS_JR>::HIGH_WIDTH_PARAM].getValue() * 100.0f));
			bassMasterLabels[2]->text = string::f("%.1f", math::normalizeZero(module->params[BassMaster<IS_JR>::HIGH_GAIN_PARAM].getValue() * 20.0f));
			bassMasterLabels[3]->text = string::f("%.1f",   math::normalizeZero(module->params[BassMaster<IS_JR>::LOW_WIDTH_PARAM].getValue() * 100.0f));
			bassMasterLabels[4]->text = string::f("%.1f", math::normalizeZero(module->params[BassMaster<IS_JR>::LOW_GAIN_PARAM].getValue() * 20.0f));
		}
		Widget::step();
	}
};


Model *modelBassMaster = createModel<BassMaster<false>, BassMasterWidget<false>>("BassMaster");
Model *modelBassMasterJr = createModel<BassMaster<true>, BassMasterWidget<true>>("BassMasterJr");
