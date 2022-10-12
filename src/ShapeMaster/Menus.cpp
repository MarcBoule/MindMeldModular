//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//Based on code from the Fundamental plugin by Andrew Belt
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "Menus.hpp"


// Gain adjust base
struct GainAdjustBaseQuantity : Quantity {
	Channel* channel = NULL;
	float minDb;
	float maxDb;
	  
	GainAdjustBaseQuantity(Channel* _channel, float _minDb, float _maxDb) {
		channel = _channel;
		minDb = _minDb;
		maxDb = _maxDb;
	}
	// void setValue(float value) override {}
	// float getValue() override {}
	float getMinValue() override {return minDb;}
	float getMaxValue() override {return maxDb;}
	float getDefaultValue() override {return 0.0f;}
	float getDisplayValue() override {return getValue();}
	std::string getDisplayValueString() override {
		return channel->getGainAdjustDbText(getDisplayValue());
	}
	void setDisplayValue(float displayValue) override {setValue(displayValue);}
	std::string getLabel() override {return "Gain adjust";}
	std::string getUnit() override {return " dB";}
};


// Display background menu
// --------------------

struct InsertPointItem : MenuItem {
	Shape* shape;
	Vec normPos;

	void onAction(const event::Action &e) override {
		shape->insertPointWithSafetyAndBlock(normPos, true);// with history
	}
};

void createBackgroundMenu(ui::Menu* menu, Shape* shape, Vec normPos) {
	InsertPointItem *addPointItem = createMenuItem<InsertPointItem>("Add node", "");
	addPointItem->shape = shape;
	addPointItem->normPos = normPos;
	menu->addChild(addPointItem);

	// menu->addChild(new MenuSeparator());
}



// Normal point menus
// --------------------

void captureNewTime(std::string* text, Channel* channel, int pt, float length) {
	Shape* shape = channel->getShape();
	Vec ptVec = shape->getPointVect(pt);
	if (channel->getTrigMode() == TM_CV) {
		float newVolts;
		if (std::sscanf(text->c_str(), "%f", &newVolts) == 1) {
			if (channel->getBipolCvMode() != 0) {
				newVolts += 5.0f;
			}
			newVolts /= 10.0f;
			ptVec.x = clamp(newVolts, 0.0f, 1.0f);
			channel->setPointWithSafety(pt, ptVec, -1, -1);
		}		
	}
	else {
		int newMins;
		float newSecs;
		if (std::sscanf(text->c_str(), "%i:%f", &newMins, &newSecs) >= 2) {
			ptVec.x = ((float)newMins * 60.0f + newSecs) / length;
			channel->setPointWithSafety(pt, ptVec, -1, -1);
		}
		else if (std::sscanf(text->c_str(), "%f", &newSecs) >= 1) {
			ptVec.x = newSecs / length;
			channel->setPointWithSafety(pt, ptVec, -1, -1);
		}		
	}
}

void captureNewVolts(std::string* text, Channel* channel, int pt) {
	Shape* shape = channel->getShape();
	Vec ptVec = shape->getPointVect(pt);
	float newVolts;
	if (std::sscanf(text->c_str(), "%f", &newVolts) == 1) {
		if (newVolts > 10.0f) {
			// interpret newVolts as a frequency, and convert it to a pitch CV
			newVolts = std::log(newVolts/dsp::FREQ_C4) / std::log(2.0f);
		}
		ptVec.y = channel->applyInverseRange(newVolts);
		channel->setPointWithSafety(pt, ptVec, -1, -1);
	}
	else {
		newVolts = stringToVoct(text);
		if (newVolts != -100.0f) {
			ptVec.y = channel->applyInverseRange(newVolts);
			channel->setPointWithSafety(pt, ptVec, -1, -1);
		}
	}
}

// code below adapted from ParamWidget::ParamField() by Andrew Belt
struct TimeValueField : ui::TextField {
	Channel* channel;
	int pt;
	float length;
	std::string* voltText;// since we save both time and volts when hitting enter on time

	void step() override {
		// Keep selected
		// APP->event->setSelectedWidget(this);
		// TextField::step();
	}

	void setChannel(Channel* _channel, std::string initText, int _pt, float _length) {
		channel = _channel;
		text = initText;
		pt = _pt;
		length = _length;
		selectAll();
	}

	void onSelectKey(const event::SelectKey& e) override {
		if (e.action == GLFW_PRESS && (e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_KP_ENTER)) {
			Shape* shape = channel->getShape();
			
			// Push DragMiscChange history action (rest is done further down)
			DragMiscChange* h = new DragMiscChange;
			h->shapeSrc = shape;
			h->dragType = DM_POINT;
			h->pt = pt;
			h->oldVec = shape->getPointVect(pt);
			
			captureNewTime(&text, channel, pt, length);
			captureNewVolts(voltText, channel, pt);// must save other text entry also in case user changed it
			
			h->newVec = shape->getPointVect(pt);
			h->name = "update node";
			APP->history->push(h);

			ui::MenuOverlay* overlay = getAncestorOfType<ui::MenuOverlay>();
			overlay->requestDelete();
			e.consume(this);
		}

		if (!e.getTarget())
			TextField::onSelectKey(e);
	}
};



// code below adapted from ParamWidget::ParamField() by Andrew Belt
struct VoltValueField : ui::TextField {
	Channel* channel;
	int pt;
	float length;// needed since must save time also
	std::string* timeText;// since we save both time and volts when hitting enter on volts

	void step() override {
		// Keep selected
		// APP->event->setSelectedWidget(this);
		// TextField::step();
	}

	void setChannel(Channel* _channel, std::string initText, int _pt, float _length) {
		channel = _channel;
		text = initText;
		pt = _pt;
		length = _length;
		selectAll();
	}

	void onSelectKey(const event::SelectKey& e) override {
		if (e.action == GLFW_PRESS && (e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_KP_ENTER)) {
			Shape* shape = channel->getShape();
			
			// Push DragMiscChange history action (rest is done further down)
			DragMiscChange* h = new DragMiscChange;
			h->shapeSrc = shape;
			h->dragType = DM_POINT;
			h->pt = pt;
			h->oldVec = shape->getPointVect(pt);

			captureNewVolts(&text, channel, pt);
			captureNewTime(timeText, channel, pt, length);// must save other text entry also in case user changed it
			
			h->newVec = shape->getPointVect(pt);
			h->name = "update node";
			APP->history->push(h);

			ui::MenuOverlay* overlay = getAncestorOfType<ui::MenuOverlay>();
			overlay->requestDelete();
			e.consume(this);
		}

		if (!e.getTarget())
			TextField::onSelectKey(e);
	}
};



