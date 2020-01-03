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
		ENUMS(TRACK_CV_INPUTS, 24),
		ENUMS(ACTIVE_CV_INPUTS, 2),
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
	bool motherPresentLeft = false;
	bool motherPresentRight = false;
	
	
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
		motherPresentLeft = (leftExpander.module && leftExpander.module->model == modelEqMaster);
		motherPresentRight = (rightExpander.module && rightExpander.module->model == modelEqMaster);
		
		if (motherPresentLeft || motherPresentRight) {
			// To Mother
			// ***********
			
			float *messagesToMother =  motherPresentLeft ? 
										(float*)leftExpander.module->rightExpander.producerMessage :
										(float*)rightExpander.module->leftExpander.producerMessage;
			
			// track band values
			int index6 = refreshCounter24 % 6;
			messagesToMother[Intf::MFE_TRACK_CVS_INDEX6] = (float)index6;
			int cvConnectedSubset = 0;
			for (int i = 0; i < 4; i++) {
				if (inputs[TRACK_CV_INPUTS + (index6 << 2) + i].isConnected()) {
					cvConnectedSubset |= (1 << i);
					memcpy(&messagesToMother[Intf::MFE_TRACK_CVS + 16 * i], inputs[TRACK_CV_INPUTS + (index6 << 2) + i].getVoltages(), 16 * 4);
				}
			}
			messagesToMother[Intf::MFE_TRACK_CVS_CONNECTED] = (float)cvConnectedSubset;
			
			// track enables
			messagesToMother[Intf::MFE_TRACK_ENABLE] = refreshCounter24 < 16 ? 
				inputs[ACTIVE_CV_INPUTS + 0].getVoltage(refreshCounter24) :
				inputs[ACTIVE_CV_INPUTS + 1].getVoltage(refreshCounter24 - 16);
			messagesToMother[Intf::MFE_TRACK_ENABLE_INDEX] = (float)refreshCounter24;
			
			refreshCounter24++;
			if (refreshCounter24 >= 24) {
				refreshCounter24 = 0;
			}
			
			if (motherPresentLeft) {
				leftExpander.module->rightExpander.messageFlipRequested = true;
			}
			else {
				rightExpander.module->leftExpander.messageFlipRequested = true;
			}
			
		}

	}// process()
};


//-----------------------------------------------------------------------------


struct EqExpanderWidget : ModuleWidget {
	bool oldMotherPresentLeft = false;
	bool oldMotherPresentRight = false;
	PanelBorder* panelBorder;

	
	EqExpanderWidget(EqExpander *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/EqSpander.svg")));
		panelBorder = findBorder(panel);
		
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


	void step() override {
		if (module) {
			EqExpander* module = (EqExpander*)this->module;
			// Borders			
			if ( (module->motherPresentLeft != oldMotherPresentLeft) || (module->motherPresentRight != oldMotherPresentRight) ) {
				oldMotherPresentLeft = module->motherPresentLeft;
				oldMotherPresentRight = module->motherPresentRight;
				if (module->motherPresentLeft) {
					panelBorder->box.pos.x = -3;
					panelBorder->box.size.x = box.size.x + 3;
				}
				else if (module->motherPresentRight) {
					panelBorder->box.pos.x = 0;
					panelBorder->box.size.x = box.size.x + 3;
				}
				else {
					panelBorder->box.pos.x = 0;
					panelBorder->box.size.x = box.size.x;
				}
				((SvgPanel*)panel)->dirty = true;// weird zoom bug: if the if/else above is commented, zoom bug when this executes
			}
		}
		ModuleWidget::step();
	}

};


Model *modelEqExpander = createModel<EqExpander, EqExpanderWidget>("EqExpander");
