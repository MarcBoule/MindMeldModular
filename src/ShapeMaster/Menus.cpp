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

struct DeletePointItem : MenuItem {
	Shape* shape;
	int pt;

	void onAction(const event::Action &e) override {
		shape->deletePointWithBlock(pt, true);// with history
	}
};



void captureNewTime(std::string* text, Channel* channel, int pt, float length) {
	Shape* shape = channel->getShape();
	Vec ptVec = shape->getPointVect(pt);
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
		float newVolts = stringToVoct(text);
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
		// APP->event->setSelected(this);
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
		// APP->event->setSelected(this);
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
	
	MenuLabel *timeLabel = new MenuLabel();
	timeLabel->text = "Horiz.: ";
	timeLabel->text.append(timeText).append("s");
	menu->addChild(timeLabel);

	TimeValueField* timeValueField = new TimeValueField;
	timeValueField->box.size.x = 100;
	timeValueField->setChannel(channel, timeText, pt, length);
	// timeValueField->voltText = ?;// done below since voltValueField doesn't exist yet
	menu->addChild(timeValueField);


	menu->addChild(new MenuSeparator());


	// Vert.
	float voltRanged = channel->applyRange(ptVec.y);
	std::string voltRangedText = string::f("%.4g", voltRanged);
	
	MenuLabel *voltLabel = new MenuLabel();
	voltLabel->text = string::f("Vert.: %s V", voltRangedText.c_str());
	menu->addChild(voltLabel);

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
		MenuLabel *volt2Label = new MenuLabel();
		volt2Label->text = string::f("(%s, %s)", freqStr.c_str(), note);
		menu->addChild(volt2Label);
	}	

	VoltValueField* voltValueField = new VoltValueField;
	voltValueField->box.size.x = 100;
	voltValueField->setChannel(channel, voltRangedText, pt, length);
	voltValueField->timeText = &timeValueField->text;
	timeValueField->voltText = &voltValueField->text;
	menu->addChild(voltValueField);

	
	menu->addChild(new MenuSeparator());
	
	DeletePointItem *delPointItem = createMenuItem<DeletePointItem>("Delete node", "");
	delPointItem->shape = shape;
	delPointItem->pt = pt;
	menu->addChild(delPointItem);
	
	// menu->addChild(new MenuSeparator());
}



// Control point menus
// --------------------


struct ResetCtrlItem : MenuItem {
	Shape* shape;
	int pt;

	void onAction(const event::Action &e) override {
		shape->makeLinear(pt);
	}
};


struct CtrlTypeItem : MenuItem {
	Shape* shape;
	int pt;
	int8_t setVal = 0;

	void onAction(const event::Action &e) override {
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
};


void createCtrlMenu(ui::Menu* menu, Shape* shape, int pt) {
	CtrlTypeItem *smoothTypeItem = createMenuItem<CtrlTypeItem>("Smooth", CHECKMARK(shape->getType(pt) == 0));
	smoothTypeItem->shape = shape;
	smoothTypeItem->pt = pt;
	menu->addChild(smoothTypeItem);

	CtrlTypeItem *sshapeTypeItem = createMenuItem<CtrlTypeItem>("S-Shape", CHECKMARK(shape->getType(pt) == 1));
	sshapeTypeItem->shape = shape;
	sshapeTypeItem->setVal = 1;
	sshapeTypeItem->pt = pt;
	menu->addChild(sshapeTypeItem);

	menu->addChild(new MenuSeparator());

	ResetCtrlItem *resetCtrlItem = createMenuItem<ResetCtrlItem>("Reset", "");
	resetCtrlItem->shape = shape;
	resetCtrlItem->pt = pt;
	menu->addChild(resetCtrlItem);
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

struct RetrigItem : MenuItem {
	Channel* channel;

	void onAction(const event::Action &e) override {
		channel->toggleAllowRetrig();
	}
};

#ifdef SM_PRO
struct SmTriggersItem : MenuItem {
	Channel* channel;
	
	struct EocOnLastOnlyItem : MenuItem {
		Channel* channel;
		void onAction(const event::Action &e) override {
			channel->toggleEocOnLastOnly();
		}
	};

	struct ExcludeEocFromOrItem : MenuItem {
		Channel* channel;
		void onAction(const event::Action &e) override {
			channel->toggleExcludeEocFromOr();
		}
	};
	
	struct EosOnlyAfterSosItem : MenuItem {
		Channel* channel;
		void onAction(const event::Action &e) override {
			channel->toggleEosOnlyAfterSos();
		}
	};


	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		
		EocOnLastOnlyItem *eocLastOnlyItem = createMenuItem<EocOnLastOnlyItem>("EOC on last cycle only", CHECKMARK(channel->getEocOnLastOnly()));
		eocLastOnlyItem->channel = channel;
		menu->addChild(eocLastOnlyItem);
		
		ExcludeEocFromOrItem *eocExItem = createMenuItem<ExcludeEocFromOrItem>("EOC excluded from OR", CHECKMARK(channel->isExcludeEocFromOr()));
		eocExItem->channel = channel;
		menu->addChild(eocExItem);
		
		EosOnlyAfterSosItem *eosAfterSosItem = createMenuItem<EosOnlyAfterSosItem>("EOS only after SOS", CHECKMARK(channel->getEosOnlyAfterSos()));
		eosAfterSosItem->channel = channel;
		menu->addChild(eosAfterSosItem);

		return menu;
	}
};
#endif

struct LfoGateResettingItem : MenuItem {
	Channel* channel;

