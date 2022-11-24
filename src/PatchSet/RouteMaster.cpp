//***********************************************************************************************
//Remote control switcher module pair
//For VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "../MindMeldModular.hpp"
#include "PatchSetWidgets.hpp"
#include "PatchMasterUtil.hpp"


static const uint8_t MAX_NUM = 5;// if more than 8, must bump up size of SwitcherMessage::labels array and change types uint8_t located here and in RouteMasterMessageBus
static const std::string defaultName = "RouteMaster";


template <int INS, int OUTS, int WIDTH>
struct RouteMaster : Module {
	// constraints on template params:
	//   either INS or OUTS must be 1, and the other must be MAX_NUM
	//   WIDTH is 1 or 2
	
	enum ParamIds {
		ENUMS(SEL_PARAMS, MAX_NUM),
		NUM_PARAMS
	};
	
	enum InputIds {
		ENUMS(IN_INPUTS, INS * WIDTH),
		NUM_INPUTS
	};
	
	enum OutputIds {
		ENUMS(OUT_OUTPUTS, OUTS * WIDTH),
		NUM_OUTPUTS
	};
	
	enum LightIds {
		ENUMS(SEL_LIGHTS, MAX_NUM),
		NUM_LIGHTS
	};
	

	// Constants
	static constexpr float SLEW_RATE = 25.0f;

	// Need to save, no reset
	// none
	
	// Need to save, with reset
	int sel;
	std::string name;
	std::string labels[MAX_NUM];
	PackedBytes4 miscSettings;
	
	// No need to save, with reset
	SlewLimiterSingle gainSlewers[MAX_NUM];
	int updateControllerLabelsRequest;// 0 when nothing to do, 1 for read labels in widget
	
	// No need to save, no reset
	RefreshCounter refresh;
	Trigger selTriggers[MAX_NUM];
	
	
	RouteMaster() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		// config params		
		for (int i = 0; i < MAX_NUM; i++) {
			configParam(SEL_PARAMS + i, 0.0f, 1.0f, 0.0f, string::f("Channel %i", i + 1), "");
		}

		// config outputs
		if (OUTS == 1) {
			if (WIDTH == 1 ) {
				configOutput(OUT_OUTPUTS + 0, "Main");
			}
			else {
				configOutput(OUT_OUTPUTS + 0 + 0, "Left");
				configOutput(OUT_OUTPUTS + 0 + 1, "Right");
			}
		}
		else {
			if (WIDTH == 1 ) {
				for (int i = 0; i < OUTS; i++) {
					configOutput(OUT_OUTPUTS + i, string::f("Channel %i", i + 1));
				}
			}
			else {
				for (int i = 0; i < OUTS; i++) {
					configOutput(OUT_OUTPUTS + i + 0, string::f("Channel %i left", i + 1));
					configOutput(OUT_OUTPUTS + i + OUTS, string::f("Channel %i right", i + 1));
				}	
			}
		}
		
		// config inputs
		if (INS == 1) {
			if (WIDTH == 1 ) {
				configInput(IN_INPUTS + 0, "Main");
			}
			else {
				configInput(IN_INPUTS + 0 + 0, "Left");
				configInput(IN_INPUTS + 0 + 1, "Right");
			}
		}
		else {
			if (WIDTH == 1 ) {
				for (int i = 0; i < INS; i++) {
					configInput(IN_INPUTS + i, string::f("Channel %i", i + 1));
				}
			}
			else {
				for (int i = 0; i < INS; i++) {
					configInput(IN_INPUTS + i + 0, string::f("Channel %i left", i + 1));
					configInput(IN_INPUTS + i + INS, string::f("Channel %i right", i + 1));
				}			
			}
		}
		
		for (int i = 0; i < MAX_NUM; i++) {
			gainSlewers[i].setRiseFall(SLEW_RATE);
		}
	
