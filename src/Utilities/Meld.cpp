//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "../MindMeldModular.hpp"


struct Meld : Module {
	
	enum ParamIds {
		ENUMS(BYPASS_PARAMS, 8),
		NUM_PARAMS
	};
	
	enum InputIds {
		POLY_INPUT,
		ENUMS(MERGE_INPUTS, 16),
		NUM_INPUTS
	};
	
	enum OutputIds {
		OUT_OUTPUT,
		NUM_OUTPUTS
	};
	
	enum LightIds {
		ENUMS(CHAN_LIGHTS, 16 * 2),// room for white-blue lights
		ENUMS(BYPASS_LIGHTS, 8 * 2),// room for red-green lights
		NUM_LIGHTS
	};
	

	// Constants
	// none


	// Need to save, no reset
	int facePlate;
	
	// Need to save, with reset
	int bypassState[8];// 0 = pass, 1 = bypass
	
	// No need to save, with reset
	int lastMergeInputIndex;// can be -1 when nothing connected
	TSlewLimiterSingle<simd::float_4> bypassSlewersVect[4];
	
	// No need to save, no reset
	RefreshCounter refresh;	
	Trigger bypassTriggers[8];
	
	
	void calcLastMergeInputIndex() {// can set to -1 when nothing connected
		int i = 15;
		for (; i >= 0; i--) {
			if (inputs[MERGE_INPUTS + i].isConnected()) {
				break;
			}
		}
		lastMergeInputIndex = i;
		return;
	}
	
	bool trackInUse(int trk) {// trk is 0 to 7
		return inputs[MERGE_INPUTS + trk * 2 + 0].isConnected() || inputs[MERGE_INPUTS + trk * 2 + 1].isConnected();
	}
	
	
	Meld() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		facePlate = 0;
		
		for (int i = 0; i < 8; i++) {
			configParam(BYPASS_PARAMS + i, 0.0f, 1.0f, 0.0f, string::f("Bypass %i", i + 1));
		}
		
		configInput(POLY_INPUT, "Polyphonic");
		for (int i = 0; i < 8; i++) {
			configInput(MERGE_INPUTS + 2 * i + 0, string::f("Track %i left", i + 1));
			configInput(MERGE_INPUTS + 2 * i + 1, string::f("Track %i right", i + 1));
		}

		configOutput(OUT_OUTPUT, "Polyphonic");
		
		configBypass(POLY_INPUT, OUT_OUTPUT);

		for (int i = 0; i < 4; i++) {
			bypassSlewersVect[i].setRiseFall(simd::float_4(100.0f)); // slew rate is in input-units per second (ex: V/s)			
		}