void createPointMenu(ui::Menu* menu, Channel* channel, int pt) {
	Shape* shape = channel->getShape();
	Vec ptVec = shape->getPointVect(pt);
	

	// Horiz.
	float length;
	std::string timeText = "";
	std::string timeLabel = "Horiz.: ";
	if (channel->getTrigMode() == TM_CV) {
		length = ptVec.x * 10.0f;
		if (channel->getBipolCvMode() != 0) {
			length -= 5.0f;
		}
		timeText = string::f("%.4g", length);
		timeLabel.append(timeText).append(" V");
	}
	else {
		#ifdef SM_PRO
		if (channel->isSync()) {
			length = channel->calcLengthSyncTime();
		}
		else 
		#endif
		{
			length = channel->calcLengthUnsyncTime();
		}
		float time = length * ptVec.x;	
		std::string timeText = timeToString(time, false);
		timeLabel.append(timeText).append(" s");
	}
	
	menu->addChild(createMenuLabel(timeLabel));

	TimeValueField* timeValueField = new TimeValueField;
	timeValueField->box.size.x = 100;
	timeValueField->setChannel(channel, timeText, pt, length);
	// timeValueField->voltText = ?;// done below since voltValueField doesn't exist yet
	menu->addChild(timeValueField);


	menu->addChild(new MenuSeparator());


	// Vert.
	float voltRanged = channel->applyRange(ptVec.y);
	std::string voltRangedText = string::f("%.4g", voltRanged);
	
	std::string voltLabel = string::f("Vert.: %s V", voltRangedText.c_str());
	menu->addChild(createMenuLabel(voltLabel));

	if (voltRanged <= 6.25635f && voltRanged >= -3.70943f) {// values here are v/oct for 20kHz and 20Hz respectively 
		float freq = 261.6256f * std::pow(2.0f, voltRanged); 
		bool kHz = freq >= 1e3;
		std::string freqStr = "Hz";
		if (kHz) {
			freq /= 1e3;
			freqStr.insert(0, string::f("%.3f k", freq));
		}
		else {
			freqStr.insert(0, string::f("%.2f ", freq));
		}
		char note[8];
		printNote(voltRanged, note, true);
		std::string volt2Label = string::f("(%s, %s)", freqStr.c_str(), note);
		menu->addChild(createMenuLabel(volt2Label));
	}	

	VoltValueField* voltValueField = new VoltValueField;
	voltValueField->box.size.x = 100;
	voltValueField->setChannel(channel, voltRangedText, pt, length);
	voltValueField->timeText = &timeValueField->text;
	timeValueField->voltText = &voltValueField->text;
	menu->addChild(voltValueField);

	
	menu->addChild(new MenuSeparator());
	
	menu->addChild(createMenuItem("Delete node", "",
		[=]() {shape->deletePointWithBlock(pt, true);}// with history
	));
}



// Control point menus
// --------------------

void myActionCtrlType(Shape* shape, int pt, int8_t setVal) {
	// Push TypeAndCtrlChange history action (rest is done in onDragEnd())
	TypeAndCtrlChange* h = new TypeAndCtrlChange;
	h->shapeSrc = shape;
	h->pt = pt;
	h->oldCtrl = shape->getCtrl(pt);// only type is changing but must grab both since undo/redo will change both
	h->oldType = shape->getType(pt);
	
	shape->setType(pt, setVal);

	h->newCtrl = shape->getCtrl(pt);// only type is changing but must grab both since undo/redo will change both
	h->newType = shape->getType(pt);
	h->name = "change control point";
	APP->history->push(h);
}


void createCtrlMenu(ui::Menu* menu, Shape* shape, int pt) {
	menu->addChild(createCheckMenuItem("Smooth", "",
		[=]() {return shape->getType(pt) == 0;},
		[=]() {myActionCtrlType(shape, pt, 0);}
	));	
	menu->addChild(createCheckMenuItem("S-Shape", "",
		[=]() {return shape->getType(pt) == 1;},
		[=]() {myActionCtrlType(shape, pt, 1);}
	));	

	menu->addChild(new MenuSeparator());

	menu->addChild(createMenuItem("Reset", "",
		[=]() {shape->makeLinear(pt);}// has implicit history
	));
}



// Channel menu
// --------------------

struct GainAdjustVcaQuantity : GainAdjustBaseQuantity {
	GainAdjustVcaQuantity(Channel* channel, float minDb, float maxDb) : GainAdjustBaseQuantity(channel, minDb, maxDb) {};
	void setValue(float value) override {
		float gainInDB = math::clamp(value, getMinValue(), getMaxValue());
		channel->setGainAdjustVca(std::pow(10.0f, gainInDB / 20.0f));
	}
	float getValue() override {
		return channel->getGainAdjustVcaDb();
	}
};
struct GainAdjustVcaSlider : ui::Slider {
	GainAdjustVcaSlider(Channel* channel, float minDb, float maxDb) {
		quantity = new GainAdjustVcaQuantity(channel, minDb, maxDb);
	}
	~GainAdjustVcaSlider() {
		delete quantity;
	}
};


#ifdef SM_PRO
struct SmTriggersItem : MenuItem {
	Channel* channel;
		
	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		
		menu->addChild(createCheckMenuItem("EOC on last cycle only", "",
			[=]() {return channel->getEocOnLastOnly();},
			[=]() {channel->toggleEocOnLastOnly();}
		));	
		menu->addChild(createCheckMenuItem("EOC excluded from OR", "",
			[=]() {return channel->isExcludeEocFromOr();},
			[=]() {channel->toggleExcludeEocFromOr();}
		));	

		menu->addChild(createCheckMenuItem("EOS only after SOS", "",
			[=]() {return channel->getEosOnlyAfterSos();},
			[=]() {channel->toggleEosOnlyAfterSos();}
		));	

		return menu;
	}
};

struct Allow1BarLocksItem : MenuItem {
	Channel* channel;
	bool* running;

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		
		if (*running) {
			menu->addChild(createMenuLabel("[Turn off run]"));
		}			

		menu->addChild(createCheckMenuItem("Quantise to max 1 bar", "",
			[=]() {return channel->isAllow1BarLocks();},
			[=]() {if (!channel->isAllow1BarLocks()) channel->toggleAllow1BarLocks();},
			*running
		));	
		menu->addChild(createCheckMenuItem("Quantise to cycle length", "",
			[=]() {return !channel->isAllow1BarLocks();},
			[=]() {if (channel->isAllow1BarLocks()) channel->toggleAllow1BarLocks();},
			*running
		));	
		
		return menu;
	}
};
#endif

struct ShowTooltipVoltsAsItem : MenuItem {
	Channel* channel;
	
	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		menu->addChild(createCheckMenuItem("Volts (default)", "",
			[=]() {return channel->getShowTooltipVoltsAs() == 1;},
			[=]() {channel->setShowTooltipVoltsAs(1);}
		));	
		menu->addChild(createCheckMenuItem("Frequency", "",
			[=]() {return channel->getShowTooltipVoltsAs() == 0;},
			[=]() {channel->setShowTooltipVoltsAs(0);}
		));	
		menu->addChild(createCheckMenuItem("Note", "",
			[=]() {return channel->getShowTooltipVoltsAs() == 2;},
			[=]() {channel->setShowTooltipVoltsAs(2);}
		));	

		return menu;
	}
};

struct DecoupledFirstAndLastItem : MenuItem {
	Channel* channel;

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		menu->addChild(createCheckMenuItem("Coupled (default)", "",
			[=]() {return !channel->isDecoupledFirstAndLast();},
			[=]() {channel->toggleDecoupledFirstAndLast();}
		));	
		menu->addChild(createCheckMenuItem("Decoupled", "",
			[=]() {return channel->isDecoupledFirstAndLast();},
			[=]() {channel->toggleDecoupledFirstAndLast();}
		));	
		return menu;
	}
};


struct PolySumItem : MenuItem {
	Channel* channel;

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		
		for (int i = 0; i < NUM_POLY_MODES; i++) {			
			menu->addChild(createCheckMenuItem(polyModeNames[i], "",
				[=]() {return channel->getPolyMode() == i;},
				[=]() {channel->channelSettings.cc4[2] = i;},
				channel->getNodeTriggers() != 0
			));	
		}
		return menu;
	}
	
	void step() override {
		disabled = channel->getNodeTriggers() != 0;
		MenuItem::step();
	}
};


struct CopyChanelItem : MenuItem {
	Channel* channelSource;
	
