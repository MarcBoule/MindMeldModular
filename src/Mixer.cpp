//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.txt for all licenses
//***********************************************************************************************


#include "MBSB.hpp"


struct Mixer : Module {
	enum ParamIds {
		ENUMS(PAN_PARAMS, 16),
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
		
		char strBuf[32];
		for (int i = 0; i < 16; i++) {
			snprintf(strBuf, 32, "Pan knob channel #%i", i + 1);
			configParam(PAN_PARAMS + i, 0.0f, 1.0f, 0.5f, strBuf);
		}
		
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
	TWidget *dynWidget = createWidgetCentered<TWidget>(pos);
	static char buf[5];
	snprintf(buf, 5, "-%02u-", (unsigned)index);
	dynWidget->text = std::string(buf);
	return dynWidget;
}


struct TrackDisplay : LedDisplayTextField {
	TrackDisplay() {
		box.size = Vec(38, 16);// 37 no good, will overflow when zoom out too much
		textOffset = Vec(3.0f, -2.8f);
	};
	void draw(const DrawArgs &args) override {
		if (cursor > 4) {
			text.resize(4);
			cursor = 4;
			selection = 4;
		}
		
		//LedDisplayTextField::draw(args); do this manually since background should be in panel svg for better performance
		// -------
		nvgScissor(args.vg, RECT_ARGS(args.clipBox));
		// Background:
		// nvgBeginPath(args.vg);
		// nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 5.0);
		// nvgFillColor(args.vg, nvgRGB(0x00, 0x00, 0x00));
		// nvgFill(args.vg);
		// Text:
		if (font->handle >= 0) {
			bndSetFont(font->handle);

			NVGcolor highlightColor = color;
			highlightColor.a = 0.5;
			int begin = std::min(cursor, selection);
			int end = (this == APP->event->selectedWidget) ? std::max(cursor, selection) : -1;
			bndIconLabelCaret(args.vg, textOffset.x, textOffset.y,
				box.size.x - 2*textOffset.x, box.size.y - 2*textOffset.y,
				-1, color, 12, text.c_str(), highlightColor, begin, end);

			bndSetFont(APP->window->uiFont->handle);
		}
		nvgResetScissor(args.vg);
		// -------
	}
};


struct MixerWidget : ModuleWidget {
	TrackDisplay* trackDisplays[16];
	unsigned int trackLabelIndexToPush = 0;

	MixerWidget(Mixer *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/mixmaster-jr.svg")));
		
		// Track labels
		for (int i = 0; i < 16; i++) {
			addChild(trackDisplays[i] = createLedDisplayTextField<TrackDisplay>(mm2px(Vec(11.43 + 12.7 * i, 4.2)), i));
		}

		// Pan knobs
		// Track labels
		for (int i = 0; i < 16; i++) {
			addParam(createDynamicParamCentered<DynSmallKnob>(mm2px(Vec(11.43 + 12.7 * i, 51)), module, Mixer::PAN_PARAMS + i, NULL));
		}
	}
	
	void step() override {
		Mixer* moduleM = (Mixer*)module;
		if (moduleM) {
			int trackLabelIndexToPull = moduleM->resetTrackLabelRequest;
			if (trackLabelIndexToPull >= 0) {// pull request from module
				trackDisplays[trackLabelIndexToPull]->text = std::string(&(moduleM->trackLabels[trackLabelIndexToPull * 4]), 4);
				moduleM->resetTrackLabelRequest++;
				if (moduleM->resetTrackLabelRequest >= 16) {
					moduleM->resetTrackLabelRequest = -1;// all done pulling
				}
			}
			else {// push to module
				int unsigned i = 0;
				*((uint32_t*)(&(moduleM->trackLabels[trackLabelIndexToPush * 4]))) = 0x20202020;
				for (; i < trackDisplays[trackLabelIndexToPush]->text.length(); i++) {
					moduleM->trackLabels[trackLabelIndexToPush * 4 + i] = trackDisplays[trackLabelIndexToPush]->text[i];
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

Model *modelMixer = createModel<Mixer, MixerWidget>("MixMaster-jr");