		onReset();
	}
  
	void onReset() override {
		sel = 0;
		name = defaultName;
		for (int i = 0; i < MAX_NUM; i++) {
			std::string defaultItemPrefix = (INS == 1 ? "Output" : "Input");
			labels[i] = string::f(" %i", i + 1).insert(0, defaultItemPrefix);
		}
		miscSettings.cc4[0] = 0x0;// name color
		miscSettings.cc4[1] = 0x1;// get labels from param mapping
		miscSettings.cc4[2] = 0x0;// unused
		miscSettings.cc4[3] = 0x0;// unused
		
		resetNonJson();
	}
	void resetNonJson() {
		for (int i = 0; i < MAX_NUM; i++) {
			gainSlewers[i].reset();
		}
		updateControllerLabelsRequest = 1;
	}


	void onRandomize() override {
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// sel
		json_object_set_new(rootJ, "sel", json_integer(sel));

		// name
		json_object_set_new(rootJ, "name", json_string(name.c_str()));

		// labels
		json_t* labelsJ = json_array();
		for (int n = 0; n < MAX_NUM; n++) {
			json_array_insert_new(labelsJ, n, json_string(labels[n].c_str()));
		}
		json_object_set_new(rootJ, "labels", labelsJ);
	
		// miscSettings
		json_object_set_new(rootJ, "miscSettings", json_integer(miscSettings.cc1));

		return rootJ;
	}


	void dataFromJson(json_t *rootJ) override {
		// sel
		json_t *selJ = json_object_get(rootJ, "sel");
		if (selJ)
			sel = json_integer_value(selJ);

		// name
		json_t *nameJ = json_object_get(rootJ, "name");
		if (nameJ)
			name = json_string_value(nameJ);

		// labels
		json_t* labelsJ = json_object_get(rootJ, "labels");
		if (labelsJ) {
			for (int n = 0; n < MAX_NUM; n++) {
				json_t *labelsArrayJ = json_array_get(labelsJ, n);
				if (labelsArrayJ)
					labels[n] = json_string_value(labelsArrayJ);
			}
		}
		updateControllerLabelsRequest = 1;

		// miscSettings
		json_t *miscSettingsJ = json_object_get(rootJ, "miscSettings");
		if (miscSettingsJ)
			miscSettings.cc1 = json_integer_value(miscSettingsJ);

		resetNonJson();
	}


	void onSampleRateChange() override {
	}
	

	void process(const ProcessArgs &args) override {
		// Controls
		if (refresh.processInputs()) {
			for (int i = 0; i < MAX_NUM; i++) {
				if (selTriggers[i].process(params[SEL_PARAMS + i].getValue())) {
					sel = i;
				}
			}		
		}// userInputs refresh

		
		// gainSlewers
		for (int i = 0; i < MAX_NUM; i++) {
			gainSlewers[i].process(args.sampleTime, i == sel ? 1.0f : 0.0f);
		}
		
		
		// Outputs
		if (OUTS == 1) {
			// 5 to 1 mux
			for (int w = 0; w < WIDTH; w++) {
				int maxInChans = -1;
				for (int i = 0; i < INS; i++) {
					maxInChans = std::max(maxInChans, inputs[IN_INPUTS + i + (INS * w)].getChannels());
				}
				if (outputs[OUT_OUTPUTS + w].getChannels() != maxInChans) {
					outputs[OUT_OUTPUTS + w].setChannels(maxInChans);
				}			
				for (int c = 0; c < maxInChans; c++) {
					float cmix = 0.0f;
					for (int i = 0; i < INS; i++) {
						cmix += inputs[IN_INPUTS + i + (INS * w)].getVoltage(c) * gainSlewers[i].out;
					}
					outputs[OUT_OUTPUTS + w].setVoltage(cmix, c);
				}
			}
		}
		else {
			// 1 to 5 demux
			for (int w = 0; w < WIDTH; w++) {
				int numInChans = inputs[IN_INPUTS + w].getChannels();
				for (int o = 0; o < OUTS; o++) {
					if (outputs[OUT_OUTPUTS + o + (OUTS * w)].getChannels() != numInChans) {
						outputs[OUT_OUTPUTS + o + (OUTS * w)].setChannels(numInChans);
					}			
				}
				for (int o = 0; o < OUTS; o++) {
					for (int c = 0; c < numInChans; c++) {
						outputs[OUT_OUTPUTS + o + (OUTS * w)].setVoltage(
							inputs[IN_INPUTS + w].getVoltage(c) * gainSlewers[o].out, c);
					}
				}
			}
		}
		
		
		// Lights
		if (refresh.processLights()) {			
			// port lights
			for (int i = 0; i < MAX_NUM; i++) {
				lights[SEL_LIGHTS + i].setBrightness(sel == i ? 1.0f : 0.0f);
			}
		}
	}// process()
};


//-----------------------------------------------------------------------------

struct RmSeparatorBg : SvgWidget {
	RmSeparatorBg() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/patchset/title-divider.svg")));
	}
};

template <int INS, int OUTS, int WIDTH>
struct RouteMasterWidget : ModuleWidget {
	SvgPanel* svgPanel;
	TileDisplaySep* nameDisplay = NULL;
	TileDisplayController* labelDisplays[MAX_NUM] = {};
	int8_t defaultColor = 0;// yellow, used when module == NULL
	time_t oldTime = 0;


	struct NameOrLabelValueField : ui::TextField {
		RouteMaster<INS, OUTS, WIDTH>* module = NULL;
		int labelNumber = -1;// -1 means name, 0 to MAX_NUM-1 is for labels