	void onAction(const event::Action &e) override {
		json_t* channelJ = channelSource->dataToJsonChannel(WITH_PARAMS, WITHOUT_PRO_UNSYNC_MATCH, WITH_FULL_SETTINGS);
		json_t* clipboardJ = json_object();		
		json_object_set_new(clipboardJ, "MindMeld-ShapeMaster-Clipboard-Channel", channelJ);
		char* channelClip = json_dumps(clipboardJ, JSON_INDENT(2) | JSON_REAL_PRECISION(9));
		json_decref(clipboardJ);
		glfwSetClipboardString(APP->window->win, channelClip);
		free(channelClip);
	}
};

struct PasteChanelItem : MenuItem {
	Channel* channelDestination;

	void onAction(const event::Action &e) override {
		// Push ChannelChange history action (rest is done below)
		ChannelChange* h = new ChannelChange;
		h->channelSrc = channelDestination;
		h->oldJson = channelDestination->dataToJsonChannel(WITH_PARAMS, WITHOUT_PRO_UNSYNC_MATCH, WITH_FULL_SETTINGS);
		

		bool successPaste = false;
		const char* channelClip = glfwGetClipboardString(APP->window->win);
		if (!channelClip) {
			WARN("IOP error getting clipboard string");
		}
		else {
			json_error_t error;
			json_t* clipboardJ = json_loads(channelClip, 0, &error);
			if (!clipboardJ) {
				WARN("IOP error json parsing clipboard");
			}
			else {
				DEFER({json_decref(clipboardJ);});
				// MindMeld-ShapeMaster-Clipboard-Channel
				json_t* channelJ = json_object_get(clipboardJ, "MindMeld-ShapeMaster-Clipboard-Channel");
				if (!channelJ) {
					WARN("IOP error no MindMeld-ShapeMaster-Clipboard-Channel present in clipboard");
				}
				else {
					channelDestination->dataFromJsonChannel(channelJ, WITH_PARAMS, ISNOT_DIRTY_CACHE_LOAD, WITH_FULL_SETTINGS);
					successPaste = true;
				}
			}	
		}

		if (successPaste) {
			h->newJson = channelDestination->dataToJsonChannel(WITH_PARAMS, WITHOUT_PRO_UNSYNC_MATCH, WITH_FULL_SETTINGS);
			h->name = "paste channel";
			APP->history->push(h);
		}
		else {
			delete h;// h->oldJson will be automatically json_decref'ed by desctructor
		}
	}
};

struct InitializeChanelItem : MenuItem {
	Channel* channel;

	void onAction(const event::Action &e) override {
		// Push ChannelChange history action (rest is done in onDragEnd())
		ChannelChange* h = new ChannelChange;
		h->channelSrc = channel;
		h->oldJson = channel->dataToJsonChannel(WITH_PARAMS, WITHOUT_PRO_UNSYNC_MATCH, WITH_FULL_SETTINGS);

		channel->onReset(true);
		
		h->newJson = channel->dataToJsonChannel(WITH_PARAMS, WITHOUT_PRO_UNSYNC_MATCH, WITH_FULL_SETTINGS);
		h->name = "initialize channel";
		APP->history->push(h);
	}
};


struct ScopeVcaPolySelItem : MenuItem {
	int8_t *srcScopeVcaPolySelItem;
	Channel* channel;

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		
		int maxChan = std::max(channel->getVcaPostSize(), channel->getVcaPreSize());
		
		menu->addChild(createCheckMenuItem("Poly-chans 1+2", "",
			[=]() {return *srcScopeVcaPolySelItem == 16;},
			[=]() {*srcScopeVcaPolySelItem = 16;},
			maxChan <= 1 || (channel->getNodeTriggers() != 0)
		));	
		
		for (int i = 0; i < 16; i++) {
			menu->addChild(createCheckMenuItem(string::f("Poly-chan %i", i + 1), "",
				[=]() {return *srcScopeVcaPolySelItem == i;},
				[=]() {*srcScopeVcaPolySelItem = i;},
				i >= maxChan || (channel->getNodeTriggers() != 0)
			));
		}

		return menu;
	}

	void step() override {
		disabled = channel->getNodeTriggers() != 0;
		MenuItem::step();
	}
};


struct NodeTriggersItem : MenuItem {
	Channel* channel;
	
	// Node triggers duration quantity and slider
	struct NodeTrigDurationQuantity : Quantity {
		Channel* channel = NULL;
		
		NodeTrigDurationQuantity(Channel* _channel) {
			channel = _channel;
		}
		void setValue(float value) override {
			channel->setNodeTrigDuration(math::clamp(value, getMinValue(), getMaxValue()));
		}
		float getValue() override {
			return channel->getNodeTrigDuration();
		}
		float getMinValue() override {return 0.001f;}
		float getMaxValue() override {return 0.1f;}
		float getDefaultValue() override {return Channel::DEFAULT_NODETRIG_DURATION;}
		float getDisplayValue() override {return getValue();}
		std::string getDisplayValueString() override {
			if (channel->getNodeTriggers() != 1) {
				return "N/A";
			}
			return string::f("%i", (int)(getValue() * 1000.0f + 0.5f));
		}
		void setDisplayValue(float displayValue) override {setValue(displayValue);}
		std::string getLabel() override {
			return "Node triggers";}
		std::string getUnit() override {
			if (channel->getNodeTriggers() != 1) {
				return "";
			}
			return "ms";
		}
	};
	struct NodeTrigDurationSlider : ui::Slider {
		NodeTrigDurationSlider(Channel* channel) {
			quantity = new NodeTrigDurationQuantity(channel);
		}
		~NodeTrigDurationSlider() {
			delete quantity;
		}
	};	
	
	
	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		
		menu->addChild(createCheckMenuItem("VCA (default)", "",
			[=]() {return channel->getNodeTriggers() == 0;},
			[=]() {channel->setNodeTriggers(0);}
		));	
		
		#ifdef SM_PRO
		menu->addChild(createCheckMenuItem("ShapeTracker (ST)", "",
			[=]() {return channel->getNodeTriggers() == 2;},
			[=]() {channel->setNodeTriggers(2);}
		));	
		#endif

		menu->addChild(createCheckMenuItem("Node triggers", "",
			[=]() {return channel->getNodeTriggers() == 1;},
			[=]() {channel->setNodeTriggers(1);}
		));	

		NodeTrigDurationSlider *nodetrigSlider = new NodeTrigDurationSlider(channel);
		nodetrigSlider->box.size.x = 200.0f;
		menu->addChild(nodetrigSlider);

		return menu;
	}
};


struct ChanColorItem : MenuItem {
	int8_t *srcChanColor;

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		for (int i = 0; i < numChanColors; i++) {
			menu->addChild(createCheckMenuItem(chanColorNames[i], "",
				[=]() {return *srcChanColor == i;},
				[=]() {*srcChanColor = i;}
			));	
		}
		return menu;
	}
};

// code below adapted from ParamWidget::ParamField() by Andrew Belt
struct ChanNameField : ui::TextField {
	Channel* channel;

	void step() override {
		// Keep selected
		APP->event->setSelectedWidget(this);
		TextField::step();
	}

	void setChannel(Channel* _channel) {
		channel = _channel;
		text = channel->getChanName();
		selectAll();
	}

	void onSelectKey(const event::SelectKey& e) override {
		if (e.action == GLFW_PRESS) {
			if (e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_KP_ENTER) {
				ui::MenuOverlay* overlay = getAncestorOfType<ui::MenuOverlay>();
				overlay->requestDelete();
				e.consume(this);
			}
		}
		else if (e.action == GLFW_RELEASE) {
			channel->setChanName(text);
		}

		if (!e.getTarget())
			TextField::onSelectKey(e);
	}
};

