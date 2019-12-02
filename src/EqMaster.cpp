//***********************************************************************************************
//EQ module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "MindMeldModular.hpp"


struct EqMaster : Module {
	
	enum ParamIds {
		TRACK_PARAM,// always have 1-8, 9-16 and 17-24 are added when input cables are present.
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
	

	// Constants
	// none


	// Need to save, no reset
	int panelTheme;
	
	// Need to save, with reset
	int mappedId;// 0 means manual
	char trkGrpAuxLabels[(16 + 4 + 4) * 4];// needs to be saved in case we are detached
	
	// No need to save, with reset
	// none

	// No need to save, no reset
	RefreshCounter refresh;	
	
	
	EqMaster() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		onReset();
		
		panelTheme = 0;
	}
  
	void onReset() override {
		resetNonJson(false);
	}
	void resetNonJson(bool recurseNonJson) {
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

		resetNonJson(true);	}


	void onSampleRateChange() override {
	}
	

	void process(const ProcessArgs &args) override {

	}// process()
};


//-----------------------------------------------------------------------------


struct EqMasterWidget : ModuleWidget {
	time_t oldTime = 0;
	
	EqMasterWidget(EqMaster *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/labeltester.svg")));
		

	}
	
	void step() override {
		if (module) {
			// update labels from message bus at 1Hz
			time_t currentTime = time(0);
			if (currentTime != oldTime) {
				oldTime = currentTime;
				std::vector<MessageBase>* mixerMessageSurvey = mixerMessageBus.surveyValues();
				for (MessageBase pl : *mixerMessageSurvey) {
					INFO("id_%i: master label = %s", pl.id, std::string(&(pl.name[0]), 6).c_str());
					MixerMessage message;
					message.id = pl.id;
					mixerMessageBus.receive(&message);
					if (message.id != 0) {
						INFO("  track 0 label = %s", std::string(&(message.trkGrpAuxLabels[0]), 4).c_str());
					}
				}
				delete mixerMessageSurvey;
			}
		}
		Widget::step();
	}
};


Model *modelEqMaster = createModel<EqMaster, EqMasterWidget>("EqMaster");
