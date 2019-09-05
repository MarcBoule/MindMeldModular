//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.txt for all licenses
//***********************************************************************************************

#include "MindMeldModular.hpp"


struct EqExpander : Module {
	enum ParamIds {
		TEST_PARAM,
		TEST2_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		NUM_INPUTS
	};
	enum OutputIds {
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	// Expander
	// none since uses message bus

	// Constants
	// none

	// Need to save, no reset
	int panelTheme;
	
	// Need to save, with reset
	// none
	
	// No need to save, with reset
	float testParamOld;


	// No need to save, no reset
	RefreshCounter refresh;	
	// std::string busId;

		
	EqExpander() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);		
		
		configParam(TEST_PARAM, 0.0f, 1.0f, 0.5f, "Test param 1");
		configParam(TEST2_PARAM, 0.0f, 1.0f, 0.5f, "Test param 2");

		onReset();

		panelTheme = 0;//(loadDarkAsDefault() ? 1 : 0);

		// busId = messages->registerMember();
		// INFO("*** EQ-Expander registered with ID %s ***", busId.c_str());
	}
  
	// ~EqExpander() {
		// messages->deregisterMember(busId);
	// }
	
	void onReset() override {
		resetNonJson(false);
	}
	void resetNonJson(bool recurseNonJson) {
		testParamOld = params[TEST_PARAM].getValue();
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



	void process(const ProcessArgs &args) override {
		//float testParamNew = params[TEST_PARAM].getValue();
		// if (testParamOld != testParamNew) {
			// testParamOld = testParamNew;
			//INFO("*** id %s is sending message ***", busId.c_str());
			// Payload payload;
			// payload.values[0] = params[TEST_PARAM].getValue();
			// Message<Payload> *message = new Message<Payload>();
			// message->value = payload;

			// messages->send(busId, message);		
		// }
		
	}// process()
};



struct EqExpanderWidget : ModuleWidget {

	EqExpanderWidget(EqExpander *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/eq-expander.svg")));

		addParam(createDynamicParamCentered<DynSmallKnobGrey>(mm2px(Vec(11.43 + 12.7 * 0, 51.8)), module, EqExpander::TEST_PARAM, module ? &module->panelTheme : NULL));
		addParam(createDynamicParamCentered<DynSmallKnobGrey>(mm2px(Vec(11.43 + 12.7 * 1, 51.8)), module, EqExpander::TEST2_PARAM, module ? &module->panelTheme : NULL));

	}
};

Model *modelEqExpander = createModel<EqExpander, EqExpanderWidget>("EQ-Expander");