void createChannelMenu(ui::Menu* menu, Channel* channels, int chan, PackedBytes4* miscSettings2GlobalSrc, bool trigExpPresent, bool* running) {
	ChanNameField* chanNameField = new ChanNameField;
	chanNameField->box.size.x = 100;
	chanNameField->setChannel(&(channels[chan]));
	menu->addChild(chanNameField);

	menu->addChild(createMenuLabel("Settings"));

	GainAdjustVcaSlider *vcaGainAdjustSlider = new GainAdjustVcaSlider(&(channels[chan]), -20.0f, 20.0f);
	vcaGainAdjustSlider->box.size.x = 200.0f;
	menu->addChild(vcaGainAdjustSlider);

	menu->addChild(createCheckMenuItem("T/G, SC - Retrigger", "",
		[=]() {return channels[chan].getAllowRetrig();},
		[=]() {channels[chan].toggleAllowRetrig();}
	));	

	menu->addChild(createCheckMenuItem("CTRL - Restart", "",
		[=]() {return channels[chan].getGateRestart();},
		[=]() {channels[chan].toggleGateRestart();}
	));	

	#ifdef SM_PRO
	menu->addChild(createCheckMenuItem("Playhead never jumps", "",
		[=]() {return channels[chan].getPlayheadNeverJumps();},
		[=]() {channels[chan].togglePlayheadNeverJumps();}
	));	

	Allow1BarLocksItem *allow1barItem = createMenuItem<Allow1BarLocksItem>("Sync lock quantising", RIGHT_ARROW);
	allow1barItem->channel = &(channels[chan]);
	allow1barItem->running = running;
	allow1barItem->disabled = *running;
	menu->addChild(allow1barItem);
	#endif

	DecoupledFirstAndLastItem *decFlItem = createMenuItem<DecoupledFirstAndLastItem>("First/last nodes", RIGHT_ARROW);
	decFlItem->channel = &(channels[chan]);
	menu->addChild(decFlItem);

	PolySumItem *sumSteItem = createMenuItem<PolySumItem>("Poly VCA summing", "");// arrow done in PolySumItem
	sumSteItem->channel = &(channels[chan]);
	menu->addChild(sumSteItem);

	ScopeVcaPolySelItem *polyVcaItem = createMenuItem<ScopeVcaPolySelItem>("Poly VCA in scope select", RIGHT_ARROW);
	polyVcaItem->srcScopeVcaPolySelItem = &(channels[chan].channelSettings2.cc4[0]);
	polyVcaItem->channel = &(channels[chan]);
	menu->addChild(polyVcaItem);

	NodeTriggersItem *nodetrigItem = createMenuItem<NodeTriggersItem>("VCA output", RIGHT_ARROW);
	nodetrigItem->channel = &(channels[chan]);
	menu->addChild(nodetrigItem);

	menu->addChild(createCheckMenuItem("Force 0V CV when stopped", "",
		[=]() {return channels[chan].isForced0VWhenStopped();},
		[=]() {channels[chan].toggleForced0VWhenStopped();}
	));	

	menu->addChild(createCheckMenuItem("Use sustain as channel reset", "",
		[=]() {return channels[chan].isChannelResetOnSustain();},
		[=]() {channels[chan].toggleChannelResetOnSustain();}
	));	

	#ifdef SM_PRO
	if (trigExpPresent) {
		SmTriggersItem *smTrigItem = createMenuItem<SmTriggersItem>("SM-Triggers", RIGHT_ARROW);
		smTrigItem->channel = &(channels[chan]);
		menu->addChild(smTrigItem);
	}
	#endif

	ShowTooltipVoltsAsItem *tthItem = createMenuItem<ShowTooltipVoltsAsItem>("Node tooltip when shown", RIGHT_ARROW);
	tthItem->channel = &(channels[chan]);
	menu->addChild(tthItem);

	ChanColorItem *chanColItem = createMenuItem<ChanColorItem>("Channel colour", RIGHT_ARROW);
	chanColItem->srcChanColor = &(channels[chan].channelSettings.cc4[1]);
	menu->addChild(chanColItem);

	if (miscSettings2GlobalSrc->cc4[0] >= 2) {
		InvShadowItem *invShadItem = createMenuItem<InvShadowItem>("Shadow", RIGHT_ARROW);
		invShadItem->srcInvShadow = &(channels[chan].channelSettings.cc4[0]);
		menu->addChild(invShadItem);
	}


	menu->addChild(new MenuSeparator());

	menu->addChild(createMenuLabel("Actions"));

	CopyChanelItem *copyChanItem = createMenuItem<CopyChanelItem>("Copy channel", "");
	copyChanItem->channelSource = &(channels[chan]);
	// copyChanItem->channelCopyPasteCache = channelCopyPasteCache;
	menu->addChild(copyChanItem);
	
	PasteChanelItem *pasteChanItem = createMenuItem<PasteChanelItem>("Paste channel", "");
	pasteChanItem->channelDestination = &(channels[chan]);
	// pasteChanItem->channelCopyPasteCache = channelCopyPasteCache;
	menu->addChild(pasteChanItem);
	
	InitializeChanelItem *initChanItem = createMenuItem<InitializeChanelItem>("Initialize channel", "");
	initChanItem->channel = &(channels[chan]);
	menu->addChild(initChanItem);
}



// Module's context menu
// --------------------

// none



// Sidechain settings menu
// --------------------


// Gain adjust menu item
struct GainAdjustScQuantity : GainAdjustBaseQuantity {
	GainAdjustScQuantity(Channel* channel, float minDb, float maxDb) : GainAdjustBaseQuantity(channel, minDb, maxDb) {};
	void setValue(float value) override {
		float gainInDB = math::clamp(value, getMinValue(), getMaxValue());
		channel->setGainAdjustSc(std::pow(10.0f, gainInDB / 20.0f));
	}
	float getValue() override {
		return channel->getGainAdjustScDb();
	}
};
struct GainAdjustScSlider : ui::Slider {
	GainAdjustScSlider(Channel* channel, float minDb, float maxDb) {
		quantity = new GainAdjustScQuantity(channel, minDb, maxDb);
	}
	~GainAdjustScSlider() {
		delete quantity;
	}
};



// HPF filter cutoff menu item

struct HPFCutoffQuantity : Quantity {
	Channel* channel = NULL;
	
	HPFCutoffQuantity(Channel* _channel) {
		channel = _channel;
	}
	void setValue(float value) override {
		channel->setHPFCutoffSqFreq(math::clamp(value, getMinValue(), getMaxValue()));
	}
	float getValue() override {
		return channel->getHPFCutoffSqFreq();
	}
	float getMinValue() override {return Channel::SC_HPF_SQFREQ_MIN;}
	float getMaxValue() override {return Channel::SC_HPF_SQFREQ_MAX;}
	float getDefaultValue() override {return Channel::SC_HPF_SQFREQ_DEF;}
	float getDisplayValue() override {return std::pow(getValue(), Channel::SCF_SCALING_EXP);}
	std::string getDisplayValueString() override {
		return channel->getHPFCutoffFreqText();
	}
	void setDisplayValue(float displayValue) override {setValue(displayValue);}
	std::string getLabel() override {return "HPF Cutoff";}
	std::string getUnit() override {
		if (channel->isHpfCutoffActive()) {
			float dispVal = getDisplayValue();
			if (dispVal >= 1e3f) {
				return " kHz";
			}
			return " Hz";
		}
		else {
			return "";
		}
	}
};

struct HPFCutoffSlider : ui::Slider {
	HPFCutoffSlider(Channel* channel) {
		quantity = new HPFCutoffQuantity(channel);
	}
	~HPFCutoffSlider() {
		delete quantity;
	}
};



// LPF filter cutoff menu item

