//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.txt for all licenses
//***********************************************************************************************


#include "MBSB.hpp"


struct Mixer : Module {
	enum ParamIds {
		KNOB_PARAM,
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
	
	// Need to save, no reset
	// none
	
	// Need to save, with reset
	char trackLabels[4 * 16 + 1];// 4 chars per track, null terminate the end of the 16th track only
	
	// No need to save, with reset
	int resetTrackLabelRequest;// -1 when nothing to do, 0 to 15 for incremental read in widget
	
	// No need to save, no reset
	RefreshCounter refresh;	

		
	Mixer() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);		
		
		configParam(KNOB_PARAM, 0.0f, 1.0f, 0.5f, "Knob");
		
		onReset();
	}

	
	void onReset() override {
		snprintf(trackLabels, 4 * 16 + 1, "-01--02--03--04--05--06--07--08--09--10--11--12--13--14--15--16-");		
		resetNonJson();
	}
	void resetNonJson() {
		resetTrackLabelRequest = 0;// setting to 0 will trigger 1, 2, 3 etc on each video frame afterwards
	}


	void onRandomize() override {
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// text
		json_object_set_new(rootJ, "text", json_string(trackLabels));

		return rootJ;
	}


	void dataFromJson(json_t *rootJ) override {
		// text
		json_t *textJ = json_object_get(rootJ, "text");
		if (textJ)
			snprintf(trackLabels, 4 * 16 + 1, json_string_value(textJ));
		
		resetNonJson();
	}

	
	void process(const ProcessArgs &args) override {
		if (refresh.processInputs()) {

		}// userInputs refresh
		
		
		
		//********** Outputs and lights **********
		
		
		// outputs
		// none
		
		
		// lights
		if (refresh.processLights()) {

		}
		
	}// process()

};


template <class TWidget>
TWidget* createLedDisplayTextField(Vec pos, int index) {
	TWidget *dynWidget = createWidget<TWidget>(pos);
	dynWidget->index = index;
	return dynWidget;
}


struct TrackDisplay : LedDisplayTextField {
	int index;
	
	TrackDisplay() {
		box.size = Vec(38, 16);// 37 no good, will overflow when zoom out too much
		textOffset = Vec(3.0f, -2.0f);
		text = "-13-";// TODO: in case module is null, need to set properly
	};
	void draw(const DrawArgs &args) override {
		if (cursor > 4) {
			text.resize(4);
			cursor = 4;
			selection = 4;
		}
		LedDisplayTextField::draw(args);
	}
};


struct MixerWidget : ModuleWidget {
	TrackDisplay* trackDisplays[16];
	unsigned int trackLabelIndexToPush = 0;

	MixerWidget(Mixer *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Mixer.svg")));
		
		// Track label master
		for (int i = 0; i < 16; i++) {
			addChild(trackDisplays[i] = createLedDisplayTextField<TrackDisplay>(Vec(30 + 44 * i, 17), i));
		}

		// Knob with arc effect
		addParam(createDynamicParamCentered<DynSmallKnob>(Vec(100, 100), module, Mixer::KNOB_PARAM, NULL));
	}
	
	void step() override {
		Mixer* moduleM = (Mixer*)module;
		if (moduleM) {
			int trackLabelIndexToPull = moduleM->resetTrackLabelRequest;
			if (trackLabelIndexToPull >= 0) {// pull from module
				trackDisplays[trackLabelIndexToPull]->text = std::string(&(moduleM->trackLabels[trackLabelIndexToPull * 4]), 4);
				moduleM->resetTrackLabelRequest++;
				if (moduleM->resetTrackLabelRequest >= 16) {
					moduleM->resetTrackLabelRequest = -1;// all done pulling
				}
			}
			else {// push to module
				int unsigned i = 0;
				for (; i < trackDisplays[trackLabelIndexToPush]->text.length(); i++) {
					moduleM->trackLabels[trackLabelIndexToPush * 4 + i] = trackDisplays[trackLabelIndexToPush]->text[i];
				}
				for (; i < 4; i++) {
					moduleM->trackLabels[trackLabelIndexToPush * 4 + i] = ' ';
				}
				trackLabelIndexToPush++;
				if (trackLabelIndexToPush >= 16) {
					trackLabelIndexToPush = 0;
				}
			}
			
		}
		Widget::step();
	}
};

Model *modelMixer = createModel<Mixer, MixerWidget>("Mixer");
