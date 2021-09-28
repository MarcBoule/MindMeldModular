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
	

	int refreshCounter6;
	int refreshCounter25;
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
		refreshCounter25 = 0;
		refreshCounter6 = 0;
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
			
			MfeExpInterface *messagesToMother =  motherPresentLeft ? 
										(MfeExpInterface*)leftExpander.module->rightExpander.producerMessage :
										(MfeExpInterface*)rightExpander.module->leftExpander.producerMessage;
			
			messagesToMother->trackCvsIndex6 = refreshCounter6;
			messagesToMother->trackEnableIndex = refreshCounter25;
			
			// track band values
			int cvConnectedSubset = 0;
			for (int i = 0; i < 4; i++) {
				if (inputs[TRACK_CV_INPUTS + (refreshCounter6 << 2) + i].isConnected()) {
					cvConnectedSubset |= (1 << i);
					memcpy(&(messagesToMother->trackCvs[16 * i]), inputs[TRACK_CV_INPUTS + (refreshCounter6 << 2) + i].getVoltages(), 16 * 4);
				}
			}
			messagesToMother->trackCvsConnected = cvConnectedSubset;
			
			// track enables
			messagesToMother->trackEnable = refreshCounter25 < 16 ? 
				inputs[ACTIVE_CV_INPUTS + 0].getVoltage(refreshCounter25) :
				inputs[ACTIVE_CV_INPUTS + 1].getVoltage(refreshCounter25 - 16);
			
			refreshCounter25++;
			if (refreshCounter25 >= 25) {
				refreshCounter25 = 0;
			}
			refreshCounter6++;
			if (refreshCounter6 >= 6) {
				refreshCounter6 = 0;
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
	PanelBorder* panelBorder;

	
	EqExpanderWidget(EqExpander *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/EqSpander.svg")));
		Widget* pw = getPanel();
		SvgPanel* panel = dynamic_cast<SvgPanel*>(pw);
		panelBorder = findBorder(panel->fb);
		
		addInput(createInputCentered<MmPortGold>(mm2px(Vec(12.87f, 17.75f)), module, EqExpander::ACTIVE_CV_INPUTS + 0));		
		addInput(createInputCentered<MmPortGold>(mm2px(Vec(22.69f, 17.75f)), module, EqExpander::ACTIVE_CV_INPUTS + 1));		
		

		static const float leftX = 7.96f;
		static const float midX = 17.78f;
		static const float rightX = 27.6f;
		static const float topY = 34.5f;
		static const float delY = 10.85f;
		
		for (int y = 0; y < 8; y++) {
			addInput(createInputCentered<MmPortGold>(mm2px(Vec(leftX, topY + y * delY)), module, EqExpander::TRACK_CV_INPUTS + y));		
			addInput(createInputCentered<MmPortGold>(mm2px(Vec(midX, topY + y * delY)), module, EqExpander::TRACK_CV_INPUTS + y + 8));		
			addInput(createInputCentered<MmPortGold>(mm2px(Vec(rightX, topY + y * delY)), module, EqExpander::TRACK_CV_INPUTS + y + 16));		
		}
	}


	void step() override {
		if (module) {
			EqExpander* module = (EqExpander*)this->module;
	
			// Borders			
			int newSizeAdd = 0;
			if (module->motherPresentLeft) {
				newSizeAdd = 3;
			}
			else if (module->motherPresentRight) {
				newSizeAdd = 6;// should be +3 but already using +6 above, and needs to be different so no zoom bug
			}
			if (panelBorder->box.size.x != (box.size.x + newSizeAdd)) {
				panelBorder->box.pos.x = (newSizeAdd == 3 ? -3 : 0);
				panelBorder->box.size.x = (box.size.x + newSizeAdd);
				Widget* panel = getPanel();
				// SvgPanel* panel = dynamic_cast<SvgPanel*>(pw);
				((FramebufferWidget*)panel)->dirty = true;
			}
		}
		ModuleWidget::step();
	}

};


Model *modelEqExpander = createModel<EqExpander, EqExpanderWidget>("EqExpander");