		onReset();
	}
  
	void onReset() override {
		for (int trk = 0; trk < 8; trk++) {
			bypassState[trk] = 0;
		}
		resetNonJson(false);
	}
	void resetNonJson(bool recurseNonJson) {
		calcLastMergeInputIndex();
		for (int i = 0; i < 16; i++) {
			bypassSlewersVect[i >> 2].out[i & 0x3] = (float)bypassState[i >> 1];
		}
	}


	void onRandomize() override {
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// facePlate
		json_object_set_new(rootJ, "facePlate", json_integer(facePlate));
		
		// bypassState
		json_t *bypassStateJ = json_array();
		for (int i = 0; i < 8; i++) {
			json_array_insert_new(bypassStateJ, i, json_integer(bypassState[i]));
		}
		json_object_set_new(rootJ, "bypassState2", bypassStateJ);// 2 to avoid loading corrupt values in 1.1.3's bug
		
		return rootJ;
	}


	void dataFromJson(json_t *rootJ) override {
		// facePlate
		json_t *facePlateJ = json_object_get(rootJ, "facePlate");
		if (facePlateJ)
			facePlate = json_integer_value(facePlateJ);

		// bypassState
		json_t *bypassStateJ = json_object_get(rootJ, "bypassState2");// 2 to avoid loading corrupt values in 1.1.3's bug
		if (bypassStateJ) {
			for (int i = 0; i < 8; i++) {
				json_t *bypassStateArrayJ = json_array_get(bypassStateJ, i);
				if (bypassStateArrayJ) {
					bypassState[i] = json_integer_value(bypassStateArrayJ);
				}
			}			
		}

		resetNonJson(true);
	}


	void onSampleRateChange() override {
	}
	

	void process(const ProcessArgs &args) override {
		// Controls
		if (refresh.processInputs()) {
			calcLastMergeInputIndex();
			
			for (int trk = 0; trk < 8; trk++) {
				if (bypassTriggers[trk].process(params[BYPASS_PARAMS + trk].getValue())) {
					if (trackInUse(trk)) {
						bypassState[trk] ^= 0x1;
					}
				}
			}
		}// userInputs refresh
		
		
		// Meld poly output
		int numChan = inputs[POLY_INPUT].isConnected() ? inputs[POLY_INPUT].getChannels() : 0;
		numChan = std::max(numChan, lastMergeInputIndex + 1);
		// here numChan can be 0		
		outputs[OUT_OUTPUT].setChannels(numChan);// clears all and sets num chan to 1 when numChan == 0

		// simd version
		simd::float_4 bypassVect[4];
		for (int c = 0; c < 16; c++) {
			bypassVect[c >> 2][c & 0x3] = (!inputs[MERGE_INPUTS + c].isConnected() || (bypassState[c >> 1] == 1)) ? 1.0f : 0.0f;
		}
		for (int i = 0; i < 4; i++) {
			bypassSlewersVect[i].process(args.sampleTime, bypassVect[i]);
			simd::float_4 vVect{inputs[MERGE_INPUTS + i * 4 + 0].getVoltage(),
				inputs[MERGE_INPUTS + i * 4 + 1].getVoltage(),
				inputs[MERGE_INPUTS + i * 4 + 2].getVoltage(),
				inputs[MERGE_INPUTS + i * 4 + 3].getVoltage()};
			vVect = crossfade(vVect, inputs[POLY_INPUT].getVoltageSimd<simd::float_4>(i << 2), bypassSlewersVect[i].out);
			outputs[OUT_OUTPUT].setVoltageSimd(vVect, i << 2);
		}
		
		// Lights
		if (refresh.processLights()) {
			// channel lights
			for (int i = 0; i < 16; i++) {
				float greenBright = 0.0f;
				float blueBright = 0.0f;
				if (i <= lastMergeInputIndex && inputs[MERGE_INPUTS + i].isConnected() && (bypassState[i >> 1] == 0)) {
					greenBright = 1.0f;
				}
				else if (i < numChan) {
					blueBright = 1.0f;
				}
				lights[CHAN_LIGHTS + 2 * i + 0].setBrightness(greenBright);
				lights[CHAN_LIGHTS + 2 * i + 1].setBrightness(blueBright);
			}
			// bypass lights
			for (int trk = 0; trk < 8; trk++) {
				bool ledEnable = trackInUse(trk);
				lights[BYPASS_LIGHTS + 2 * trk + 0].setBrightness(ledEnable && bypassState[trk] == 0 ? 1.0f : 0.0f);// green
				lights[BYPASS_LIGHTS + 2 * trk + 1].setBrightness(ledEnable && bypassState[trk] != 0 ? 1.0f : 0.0f);// red
			}
		}
	}// process()
};


//-----------------------------------------------------------------------------


static const int NUM_M = 3;// num panels in menu of Audio panels
static const int NUM_16 = 12;// num panels in menu of CV panels for Jr
static const int NUM_8 = 8;// num panels in menu of CV panels for Sr
static const int NUM_PANELS = NUM_M + NUM_16 + NUM_8;