struct LPFCutoffQuantity : Quantity {
	Channel* channel = NULL;
	
	LPFCutoffQuantity(Channel* _channel) {
		channel = _channel;
	}
	void setValue(float value) override {
		channel->setLPFCutoffSqFreq(math::clamp(value, getMinValue(), getMaxValue()));
	}
	float getValue() override {
		return channel->getLPFCutoffSqFreq();
	}
	float getMinValue() override {return Channel::SC_LPF_SQFREQ_MIN;}
	float getMaxValue() override {return Channel::SC_LPF_SQFREQ_MAX;}
	float getDefaultValue() override {return Channel::SC_LPF_SQFREQ_DEF;}
	float getDisplayValue() override {return std::pow(getValue(), Channel::SCF_SCALING_EXP);}
	std::string getDisplayValueString() override {
		return channel->getLPFCutoffFreqText();
	}
	void setDisplayValue(float displayValue) override {setValue(displayValue);}
	std::string getLabel() override {return "LPF Cutoff";}
	std::string getUnit() override {
		if (channel->isLpfCutoffActive()) {
			float dispVal = getDisplayValue();
			if (dispVal >= 1e3f) {
				return " kHz";
			}
			return " Hz";
		}
		else {
			return "";
		}
	}
};

struct LPFCutoffSlider : ui::Slider {
	LPFCutoffSlider(Channel* channel) {
		quantity = new LPFCutoffQuantity(channel);
	}
	~LPFCutoffSlider() {
		delete quantity;
	}
};



// Hysteresis menu item

struct HysteresisQuantity : Quantity {
	Channel* channel = NULL;
	
	HysteresisQuantity(Channel* _channel) {
		channel = _channel;
	}
	void setValue(float value) override {
		channel->setHysteresis(math::clamp(value, getMinValue(), getMaxValue()));
	}
	float getValue() override {
		return channel->getHysteresis();
	}
	float getMinValue() override {return PlayHead::HYSTERESIS_MIN;}
	float getMaxValue() override {return PlayHead::HYSTERESIS_MAX;}
	float getDefaultValue() override {return PlayHead::DEFAULT_HYSTERESIS;}
	float getDisplayValue() override {return getValue();}
	std::string getDisplayValueString() override {
		return channel->getHysteresisText();
	}
	void setDisplayValue(float displayValue) override {setValue(displayValue);}
	std::string getLabel() override {return "Hysteresis";}
	std::string getUnit() override {
		return " %";
	}
};

struct HysteresisSlider : ui::Slider {
	HysteresisSlider(Channel* channel) {
		quantity = new HysteresisQuantity(channel);
	}
	~HysteresisSlider() {
		delete quantity;
	}
};


// Hold Off menu item

struct HoldOffQuantity : Quantity {
	Channel* channel = NULL;
	
	HoldOffQuantity(Channel* _channel) {
		channel = _channel;
	}
	void setValue(float value) override {
		channel->setHoldOff(math::clamp(value, getMinValue(), getMaxValue()));
	}
	float getValue() override {
		return channel->getHoldOff();
	}
	float getMinValue() override {return PlayHead::HOLDOFF_MIN;}
	float getMaxValue() override {return PlayHead::HOLDOFF_MAX;}
	float getDefaultValue() override {return PlayHead::DEFAULT_HOLDOFF;}
	float getDisplayValue() override {return getValue();}
	std::string getDisplayValueString() override {
		return channel->getHoldOffText();
	}
	void setDisplayValue(float displayValue) override {setValue(displayValue);}
	std::string getLabel() override {return "Hold off";}
	std::string getUnit() override {
		if (getDisplayValue() < 1.0f) {
			return " ms";
		}
		return " s";
	}
};

struct HoldOffSlider : ui::Slider {
	HoldOffSlider(Channel* channel) {
		quantity = new HoldOffQuantity(channel);
	}
	~HoldOffSlider() {
		delete quantity;
	}
};


// Sensitivity menu item

struct SensitivityQuantity : Quantity {
	Channel* channel = NULL;
	
	SensitivityQuantity(Channel* _channel) {
		channel = _channel;
	}
	void setValue(float value) override {
		channel->setSensitivity(math::clamp(value, getMinValue(), getMaxValue()));
	}
	float getValue() override {
		return channel->getSensitivity();
	}
	float getMinValue() override {return Channel::SENSITIVITY_MIN;}
	float getMaxValue() override {return Channel::SENSITIVITY_MAX;}
	float getDefaultValue() override {return Channel::DEFAULT_SENSITIVITY;}
	float getDisplayValue() override {return getValue();}
	std::string getDisplayValueString() override {
		return channel->getSensitivityText(getDisplayValue());
	}
	void setDisplayValue(float displayValue) override {setValue(displayValue);}
	std::string getLabel() override {return "Sensitivity";}
	std::string getUnit() override {
		return " %";
	}
};

struct SensitivitySlider : ui::Slider {
	SensitivitySlider(Channel* channel) {
		quantity = new SensitivityQuantity(channel);
	}
	~SensitivitySlider() {
		delete quantity;
	}
};

void createSidechainSettingsMenu(Channel* channel) {
	ui::Menu *menu = createMenu();

	menu->addChild(createCheckMenuItem("Use VCA input", "",
		[=]() {return channel->isSidechainUseVca();},
		[=]() {channel->toggleSidechainUseVca();}
	));	

	GainAdjustScSlider *scGainAdjustSlider = new GainAdjustScSlider(channel, -20.0f, 20.0f);
	scGainAdjustSlider->box.size.x = 200.0f;
	menu->addChild(scGainAdjustSlider);

	HPFCutoffSlider *auxHPFAdjustSlider = new HPFCutoffSlider(channel);
	auxHPFAdjustSlider->box.size.x = 200.0f;
	menu->addChild(auxHPFAdjustSlider);

	LPFCutoffSlider *auxLPFAdjustSlider = new LPFCutoffSlider(channel);
	auxLPFAdjustSlider->box.size.x = 200.0f;
	menu->addChild(auxLPFAdjustSlider);


	menu->addChild(new MenuSeparator());

	menu->addChild(createCheckMenuItem("Low range trigger level", "",
		[=]() {return channel->isSidechainLowTrig();},
		[=]() {channel->toggleSidechainLowTrig();}
	));	

	HysteresisSlider *hysteresisSlider = new HysteresisSlider(channel);
	hysteresisSlider->box.size.x = 200.0f;
	menu->addChild(hysteresisSlider);

	HoldOffSlider *holdOffSlider = new HoldOffSlider(channel);
	holdOffSlider->box.size.x = 200.0f;
	menu->addChild(holdOffSlider);

	SensitivitySlider *sensitivitySlider = new SensitivitySlider(channel);
	sensitivitySlider->box.size.x = 200.0f;
	menu->addChild(sensitivitySlider);
}



// Display menus (Randon, Snap and Range)
// --------------------

// Random menu item

struct NumNodeRangeQuantity : Quantity {
	float* mainNum = NULL;
	float* otherNum = NULL;
	float defaultVal = 5.0f;
	bool isMin = true;
	int8_t* deltaMode = NULL;
	
