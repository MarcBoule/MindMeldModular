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
		SLOPE_PARAM,
		LOW_WIDTH_PARAM,// 0 to 1.0f; 0 = mono, 1 = stereo
		HIGH_WIDTH_PARAM,// 0 to 2.0f; 0 = mono, 1 = stereo, 2 = 200% wide
		LOW_SOLO_PARAM,
		HIGH_SOLO_PARAM,
		LOW_GAIN_PARAM,// -20 to +20 dB
		HIGH_GAIN_PARAM,// -20 to +20 dB
		BYPASS_PARAM,
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
	// none
	
	// No need to save, with reset
	float crossover;
	bool is24db;
	bool lowSolo;
	bool highSolo;
	LinkwitzRileyCrossover xover;
	dsp::SlewLimiter lowWidthSlewer;
	dsp::SlewLimiter highWidthSlewer;
	
	// No need to save, no reset
	RefreshCounter refresh;
	
	
	BassMaster() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configParam(CROSSOVER_PARAM, 50.0f, 500.0f, 120.0f, "Crossover", " Hz");
		configParam(LOW_WIDTH_PARAM, 0.0f, 1.0f, 1.0f, "Low width", "%", 0.0f, 100.0f);// diplay params are: base, mult, offset
		configParam(HIGH_WIDTH_PARAM, 0.0f, 2.0f, 1.0f, "High width", "%", 0.0f, 100.0f);// diplay params are: base, mult, offset
		configParam(LOW_SOLO_PARAM, 0.0f, 1.0f, 0.0f, "Low solo");
		configParam(HIGH_SOLO_PARAM, 0.0f, 1.0f, 0.0f, "High solo");
		configParam(LOW_GAIN_PARAM, -20.0f, 20.0f, 0.0f, "Low gain", " dB");
		configParam(HIGH_GAIN_PARAM, -20.0f, 20.0f, 0.0f, "High gain", " dB");
		configParam(SLOPE_PARAM, 0.0f, 1.0f, DEFAULT_SLOPE, "Slope 24 dB/oct");
					
		lowWidthSlewer.setRiseFall(125.0f, 125.0f); // slew rate is in input-units per second (ex: V/s)
		highWidthSlewer.setRiseFall(125.0f, 125.0f); // slew rate is in input-units per second (ex: V/s)		

		onReset();
		
		panelTheme = 0;
	}
  
	void onReset() override {
		params[SLOPE_PARAM].setValue(DEFAULT_SLOPE);// need this since no wigdet exists
		resetNonJson(false);
	}
	void resetNonJson(bool recurseNonJson) {
		crossover = params[CROSSOVER_PARAM].getValue();
		is24db = params[SLOPE_PARAM].getValue() >= 0.5f;
		lowSolo = params[LOW_SOLO_PARAM].getValue() >= 0.5f;
		highSolo = params[HIGH_SOLO_PARAM].getValue() >= 0.5f;
		xover.setFilterCutoffs(crossover / APP->engine->getSampleRate(), is24db);
		xover.reset();
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
		xover.setFilterCutoffs(crossover / APP->engine->getSampleRate(), is24db);
	}
	

	void process(const ProcessArgs &args) override {
		// crossover knob and 24dB refresh
		float newCrossover = params[CROSSOVER_PARAM].getValue();
		bool newIs24db = params[SLOPE_PARAM].getValue() >= 0.5f;
		if (crossover != newCrossover || is24db != newIs24db) {
			crossover = newCrossover;
			is24db = newIs24db;
			xover.setFilterCutoffs(crossover / args.sampleRate, is24db);
		}
	
		// solo buttons' refresh
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
		
		float outs[4];// [0] = left low, left high, right low, [3] = right high
		xover.process(inputs[IN_INPUTS + 0].getVoltage(), inputs[IN_INPUTS + 1].getVoltage(), outs);

		// Bass width (apply to left low and right low)
		float newLowWidth = params[LOW_WIDTH_PARAM].getValue();
		if (newLowWidth != lowWidthSlewer.out) {
			lowWidthSlewer.process(args.sampleTime, newLowWidth);
		}
		// lowWidthSlewer is 0 to 1.0f; 0 is mono, 1 is stereo
		applyStereoWidth(lowWidthSlewer.out, &outs[0], &outs[2]);
		
		// High width (apply to left high and right high)
		float newHighWidth = params[HIGH_WIDTH_PARAM].getValue();
		if (newHighWidth != highWidthSlewer.out) {
			highWidthSlewer.process(args.sampleTime, newHighWidth);
		}
		// highWidthSlewer is 0 to 2.0f; 0 is mono, 1 is stereo, 2 is 200% wide
		applyStereoWidth(highWidthSlewer.out, &outs[1], &outs[3]);

		// dumb solo for now
		if (highSolo) {
			outs[0] = outs[2] = 0.0f;// kill low
		}
		if (lowSolo) {
			outs[1] = outs[3] = 0.0f;// kill high
		}

		outputs[OUT_OUTPUTS + 0].setVoltage(outs[0] + outs[1]);
		outputs[OUT_OUTPUTS + 1].setVoltage(outs[2] + outs[3]);
	}// process()
};