static const std::string facePlateNames[NUM_PANELS] = {
	"1-8",
	"9-16",
	"Group/Aux",
	
	"Mute 1-16",
	"Solo 1-16",
	"M/S Grp Mstr",
	"Aux A 1-16",
	"Aux B 1-16",
	"Aux C 1-16",
	"Aux D 1-16",
	"Aux Mute 1-16",
	"Aux A-D Grps",
	"Aux Mute Grps",
	"Aux Bus Snd Pan Rtn",
	"Aux Bus M_S",
	
	"Mute/Solo 1-8",
	"M/S Grp Mstr",
	"Aux A/B 1-8",
	"Aux C/D 1-8",
	"Aux A-D Grps",
	"Aux Mute 1-8 Grps",
	"Aux Bus Snd Pan Rtn",
	"Aux Bus M_S",
};

static const std::string facePlateFileNames[NUM_PANELS] = {
	"res/dark/meld/meld-1-8.svg",
	"res/dark/meld/meld-9-16.svg",
	"res/dark/meld/meld-grp-aux.svg",
	
	"res/dark/meld/16track/Mute.svg",
	"res/dark/meld/16track/Solo.svg",
	"res/dark/meld/16track/M_S-Grp-Mstr.svg",
	"res/dark/meld/16track/Aux-A.svg",
	"res/dark/meld/16track/Aux-B.svg",
	"res/dark/meld/16track/Aux-C.svg",
	"res/dark/meld/16track/Aux-D.svg",
	"res/dark/meld/16track/Mute.svg",
	"res/dark/meld/16track/Aux-A-D-Grps.svg",
	"res/dark/meld/16track/Mute-Grps.svg",
	"res/dark/meld/16track/Bus-Snd-Pan-Rtn.svg",
	"res/dark/meld/16track/Bus-M_S.svg",
	
	"res/dark/meld/8track/M_S-1-8Jr.svg",
	"res/dark/meld/8track/M_S-Grp-MstrJr.svg",
	"res/dark/meld/8track/Aux-A_B-1-8Jr.svg",
	"res/dark/meld/8track/Aux-C_D-1-8Jr.svg",
	"res/dark/meld/8track/Aux-A-D-GrpsJr.svg",
	"res/dark/meld/8track/Aux-M-1-8-GrpsJr.svg",
	"res/dark/meld/16track/Bus-Snd-Pan-Rtn.svg",
	"res/dark/meld/16track/Bus-M_S.svg",
};
	
	
struct MeldWidget : ModuleWidget {
	std::shared_ptr<window::Svg> svgs[NUM_PANELS] = {};
	int lastFacePlate = 0;
	LedButton *ledButtons[8];
	SmallLight<GreenRedLight> *smallLights[8];
	PortWidget* pwPolyOut;
		
	struct PanelsItem : MenuItem {
		Meld *module;
		int start;
		int end;
		
		Menu *createChildMenu() override {
			Menu *menu = new Menu;
			for (int i = start; i < end; i++) {
				menu->addChild(createCheckMenuItem(facePlateNames[i], "",
					[=]() {return module->facePlate == i;},
					[=]() {module->facePlate = i;}
				));	
			}
			return menu;
		}
		
		void step() override {
			std::string _rightText = RIGHT_ARROW;
			int fp = module->facePlate;
			if (fp >= start && fp < end) {
				_rightText.insert(0, " ");
				_rightText.insert(0, CHECKMARK_STRING);
			}
			rightText = _rightText;
		}
	};