	NumNodeRangeQuantity(float* _mainNum, float* _otherNum, float _defaultVal, bool _isMin, int8_t* _deltaMode) {
		mainNum = _mainNum;
		otherNum = _otherNum;
		defaultVal = _defaultVal;
		isMin = _isMin;
		deltaMode = _deltaMode;
	}
	void setValue(float value) override {
		if (*deltaMode == 0) {
			*mainNum = math::clamp(value, getMinValue(), getMaxValue());
			if (isMin) {
				*otherNum = std::fmax(*mainNum, *otherNum);
			}
			else {
				*otherNum = std::fmin(*mainNum, *otherNum);
			}
		}
	}
	float getValue() override {
		return *mainNum;
	}
	float getMinValue() override {return RandomSettings::RAND_NODES_MIN;}
	float getMaxValue() override {return RandomSettings::RAND_NODES_MAX;}
	float getDefaultValue() override {return defaultVal;}
	float getDisplayValue() override {return getValue();}
	std::string getDisplayValueString() override {
		if (*deltaMode != 0) 
			return "--";
		return string::f("%i", (int)(*mainNum + 0.5f));
	}
	void setDisplayValue(float displayValue) override {setValue(displayValue);}
	std::string getLabel() override {
		if (*deltaMode != 0) 
			return "N/A";
		if (isMin) 
			return "Min segments";
		return "Max segments";
	}
	std::string getUnit() override {
		return "";
	}
};

struct NumNodeRangeSlider : ui::Slider {
	NumNodeRangeSlider(float* mainNum, float* otherNum, float defaultVal, bool isMin, int8_t* deltaMode) {
		quantity = new NumNodeRangeQuantity(mainNum, otherNum, defaultVal, isMin, deltaMode);
	}
	~NumNodeRangeSlider() {
		delete quantity;
	}
};


struct RandCtrlQuantity : Quantity {
	float* ctrlMax = NULL;
	int8_t* deltaMode = NULL;
	
	RandCtrlQuantity(float* _ctrlMax, int8_t* _deltaMode) {
		ctrlMax = _ctrlMax;
		deltaMode = _deltaMode;
	}
	void setValue(float value) override {
		if (*deltaMode == 0) {
			*ctrlMax = math::clamp(value, getMinValue(), getMaxValue());
		}
	}
	float getValue() override {
		return *ctrlMax;
	}
	float getMinValue() override {return 0.0f;}
	float getMaxValue() override {return 100.0f;}
	float getDefaultValue() override {return RandomSettings::RAND_CTRL_DEF;}
	float getDisplayValue() override {return getValue();}
	std::string getDisplayValueString() override {
		if (*deltaMode != 0) 
			return "--";
		return string::f("%.1f", *ctrlMax);
	}
	void setDisplayValue(float displayValue) override {setValue(displayValue);}
	std::string getLabel() override {
		if (*deltaMode != 0) 
			return "N/A";
		return "Curve max";
	}
	std::string getUnit() override {
		if (*deltaMode != 0) 
			return "";
		return "%";
	}
};

struct CtrlMaxSlider : ui::Slider {
	CtrlMaxSlider(float* ctrlMax, int8_t* deltaMode) {
		quantity = new RandCtrlQuantity(ctrlMax, deltaMode);
	}
	~CtrlMaxSlider() {
		delete quantity;
	}
};


struct ZeroOrMaxQuantity : Quantity {
	float* thisV = NULL;
	float* otherV = NULL;
	bool isZero = true;
	int8_t* deltaMode = NULL;
	
	ZeroOrMaxQuantity(float* _thisV, float* _otherV, bool _isZero, int8_t* _deltaMode) {
		thisV = _thisV;
		otherV = _otherV;
		isZero = _isZero;
		deltaMode = _deltaMode;
	}
	void setValue(float value) override {
		if (*deltaMode == 0) {
			*thisV = math::clamp(value, getMinValue(), getMaxValue());
			if (*otherV > (100.0f - *thisV)) {
				*otherV = 100.0f - *thisV;
			}
		}
	}
	float getValue() override {
		return *thisV;
	}
	float getMinValue() override {return 0.0f;}
	float getMaxValue() override {return 100.0f;}
	float getDefaultValue() override {return RandomSettings::RAND_ZERO_DEF;}
	float getDisplayValue() override {return getValue();}
	std::string getDisplayValueString() override {
		if (*deltaMode != 0) 
			return "--";
		return string::f("%.1f", *thisV);
	}
	void setDisplayValue(float displayValue) override {setValue(displayValue);}
	std::string getLabel() override {
		if (*deltaMode != 0) 
			return "N/A";
		if (isZero) 
			return "0V ratio";
		return "MaxV ratio";
	}
	std::string getUnit() override {
		if (*deltaMode != 0) 
			return "";
		return "%";
	}
};
struct ZeroOrMaxSlider : ui::Slider {
	ZeroOrMaxSlider(float* _thisV, float* _otherV, bool _isZero, int8_t* _deltaMode) {
		quantity = new ZeroOrMaxQuantity(_thisV, _otherV, _isZero, _deltaMode);
	}
	~ZeroOrMaxSlider() {
		delete quantity;
	}
};


struct DeltaChangeQuantity : Quantity {
	float* deltaChange = NULL;
	int8_t* deltaMode = NULL;
	
	DeltaChangeQuantity(float* _deltaChange, int8_t* _deltaMode) {
		deltaChange = _deltaChange;
		deltaMode = _deltaMode;
	}
	void setValue(float value) override {
		if (*deltaMode != 0) {
			*deltaChange = math::clamp(value, getMinValue(), getMaxValue());
		}
	}
	float getValue() override {
		return *deltaChange;
	}
	float getMinValue() override {return 0.0f;}
	float getMaxValue() override {return 100.0f;}
	float getDefaultValue() override {return RandomSettings::RAND_DELTA_CHANGE_DEF;}
	float getDisplayValue() override {return getValue();}
	std::string getDisplayValueString() override {
		if (*deltaMode == 0) 
			return "--";
		return string::f("%.1f", *deltaChange);
	}
	void setDisplayValue(float displayValue) override {setValue(displayValue);}
	std::string getLabel() override {
		if (*deltaMode == 0) 
			return "N/A";
		return "Range";
	}
	std::string getUnit() override {
		if (*deltaMode == 0) 
			return "";
		return "%";
	}
};
struct DeltaChangeSlider : ui::Slider {
	DeltaChangeSlider(float* deltaChange, int8_t* deltaMode) {
		quantity = new DeltaChangeQuantity(deltaChange, deltaMode);
	}
	~DeltaChangeSlider() {
		delete quantity;
	}
};


struct DeltaNodesQuantity : Quantity {
	float* deltaNodes = NULL;
	int8_t* deltaMode = NULL;
	
	DeltaNodesQuantity(float* _deltaNodes, int8_t* _deltaMode) {
		deltaNodes = _deltaNodes;
		deltaMode = _deltaMode;
	}
	void setValue(float value) override {
		if (*deltaMode != 0) {
			*deltaNodes = math::clamp(value, getMinValue(), getMaxValue());
		}
	}
	float getValue() override {
		return *deltaNodes;
	}
	float getMinValue() override {return 0.0f;}
	float getMaxValue() override {return 100.0f;}
	float getDefaultValue() override {return RandomSettings::RAND_DELTA_NODES_DEF;}
	float getDisplayValue() override {return getValue();}
	std::string getDisplayValueString() override {
		if (*deltaMode == 0) 
			return "--";
		return string::f("%.1f", *deltaNodes);
	}
	void setDisplayValue(float displayValue) override {setValue(displayValue);}
	std::string getLabel() override {
		if (*deltaMode == 0) 
			return "N/A";
		return "Segments";
	}
	std::string getUnit() override {
		if (*deltaMode == 0) 
			return "";
		return "%";
	}
};
struct DeltaNodesSlider : ui::Slider {
	DeltaNodesSlider(float* deltaNodes, int8_t* deltaMode) {
		quantity = new DeltaNodesQuantity(deltaNodes, deltaMode);
	}
	~DeltaNodesSlider() {
		delete quantity;
	}
};


struct RandomNoteItem : MenuItem {
	RandomSettings* randomSettings;
	