	void onAction(const event::Action &e) override {
		channel->toggleGateRestart();
	}
};


#ifdef SM_PRO
struct PlayheadNeverJumpsItem : MenuItem {
	Channel* channel;

	void onAction(const event::Action &e) override {
		channel->togglePlayheadNeverJumps();
	}
};
struct Allow1BarLocksItem : MenuItem {
	Channel* channel;
	bool* running;

	struct Allow1BarLocksSubItem : MenuItem {
		Channel* channel;
		bool shouldBe = true;

		void onAction(const event::Action &e) override {
			if (channel->isAllow1BarLocks() != shouldBe) {
				channel->toggleAllow1BarLocks();
			}
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		
		if (*running) {
			MenuLabel *disabledLabel = new MenuLabel();
			disabledLabel->text = "[Turn off run]";
			menu->addChild(disabledLabel);
		}			
		
		Allow1BarLocksSubItem *allow1barItem = createMenuItem<Allow1BarLocksSubItem>("Quantise to max 1 bar", CHECKMARK(channel->isAllow1BarLocks()));
		allow1barItem->channel = channel;
		allow1barItem->disabled = *running;
		menu->addChild(allow1barItem);
		
		Allow1BarLocksSubItem *disallow1barItem = createMenuItem<Allow1BarLocksSubItem>("Quantise to cycle length", CHECKMARK(!channel->isAllow1BarLocks()));
		disallow1barItem->channel = channel;
		disallow1barItem->shouldBe = false;
		disallow1barItem->disabled = *running;
		menu->addChild(disallow1barItem);
		
		return menu;
	}
};
#endif

/*
struct SlewBehaviorItem : MenuItem {
	Channel* channel;

	struct SlewBehaviorSubItem : MenuItem {
		Channel* channel;

		void onAction(const event::Action &e) override {
			channel->toggleRelativeSlew();
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		
		SlewBehaviorSubItem *slewBeh0Item = createMenuItem<SlewBehaviorSubItem>("Constant (default)", CHECKMARK(channel->getRelativeSlew() == 0));
		slewBeh0Item->channel = channel;
		menu->addChild(slewBeh0Item);
		
		SlewBehaviorSubItem *slewBeh1Item = createMenuItem<SlewBehaviorSubItem>("Relative", CHECKMARK(channel->getRelativeSlew() != 0));
		slewBeh1Item->channel = channel;
		menu->addChild(slewBeh1Item);
		
		return menu;
	}
};
*/

struct DecoupledFirstAndLastItem : MenuItem {
	Channel* channel;

	struct DecoupledFirstAndLastSubItem : MenuItem {
		Channel* channel;

		void onAction(const event::Action &e) override {
			channel->toggleDecoupledFirstAndLast();
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		
		DecoupledFirstAndLastSubItem *dec0Item = createMenuItem<DecoupledFirstAndLastSubItem>("Coupled (default)", CHECKMARK(!channel->isDecoupledFirstAndLast()));
		dec0Item->channel = channel;
		menu->addChild(dec0Item);
		
		DecoupledFirstAndLastSubItem *dec1Item = createMenuItem<DecoupledFirstAndLastSubItem>("Decoupled", CHECKMARK(channel->isDecoupledFirstAndLast()));
		dec1Item->channel = channel;
		menu->addChild(dec1Item);
		
		return menu;
	}
};


struct PolySumItem : MenuItem {
	Channel* channel;