//-----------------------------------------------------------------------------


struct BassMasterWidget : ModuleWidget {
	
	struct PairedSoloButton : DynSoloRoundButton {
	};
	
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
		BassMaster* module = (BassMaster*)(this->module);
		assert(module);

		menu->addChild(new MenuSeparator());
		
		SlopeItem *slopeItem = createMenuItem<SlopeItem>("Crossover slope", RIGHT_ARROW);
		slopeItem->srcParam = &(module->params[BassMaster::SLOPE_PARAM]);
		menu->addChild(slopeItem);		
	}

	BassMasterWidget(BassMaster *module) {
		setModule(module);

		// Main panel from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/BassMaster.svg")));
 
		// crossover knob
		addParam(createDynamicParamCentered<DynBigKnobWhite>(mm2px(Vec(15.24, 22.49)), module, BassMaster::CROSSOVER_PARAM, module ? &module->panelTheme : NULL));
		
		// high solo button
		addParam(createDynamicParamCentered<DynSoloRoundButton>(mm2px(Vec(15.24, 45.93)), module, BassMaster::HIGH_SOLO_PARAM, module ? &module->panelTheme : NULL));
		// low solo button
		addParam(createDynamicParamCentered<DynSoloRoundButton>(mm2px(Vec(15.24, 73.71)), module, BassMaster::LOW_SOLO_PARAM, module ? &module->panelTheme : NULL));
		// bypass button
		addParam(createDynamicParamCentered<DynBypassRoundButton>(mm2px(Vec(15.24, 95.4)), module, BassMaster::BYPASS_PARAM, module ? &module->panelTheme : NULL));

		// high width and gain
		addParam(createDynamicParamCentered<DynSmallKnobGrey8mm>(mm2px(Vec(7.5, 51.68)), module, BassMaster::HIGH_WIDTH_PARAM, module ? &module->panelTheme : NULL));
		addParam(createDynamicParamCentered<DynSmallKnobGrey8mm>(mm2px(Vec(22.9, 51.68)), module, BassMaster::HIGH_GAIN_PARAM, module ? &module->panelTheme : NULL));
 
		// low width and gain
		addParam(createDynamicParamCentered<DynSmallKnobGrey8mm>(mm2px(Vec(7.5, 79.46)), module, BassMaster::LOW_WIDTH_PARAM, module ? &module->panelTheme : NULL));
		addParam(createDynamicParamCentered<DynSmallKnobGrey8mm>(mm2px(Vec(22.9, 79.46)), module, BassMaster::LOW_GAIN_PARAM, module ? &module->panelTheme : NULL));
 
		// inputs
		addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(6.81, 102.03)), true, module, BassMaster::IN_INPUTS + 0, module ? &module->panelTheme : NULL));
		addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(6.81, 111.45)), true, module, BassMaster::IN_INPUTS + 1, module ? &module->panelTheme : NULL));
			
		// outputs
		addOutput(createDynamicPortCentered<DynPort>(mm2px(Vec(23.52, 102.03)), false, module, BassMaster::OUT_OUTPUTS  + 0, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPortCentered<DynPort>(mm2px(Vec(23.52, 111.45)), false, module, BassMaster::OUT_OUTPUTS + 1, module ? &module->panelTheme : NULL));
	}
};


Model *modelBassMaster = createModel<BassMaster, BassMasterWidget>("BassMaster");