		NameOrLabelValueField(RouteMaster<INS, OUTS, WIDTH>* _module, int _labelNumber) {
			module = _module;
			labelNumber = _labelNumber;
			if (labelNumber < 0) {
				text = module->name;
			}
			else {
				text = module->labels[labelNumber];
			}
			selectAll();
		}

		void step() override {
			// Keep selected
			// APP->event->setSelectedWidget(this);
			// TextField::step();
		}

		void onSelectKey(const event::SelectKey& e) override {
			if (e.action == GLFW_RELEASE){// && (e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_KP_ENTER)) {
				if (labelNumber < 0) {
					module->name = text;
				}
				else {
					module->labels[labelNumber] = text;
				}
				module->updateControllerLabelsRequest = 1;
				if (e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_KP_ENTER) {
					ui::MenuOverlay* overlay = getAncestorOfType<ui::MenuOverlay>();
					overlay->requestDelete();
					e.consume(this);
				}
			}

			if (!e.getTarget())
				TextField::onSelectKey(e);
		}
	};
	
	
	void appendContextMenu(Menu *menu) override {
		RouteMaster<INS, OUTS, WIDTH> *module = (RouteMaster<INS, OUTS, WIDTH>*)(this->module);
		assert(module);

		menu->addChild(new MenuSeparator());
		
		// name menu entry
		menu->addChild(createMenuLabel("Top label:"));
		NameOrLabelValueField* nameValueField = new NameOrLabelValueField(module, -1);
		nameValueField->box.size.x = 100;
		menu->addChild(nameValueField);	

		menu->addChild(createSubmenuItem("Label colour", "", [=](Menu* menu) {
			for (int i = 0; i < numPsColors; i++) {
				menu->addChild(createCheckMenuItem(psColorNames[i], "",
					[=]() {return module->miscSettings.cc4[0] == i;},
					[=]() {module->miscSettings.cc4[0] = i;}
				));
			}
		}));	


		menu->addChild(new MenuSeparator());
		
		menu->addChild(createMenuLabel("Channel names:"));
		
		// get labels from parameter mapping
		menu->addChild(createCheckMenuItem("Get channel names from mappings", "",
			[=]() {return module->miscSettings.cc4[1] != 0x0;},
			[=]() {module->miscSettings.cc4[1] ^= 0x1;}
		));
		
		// labels menu entries
		for (int i = 0; i < MAX_NUM; i++) {
			NameOrLabelValueField* labelsValueField = new NameOrLabelValueField(module, i);
			labelsValueField->box.size.x = 100;
			menu->addChild(labelsValueField);	
		}
	}		
	
	
	RouteMasterWidget(RouteMaster<INS, OUTS, WIDTH> *module) {
		setModule(module);

		// Main panels from Inkscape
        if (INS == 1) {
			if (WIDTH == 1) {
				setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/patchset/RouteMaster1to5.svg")));
			}
			else {
				setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/patchset/RouteMaster1to5stereo.svg")));
			}
		}
		else {
			if (WIDTH == 1) {
				setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/patchset/RouteMaster5to1.svg")));
			}
			else {
				setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/patchset/RouteMaster5to1stereo.svg")));
			}
		}
		svgPanel = (SvgPanel*)getPanel();

		static constexpr float midX = (WIDTH == 1 ? 10.16f : 15.24f);
		static constexpr float lofsx = 6.5f;// light offset x
		static constexpr float lofsy = 6.7f;// label offset y
		static constexpr float leftX = 10.16f;
		static constexpr float rightX = 19.98f;
		static constexpr float jofsy = 17.343f;
		static constexpr float lbmlt = 2.0f;// led-button map-light tweak

		// remote name label
		addChild(nameDisplay = createWidgetCentered<TileDisplaySep>(mm2px(Vec(midX, 11.53f))));
		nameDisplay->text = module ? module->name : defaultName;
		nameDisplay->dispColor = module ? &(module->miscSettings.cc4[0]) : &defaultColor;
	
		float y = 22.02f;
		
		// input ports, lights (when applicable) and labels
		for (int i = 0; i < INS; i++) {
			// ports
			if (WIDTH == 1) {
				addInput(createInputCentered<MmPort>(mm2px(Vec(midX, y)), module, RouteMaster<INS, OUTS, WIDTH>::IN_INPUTS + i));
			}
			else {
				addInput(createInputCentered<MmPort>(mm2px(Vec(leftX, y)), module, RouteMaster<INS, OUTS, WIDTH>::IN_INPUTS + i + 0));
				addInput(createInputCentered<MmPort>(mm2px(Vec(rightX, y)), module, RouteMaster<INS, OUTS, WIDTH>::IN_INPUTS + i + INS));		
			}
			
			// selection led buttons
			if (INS != 1) {
				float lx = (WIDTH == 1 ? midX + lofsx : rightX + lofsx);
				LedButton* lb = createParamCentered<LedButton>(mm2px(Vec(lx, y)), module, RouteMaster<INS, OUTS, WIDTH>::SEL_PARAMS + i);
				addParam(lb);
				lb->box.size.x += lbmlt;
				lb->box.size.y += lbmlt;
				addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(lx, y)), module, RouteMaster<INS, OUTS, WIDTH>::SEL_LIGHTS + i));		
			}
			
			// label
			TileDisplayController* inputLabel;
			addChild(inputLabel = createWidgetCentered<TileDisplayController>(mm2px(Vec(midX, y + lofsy))));
			if (INS != 1) {
				inputLabel->text = string::f("Input %i", i + 1);
				labelDisplays[i] = inputLabel;
			}
			else {
				inputLabel->text = "Input";
			}
			
			y += jofsy;
		}
		
		// output ports, lights (when applicable) and labels
		for (int o = 0; o < OUTS; o++) {
			// ports
			if (WIDTH == 1) {
				addOutput(createOutputCentered<MmPort>(mm2px(Vec(midX, y)), module, RouteMaster<INS, OUTS, WIDTH>::OUT_OUTPUTS + o));
			}
			else {
				addOutput(createOutputCentered<MmPort>(mm2px(Vec(leftX, y)), module, RouteMaster<INS, OUTS, WIDTH>::OUT_OUTPUTS + o + 0));
				addOutput(createOutputCentered<MmPort>(mm2px(Vec(rightX, y)), module, RouteMaster<INS, OUTS, WIDTH>::OUT_OUTPUTS + o + OUTS));
			}

			// selection led buttons
			if (OUTS != 1) {
				float lx = (WIDTH == 1 ? midX + lofsx : rightX + lofsx);
				LedButton* lb = createParamCentered<LedButton>(mm2px(Vec(lx, y)), module, RouteMaster<INS, OUTS, WIDTH>::SEL_PARAMS + o);
				addParam(lb);
				lb->box.size.x += lbmlt;
				lb->box.size.y += lbmlt;
				addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(lx, y)), module, RouteMaster<INS, OUTS, WIDTH>::SEL_LIGHTS + o));		
			}
			
			// label
			TileDisplayController* outputLabel;
			addChild(outputLabel = createWidgetCentered<TileDisplayController>(mm2px(Vec(midX, y + lofsy))));
			if (OUTS != 1) {
				outputLabel->text = string::f("Output %i", o + 1);
				labelDisplays[o] = outputLabel;
			}
			else {
				outputLabel->text = "Output";
			}
			
			y += jofsy;
		}
	}
	

	void step() override {
		if (module) {
			RouteMaster<INS, OUTS, WIDTH>* module = (RouteMaster<INS, OUTS, WIDTH>*)(this->module);
			
			// Update all labels at 1Hz (don't do port tooltips)
			time_t currentTime = time(0);
			if (currentTime != oldTime) {
				oldTime = currentTime;

				if (module->miscSettings.cc4[1] != 0) {
					// allow get labels from mappings
					for (int i = 0; i < MAX_NUM; i++) {
						int paramId = RouteMaster<INS, OUTS, WIDTH>::SEL_PARAMS + i;
						engine::ParamHandle* paramHandle = module ? APP->engine->getParamHandle(module->id, paramId) : NULL;
						if (paramHandle) {
							module->labels[i] = paramHandle->text;
						}
					}
					module->updateControllerLabelsRequest = 1;
				}
			}
			
			// labels (pull from module)
			if (module->updateControllerLabelsRequest != 0) {// pull request from module
				for (int i = 0; i < MAX_NUM; i++) {
					nameDisplay->text = module->name;
					if (labelDisplays[i] != NULL) {
						labelDisplays[i]->text = module->labels[i];
					}
				}
				module->updateControllerLabelsRequest = 0;// all done pulling
			}


		}			
		ModuleWidget::step();
	}// void step()	
};



Model *modelRouteMasterMono5to1 = 
	createModel<RouteMaster<5, 1, 1>, RouteMasterWidget<5, 1, 1>>("RouteMasterMono5to1");
Model *modelRouteMasterStereo5to1 = 
	createModel<RouteMaster<5, 1, 2>, RouteMasterWidget<5, 1, 2>>("RouteMasterStereo5to1");
Model *modelRouteMasterMono1to5 = 
	createModel<RouteMaster<1, 5, 1>, RouteMasterWidget<1, 5, 1>>("RouteMasterMono1to5");
Model *modelRouteMasterStereo1to5 = 
	createModel<RouteMaster<1, 5, 2>, RouteMasterWidget<1, 5, 2>>("RouteMasterStereo1to5");