	struct PolySumSubItem : MenuItem {
		Channel* channel;
		int8_t setVal;
		void onAction(const event::Action &e) override {
			channel->channelSettings.cc4[2] = setVal;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		
		for (int i = 0; i < NUM_POLY_MODES; i++) {
			PolySumSubItem *polyMdItem = createMenuItem<PolySumSubItem>(polyModeNames[i], CHECKMARK(channel->getPolyMode() == i));
			polyMdItem->channel = channel;
			polyMdItem->setVal = i;
			menu->addChild(polyMdItem);
		}
		
		return menu;
	}
};


struct CopyChanelItem : MenuItem {
	Channel* channelSource;
	json_t** channelCopyPasteCache;
	
	void onAction(const event::Action &e) override {
		if (*channelCopyPasteCache != NULL) {
			json_decref(*channelCopyPasteCache);
		}
		(*channelCopyPasteCache) = channelSource->dataToJsonChannel(WITH_PARAMS, WITHOUT_PRO_UNSYNC_MATCH, WITH_FULL_SETTINGS);
	}
};

struct PasteChanelItem : MenuItem {
	Channel* channelDestination;
	json_t** channelCopyPasteCache;

	void onAction(const event::Action &e) override {
		if (*channelCopyPasteCache != NULL) {
			// Push ChannelChange history action (rest is done in onDragEnd())
			ChannelChange* h = new ChannelChange;
			h->channelSrc = channelDestination;
			h->oldJson = channelDestination->dataToJsonChannel(WITH_PARAMS, WITHOUT_PRO_UNSYNC_MATCH, WITH_FULL_SETTINGS);
			
			channelDestination->dataFromJsonChannel(*channelCopyPasteCache, WITH_PARAMS, ISNOT_DIRTY_CACHE_LOAD, WITH_FULL_SETTINGS);
		
			h->newJson = channelDestination->dataToJsonChannel(WITH_PARAMS, WITHOUT_PRO_UNSYNC_MATCH, WITH_FULL_SETTINGS);
			h->name = "paste channel";
			APP->history->push(h);
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

	struct ScopeVcaPolySelSubItem : MenuItem {
		int8_t *srcScopeVcaPolySelItem;
		int setVal;
		void onAction(const event::Action &e) override {
			*srcScopeVcaPolySelItem = setVal;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		
		int maxChan = std::max(channel->getVcaPostSize(), channel->getVcaPreSize());
		
		ScopeVcaPolySelSubItem *polySelStereoItem = createMenuItem<ScopeVcaPolySelSubItem>("Poly-chans 1+2", CHECKMARK(*srcScopeVcaPolySelItem == 16));
		polySelStereoItem->srcScopeVcaPolySelItem = srcScopeVcaPolySelItem;
		polySelStereoItem->setVal = 16;
		polySelStereoItem->disabled = maxChan <= 1;
		menu->addChild(polySelStereoItem);
		
		for (int i = 0; i < 16; i++) {
			ScopeVcaPolySelSubItem *polySelItem = createMenuItem<ScopeVcaPolySelSubItem>(string::f("Poly-chan %i", i + 1), CHECKMARK(*srcScopeVcaPolySelItem == i));
			polySelItem->srcScopeVcaPolySelItem = srcScopeVcaPolySelItem;
			polySelItem->setVal = i;
			polySelItem->disabled = i >= maxChan;
			menu->addChild(polySelItem);
		}

		return menu;
	}
};


struct ChanColorItem : MenuItem {
	int8_t *srcChanColor;

	struct ChanColorSubItem : MenuItem {
		int8_t *srcChanColor;
		int setVal;
		void onAction(const event::Action &e) override {
			*srcChanColor = setVal;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		
		for (int i = 0; i < numChanColors; i++) {
			ChanColorSubItem *chanColItem = createMenuItem<ChanColorSubItem>(chanColorNames[i], CHECKMARK(*srcChanColor == i));
			chanColItem->srcChanColor = srcChanColor;
			chanColItem->setVal = i;
			menu->addChild(chanColItem);
		}
		
		return menu;
	}
};

// code below adapted from ParamWidget::ParamField() by Andrew Belt
struct ChanNameField : ui::TextField {
	Channel* channel;

	void step() override {
		// Keep selected
		APP->event->setSelected(this);
		TextField::step();
	}

	void setChannel(Channel* _channel) {
		channel = _channel;
		text = channel->getChanName();
		selectAll();
	}

	void onSelectKey(const event::SelectKey& e) override {
		if (e.action == GLFW_PRESS) {
			channel->setChanName(text);
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


void createChannelMenu(ui::Menu* menu, Channel* channels, int chan, PackedBytes4* miscSettings2GlobalSrc, bool trigExpPresent, json_t** channelCopyPasteCache, bool* running) {
	ChanNameField* chanNameField = new ChanNameField;
	chanNameField->box.size.x = 100;
	chanNameField->setChannel(&(channels[chan]));
	menu->addChild(chanNameField);

	MenuLabel *settingsLabel = new MenuLabel();
	settingsLabel->text = "Settings";
	menu->addChild(settingsLabel);

	GainAdjustVcaSlider *vcaGainAdjustSlider = new GainAdjustVcaSlider(&(channels[chan]), -20.0f, 20.0f);
	vcaGainAdjustSlider->box.size.x = 200.0f;
	menu->addChild(vcaGainAdjustSlider);

	RetrigItem *retrgItem = createMenuItem<RetrigItem>("T/G, SC - Retrigger", CHECKMARK(channels[chan].getAllowRetrig()));
	retrgItem->channel = &(channels[chan]);
	menu->addChild(retrgItem);

	LfoGateResettingItem *lfoGateResetItem = createMenuItem<LfoGateResettingItem>("CTRL - Restart", CHECKMARK(channels[chan].getGateRestart()));
	lfoGateResetItem->channel = &(channels[chan]);
	menu->addChild(lfoGateResetItem);

	#ifdef SM_PRO
	PlayheadNeverJumpsItem *playNoJumpItem = createMenuItem<PlayheadNeverJumpsItem>("Playhead never jumps", CHECKMARK(channels[chan].getPlayheadNeverJumps()));
	playNoJumpItem->channel = &(channels[chan]);
	menu->addChild(playNoJumpItem);

	Allow1BarLocksItem *allow1barItem = createMenuItem<Allow1BarLocksItem>("Sync lock quantising", RIGHT_ARROW);
	allow1barItem->channel = &(channels[chan]);
	allow1barItem->running = running;
	allow1barItem->disabled = *running;
	menu->addChild(allow1barItem);
	#endif

	DecoupledFirstAndLastItem *decFlItem = createMenuItem<DecoupledFirstAndLastItem>("First/last nodes", RIGHT_ARROW);
	decFlItem->channel = &(channels[chan]);
	menu->addChild(decFlItem);

	PolySumItem *sumSteItem = createMenuItem<PolySumItem>("Poly VCA summing", RIGHT_ARROW);
	sumSteItem->channel = &(channels[chan]);
	menu->addChild(sumSteItem);

	ScopeVcaPolySelItem *polyVcaItem = createMenuItem<ScopeVcaPolySelItem>("Poly VCA in scope select", RIGHT_ARROW);
	polyVcaItem->srcScopeVcaPolySelItem = &(channels[chan].channelSettings2.cc4[0]);
	polyVcaItem->channel = &(channels[chan]);
	menu->addChild(polyVcaItem);

	// SlewBehaviorItem *slewBehaviorItem = createMenuItem<SlewBehaviorItem>("Slew behavior", RIGHT_ARROW);
	// slewBehaviorItem->channel = &(channels[chan]);
	// menu->addChild(slewBehaviorItem);

	#ifdef SM_PRO
	if (trigExpPresent) {
		SmTriggersItem *smTrigItem = createMenuItem<SmTriggersItem>("SM-Triggers", RIGHT_ARROW);
		smTrigItem->channel = &(channels[chan]);
		menu->addChild(smTrigItem);
	}
	#endif

	ChanColorItem *chanColItem = createMenuItem<ChanColorItem>("Channel colour", RIGHT_ARROW);
	chanColItem->srcChanColor = &(channels[chan].channelSettings.cc4[1]);
	menu->addChild(chanColItem);

	if (miscSettings2GlobalSrc->cc4[0] >= 2) {
		InvShadowItem *invShadItem = createMenuItem<InvShadowItem>("Shadow", RIGHT_ARROW);
		invShadItem->srcInvShadow = &(channels[chan].channelSettings.cc4[0]);
		menu->addChild(invShadItem);
	}


	menu->addChild(new MenuSeparator());

	MenuLabel *actionsLabel = new MenuLabel();
	actionsLabel->text = "Actions";
	menu->addChild(actionsLabel);

	CopyChanelItem *copyChanItem = createMenuItem<CopyChanelItem>("Copy channel", "");
	copyChanItem->channelSource = &(channels[chan]);
	copyChanItem->channelCopyPasteCache = channelCopyPasteCache;
	menu->addChild(copyChanItem);
	
	PasteChanelItem *pasteChanItem = createMenuItem<PasteChanelItem>("Paste channel", "");
	pasteChanItem->channelDestination = &(channels[chan]);
	pasteChanItem->channelCopyPasteCache = channelCopyPasteCache;
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

struct SidechainUseVcaItem : MenuItem {
	Channel* channel;

	void onAction(const event::Action &e) override {
		channel->toggleSidechainUseVca();
	}
};

struct SidechainLowTrig : MenuItem {
	Channel* channel;

	void onAction(const event::Action &e) override {
		channel->toggleSidechainLowTrig();
	}
};

void createSidechainSettingsMenu(Channel* channel) {
	ui::Menu *menu = createMenu();

	SidechainUseVcaItem *scVcaItem = createMenuItem<SidechainUseVcaItem>("Use VCA input", CHECKMARK(channel->isSidechainUseVca()));
	scVcaItem->channel = channel;
	menu->addChild(scVcaItem);

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

	SidechainLowTrig *scLowTrigItem = createMenuItem<SidechainLowTrig>("Low range trigger level", CHECKMARK(channel->isSidechainLowTrig()));
	scLowTrigItem->channel = channel;
	menu->addChild(scLowTrigItem);

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
	
	NumNodeRangeQuantity(float* _mainNum, float* _otherNum, float _defaultVal, bool _isMin) {
		mainNum = _mainNum;
		otherNum = _otherNum;
		defaultVal = _defaultVal;
		isMin = _isMin;
	}
	void setValue(float value) override {
		*mainNum = math::clamp(value, getMinValue(), getMaxValue());
		if (isMin) {
			*otherNum = std::fmax(*mainNum, *otherNum);
		}
		else {
			*otherNum = std::fmin(*mainNum, *otherNum);
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
		return string::f("%i", (int)(*mainNum + 0.5f));
	}
	void setDisplayValue(float displayValue) override {setValue(displayValue);}
	std::string getLabel() override {
		if (isMin) return "Min segments";
		return "Max segments";
	}
	std::string getUnit() override {return "";}
};

struct NumNodeRangeSlider : ui::Slider {
	NumNodeRangeSlider(float* mainNum, float* otherNum, float defaultVal, bool isMin) {
		quantity = new NumNodeRangeQuantity(mainNum, otherNum, defaultVal, isMin);
	}
	~NumNodeRangeSlider() {
		delete quantity;
	}
};


struct RandCtrlQuantity : Quantity {
	float* ctrlMax = NULL;
	
	RandCtrlQuantity(float* _ctrlMax) {
		ctrlMax = _ctrlMax;
	}
	void setValue(float value) override {
		*ctrlMax = math::clamp(value, getMinValue(), getMaxValue());
	}
	float getValue() override {
		return *ctrlMax;
	}
	float getMinValue() override {return 0.0f;}
	float getMaxValue() override {return 100.0f;}
	float getDefaultValue() override {return RandomSettings::RAND_CTRL_DEF;}
	float getDisplayValue() override {return getValue();}
	std::string getDisplayValueString() override {
		return string::f("%.1f", *ctrlMax);
	}
	void setDisplayValue(float displayValue) override {setValue(displayValue);}
	std::string getLabel() override {
		return "Curve max";
	}
	std::string getUnit() override {return "%";}
};

struct CtrlMaxSlider : ui::Slider {
	CtrlMaxSlider(float* ctrlMax) {
		quantity = new RandCtrlQuantity(ctrlMax);
	}
	~CtrlMaxSlider() {
		delete quantity;
	}
};

struct ZeroOrMaxQuantity : Quantity {
	float* thisV = NULL;
	float* otherV = NULL;
	bool isZero = true;
	
	ZeroOrMaxQuantity(float* _thisV, float* _otherV, bool _isZero) {
		thisV = _thisV;
		otherV = _otherV;
		isZero = _isZero;
	}
	void setValue(float value) override {
		*thisV = math::clamp(value, getMinValue(), getMaxValue());
		if (*otherV > (100.0f - *thisV)) {
			*otherV = 100.0f - *thisV;
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
		return string::f("%.1f", *thisV);
	}
	void setDisplayValue(float displayValue) override {setValue(displayValue);}
	std::string getLabel() override {
		if (isZero) return "0V ratio";
		return "MaxV ratio";
	}
	std::string getUnit() override {return "%";}
};

struct ZeroOrMaxSlider : ui::Slider {
	ZeroOrMaxSlider(float* _thisV, float* _otherV, bool _isZero) {
		quantity = new ZeroOrMaxQuantity(_thisV, _otherV, _isZero);
	}
	~ZeroOrMaxSlider() {
		delete quantity;
	}
};


struct RandomBoolSubItem : MenuItem {
	int8_t* setting;
	float* ctrlMaxPtr = NULL;// used only when this item is the "Stepped" item
	
	void onAction(const event::Action &e) override {
		*setting ^= 0x1;
		if (ctrlMaxPtr != NULL) {
			if (*setting) {// i.e. if Stepped
				*ctrlMaxPtr = 0.0f;
			}
			else {
				*ctrlMaxPtr = 100.0f;
			}
		}
		// unconsume event:
		e.context->propagating = false;
		e.context->consumed = false;
		e.context->target = NULL;
	}
	void step() override {
		rightText = CHECKMARK(*setting != 0);
		MenuItem::step();
	}
};

struct RandomizeSubItem : MenuItem {
	Channel* channel;
	
	void onAction(const event::Action &e) override {
		channel->randomizeShape(true);
	}
};


struct RandomNoteItem : MenuItem {
	RandomSettings* randomSettings;
	
	struct RandomNoteSubItem : MenuItem {
		RandomSettings* randomSettings;
		int key;
		
		void onAction(const event::Action &e) override {
			randomSettings->toggleScaleKey(key);
			// unconsume event:
			e.context->propagating = false;
			e.context->consumed = false;
			e.context->target = NULL;
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
			RandomNoteSubItem *rndNote0Item = createMenuItem<RandomNoteSubItem>(noteText, CHECKMARK(randomSettings->getScaleKey(i)));
			rndNote0Item->randomSettings = randomSettings;
			rndNote0Item->key = i;
			menu->addChild(rndNote0Item);	
		}
		
		return menu;
	}
};


void addRandomMenu(Menu* menu, Channel* channel) {
	RandomSettings* randomSettings = channel->getRandomSettings();
	
	RandomizeSubItem *radizeItem = createMenuItem<RandomizeSubItem>("Randomise", "");
	radizeItem->channel = channel;
	menu->addChild(radizeItem);

	menu->addChild(new MenuSeparator());
	
	MenuLabel *rndsetLabel = new MenuLabel();
	rndsetLabel->text = "Randomization settings:";
	menu->addChild(rndsetLabel);
	

	NumNodeRangeSlider *nodeRange1Slider = new NumNodeRangeSlider(&(randomSettings->numNodesMin), &(randomSettings->numNodesMax), RandomSettings::RAND_NODES_MIN_DEF, true);
	nodeRange1Slider->box.size.x = 200.0f;
	menu->addChild(nodeRange1Slider);
	
	NumNodeRangeSlider *nodeRange2Slider = new NumNodeRangeSlider(&(randomSettings->numNodesMax), &(randomSettings->numNodesMin), RandomSettings::RAND_NODES_MAX_DEF, false);
	nodeRange2Slider->box.size.x = 200.0f;
	menu->addChild(nodeRange2Slider);
	
	CtrlMaxSlider *ctrMaxSlider = new CtrlMaxSlider(&(randomSettings->ctrlMax));
	ctrMaxSlider->box.size.x = 200.0f;
	menu->addChild(ctrMaxSlider);
	
	ZeroOrMaxSlider *zeroVSlider = new ZeroOrMaxSlider(&(randomSettings->zeroV), &(randomSettings->maxV), true);
	zeroVSlider->box.size.x = 200.0f;
	menu->addChild(zeroVSlider);
	
	ZeroOrMaxSlider *maxVSlider = new ZeroOrMaxSlider(&(randomSettings->maxV), &(randomSettings->zeroV), false);
	maxVSlider->box.size.x = 200.0f;
	menu->addChild(maxVSlider);
	
	RandomBoolSubItem *steppedItem = createMenuItem<RandomBoolSubItem>("Stepped", CHECKMARK(randomSettings->stepped != 0));
	steppedItem->setting = &randomSettings->stepped;
	steppedItem->ctrlMaxPtr = &randomSettings->ctrlMax;
	menu->addChild(steppedItem);
	
	RandomBoolSubItem *gridItem = createMenuItem<RandomBoolSubItem>("Lock to Grid-X", CHECKMARK(randomSettings->grid != 0));
	gridItem->setting = &randomSettings->grid;
	menu->addChild(gridItem);
	
	RandomBoolSubItem *quantItem = createMenuItem<RandomBoolSubItem>("Quantized", CHECKMARK(randomSettings->quantized != 0));
	quantItem->setting = &randomSettings->quantized;
	menu->addChild(quantItem);
	
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
		APP->event->setSelected(this);
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


struct SnapSubItem : MenuItem {
	Channel* channel;
	int8_t setVal;
	void onAction(const event::Action &e) override {
		channel->setGridX(setVal, true);
	}
};


void addSnapMenu(Menu* menu, Channel* channel) {
	for (int p = 0; p < NUM_SNAP_OPTIONS; p++) {
		SnapSubItem *snapChoice = createMenuItem<SnapSubItem>(string::f("%i", snapValues[p]), CHECKMARK(channel->getGridX() == snapValues[p]));
		snapChoice->channel = channel;
		snapChoice->setVal = snapValues[p];
		menu->addChild(snapChoice);
	}

	SnapValueField* snapValueField = new SnapValueField;
	snapValueField->box.size.x = 100;
	snapValueField->setChannel(channel);
	menu->addChild(snapValueField);
}

// Range menu item

struct RangeSubItem : MenuItem {
	Channel* channel;
	int8_t setVal;
	void onAction(const event::Action &e) override {
		channel->setRangeIndex(setVal, true);
	}
};

void addRangeMenu(Menu* menu, Channel* channel) {
	for (int p = 0; p < NUM_RANGE_OPTIONS; p++) {
		if (p == 5) {
			menu->addChild(new MenuSeparator());
		}
		RangeSubItem *rangeChoice = createMenuItem<RangeSubItem>(channel->getRangeText(p), CHECKMARK(channel->getRangeIndex() == p));
		rangeChoice->channel = channel;
		rangeChoice->setVal = p;
		menu->addChild(rangeChoice);
	}
}


// Block 1 menus
// --------------------

// Trig mode sub item

struct TrigModeSubItem : MenuItem {
	Channel* channel;
	int8_t setVal;
	void onAction(const event::Action &e) override {
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
};

void addTrigModeMenu(Menu* menu, Channel* channel) {
	for (int t = 0; t < NUM_TRIG_MODES; t++) {
		TrigModeSubItem *trigModeChoice = createMenuItem<TrigModeSubItem>(trigModeNamesLong[t], CHECKMARK(channel->getTrigMode() == t));
		trigModeChoice->channel = channel;
		trigModeChoice->setVal = t;
		menu->addChild(trigModeChoice);
	}
}


// Play mode menu item

struct PlayModeSubItem : MenuItem {
	Channel* channel;
	int8_t setVal;
	void onAction(const event::Action &e) override {
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
};

struct BipolCvModeSubItem : MenuItem {
	Channel* channel;
	int8_t setVal;
	void onAction(const event::Action &e) override {
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
};

void addPlayModeMenu(Menu* menu, Channel* channel) {
	if (channel->getTrigMode() == TM_CV) {
		BipolCvModeSubItem *bipol0ModeChoice = createMenuItem<BipolCvModeSubItem>("Unipolar T/G in", CHECKMARK(channel->getBipolCvMode() == 0));
		bipol0ModeChoice->channel = channel;
		bipol0ModeChoice->setVal = 0;
		menu->addChild(bipol0ModeChoice);
		
		BipolCvModeSubItem *bipol1ModeChoice = createMenuItem<BipolCvModeSubItem>("Bipolar T/G in", CHECKMARK(channel->getBipolCvMode() == 1));
		bipol1ModeChoice->channel = channel;
		bipol1ModeChoice->setVal = 1;
		menu->addChild(bipol1ModeChoice);
	}
	else {
		for (int p = 0; p < NUM_PLAY_MODES; p++) {
			PlayModeSubItem *playModeChoice = createMenuItem<PlayModeSubItem>(playModeNamesLong[p], CHECKMARK(channel->getPlayMode() == p));
			playModeChoice->channel = channel;
			playModeChoice->setVal = p;
			menu->addChild(playModeChoice);
		}
	}
}



// Length menu

#ifdef SM_PRO
struct SyncRatioSubItem : MenuItem {
	Param* lengthSyncParamSrc;
	int setVal;
	
	void onAction(const event::Action &e) override {
		// Push SyncLengthChange history action
		SyncLengthChange* h = new SyncLengthChange;
		h->lengthSyncParamSrc = lengthSyncParamSrc;
		h->oldSyncLength = lengthSyncParamSrc->getValue();
		
		lengthSyncParamSrc->setValue((float)(setVal));
	
		h->newSyncLength = lengthSyncParamSrc->getValue();
		APP->history->push(h);
	}
};

void addSyncRatioMenu(Menu* menu, Param* lengthSyncParamSrc, Channel* channel) {
	int i = 0;
	for (int s = 0; s < NUM_SECTIONS; s++) {
		if (s != 0) {
			menu->addChild(new MenuSeparator());
		}
		MenuLabel *sectionLabel = new MenuLabel();
		sectionLabel->text = sectionNames[s];
		menu->addChild(sectionLabel);
		for (int r = 0; r < numRatiosPerSection[s]; r++) {
			int lengthSyncIndex = channel->getLengthSync();
			std::string longNameI = multDivTable[i].longName;
			int lengthSyncIndexWithCv = channel->getLengthSyncWithCv();
			if (lengthSyncIndexWithCv != lengthSyncIndex && i == lengthSyncIndexWithCv) {
				longNameI.append(" *cv");
			}							
			SyncRatioSubItem *ratioChoice = createMenuItem<SyncRatioSubItem>(longNameI, CHECKMARK(lengthSyncIndex == i));
			ratioChoice->lengthSyncParamSrc = lengthSyncParamSrc;
			ratioChoice->setVal = i;
			ratioChoice->disabled = channel->inadmissibleLengthSync(i);
			menu->addChild(ratioChoice);
			i++;
		}
	}
}




struct SyncRatioSectionItem : MenuItem {
	Channel* channel;
	Param* lengthSyncParamSrc;
	int startRatio;
	int endRatio;

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		
		for (int i = startRatio; i < endRatio; i++) {
			int lengthSyncIndex = channel->getLengthSync();
			std::string longNameI = multDivTable[i].longName;
			int lengthSyncIndexWithCv = channel->getLengthSyncWithCv();
			if (lengthSyncIndexWithCv != lengthSyncIndex && i == lengthSyncIndexWithCv) {
				longNameI.append(" *cv");
			}							
			SyncRatioSubItem *ratioChoice = createMenuItem<SyncRatioSubItem>(longNameI, CHECKMARK(lengthSyncIndex == i));
			ratioChoice->lengthSyncParamSrc = lengthSyncParamSrc;
			ratioChoice->setVal = i;
			ratioChoice->disabled = channel->inadmissibleLengthSync(i);
			menu->addChild(ratioChoice);
		}
		
		return menu;
	}
};

void addSyncRatioMenuTwoLevel(Menu* menu, Param* lengthSyncParamSrc, Channel* channel) {
	int i = 0;
	for (int s = 0; s < NUM_SECTIONS; s++) {
		std::string rightText = RIGHT_ARROW;
		if (channel->getLengthSync() >= i && channel->getLengthSync() < i + numRatiosPerSection[s]) {
			rightText.insert(0, " ");
			rightText.insert(0, CHECKMARK(true));
		}
		SyncRatioSectionItem *srsecItem = createMenuItem<SyncRatioSectionItem>(sectionNames[s], rightText);
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
	
	struct ShowULengthAsSubItem : MenuItem {
		Channel* channel;
		int8_t setVal;

		void onAction(const event::Action &e) override {
			channel->setShowUnsyncLengthAs(setVal);
		}
	};

	Menu *createChildMenu() override {
		
		std::string showULengthTypes[3] = {
			"Time (default)", 
			"Frequency", 
			"Note"
		};
		
		Menu *menu = new Menu;
		
		for (int i = 0; i < 3; i++) {			
			ShowULengthAsSubItem *showU0Item = createMenuItem<ShowULengthAsSubItem>(showULengthTypes[i], CHECKMARK(channel->getShowUnsyncedLengthAs() == i));
			showU0Item->channel = channel;
			showU0Item->setVal = i;
			menu->addChild(showU0Item);
		}		
		return menu;
	}
};


struct UnsyncedLengthValueField : ui::TextField {
	Param* lengthUnsyncParamSrc;

	void step() override {
		// Keep selected
		APP->event->setSelected(this);
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

	MenuLabel *freqLabel = new MenuLabel();
	freqLabel->text = "Length (Hz) or note (ex. C#4)";
	menu->addChild(freqLabel);
	
	UnsyncedLengthValueField* unsyncLengthField = new UnsyncedLengthValueField;
	unsyncLengthField->box.size.x = 100;
	unsyncLengthField->setParamSrc(lengthUnsyncParamSrc);
	menu->addChild(unsyncLengthField);
}