	struct RandomNoteSubItem : MenuItem {
		RandomSettings* randomSettings;
		int key;
		
		void onAction(const event::Action &e) override {
			randomSettings->toggleScaleKey(key);
			e.unconsume();
		}
		void step() override {
			rightText = CHECKMARK(randomSettings->getScaleKey(key));
			MenuItem::step();
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		
		for (int i = 0; i < 12; i++) {
			std::string noteText = string::f("%c", noteLettersSharp[i]);
			if (isBlackKey[i] != 0) {
				noteText.insert(0, "--- ");
				noteText.append("#");
			}
			else {
				noteText.insert(0, "- ");
			}
			RandomNoteSubItem *rndNote0Item = createMenuItem<RandomNoteSubItem>(noteText, "");// rightText set in RandomNoteSubItem
			rndNote0Item->randomSettings = randomSettings;
			rndNote0Item->key = i;
			menu->addChild(rndNote0Item);	
		}
		
		return menu;
	}
};

struct VerticalOnlySubItem : MenuItem {
	RandomSettings* randomSettings;
	
	void onAction(const event::Action &e) override {
		randomSettings->deltaMode ^= 0x1;
		e.consume(this);// don't allow ctrl-click to keep menu open
	}
};


void addRandomMenu(Menu* menu, Channel* channel) {
	RandomSettings* randomSettings = channel->getRandomSettings();
		
	menu->addChild(createMenuItem("Randomise", "",
		[=]() {channel->randomizeShape(true);}
	));

	menu->addChild(new MenuSeparator());

	VerticalOnlySubItem *vonlyItem = createMenuItem<VerticalOnlySubItem>("Vertical only", CHECKMARK(randomSettings->deltaMode != 0));
	vonlyItem->randomSettings = randomSettings;
	menu->addChild(vonlyItem);
	menu->addChild(new MenuSeparator());
	
	menu->addChild(createMenuLabel("Randomization settings:"));
	
	if (randomSettings->deltaMode == 0) {
		// normal mode
	
		NumNodeRangeSlider *nodeRange1Slider = new NumNodeRangeSlider(&(randomSettings->numNodesMin), &(randomSettings->numNodesMax), RandomSettings::RAND_NODES_MIN_DEF, true, &(randomSettings->deltaMode));
		nodeRange1Slider->box.size.x = 200.0f;
		menu->addChild(nodeRange1Slider);
		
		NumNodeRangeSlider *nodeRange2Slider = new NumNodeRangeSlider(&(randomSettings->numNodesMax), &(randomSettings->numNodesMin), RandomSettings::RAND_NODES_MAX_DEF, false, &(randomSettings->deltaMode));
		nodeRange2Slider->box.size.x = 200.0f;
		menu->addChild(nodeRange2Slider);
		
		CtrlMaxSlider *ctrMaxSlider = new CtrlMaxSlider(&(randomSettings->ctrlMax), &(randomSettings->deltaMode));
		ctrMaxSlider->box.size.x = 200.0f;
		menu->addChild(ctrMaxSlider);
		
		ZeroOrMaxSlider *zeroVSlider = new ZeroOrMaxSlider(&(randomSettings->zeroV), &(randomSettings->maxV), true, &(randomSettings->deltaMode));
		zeroVSlider->box.size.x = 200.0f;
		menu->addChild(zeroVSlider);
		
		ZeroOrMaxSlider *maxVSlider = new ZeroOrMaxSlider(&(randomSettings->maxV), &(randomSettings->zeroV), false, &(randomSettings->deltaMode));
		maxVSlider->box.size.x = 200.0f;
		menu->addChild(maxVSlider);
		
		menu->addChild(createCheckMenuItem("Stepped", "",
			[=]() {return randomSettings->stepped != 0;},
			[=]() {randomSettings->stepped ^= 0x1; randomSettings->ctrlMax = randomSettings->stepped ? 0.0f : 100.0f;}
		));	

		menu->addChild(createCheckMenuItem("Lock to Grid-X", "",
			[=]() {return randomSettings->grid != 0;},
			[=]() {randomSettings->grid ^= 0x1;}
		));	
	}
	else {
		// delta mode (aka vertical only mode)
		
		DeltaNodesSlider *delt2Slider = new DeltaNodesSlider(&(randomSettings->deltaNodes), &(randomSettings->deltaMode));
		delt2Slider->box.size.x = 200.0f;
		menu->addChild(delt2Slider);

		DeltaChangeSlider *deltSlider = new DeltaChangeSlider(&(randomSettings->deltaChange), &(randomSettings->deltaMode));
		deltSlider->box.size.x = 200.0f;
		menu->addChild(deltSlider);
	}
	
	menu->addChild(createCheckMenuItem("Quantized", "",
		[=]() {return randomSettings->quantized != 0;},
		[=]() {randomSettings->quantized ^= 0x1;}
	));	
	
	RandomNoteItem *rndNoteItem = createMenuItem<RandomNoteItem>("Quantization scale", RIGHT_ARROW);
	rndNoteItem->randomSettings = randomSettings;
	menu->addChild(rndNoteItem);
}

// Snap menu item

// code below adapted from ParamWidget::ParamField() by Andrew Belt
struct SnapValueField : ui::TextField {
	Channel* channel;

	void step() override {
		// Keep selected
		APP->event->setSelectedWidget(this);
		TextField::step();
	}

	void setChannel(Channel* _channel) {
		channel = _channel;
		text = string::f("%i", channel->getGridX());
		selectAll();
	}

