//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "MindMeldModular.hpp"


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
			// bypass
			configParam(BYPASS_PARAMS + i, 0.0f, 1.0f, 0.0f, string::f("Bypass %i", i + 1));
		}
		
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

static const std::string facePlateNames[NUM_M + NUM_16 + NUM_8] = {
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

static const std::string facePlateFileNames[NUM_M + NUM_16 + NUM_8] = {
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
	"res/dark/meld/16track/Aux-A-D-Grps.svg",
	"res/dark/meld/8track/Aux-M-1-8-GrpsJr.svg",
	"res/dark/meld/16track/Bus-Snd-Pan-Rtn.svg",
	"res/dark/meld/16track/Bus-M_S.svg",
	
};
	
	
struct MeldWidget : ModuleWidget {
	int lastFacePlate = 0;
	LedButton *ledButtons[8];
	SmallLight<GreenRedLight> *smallLights[8];
		
	struct PanelsItem : MenuItem {
		Meld *module;
		int start;
		int end;
		
		struct PanelsSubItem : MenuItem {
			Meld *module;
			int plate;
			void onAction(const event::Action &e) override {
				module->facePlate = plate;
			}
		};

		Menu *createChildMenu() override {
			Menu *menu = new Menu;
				
			for (int i = start; i < end; i++) {
				PanelsSubItem *apItem = createMenuItem<PanelsSubItem>(facePlateNames[i], CHECKMARK(module->facePlate == i));
				apItem->module = module;
				apItem->plate = i;
				menu->addChild(apItem);
			}
			
			return menu;
		}
	};

	void appendContextMenu(Menu *menu) override {
		Meld *module = (Meld*)(this->module);
		assert(module);

		menu->addChild(new MenuSeparator());

		MenuLabel *themeLabel = new MenuLabel();
		themeLabel->text = "Panel choices";
		menu->addChild(themeLabel);

		int fp = module->facePlate;
		char rights[8] = CHECKMARK_STRING; if (fp >= NUM_M) rights[0] = 0;
		PanelsItem *audiopItem = createMenuItem<PanelsItem>("Audio panels", strcat(strcat(rights, " "), RIGHT_ARROW));
		audiopItem->module = module;
		audiopItem->start = 0;
		audiopItem->end = NUM_M;
		menu->addChild(audiopItem);

		char rights2[8] = CHECKMARK_STRING; if (fp < NUM_M || fp >= (NUM_M + NUM_16)) rights2[0] = 0;
		PanelsItem *cvsrpItem = createMenuItem<PanelsItem>("CV panels", strcat(strcat(rights2, " "), RIGHT_ARROW));
		cvsrpItem->module = module;
		cvsrpItem->start = NUM_M;
		cvsrpItem->end = NUM_M + NUM_16;
		menu->addChild(cvsrpItem);

		char rights3[8] = CHECKMARK_STRING; if (fp < (NUM_M + NUM_16)) rights3[0] = 0;
		PanelsItem *cvjrpItem = createMenuItem<PanelsItem>("CV panels Jr", strcat(strcat(rights3, " "), RIGHT_ARROW));
		cvjrpItem->module = module;
		cvjrpItem->start = NUM_M + NUM_16;
		cvjrpItem->end = NUM_M + NUM_16 + NUM_8;
		menu->addChild(cvjrpItem);

	}	
	
	
	MeldWidget(Meld *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, facePlateFileNames[lastFacePlate])));

		// poly in/thru
		addInput(createInputCentered<MmPortGold>(mm2px(Vec(6.84, 18.35)), module, Meld::POLY_INPUT));
		addOutput(createOutputCentered<MmPortGold>(mm2px(Vec(23.64, 18.35)), module, Meld::OUT_OUTPUT));
		
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
			int facePlate = (((Meld*)module)->facePlate);
			if (facePlate != lastFacePlate) {
				// change main panel
				setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, facePlateFileNames[facePlate])));
				lastFacePlate = facePlate;
				
				// update bypass led-buttons' tooltips
				for (int i = 0; i < 8; i++) {
					if (facePlate == 0) {
						module->paramQuantities[Meld::BYPASS_PARAMS + i]->label = string::f("Bypass %i", i + 1);
					}
					else if (facePlate == 1) {
						module->paramQuantities[Meld::BYPASS_PARAMS + i]->label = string::f("Bypass %i", i + 1 + 8);
					}
					else {
						if (i < 4) {
							module->paramQuantities[Meld::BYPASS_PARAMS + i]->label = string::f("Bypass G%i", i + 1);
						}
						else {
							module->paramQuantities[Meld::BYPASS_PARAMS + i]->label = string::f("Bypass A%i", i + 1 - 4);
						}
					}
				}
				
				// update bypass led-buttons' visibilities
				for (int i = 0; i < 8; i++) {
					ledButtons[i]->visible = facePlate < 3;
					smallLights[i]->visible = facePlate < 3;
				}
			}
		}
		Widget::step();
	}	
};


Model *modelMeld = createModel<Meld, MeldWidget>("Meld");
