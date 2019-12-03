//***********************************************************************************************
//EQ module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "MindMeldModular.hpp"


struct EqMaster : Module {
	
	enum ParamIds {
		TRACK_PARAM,// always have 1-8, but 9-16 and 17-24 are only added when input cables are present.
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
	char trackLabels[24 * 4 + 1];// needs to be saved in case we are detached
	
	// No need to save, with reset
	int updateTrackLabelRequest;// 0 when nothing to do, 1 for read names in widget

	// No need to save, no reset
	RefreshCounter refresh;	
	
	
	EqMaster() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		configParam(TRACK_PARAM, 0.0f, 23.0f, 0.0f, "Track", "", 1.0f);
		
		onReset();
		
		panelTheme = 0;
	}
  
	void onReset() override {
		mappedId = 0;
		for (int trk = 0; trk < 24; trk++) {
			snprintf(&trackLabels[trk << 2], 5, "-%02i-", trk + 1);
		}
		
		resetNonJson();
	}
	void resetNonJson() {
		updateTrackLabelRequest = 1;
	}


	void onRandomize() override {
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));
				
		// mappedId
		json_object_set_new(rootJ, "mappedId", json_integer(mappedId));
				
		// trackLabels
		json_object_set_new(rootJ, "trackLabels", json_string(trackLabels));
		
		return rootJ;
	}


	void dataFromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// mappedId
		json_t *mappedIdJ = json_object_get(rootJ, "mappedId");
		if (mappedIdJ)
			mappedId = json_integer_value(mappedIdJ);

		// trackLabels
		json_t *textJ = json_object_get(rootJ, "trackLabels");
		if (textJ)
			snprintf(trackLabels, 24 * 4 + 1, "%s", json_string_value(textJ));

		resetNonJson();
	}


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
		
		// Track label
		
		
		// Track knob
		addParam(createDynamicParamCentered<DynSnapKnobGrey>(mm2px(Vec(60, 60)), module, EqMaster::TRACK_PARAM, module ? &module->panelTheme : NULL));
		
		
		

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
