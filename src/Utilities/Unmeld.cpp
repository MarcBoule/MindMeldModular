//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "../MindMeldModular.hpp"


struct Unmeld : Module {
	
	enum ParamIds {
		NUM_PARAMS
	};
	
	enum InputIds {
		POLY_INPUT,
		NUM_INPUTS
	};
	
	enum OutputIds {
		THRU_OUTPUT,
		ENUMS(SPLIT_OUTPUTS, 16),
		NUM_OUTPUTS
	};
	
	enum LightIds {
		ENUMS(CHAN_LIGHTS, 16),
		NUM_LIGHTS
	};
	

	// Constants
	// none


	// Need to save, no reset
	int facePlate;
	
	// Need to save, with reset
	// none
	
	// No need to save, with reset
	// none

	// No need to save, no reset
	RefreshCounter refresh;	
	
	
	Unmeld() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		configInput(POLY_INPUT, "Polyphonic");
		for (int i = 0; i < 8; i++) {
			configOutput(SPLIT_OUTPUTS + 2 * i + 0, string::f("Track %i left", i + 1));
			configOutput(SPLIT_OUTPUTS + 2 * i + 1, string::f("Track %i right", i + 1));
		}

		configOutput(THRU_OUTPUT, "Polyphonic");
		
		configBypass(POLY_INPUT, THRU_OUTPUT);		
		
		onReset();
		
		facePlate = 0;
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

		// facePlate
		json_object_set_new(rootJ, "facePlate", json_integer(facePlate));
		
		return rootJ;
	}


	void dataFromJson(json_t *rootJ) override {
		// facePlate
		json_t *facePlateJ = json_object_get(rootJ, "facePlate");
		if (facePlateJ)
			facePlate = json_integer_value(facePlateJ);

		resetNonJson(true);	}


	void onSampleRateChange() override {
	}
	

	void process(const ProcessArgs &args) override {
		// Controls
		if (refresh.processInputs()) {
			// none
		}// userInputs refresh
		
		
		// Thru
		int numChan = inputs[POLY_INPUT].getChannels();
		outputs[THRU_OUTPUT].setChannels(numChan);
		for (int c = 0; c < numChan; c++) {
			float v = inputs[POLY_INPUT].getVoltage(c);
			outputs[THRU_OUTPUT].setVoltage(v, c);
		}
		
		// Split
		int c = 0;
		for (; c < numChan; c++) {
			float v = inputs[POLY_INPUT].getVoltage(c);
			outputs[SPLIT_OUTPUTS + c].setVoltage(v);
		}		
		for (; c < 16; c++) {
			outputs[SPLIT_OUTPUTS + c].setVoltage(0.0f);
		}
		
		// Lights
		if (refresh.processLights()) {
			int i = 0;
			for (; i < numChan; i++) {
				lights[CHAN_LIGHTS + i].setBrightness(1.0f);
			}
			for (; i < 16; i++) {
				lights[CHAN_LIGHTS + i].setBrightness(0.0f);
			}
		}
	}// process()
};


//-----------------------------------------------------------------------------

static const int NUM_PANELS = 3;

std::string facePlateNames[3] = {
	"1-8",
	"9-16",
	"Group/Aux"
};

static const std::string facePlateFileNames[NUM_PANELS] = {
	"res/dark/unmeld-1-8.svg",
	"res/dark/unmeld-9-16.svg",
	"res/dark/unmeld-grp-aux.svg"
};


struct UnmeldWidget : ModuleWidget {
	std::shared_ptr<window::Svg> svgs[NUM_PANELS] = {};
	int lastFacePlate = 0;
	PortWidget* pwPolyIn;
		