	void onSelectKey(const event::SelectKey& e) override {
		if (e.action == GLFW_PRESS && (e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_KP_ENTER)) {
			int newGridX = 16;
			int n = std::sscanf(text.c_str(), "%i", &newGridX);
			if (n >= 1) {
				channel->setGridX(clamp(newGridX, 2, 128), true);
			}
			ui::MenuOverlay* overlay = getAncestorOfType<ui::MenuOverlay>();
			overlay->requestDelete();
			e.consume(this);
		}

		if (!e.getTarget())
			TextField::onSelectKey(e);
	}
};

void addGridXMenu(Menu* menu, Channel* channel) {
	for (int p = 0; p < NUM_SNAP_OPTIONS; p++) {
		menu->addChild(createCheckMenuItem(string::f("%i", snapValues[p]), "",
			[=]() {return channel->getGridX() == snapValues[p];},
			[=]() {channel->setGridX(snapValues[p], true);}
		));	
	}

	SnapValueField* snapValueField = new SnapValueField;
	snapValueField->box.size.x = 100;
	snapValueField->setChannel(channel);
	menu->addChild(snapValueField);
}

// Range menu item

void addRangeMenu(Menu* menu, Channel* channel) {
	for (int p = 0; p < NUM_RANGE_OPTIONS; p++) {
		if (p == 5) {
			menu->addChild(new MenuSeparator());
		}
		menu->addChild(createCheckMenuItem(channel->getRangeText(p), "",
			[=]() {return channel->getRangeIndex() == p;},
			[=]() {channel->setRangeIndex(p, true);}
		));	
	}
}


// Block 1 menus
// --------------------

// Trig mode sub item

void myActionTm(Channel* channel, int8_t setVal) {// action for trig mode menu
	int oldTrig = channel->getTrigMode();
	if (setVal != oldTrig) {
		channel->setTrigMode(setVal);
		// setVal is newTrigMode
		// Push TrigModeChange history action
		TrigModeChange* h = new TrigModeChange;
		h->channelSrc = channel;
		h->oldTrigMode = oldTrig;
		h->newTrigMode = setVal;
		APP->history->push(h);
	}
}

void addTrigModeMenu(Menu* menu, Channel* channel) {
	for (int t = 0; t < NUM_TRIG_MODES; t++) {
		menu->addChild(createCheckMenuItem(trigModeNamesLong[t], "",
			[=]() {return channel->getTrigMode() == t;},
			[=]() {myActionTm(channel, t);}
		));	
	}
}


// Play mode menu item

void myActionPm(Channel* channel, int8_t setVal) {// action for play mode menu when trig mode is all except CV
	int oldPlay = channel->getPlayMode();
	if (setVal != oldPlay) {
		channel->setPlayMode(setVal);
		// setVal is newPlayMode
		// Push PlayModeChange history action
		PlayModeChange* h = new PlayModeChange;
		h->channelSrc = channel;
		h->oldPlayMode = oldPlay;
		h->newPlayMode = setVal;
		APP->history->push(h);
	}
}

void myActionPmTmCv(Channel* channel, int8_t setVal) {// action for play mode menu when trig mode is CV
	int oldBipolCvMode = channel->getBipolCvMode();
	if (setVal != oldBipolCvMode) {
		channel->setBipolCvMode(setVal);
		// setVal is newPlayMode
		// Push BipolCvModeChange history action
		BipolCvModeChange* h = new BipolCvModeChange;
		h->channelSrc = channel;
		h->oldBipolCvMode = oldBipolCvMode;
		h->newBipolCvMode = setVal;
		APP->history->push(h);
	}
}

void addPlayModeMenu(Menu* menu, Channel* channel) {
	if (channel->getTrigMode() == TM_CV) {
		menu->addChild(createCheckMenuItem("Unipolar T/G in", "",
			[=]() {return channel->getBipolCvMode() == 0;},
			[=]() {myActionPmTmCv(channel, 0);}
		));	
		menu->addChild(createCheckMenuItem("Bipolar T/G in", "",
			[=]() {return channel->getBipolCvMode() == 1;},
			[=]() {myActionPmTmCv(channel, 1);}
		));	
	}
	else {
		for (int p = 0; p < NUM_PLAY_MODES; p++) {
			menu->addChild(createCheckMenuItem(playModeNamesLong[p], "",
				[=]() {return channel->getPlayMode() == p;},
				[=]() {myActionPm(channel, p);}
			));	
		}
	}
}



// Length menu

#ifdef SM_PRO
struct SyncRatioSectionItem : MenuItem {
	Channel* channel;
	Param* lengthSyncParamSrc;
	int startRatio;
	int endRatio;

	void myAction(int setVal) {
		// Push SyncLengthChange history action
		SyncLengthChange* h = new SyncLengthChange;
		h->lengthSyncParamSrc = lengthSyncParamSrc;
		h->oldSyncLength = lengthSyncParamSrc->getValue();
		
		lengthSyncParamSrc->setValue((float)(setVal));
	
		h->newSyncLength = lengthSyncParamSrc->getValue();
		APP->history->push(h);
	}

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		
		for (int i = startRatio; i < endRatio; i++) {		
			menu->addChild(createCheckMenuItem(multDivTable[i].longName, "",
				[=]() {return channel->getLengthSync() == i;},
				[=]() {myAction(i);},
				channel->inadmissibleLengthSync(i)
			));	
		}
		
		return menu;
	}
	
	void step() override {
		std::string _rightText = RIGHT_ARROW;
		if (channel->getLengthSync() >= startRatio && channel->getLengthSync() < endRatio) {
			_rightText.insert(0, " ");
			_rightText.insert(0, CHECKMARK_STRING);
		}
		rightText = _rightText;
		MenuItem::step();
	}
};

void addSyncRatioMenuTwoLevel(Menu* menu, Param* lengthSyncParamSrc, Channel* channel) {
	int i = 0;
	for (int s = 0; s < NUM_SECTIONS; s++) {
		SyncRatioSectionItem *srsecItem = createMenuItem<SyncRatioSectionItem>(sectionNames[s], RIGHT_ARROW);
		srsecItem->channel = channel;
		srsecItem->lengthSyncParamSrc = lengthSyncParamSrc;
		srsecItem->startRatio = i;
		i += numRatiosPerSection[s];
		srsecItem->endRatio = i;
		menu->addChild(srsecItem);
	}
}
#endif


struct ShowULengthAsItem : MenuItem {
	Channel* channel;
	
	Menu *createChildMenu() override {
		const std::string showULengthTypes[3] = {
			"Time (default)", 
			"Frequency", 
			"Note"
		};
		
		Menu *menu = new Menu;
		
		for (int i = 0; i < 3; i++) {			
			menu->addChild(createCheckMenuItem(showULengthTypes[i], "",
				[=]() {return channel->getShowUnsyncedLengthAs() == i;},
				[=]() {channel->setShowUnsyncLengthAs(i);}
			));	
		}		
		return menu;
	}
};


struct UnsyncedLengthValueField : ui::TextField {
	Param* lengthUnsyncParamSrc;

	void step() override {
		// Keep selected
		APP->event->setSelectedWidget(this);
		TextField::step();
	}

	void setParamSrc(Param* _lengthUnsyncParamSrc) {
		lengthUnsyncParamSrc = _lengthUnsyncParamSrc;
		float lengthInSeconds = PlayHead::LENGTH_UNSYNC_MULT * std::pow(PlayHead::LENGTH_UNSYNC_BASE, lengthUnsyncParamSrc->getValue());
		text = string::f("%.1f", 1.0f / lengthInSeconds);
		selectAll();
	}

	void onSelectKey(const event::SelectKey& e) override {
		if (e.action == GLFW_PRESS && (e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_KP_ENTER)) {
			float newFreq = 1000.0f;
			float newParam = -10.0f;
			
			int n = 0;
			if ( (n = std::sscanf(text.c_str(), "%f", &newFreq)) == 1) {
				if (newFreq > 1e-5) {
					newParam = std::log(1.0f / (newFreq * PlayHead::LENGTH_UNSYNC_MULT)) / std::log(PlayHead::LENGTH_UNSYNC_BASE);
				}
				else {
					n = 0;
				}
			}
			else {
				float newVoct = stringToVoct(&text);
				if (newVoct != -100.0f) {
					n = 2;
					newParam = voctToUnsuncedLengthParam(newVoct);
				}
				else {
					n = 0;
				}
			}
						
			if (n >= 1 && newParam >= -1.0f && newParam <= 1.0f) {
				UnsyncLengthChange* h = new UnsyncLengthChange;
				h->lengthUnsyncParamSrc = lengthUnsyncParamSrc;
				h->oldUnsyncLength = lengthUnsyncParamSrc->getValue();
				h->newUnsyncLength = newParam;
				APP->history->push(h);
				
				lengthUnsyncParamSrc->setValue(newParam);				
			}
			ui::MenuOverlay* overlay = getAncestorOfType<ui::MenuOverlay>();
			overlay->requestDelete();
			e.consume(this);
		}

		if (!e.getTarget())
			TextField::onSelectKey(e);
	}
};

void addUnsyncRatioMenu(Menu* menu, Param* lengthUnsyncParamSrc, Channel* channel) {
	ShowULengthAsItem *uslhItem = createMenuItem<ShowULengthAsItem>("Unsynced length display", RIGHT_ARROW);
	uslhItem->channel = channel;
	menu->addChild(uslhItem);

	menu->addChild(new MenuSeparator());

	menu->addChild(createMenuLabel("Length (Hz) or note (ex. C#4)"));
	
	UnsyncedLengthValueField* unsyncLengthField = new UnsyncedLengthValueField;
	unsyncLengthField->box.size.x = 100;
	unsyncLengthField->setParamSrc(lengthUnsyncParamSrc);
	menu->addChild(unsyncLengthField);
}