	void appendContextMenu(Menu *menu) override {
		Meld *module = (Meld*)(this->module);
		assert(module);

		menu->addChild(new MenuSeparator());

		MenuLabel *themeLabel = new MenuLabel();
		themeLabel->text = "Panel choices";
		menu->addChild(themeLabel);

		PanelsItem *audiopItem = createMenuItem<PanelsItem>("Audio panels", "");
		audiopItem->module = module;
		audiopItem->start = 0;
		audiopItem->end = NUM_M;
		menu->addChild(audiopItem);

		PanelsItem *cvsrpItem = createMenuItem<PanelsItem>("CV panels", "");
		cvsrpItem->module = module;
		cvsrpItem->start = NUM_M;
		cvsrpItem->end = NUM_M + NUM_16;
		menu->addChild(cvsrpItem);

		PanelsItem *cvjrpItem = createMenuItem<PanelsItem>("CV panels Jr", "");
		cvjrpItem->module = module;
		cvjrpItem->start = NUM_M + NUM_16;
		cvjrpItem->end = NUM_M + NUM_16 + NUM_8;
		menu->addChild(cvjrpItem);

	}	
	
	
	MeldWidget(Meld *module) {
		setModule(module);

		// Main panels from Inkscape
        svgs[0] = APP->window->loadSvg(asset::plugin(pluginInstance, facePlateFileNames[0]));
		setPanel(svgs[0]);

		// poly in/thru
		addInput(createInputCentered<MmPortGold>(mm2px(Vec(6.84, 18.35)), module, Meld::POLY_INPUT));
		addOutput(pwPolyOut = createOutputCentered<MmPortGold>(mm2px(Vec(23.64, 18.35)), module, Meld::OUT_OUTPUT));
		
		// leds
		for (int i = 0; i < 8; i++) {
			addChild(createLightCentered<TinyLight<MMWhiteBlueLight>>(mm2px(Vec(14.3, 9.5 + 2 * i)), module, Meld::CHAN_LIGHTS + 2 * (2 * i + 0)));
			addChild(createLightCentered<TinyLight<MMWhiteBlueLight>>(mm2px(Vec(16.18, 9.5 + 2 * i)), module, Meld::CHAN_LIGHTS + 2 * (2 * i + 1)));
		}
				
		for (int i = 0; i < 8; i++) {
			// merge signals
			addInput(createInputCentered<MmPort>(mm2px(Vec(10.33, 34.5 + 10.85 * i)), module, Meld::MERGE_INPUTS + 2 * i + 0));
			addInput(createInputCentered<MmPort>(mm2px(Vec(20.15, 34.5 + 10.85 * i)), module, Meld::MERGE_INPUTS + 2 * i + 1));
			
			// bypass led-buttons
			addParam(ledButtons[i] = createParamCentered<LedButton>(mm2px(Vec(26.93, 34.5 + 10.85 * i)), module, Meld::BYPASS_PARAMS + i));
			addChild(smallLights[i] = createLightCentered<SmallLight<GreenRedLight>>(mm2px(Vec(26.93, 34.5 + 10.85 * i)), module, Meld::BYPASS_LIGHTS + 2 * i));
		}
	}
	