	void appendContextMenu(Menu *menu) override {
		Unmeld *module = (Unmeld*)(this->module);
		assert(module);

		menu->addChild(new MenuSeparator());
		
		MenuLabel *themeLabel = new MenuLabel();
		themeLabel->text = "Panel";
		menu->addChild(themeLabel);

		for (int i = 0; i < NUM_PANELS; i++) {
			menu->addChild(createCheckMenuItem(facePlateNames[i], "",
				[=]() {return module->facePlate == i;},
				[=]() {module->facePlate = i;}
			));	
		}
	}	
	
	
	UnmeldWidget(Unmeld *module) {
		setModule(module);

		// Main panels from Inkscape
        svgs[0] = APP->window->loadSvg(asset::plugin(pluginInstance, facePlateFileNames[0]));
		setPanel(svgs[0]);
		
		// poly in/thru
		addInput(pwPolyIn = createInputCentered<MmPortGold>(mm2px(Vec(6.84, 18.35)), module, Unmeld::POLY_INPUT));
		addOutput(createOutputCentered<MmPortGold>(mm2px(Vec(23.64, 18.35)), module, Unmeld::THRU_OUTPUT));
		
		// leds
		for (int i = 0; i < 8; i++) {
			addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(14.3, 9.5 + 2 * i)), module, Unmeld::CHAN_LIGHTS + 2 * i + 0));
			addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(16.18, 9.5 + 2 * i)), module, Unmeld::CHAN_LIGHTS + 2 * i + 1));
		}
		
		// split signals
		for (int i = 0; i < 8; i++) {
			addOutput(createOutputCentered<MmPort>(mm2px(Vec(10.33, 34.5 + 10.85 * i)), module, Unmeld::SPLIT_OUTPUTS + 2 * i + 0));
			addOutput(createOutputCentered<MmPort>(mm2px(Vec(20.15, 34.5 + 10.85 * i)), module, Unmeld::SPLIT_OUTPUTS + 2 * i + 1));
		}
	}
	
	void step() override {
		if (module) {
			Unmeld* unmeldModule = (Unmeld*)module;
			int facePlate = unmeldModule->facePlate;
			
			// test input cable to see where it's connected, if to a MM mixer/auxspander, then auto-set the faceplate
			std::vector<CableWidget*> cablesOnPolyIn = APP->scene->rack->getCablesOnPort(pwPolyIn);
			for (CableWidget* cw : cablesOnPolyIn) {
				Cable* cable = cw->getCable();
				if (cable) {
					Module* srcModule = cable->outputModule;
					if (srcModule) {
						if (srcModule->model == modelMixMaster) {
							if (cable->outputId >= 5 && cable->outputId <= 7) {
								unmeldModule->facePlate = 0 + cable->outputId - 5;// facePlate 0 to 2
							}
							else if (cable->outputId >= 0 && cable->outputId <= 2) {
								unmeldModule->facePlate = cable->outputId ;// facePlate 0 to 2
							}
						}
						else if (srcModule->model == modelMixMasterJr) {
							if (cable->outputId == 4 || cable->outputId == 0) {
								unmeldModule->facePlate = 0;
							}
							else if (cable->outputId == 5 || cable->outputId == 1) {
								unmeldModule->facePlate = 2;
							}
						}
					}
				}					
			}

			if (facePlate != lastFacePlate) {
				lastFacePlate = facePlate;
				
				// change main panel
				if (svgs[facePlate] == NULL) {
					svgs[facePlate] = APP->window->loadSvg(asset::plugin(pluginInstance, facePlateFileNames[facePlate]));
				}
				SvgPanel* panel = (SvgPanel*)getPanel();
				panel->setBackground(svgs[facePlate]);
				panel->fb->dirty = true;
				
				// Update port tooltips
				if (facePlate == 0) {// "1-8"
					for (int i = 0; i < 8; i++) {
						module->outputInfos[Unmeld::SPLIT_OUTPUTS + 2 * i + 0]->name = string::f("Track %i left", i + 1);
						module->outputInfos[Unmeld::SPLIT_OUTPUTS + 2 * i + 1]->name = string::f("Track %i right", i + 1);
					}
				}
				else if (facePlate == 1) {// "9-16"
					for (int i = 0; i < 8; i++) {
						module->outputInfos[Unmeld::SPLIT_OUTPUTS + 2 * i + 0]->name = string::f("Track %i left", i + 1 + 8);
						module->outputInfos[Unmeld::SPLIT_OUTPUTS + 2 * i + 1]->name = string::f("Track %i right", i + 1 + 8);
					}
				}
				else {// if (facePlate == 2) {// "Group/Aux"
					for (int i = 0; i < 4; i++) {
						module->outputInfos[Unmeld::SPLIT_OUTPUTS + 2 * i + 0]->name = string::f("Group %i left", i + 1);
						module->outputInfos[Unmeld::SPLIT_OUTPUTS + 2 * i + 1]->name = string::f("Group %i right", i + 1);
						module->outputInfos[Unmeld::SPLIT_OUTPUTS + 2 * i + 0 + 8]->name = string::f("Aux %i left", i + 1);
						module->outputInfos[Unmeld::SPLIT_OUTPUTS + 2 * i + 1 + 8]->name = string::f("Aux %i right", i + 1);
					}
				}
			}
		}
		Widget::step();
	}
};


Model *modelUnmeld = createModel<Unmeld, UnmeldWidget>("Unmeld");
