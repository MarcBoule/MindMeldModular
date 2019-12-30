//***********************************************************************************************
//EQ module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "EqMasterCommon.hpp"


struct EqExpander : Module {
	
	enum EqParamIds {
		NUM_PARAMS
	};	
	
	enum InputIds {
		ENUMS(ACTIVE_CV_INPUTS, 2),
		ENUMS(TRACK_CV_INPUTS, 24),
		NUM_INPUTS
	};
	
	enum OutputIds {
		NUM_OUTPUTS
	};
	
	enum LightIds {
		NUM_LIGHTS
	};
	
	typedef ExpansionInterface Intf;
	
	
	int panelTheme = 0;
	int refreshCounter24;
	
	EqExpander() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		onReset();
	}
  
  
	void onReset() override {
		resetNonJson();
	}
	void resetNonJson() {
		refreshCounter24 = 0;
	}


	void onRandomize() override {
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();
				
		return rootJ;
	}


	void dataFromJson(json_t *rootJ) override {

		resetNonJson();
	}


	void onSampleRateChange() override {
	}
	

	void process(const ProcessArgs &args) override {
		bool motherPresent = (leftExpander.module && leftExpander.module->model == modelEqMaster);
		
		if (motherPresent) {
			// To Mother
			// ***********
			
			float *messagesToMother = (float*)leftExpander.module->rightExpander.producerMessage;
			
			messagesToMother[Intf::MFE_TRACK_ENABLE] = refreshCounter24 < 16 ? 
				inputs[ACTIVE_CV_INPUTS + 0].getVoltage(refreshCounter24) :
				inputs[ACTIVE_CV_INPUTS + 1].getVoltage(refreshCounter24 - 16);
			messagesToMother[Intf::MFE_TRACK_ENABLE_INDEX] = (float)refreshCounter24;
			
			refreshCounter24++;
			if (refreshCounter24 >= 24) {
				refreshCounter24 = 0;
			}
			
			leftExpander.module->rightExpander.messageFlipRequested = true;
			
		}

	}// process()
};


//-----------------------------------------------------------------------------


struct EqExpanderWidget : ModuleWidget {

	
	EqExpanderWidget(EqExpander *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/EqSpander.svg")));
		
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(12.87f, 17.75f)), true, module, EqExpander::ACTIVE_CV_INPUTS + 0, module ? &module->panelTheme : NULL));		
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(22.69f, 17.75f)), true, module, EqExpander::ACTIVE_CV_INPUTS + 1, module ? &module->panelTheme : NULL));		
		

		static const float leftX = 7.96f;
		static const float midX = 17.78f;
		static const float rightX = 27.6f;
		static const float topY = 34.5f;
		static const float delY = 10.85f;
		
		for (int y = 0; y < 8; y++) {
			addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(leftX, topY + y * delY)), true, module, EqExpander::TRACK_CV_INPUTS + y, module ? &module->panelTheme : NULL));		
			addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(midX, topY + y * delY)), true, module, EqExpander::TRACK_CV_INPUTS + y + 8, module ? &module->panelTheme : NULL));		
			addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(rightX, topY + y * delY)), true, module, EqExpander::TRACK_CV_INPUTS + y + 16, module ? &module->panelTheme : NULL));		
		}
	}
};


Model *modelEqExpander = createModel<EqExpander, EqExpanderWidget>("EqExpander");