	void step() override {
		if (module) {
			Meld* meldModule = (Meld*)module;
			int facePlate = meldModule->facePlate;
			
			// test output cable to see where it's connected, if to a MM mixer/auxspander, then auto-set the faceplate
			std::vector<CableWidget*> cablesOnPolyOut = APP->scene->rack->getCablesOnPort(pwPolyOut);
			for (CableWidget* cw : cablesOnPolyOut) {
				Cable* cable = cw->getCable();
				if (cable) {
					Module* destModule = cable->inputModule;
					if (destModule) {
						if (destModule->model == modelMixMaster) {
							if (cable->inputId >= 74 && cable->inputId <= 76) {
								meldModule->facePlate = 0 + cable->inputId - 74;// facePlate 0 to 2
							}
							else if (cable->inputId >= 77 && cable->inputId <= 79) {
								meldModule->facePlate = 3 + cable->inputId - 77;// facePlate 3 to 5
							}
						}
						else if (destModule->model == modelMixMasterJr) {
							if (cable->inputId == 38) {
								meldModule->facePlate = 0;
							}
							else if (cable->inputId == 39) {
								meldModule->facePlate = 2;
							}
							else if (cable->inputId == 40) {
								meldModule->facePlate = 15;
							}
							else if (cable->inputId == 42) {
								meldModule->facePlate = 16;
							}
						}
						else if (destModule->model == modelAuxExpander) {
							if (cable->inputId >= 8 && cable->inputId <= 16) {
								meldModule->facePlate = 0 + cable->inputId - 2;// facePlate 6 to 14
							}
						}
						else if (destModule->model == modelAuxExpanderJr) {
							if (cable->inputId == 8) {
								meldModule->facePlate = 17;
							}
							else if (cable->inputId == 9) {
								meldModule->facePlate = 18;
							}
							else if (cable->inputId == 11) {
								meldModule->facePlate = 19;
							}
							else if (cable->inputId == 10) {
								meldModule->facePlate = 20;
							}
							else if (cable->inputId == 13) {
								meldModule->facePlate = 21;
							}
							else if (cable->inputId == 14) {
								meldModule->facePlate = 22;
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
				
				
				// update bypass led-buttons' tooltips
				for (int i = 0; i < 8; i++) {
					if (facePlate == 0) {
						module->paramQuantities[Meld::BYPASS_PARAMS + i]->name = string::f("Bypass track %i", i + 1);
					}
					else if (facePlate == 1) {
						module->paramQuantities[Meld::BYPASS_PARAMS + i]->name = string::f("Bypass track %i", i + 1 + 8);
					}
					else {
						if (i < 4) {
							module->paramQuantities[Meld::BYPASS_PARAMS + i]->name = string::f("Bypass group %i", i + 1);
						}
						else {
							module->paramQuantities[Meld::BYPASS_PARAMS + i]->name = string::f("Bypass aux %i", i + 1 - 4);
						}
					}
				}
				
				// update bypass led-buttons' visibilities
				for (int i = 0; i < 8; i++) {
					ledButtons[i]->setVisible(facePlate < 3);
					smallLights[i]->setVisible(facePlate < 3);
				}
				
				// Update port tooltips				
				if (facePlate == 0) {// "1-8"
					for (int i = 0; i < 8; i++) {
						module->inputInfos[Meld::MERGE_INPUTS + 2 * i + 0]->name = string::f("Track %i left", i + 1);
						module->inputInfos[Meld::MERGE_INPUTS + 2 * i + 1]->name = string::f("Track %i right", i + 1);
					}
				}
				else if (facePlate == 1) {// "9-16"
					for (int i = 0; i < 8; i++) {
						module->inputInfos[Meld::MERGE_INPUTS + 2 * i + 0]->name = string::f("Track %i left", i + 1 + 8);
						module->inputInfos[Meld::MERGE_INPUTS + 2 * i + 1]->name = string::f("Track %i right", i + 1 + 8);
					}
				}
				else if (facePlate == 2) {// "Group/Aux"
					for (int i = 0; i < 4; i++) {
						module->inputInfos[Meld::MERGE_INPUTS + 2 * i + 0]->name = string::f("Group %i left", i + 1);
						module->inputInfos[Meld::MERGE_INPUTS + 2 * i + 1]->name = string::f("Group %i right", i + 1);
						module->inputInfos[Meld::MERGE_INPUTS + 2 * i + 0 + 8]->name = string::f("Aux %i left", i + 1);
						module->inputInfos[Meld::MERGE_INPUTS + 2 * i + 1 + 8]->name = string::f("Aux %i right", i + 1);
					}
				}
				else if (facePlate == 3) {// "Mute 1-16"
					for (int i = 0; i < 16; i++) {
						module->inputInfos[Meld::MERGE_INPUTS + i]->name = string::f("Track %i mute", i + 1);
					}
				}
				else if (facePlate == 4) {// "Solo 1-16"
					for (int i = 0; i < 16; i++) {
						module->inputInfos[Meld::MERGE_INPUTS + i]->name = string::f("Track %i solo", i + 1);
					}
				}
				else if (facePlate == 5) {// "M/S Grp Mstr"
					for (int i = 0; i < 4; i++) {
						module->inputInfos[Meld::MERGE_INPUTS + i    ]->name = string::f("Group %i mute", i + 1);
						module->inputInfos[Meld::MERGE_INPUTS + i + 4]->name = string::f("Group %i solo", i + 1 + 4);
						module->inputInfos[Meld::MERGE_INPUTS + i + 12]->name = string::f("Unused %i", i + 1);
					}
					module->inputInfos[Meld::MERGE_INPUTS + 8]->name = "Master mute";
					module->inputInfos[Meld::MERGE_INPUTS + 9]->name = "Master dim";
					module->inputInfos[Meld::MERGE_INPUTS + 10]->name = "Master mono";
					module->inputInfos[Meld::MERGE_INPUTS + 11]->name = "Master volume";
				}
				else if (facePlate == 6) {// "Aux A 1-16"
					for (int i = 0; i < 16; i++) {
						module->inputInfos[Meld::MERGE_INPUTS + i]->name = string::f("Track %i aux A send", i + 1);
					}
				}
				else if (facePlate == 7) {// "Aux B 1-16"
					for (int i = 0; i < 16; i++) {
						module->inputInfos[Meld::MERGE_INPUTS + i]->name = string::f("Track %i aux B send", i + 1);
					}
				}
				else if (facePlate == 8) {// "Aux C 1-16"
					for (int i = 0; i < 16; i++) {
						module->inputInfos[Meld::MERGE_INPUTS + i]->name = string::f("Track %i aux C send", i + 1);
					}
				}
				else if (facePlate == 9) {// "Aux D 1-16"
					for (int i = 0; i < 16; i++) {
						module->inputInfos[Meld::MERGE_INPUTS + i]->name = string::f("Track %i aux D send", i + 1);
					}
				}
				else if (facePlate == 10) {// "Aux Mute 1-16"
					for (int i = 0; i < 16; i++) {
						module->inputInfos[Meld::MERGE_INPUTS + i]->name = string::f("Track %i aux mute", i + 1);
					}
				}
				else if (facePlate == 11) {// "Aux A-D Grps"
					for (int i = 0; i < 4; i++) {
						module->inputInfos[Meld::MERGE_INPUTS + i     ]->name = string::f("Group %i aux A send", i + 1);
						module->inputInfos[Meld::MERGE_INPUTS + i + 4 ]->name = string::f("Group %i aux B send", i + 1 + 4);
						module->inputInfos[Meld::MERGE_INPUTS + i + 8 ]->name = string::f("Unused %i aux C send", i + 1);
						module->inputInfos[Meld::MERGE_INPUTS + i + 12]->name = string::f("Unused %i aux D send", i + 1 + 4);
					}
				}
				else if (facePlate == 12) {// "Aux Mute Grps"
					for (int i = 0; i < 4; i++) {
						module->inputInfos[Meld::MERGE_INPUTS + i]->name = string::f("Group %i aux mute", i + 1);
					}
					for (int i = 0; i < 12; i++) {
						module->inputInfos[Meld::MERGE_INPUTS + i + 4]->name = string::f("Unused %i", i + 1);
					}
				}
				else if (facePlate == 13 || facePlate == 21) {// "Aux Bus Snd Pan Rtn"
					for (int i = 0; i < 4; i++) {
						module->inputInfos[Meld::MERGE_INPUTS + i     ]->name = string::f("Aux %c global send", i + 'A');
						module->inputInfos[Meld::MERGE_INPUTS + i + 4 ]->name = string::f("Aux %c return pan", i + 'A');
						module->inputInfos[Meld::MERGE_INPUTS + i + 8 ]->name = string::f("Aux %c return level", i + 'A');
						module->inputInfos[Meld::MERGE_INPUTS + i + 12]->name = string::f("Unused %i", i + 1);
					}					
				}
				else if (facePlate == 14 || facePlate == 22) {// "Aux Bus M_S"
					for (int i = 0; i < 4; i++) {
						module->inputInfos[Meld::MERGE_INPUTS + i     ]->name = string::f("Aux %c return mute", i + 'A');
						module->inputInfos[Meld::MERGE_INPUTS + i + 4 ]->name = string::f("Aux %c return solo", i + 'A');
						module->inputInfos[Meld::MERGE_INPUTS + i + 8 ]->name = string::f("Unused %i", i + 1);
						module->inputInfos[Meld::MERGE_INPUTS + i + 12]->name = string::f("Unused %i", i + 1 + 4);
					}					
				}
				else if (facePlate == 15) {// "Mute/Solo 1-8"
					for (int i = 0; i < 8; i++) {
						module->inputInfos[Meld::MERGE_INPUTS + i    ]->name = string::f("Track %i mute", i + 1);
						module->inputInfos[Meld::MERGE_INPUTS + i + 8]->name = string::f("Track %i solo", i + 1);
					}
				}
				else if (facePlate == 16) {// "M/S Grp Mstr"
					module->inputInfos[Meld::MERGE_INPUTS + 0]->name = "Group 1 mute";
					module->inputInfos[Meld::MERGE_INPUTS + 1]->name = "Group 2 mute";
					module->inputInfos[Meld::MERGE_INPUTS + 2]->name = "Group 1 solo";
					module->inputInfos[Meld::MERGE_INPUTS + 3]->name = "Group 2 solo";
					module->inputInfos[Meld::MERGE_INPUTS + 4]->name = "Master mute";
					module->inputInfos[Meld::MERGE_INPUTS + 5]->name = "Master dim";
					module->inputInfos[Meld::MERGE_INPUTS + 6]->name = "Master mono";
					module->inputInfos[Meld::MERGE_INPUTS + 7]->name = "Master volume";
					for (int i = 0; i < 8; i++) {
						module->inputInfos[Meld::MERGE_INPUTS + i + 8]->name = string::f("Unused %i", i + 1);
					}
				}
				else if (facePlate == 17) {// "Aux A/B 1-8"
					for (int i = 0; i < 8; i++) {
						module->inputInfos[Meld::MERGE_INPUTS + i    ]->name = string::f("Track %i aux A send", i + 1);
						module->inputInfos[Meld::MERGE_INPUTS + i + 8]->name = string::f("Track %i aux B send", i + 1);
					}
				}
				else if (facePlate == 18) {// "Aux C/D 1-8"
					for (int i = 0; i < 8; i++) {
						module->inputInfos[Meld::MERGE_INPUTS + i    ]->name = string::f("Track %i aux C send", i + 1);
						module->inputInfos[Meld::MERGE_INPUTS + i + 8]->name = string::f("Track %i aux D send", i + 1);
					}
				}
				else if (facePlate == 19) {// "Aux A-D Grps"
					module->inputInfos[Meld::MERGE_INPUTS + 0]->name = "Group 1 aux A send";
					module->inputInfos[Meld::MERGE_INPUTS + 1]->name = "Group 2 aux A send";
					module->inputInfos[Meld::MERGE_INPUTS + 2]->name = "Group 1 aux B send";
					module->inputInfos[Meld::MERGE_INPUTS + 3]->name = "Group 2 aux B send";
					module->inputInfos[Meld::MERGE_INPUTS + 4]->name = "Group 1 aux C send";
					module->inputInfos[Meld::MERGE_INPUTS + 5]->name = "Group 2 aux C send";
					module->inputInfos[Meld::MERGE_INPUTS + 6]->name = "Group 1 aux D send";
					module->inputInfos[Meld::MERGE_INPUTS + 7]->name = "Group 2 aux D send";
					for (int i = 0; i < 8; i++) {
						module->inputInfos[Meld::MERGE_INPUTS + i + 8]->name = string::f("Unused %i", i + 1);
					}
				}
				else {// if (facePlate == 20) {// "Aux Mute 1-8 Grps"
					for (int i = 0; i < 8; i++) {
						module->inputInfos[Meld::MERGE_INPUTS + i]->name = string::f("Track %i aux mute", i + 1);
					}
					module->inputInfos[Meld::MERGE_INPUTS + 8]->name = "Group 1 aux mute";
					module->inputInfos[Meld::MERGE_INPUTS + 9]->name = "Group 2 aux mute";
					for (int i = 0; i < 6; i++) {
						module->inputInfos[Meld::MERGE_INPUTS + i + 10]->name = string::f("Unused %i", i + 1);
					}
				}
				// else if (facePlate == 21) {// "Aux Bus Snd Pan Rtn"  (Same as faceplate 13, done there)
				// else if (facePlate == 22) {// "Aux Bus M_S"          (Same as faceplate 14, done there)
			}
		}
		Widget::step();
	}	
};


Model *modelMeld = createModel<Meld, MeldWidget>("Meld");
