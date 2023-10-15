//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//Based on code from the Fundamental plugin by Andrew Belt
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#pragma once


#include "../MindMeldModular.hpp"
#include "ClockDetector.hpp"
#include "Channel.hpp"
#include "PresetAndShapeManager.hpp"
#include "Display.hpp"
#include "Menus.hpp"
#include "Widgets.hpp"


struct ShapeMaster : Module {

	// see Util.hpp for param ids

	// see Util.hpp for input ids
	
	// see Util.hpp for output ids

	enum LightIds {
		RESET_LIGHT,
		RUN_LIGHT,
		ENUMS(DEFERRAL_LIGHTS, 4),
		SC_HPF_LIGHT,
		SC_LPF_LIGHT,
		ENUMS(NODETRIG_LIGHTS, 8 * 2),// room for GreenRed
		NUM_LIGHTS
	};


	// Expander
	CvExpInterface expMessages[2] = {};// messages from cv-expander, see enum in Util.hpp


	// Constants
	int8_t cloakedMode = 0x0;
	static const int NUM_CHAN = 8;// not fully parameterized, only used for debugging

	// Need to save, no reset
	// none

	// Need to save, with reset
	bool running = false;
	ClockDetector clockDetector;
	PackedBytes4 miscSettings;
	PackedBytes4 miscSettings2;
	PackedBytes4 miscSettings3;
	RandomSettings randomSettings;
	float lineWidth = 0.0f;
	std::vector<Channel> channels;// size 8
	int currChan = 0;


	// No need to save, with reset
	long clockIgnoreOnReset = 0;
	ScopeBuffers scopeBuffers;
	
	// No need to save, no reset
	RefreshCounter refresh;
	int fsDiv8 = 0;
	int fsDiv10k = 0;
	float resetLight = 0.0f;
	bool expPresentLeft = false;
	bool expPresentRight = false;
	uint32_t sosEosEoc = 0;// always set up in this.process(), and channel/playhead should only use in process() scope
	dsp::SchmittTrigger runTrigger;
	dsp::SchmittTrigger clockTrigger;
	dsp::SchmittTrigger resetTrigger;
	PresetAndShapeManager presetAndShapeManager;
	Channel* channelDirtyCache;
	Param channelDirtyCacheParams[NUM_CHAN_PARAMS] = {};


	ShapeMaster(); 

	~ShapeMaster() {
		// if (channelCopyPasteCache != NULL) {
			// json_decref(channelCopyPasteCache);
		// }
		delete channelDirtyCache;
	}
	
	
	void onReset() override final;
	
	
	void resetNonJson() {
		clockIgnoreOnReset = (long) (0.001f * APP->engine->getSampleRate());
		scopeBuffers.reset();
	}


	void onRandomize() override {
	}


	json_t *dataToJson() override;

	void dataFromJson(json_t *rootJ) override;


	void onSampleRateChange() override {
		clockDetector.onSampleRateChange();
		for (int c = 0; c < NUM_CHAN; c++) {
			channels[c].onSampleRateChange();
		}
	}

	void process(const ProcessArgs &args) override;
	
	void processRunToggled();

	void worker_nextPresetOrShape();
};


//-----------------------------------------------------------------------------


struct ShapeMasterWidget : ModuleWidget {
	int oldVisibleChannel = 0;// corresponds to what constructor will show
	SmKnob* smKnobs[8][NUM_KNOB_PARAMS] = {};// index [0][0] is chan 0 length knob synced, index [0][1] is chan 0 length knob unsync
	SvgSwitch* smButtons[8][NUM_BUTTON_PARAMS] = {};// index [0][0] is chan 0 play, index [0][1] is chan 0 freeze
	PresetOrShapeArrowButton* arrowButtons[8][NUM_ARROW_PARAMS] = {};
	DisplayInfo displayInfo;
	bool presetOrShapeDirty = false;
	bool unsupportedSync = false;
	int stepDivider = 0;
	PanelBorder* panelBorder = nullptr;


	void appendContextMenu(Menu *menu) override;

	ShapeMasterWidget(ShapeMaster *module);

	void step() override;
	
	
	void onHoverKey(const event::HoverKey& e) override {
		if (e.action == GLFW_PRESS) {
			if ( e.key == GLFW_KEY_L && ((e.mods & RACK_MOD_CTRL) != 0) ) {
				ShapeMaster *module = static_cast<ShapeMaster*>(this->module);
				module->miscSettings2.cc4[2] ^= 0x1;
				e.consume(this);
				return;
			}
		}
		ModuleWidget::onHoverKey(e); 
	}

};

