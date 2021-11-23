//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//Based on code from the Fundamental plugin by Andrew Belt
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "ShapeMaster.hpp"



ShapeMaster::ShapeMaster() {// : worker(&ShapeMaster::worker_nextPresetOrShape, this) {
	config(NUM_SM_PARAMS, NUM_SM_INPUTS, NUM_SM_OUTPUTS, NUM_LIGHTS);

	leftExpander.producerMessage = &expMessages[0];
	leftExpander.consumerMessage = &expMessages[1];


	// channel-specific params (extra fields are base, mult, offset)
	for (int c = 0; c < 8; c++) {
		int cp = c * NUM_CHAN_PARAMS;
		configParam(LENGTH_SYNC_PARAM + cp, 0.0f, (float)(NUM_RATIOS - 1), PlayHead::DEFAULT_LENGTH_SYNC, string::f("Length -%c-", c + 0x31), " index", 0.0f, 1.0f, 1.0f);
		paramQuantities[LENGTH_SYNC_PARAM + cp]->snapEnabled = true;
		configParam(LENGTH_UNSYNC_PARAM + cp, -1.0f, 1.0f, PlayHead::DEFAULT_LENGTH_UNSYNC, string::f("Length -%c-", c + 0x31), " s", PlayHead::LENGTH_UNSYNC_BASE, PlayHead::LENGTH_UNSYNC_MULT, 0.0f);
		configParam<RepetitionsParamQuantity>(REPETITIONS_PARAM + cp, 1.0f, 100.0f, PlayHead::DEFAULT_REPETITIONS, string::f("Repetitions -%c-", c + 0x31));
		paramQuantities[REPETITIONS_PARAM + cp]->snapEnabled = true;
		configParam(OFFSET_PARAM + cp, 0.0f, PlayHead::MAX_OFFSET, PlayHead::DEFAULT_OFFSET, string::f("Offset -%c-", c + 0x31), " samples");
		paramQuantities[OFFSET_PARAM + cp]->snapEnabled = true;
		configParam(SWING_PARAM + cp, -PlayHead::MAX_SWING, PlayHead::MAX_SWING, PlayHead::DEFAULT_SWING, string::f("Swing -%c-", c + 0x31), " %", 0.0f, 100.0f, 0.0f);
		configParam(PHASE_PARAM + cp, 0.0f, 1.0f, Channel::DEFAULT_PHASE, string::f("Phase -%c-", c + 0x31), " degrees", 0.0f, 360.0f, 0.0f);
		configParam(RESPONSE_PARAM + cp, -Channel::MAX_RESPONSE, Channel::MAX_RESPONSE, Channel::DEFAULT_RESPONSE, string::f("Response -%c-", c + 0x31), " %", 0.0f, 100.0f, 0.0f);
		configParam(WARP_PARAM + cp, -Channel::MAX_WARP, Channel::MAX_WARP, Channel::DEFAULT_WARP, string::f("Warp -%c-", c + 0x31), " %", 0.0f, 100.0f, 0.0f);
		configParam(AMOUNT_PARAM + cp, 0.0f, 1.0f, Channel::DEFAULT_AMOUNT, string::f("Amount -%c-", c + 0x31), " %", 0.0f, 100.0f, 0.0f);
		configParam(SLEW_PARAM + cp, 0.0f, 1.0f, Channel::DEFAULT_SLEW, string::f("Slew -%c-", c + 0x31), " %", 0.0f, 100.0f, 0.0f);
		configParam(SMOOTH_PARAM + cp, 0.0f, 1.0f, Channel::DEFAULT_SMOOTH, string::f("Smooth -%c-", c + 0x31), " %", 0.0f, 100.0f, 0.0f);
		configParam<CrossoverParamQuantity>(CROSSOVER_PARAM + cp, -1.0f, 1.0f, Channel::DEFAULT_CROSSOVER, string::f("Crossover -%c-", c + 0x31), " Hz", Channel::CROSSOVER_BASE, Channel::CROSSOVER_MULT, 0.0f);
		configParam(HIGH_PARAM + cp, 0.0f, 1.0f, Channel::DEFAULT_HIGH, string::f("High -%c-", c + 0x31), " %", 0.0f, 100.0f, 0.0f);
		configParam(LOW_PARAM + cp, 0.0f, 1.0f, Channel::DEFAULT_LOW, string::f("Low -%c-", c + 0x31), " %", 0.0f, 100.0f, 0.0f);
		configParam(TRIGLEV_PARAM + cp, 0.0f, 10.0f, PlayHead::DEFAULT_TRIGLEVEL, string::f("Tigger level -%c-", c + 0x31), " %", 0.0f, 10.0f, 0.0f);
		configParam(PLAY_PARAM + cp, 0.0f, 1.0f, PlayHead::DEFAULT_PLAY, string::f("Play -%c-", c + 0x31));
		configParam(FREEZE_PARAM + cp, 0.0f, 1.0f, PlayHead::DEFAULT_FREEZE, string::f("Freeze -%c-", c + 0x31));
		configParam(LOOP_PARAM + cp, 0.0f, 2.0f, PlayHead::DEFAULT_LOOP, string::f("Sustain/Loop -%c-", c + 0x31));
		configParam(SYNC_PARAM + cp, 0.0f, 
		#ifdef SM_PRO 
		1.0f 
		#else 
		0.0f
		#endif 
		, PlayHead::DEFAULT_SYNC, string::f("Sync -%c-", c + 0x31));
		configParam(LOCK_PARAM + cp, 0.0f, 1.0f, PlayHead::DEFAULT_LOCK, string::f("Lock -%c-", c + 0x31));
		configParam(AUDITION_PARAM + cp, 0.0f, 1.0f, 0.0f, string::f("Audition sidechain -%c-", c + 0x31));
		configParam(PREV_NEXT_PRE_SHA + 0 + cp, 0.0f, 10.0f, 0.0f, string::f("Previous preset -%c-", c + 0x31));
		configParam(PREV_NEXT_PRE_SHA + 1 + cp, 0.0f, 10.0f, 0.0f, string::f("Next preset -%c-", c + 0x31));
		configParam(PREV_NEXT_PRE_SHA + 2 + cp, 0.0f, 10.0f, 0.0f, string::f("Previous shape -%c-", c + 0x31));
		configParam(PREV_NEXT_PRE_SHA + 3 + cp, 0.0f, 10.0f, 0.0f, string::f("Next shape -%c-", c + 0x31));
	}
	// everything below this is not a channel-specific param 
	configParam(RESET_PARAM, 0.0f, 1.0f, 0.0f, "Reset");
	configParam(RUN_PARAM, 0.0f, 1.0f, 0.0f, "Run");
	configParam(GEAR_PARAM, 0.0f, 1.0f, 0.0f, "Sidechain settings");// controls channel-specific values though
	
	for (int c = 0; c < 8; c++) {
		channels[c].construct(c, &running, &sosEosEoc, &clockDetector, &inputs[0], &outputs[0], &params[0], &paramQuantities, &presetAndShapeManager);
	}
	presetAndShapeManager.construct(channels, &channelDirtyCache, &miscSettings3);
	channelDirtyCache.construct(0, &running, NULL, NULL, &inputs[0], &outputs[0], channelDirtyCacheParams, NULL, NULL);

	onReset();
}



void ShapeMaster::onReset() {
	running = true;
	clockDetector.onReset();
	miscSettings.cc4[0] = 0x3;// detailsShow
	miscSettings.cc4[1] = 0x0;// knob display colors
	miscSettings.cc4[2] = 0x0;// scope settings
	miscSettings.cc4[3] = 0x1;// show shape points
	miscSettings2.cc4[0] = 0x0;// global invert shadow
	miscSettings2.cc4[1] = 0x1;// global run-off setting (0 is freeze, 1 is stop and reset)
	miscSettings2.cc4[2] = 0x0;// show channel names
	miscSettings2.cc4[3] = 0x0;// point tooltip
	miscSettings3.cc4[0] = 0x0;// preset EOC deferral
	miscSettings3.cc4[1] = 0x0;// shape EOC deferral
	miscSettings3.cc4[2] = 0x0;// cloaked mode
	miscSettings3.cc4[3] = 0x0;// unused
	lineWidth = 1.0f;
	for (int c = 0; c < NUM_CHAN; c++) {
		channels[c].onReset(false);
	}
	channelDirtyCache.onReset(true);
	currChan = 0;
	resetNonJson();
}



json_t* ShapeMaster::dataToJson() {
	json_t *rootJ = json_object();

	// running
	json_object_set_new(rootJ, "running", json_boolean(running));
	
	// clockDetector
	clockDetector.dataToJson(rootJ);

	// miscSettings
	json_object_set_new(rootJ, "miscSettings", json_integer(miscSettings.cc1));

	// miscSettings2
	json_object_set_new(rootJ, "miscSettings2", json_integer(miscSettings2.cc1));

	// miscSettings3
	json_object_set_new(rootJ, "miscSettings3", json_integer(miscSettings3.cc1));

	// lineWidth
	json_object_set_new(rootJ, "lineWidth", json_real(lineWidth));

	// channels
	json_t* channelsJ = json_array();
	for (size_t c = 0; c < 8; c++) {
		json_t* channelJ = channels[c].dataToJsonChannel(WITHOUT_PARAMS, WITH_PRO_UNSYNC_MATCH, WITH_FULL_SETTINGS);
		json_array_insert_new(channelsJ, c , channelJ);
	}
	json_object_set_new(rootJ, "channels", channelsJ);
	
	// currChan
	json_object_set_new(rootJ, "currChan", json_integer(currChan));

	return rootJ;
}


void ShapeMaster::dataFromJson(json_t *rootJ) {
	// running
	json_t *runningJ = json_object_get(rootJ, "running");
	if (runningJ)
		running = json_is_true(runningJ);

	// clockDetector
	clockDetector.dataFromJson(rootJ);

	// miscSettings
	json_t *miscSettingsJ = json_object_get(rootJ, "miscSettings");
	if (miscSettingsJ) miscSettings.cc1 = json_integer_value(miscSettingsJ);

	// miscSettings2
	json_t *miscSettings2J = json_object_get(rootJ, "miscSettings2");
	if (miscSettings2J) miscSettings2.cc1 = json_integer_value(miscSettings2J);

	// miscSettings3
	json_t *miscSettings3J = json_object_get(rootJ, "miscSettings3");
	if (miscSettings3J) miscSettings3.cc1 = json_integer_value(miscSettings3J);

	// lineWidth
	json_t *lineWidthJ = json_object_get(rootJ, "lineWidth");
	if (lineWidthJ) lineWidth = json_number_value(lineWidthJ);

	// channels
	json_t* channelsJ = json_object_get(rootJ, "channels");
	if (channelsJ && json_is_array(channelsJ)) {
		for (size_t c = 0; c < std::min((size_t)NUM_CHAN, json_array_size(channelsJ)); c++) {
			json_t* channelJ = json_array_get(channelsJ, c);
			channels[c].dataFromJsonChannel(channelJ, WITHOUT_PARAMS, ISNOT_DIRTY_CACHE_LOAD, WITH_FULL_SETTINGS);
		}
	}

	// currChan
	json_t *currChanJ = json_object_get(rootJ, "currChan");
	if (currChanJ) currChan = json_integer_value(currChanJ);

	resetNonJson();
}



void ShapeMaster::process(const ProcessArgs &args) {
	#ifdef SM_PRO
	expPresentRight = (rightExpander.module && rightExpander.module->model == modelShapeMasterTriggerExpander);
	expPresentLeft = (leftExpander.module && leftExpander.module->model == modelShapeMasterCvExpander);	
	CvExpInterface *cvExp = expPresentLeft ? (CvExpInterface*)leftExpander.consumerMessage : NULL;
	#include "../Pro1.hpp"
	#else
	expPresentRight = false;
	expPresentLeft = false;
	CvExpInterface *cvExp = NULL;
	#endif
	
	sosEosEoc = 0;
	
	// Slow inputs and params
	if (refresh.processInputs()) {
		// channels' vca output size and processSlow()
		int scChan = inputs[SIDECHAIN_INPUT].getChannels();
		for (int c = 0; c < NUM_CHAN; c++) 
		{
			if (outputs[OUT_OUTPUTS + c].isConnected()) {
				int inChans = inputs[IN_INPUTS + c].getChannels();
				inChans = std::min(inChans, polyModeChanOut[channels[c].getPolyMode()]);
				int scChanC = channels[c].getTrigMode() == TM_SC ? (scChan > c ? 1 : 0) : 0;
				int outChans = std::max(inChans, scChanC);
				// here outChans can be 0 (if no sc conditions nor vca input)
				outputs[OUT_OUTPUTS + c].setChannels(outChans);// true channels will be 0 even if outChans > 0, since unconnected output forces 0 channels
			}
			channels[c].processSlow(cvExp ? &(cvExp->chanCvs[c]) : NULL);
		}
	}// refresh.processInputs()

	// Run
	if (runTrigger.process(params[RUN_PARAM].getValue() + inputs[RUN_INPUT].getVoltage())) {
		processRunToggled();
	}
	
	// Clock (with no -1 allowed in ClockDetector::clockCount
	bool clockRisingEdge = clockTrigger.process(inputs[CLOCK_INPUT].getVoltage());
	if (clockIgnoreOnReset != 0) {
		clockRisingEdge = false;
		
	}
	if (running) {
		clockDetector.process(clockRisingEdge);
	}
	

	// Reset
	if (resetTrigger.process(params[RESET_PARAM].getValue() + inputs[RESET_INPUT].getVoltage())) {
		resetLight = 1.0f;
		clockDetector.resetClockDetector();
		for (int c = 0; c < NUM_CHAN; c++) {
			channels[c].initRun(true);
		}
		clockIgnoreOnReset = (long) (0.001f * args.sampleRate);
		clockTrigger.reset();
		presetAndShapeManager.clearAllWorkloads();
	}	

	// Main process
	for (int c = 0; c < NUM_CHAN; c++) {
		channels[c].process(c == fsDiv8, cvExp ? &(cvExp->chanCvs[c]) : NULL);
	}
	
	// Scope
	scopeBuffers.populate(&channels[currChan], miscSettings.cc4[2]);
	
	// Lights
	if (refresh.processLights()) {
		// Reset light
		lights[RESET_LIGHT].setSmoothBrightness(resetLight, args.sampleTime * (RefreshCounter::displayRefreshStepSkips >> 2));	
		resetLight = 0.0f;
					
		// Run light
		lights[RUN_LIGHT].setBrightness(running ? 1.0f : 0.0f);
		
		// Deferral light
		for (int i = 0; i < 4; i++) {
			lights[DEFERRAL_LIGHTS + i].setBrightness(presetAndShapeManager.isDeferred(currChan, i) ? 1.0f : 0.0f);
		}
		
		// SC lights
		lights[SC_HPF_LIGHT].setBrightness(channels[currChan].getTrigMode() == TM_SC && channels[currChan].isHpfCutoffActive() ? 1.0f : 0.0f);
		lights[SC_LPF_LIGHT].setBrightness(channels[currChan].getTrigMode() == TM_SC && channels[currChan].isLpfCutoffActive() ? 1.0f : 0.0f);
		
	}// refresh.processLights()

	fsDiv8 = ((fsDiv8 + 1) % 8);
	if (clockIgnoreOnReset > 0l) {
		clockIgnoreOnReset--;
	}
	
	// To Trig Expander
	#ifdef SM_PRO
	if (rightExpander.module && rightExpander.module->model == modelShapeMasterTriggerExpander) {
		TrigExpInterface* messagesToExpander = (TrigExpInterface*)(rightExpander.module->leftExpander.producerMessage);
		messagesToExpander->sosEosEoc = sosEosEoc;
		rightExpander.module->leftExpander.messageFlipRequested = true;
	}
	#endif
}// process()


void ShapeMaster::processRunToggled() {
	presetAndShapeManager.clearAllWorkloads();
	running = !running;
	if (running) {// run turned on
		clockIgnoreOnReset = (long) (0.001f * APP->engine->getSampleRate());
	}
	else {// run turned off
		clockDetector.resetClockDetector();
	}
	if (miscSettings2.cc4[1] != 0) {
		// reset when run toggled (need both!)
		for (int c = 0; c < NUM_CHAN; c++) {
			channels[c].initRun(!running);// slow-slew when run turned off
		}
	}
	else {
		// freeze/resume when run toggled
		for (int c = 0; c < NUM_CHAN; c++) {
			channels[c].updateRunning();
		}
	}
}


//-----------------------------------------------------------------------------


void ShapeMasterWidget::appendContextMenu(Menu *menu) {
	ShapeMaster* module = (ShapeMaster*)(this->module);
	assert(module);

	menu->addChild(new MenuSeparator());
	
	#ifdef SM_PRO
	PpqnItem<ShapeMaster> *ppqnCItem = createMenuItem<PpqnItem<ShapeMaster>>("Clock PPQN", RIGHT_ARROW);
	ppqnCItem->srcShapeMaster = module;
	ppqnCItem->disabled = module->running;
	menu->addChild(ppqnCItem);

	// PpqnAvgItem<ShapeMaster> *ppqnAItem = createMenuItem<PpqnAvgItem<ShapeMaster>>("Clock averaging", RIGHT_ARROW);
	// ppqnAItem->srcShapeMaster = module;
	// ppqnAItem->disabled = module->running;
	// menu->addChild(ppqnAItem);
	#endif

	RunOffSettingItem *runOffSetItem = createMenuItem<RunOffSettingItem>("Run off setting", RIGHT_ARROW);
	runOffSetItem->srcRunOffSetting = &(module->miscSettings2.cc4[1]);
	menu->addChild(runOffSetItem);


	menu->addChild(new MenuSeparator());

	ShowChanNamesItem *showChanNamesItem = createMenuItem<ShowChanNamesItem>("Show channel labels", CHECKMARK(module->miscSettings2.cc4[2] != 0));
	showChanNamesItem->srcShowChanNames = &(module->miscSettings2.cc4[2]);
	menu->addChild(showChanNamesItem);

	ShowPointTooltipItem *showPtToolItem = createMenuItem<ShowPointTooltipItem>("Show node tooltip", CHECKMARK(module->miscSettings2.cc4[3] != 0));
	showPtToolItem->srcPointTooltipNames = &(module->miscSettings2.cc4[3]);
	menu->addChild(showPtToolItem);

	ShowPointsItem *showPointsItem = createMenuItem<ShowPointsItem>("Show shape nodes", CHECKMARK(module->miscSettings.cc4[3] != 0));
	showPointsItem->setValSrc = &(module->miscSettings.cc4[3]);
	menu->addChild(showPointsItem);
	
	LineWidthSlider *lineWidthSlider = new LineWidthSlider(&module->lineWidth);
	lineWidthSlider->box.size.x = 200.0f;
	menu->addChild(lineWidthSlider);
	
	InvShadowItem *invShadItem = createMenuItem<InvShadowItem>("Shadow", RIGHT_ARROW);
	invShadItem->srcInvShadow = &(module->miscSettings2.cc4[0]);
	invShadItem->isGlobal = true;
	menu->addChild(invShadItem);

	KnobDispColorItem *dispColItem = createMenuItem<KnobDispColorItem>("Knob label display colour", RIGHT_ARROW);
	dispColItem->srcColor = &(module->miscSettings.cc4[1]);
	menu->addChild(dispColItem);

	KnobArcShowItem *knobArcShowItem = createMenuItem<KnobArcShowItem>("Knob arcs", RIGHT_ARROW);
	knobArcShowItem->srcDetailsShow = &(module->miscSettings.cc4[0]);
	menu->addChild(knobArcShowItem);	

	CloakedItem *cloakItem = createMenuItem<CloakedItem>("Cloaked mode", CHECKMARK(module->miscSettings3.cc4[2] != 0));
	cloakItem->srcCloaked = &(module->miscSettings3.cc4[2]);
	menu->addChild(cloakItem);
}



ShapeMasterWidget::ShapeMasterWidget(ShapeMaster *module) {
	setModule(module);

	// Main panels from Inkscape
	setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/ShapeMaster.svg")));
	SvgPanel* svgPanel = (SvgPanel*)getPanel();
	panelBorder = findBorder(svgPanel->fb);

	// Left side (audio, trig, clock, reset, run inputs)

	static constexpr float insY = 13.82f;
	static constexpr float insDY = 10.5f;
	static constexpr float trigInX = 7.62f;
	static constexpr float audioInX = 20.32f;

	for (int i = 0; i < 8; i++) {
		addInput(createInputCentered<MmPort>(mm2px(Vec(audioInX, insY + insDY * i)), module, IN_INPUTS + i));
		addInput(createInputCentered<MmPort>(mm2px(Vec(trigInX, insY + insDY * i)), module, TRIG_INPUTS + i));
	}
	#ifdef SM_PRO
	addInput(createInputCentered<MmPort>(mm2px(Vec(13.97f, 98.29f)), module, CLOCK_INPUT));
	#endif
	addInput(createInputCentered<MmPort>(mm2px(Vec(7.62f, 110.48f)), module, RESET_INPUT));
	addInput(createInputCentered<MmPort>(mm2px(Vec(20.32f, 110.48f)), module, RUN_INPUT));
	// reset led button
	addParam(createParamCentered<LedButton2>(mm2px(Vec(5.9f, 100.0f)), module, RESET_PARAM));
	addChild(createLightCentered<SmallLight<RedLight>>(mm2px(Vec(5.9f, 100.0f)), module, ShapeMaster::RESET_LIGHT));
	// run led button
	SmRunButton* runButton;
	addParam(runButton = createParamCentered<SmRunButton>(mm2px(Vec(22.04f, 100.0f)), module, RUN_PARAM));
	if (module) {
		runButton->shapeMasterSrc = module;
	}
	addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(22.04f, 100.0f)), module, ShapeMaster::RUN_LIGHT));
	
	const int NUM_SM_LABELS = 20;
	SmLabelBase* smLabels[NUM_SM_LABELS];// index 0 is KnobLabelLength



	// Main section (display)

	static constexpr float aboveDispY = 12.0f;

	// channel buttons
	SmChannelButton* channelButton;
	addChild(channelButton = createWidgetCentered<SmChannelButton>(mm2px(Vec(27.94f + 53.792f / 2.0f, aboveDispY))));
	if (module) {
		channelButton->channels = module->channels;
		channelButton->currChan = &(module->currChan);
		channelButton->miscSettings2GlobalSrc = &(module->miscSettings2);
		channelButton->trigExpPresentSrc = &(module->expPresentRight);
		// channelButton->channelCopyPasteCache = &(module->channelCopyPasteCache);
		channelButton->running = &(module->running);
	}
	
	// preset label
	addChild(smLabels[16] = createWidgetCentered<PresetLabel>(mm2px(Vec(103.52f, aboveDispY))));
	if (module) {
		((PresetLabel*)(smLabels[16]))->presetAndShapeManager = &(module->presetAndShapeManager);
		((PresetLabel*)(smLabels[16]))->presetOrShapeDirty = &(presetOrShapeDirty);
		((PresetLabel*)(smLabels[16]))->unsupportedSync = &(unsupportedSync);
	}
	// shape label
	addChild(smLabels[17] = createWidgetCentered<ShapeLabel>(mm2px(Vec(145.1f, aboveDispY))));
	if (module) {
		((ShapeLabel*)(smLabels[17]))->presetAndShapeManager = &(module->presetAndShapeManager);
		((ShapeLabel*)(smLabels[17]))->presetOrShapeDirty = &(presetOrShapeDirty);
	}
	// deferral lights
	addChild(createLightCentered<TinyLight<TRedLight<SmModuleLightWidget>>>(mm2px(Vec(103.52f - 40.0f / 2.0f + 1.05f, aboveDispY - 2.0f)), module, ShapeMaster::DEFERRAL_LIGHTS + 0));	
	addChild(createLightCentered<TinyLight<TRedLight<SmModuleLightWidget>>>(mm2px(Vec(103.52f + 40.0f / 2.0f - 1.05f, aboveDispY - 2.0f)), module, ShapeMaster::DEFERRAL_LIGHTS + 1));	
	addChild(createLightCentered<TinyLight<TRedLight<SmModuleLightWidget>>>(mm2px(Vec(145.1f - 40.0f / 2.0f + 1.05f, aboveDispY - 2.0f)), module, ShapeMaster::DEFERRAL_LIGHTS + 2));	
	addChild(createLightCentered<TinyLight<TRedLight<SmModuleLightWidget>>>(mm2px(Vec(145.1f + 40.0f / 2.0f - 1.05f, aboveDispY - 2.0f)), module, ShapeMaster::DEFERRAL_LIGHTS + 3));	
	
	// prev/next preset/shape arrow buttons
	for (int c = 0; c < 8; c++) {
		int cp = c * NUM_CHAN_PARAMS;
		// preset left (prev)
		addParam(arrowButtons[c][0] = createParamCentered<PresetOrShapeArrowButton>(mm2px(Vec(103.52f - 40.0f / 2.0f + 1.75f, aboveDispY)), module, PREV_NEXT_PRE_SHA + 0 + cp));
		// preset right (next)
		addParam(arrowButtons[c][1] = createParamCentered<PresetOrShapeArrowButton>(mm2px(Vec(103.52f + 40.0f / 2.0f - 1.75f, aboveDispY)), module, PREV_NEXT_PRE_SHA + 1 + cp));
		// shape left (prev)
		addParam(arrowButtons[c][2] = createParamCentered<PresetOrShapeArrowButton>(mm2px(Vec(145.1f - 40.0f / 2.0f + 1.75f, aboveDispY)), module, PREV_NEXT_PRE_SHA + 2 + cp));
		// shape right (next)
		addParam(arrowButtons[c][3] = createParamCentered<PresetOrShapeArrowButton>(mm2px(Vec(145.1f + 40.0f / 2.0f - 1.75f, aboveDispY)), module, PREV_NEXT_PRE_SHA + 3 + cp));
	}
	if (module) {
		for (int c = 0; c < 8; c++) {
			for (int i = 0; i < 4; i++) {
				arrowButtons[c][i]->buttonPressedSrc = module->channels[c].getArrowButtonClicked(i);
				arrowButtons[c][i]->presetAndShapeManager = &(module->presetAndShapeManager);
				arrowButtons[c][i]->chanNum = c;
			}
		}
	}

	// Scope settings button and Shape operations button
	ScopeSettingsButtons *scopeButtons;
	addChild(scopeButtons = createWidget<ScopeSettingsButtons>(mm2px(Vec(29.5f, 10.25f + 7.0f))));
	ShapeCommandsButtons *shapeButtons;
	addChild(shapeButtons = createWidget<ShapeCommandsButtons>(mm2px(Vec(106.75f, 10.25f + 7.0f))));
	if (module) {
		scopeButtons->settingSrc = &module->miscSettings.cc4[2];
		scopeButtons->currChan = &(module->currChan);
		scopeButtons->channels = module->channels;
		scopeButtons->scopeBuffers = &(module->scopeBuffers);
		shapeButtons->currChan = &(module->currChan);
		shapeButtons->channels = module->channels;
	}

	// Display area
	ShapeMasterDisplay* smDisplay;
	addChild(smDisplay = createWidgetCentered<ShapeMasterDisplay>(mm2px(Vec(96.52f, 45.0f + 7.0f))));
	ShapeMasterDisplayLight* smDisplayLight;
	addChild(smDisplayLight = createWidgetCentered<ShapeMasterDisplayLight>(mm2px(Vec(96.52f, 45.0f + 7.0f))));
	smDisplay->shaY = smDisplayLight->shaY;
	smDisplayLight->hoverPtSelect = &(smDisplay->hoverPtSelect);
	smDisplayLight->dragPtSelect = &(smDisplay->dragPtSelect);
	if (module) {
		smDisplay->currChan = &(module->currChan);
		smDisplay->channels = module->channels;
		smDisplay->lineWidthSrc = &(module->lineWidth);
		smDisplay->setting3Src = &(module->miscSettings3);

		smDisplayLight->currChan = &(module->currChan);
		smDisplayLight->channels = module->channels;
		smDisplayLight->displayInfo = &displayInfo;
		smDisplayLight->settingSrc = &(module->miscSettings);
		smDisplayLight->setting2Src = &(module->miscSettings2);
		smDisplayLight->setting3Src = &(module->miscSettings3);
		smDisplayLight->lineWidthSrc = &(module->lineWidth);
		smDisplayLight->scopeBuffers = &(module->scopeBuffers);
	}

	// Screen - Big Numbers
	BigNumbers* bigNumbers;
	addChild(bigNumbers = createWidgetCentered<BigNumbers>(mm2px(Vec(96.52f, 79.0f))));
	if (module) {
		bigNumbers->currChan = &(module->currChan);
		bigNumbers->channels = module->channels;
		bigNumbers->displayInfo = &displayInfo;
	}


	// Buttons below display
	static constexpr float belowDispY = 79.3f;

	// play, freeze and loop
	for (int c = 0; c < 8; c++) {
		int cp = c * NUM_CHAN_PARAMS;
		addParam(smButtons[c][3] = createParamCentered<SmPlayButton>(mm2px(Vec(35.02f, belowDispY)), module, PLAY_PARAM + cp));
		addParam(smButtons[c][2] = createParamCentered<SmFreezeButton>(mm2px(Vec(48.16f, belowDispY)), module, FREEZE_PARAM + cp));
		addParam(smButtons[c][1] = createParamCentered<SmLoopButton>(mm2px(Vec(61.23f, belowDispY)), module, LOOP_PARAM + cp));
		if (module) {
			((SmPlayButton*)smButtons[c][3])->currChan = &(module->currChan);
			((SmPlayButton*)smButtons[c][3])->channels = module->channels;
			((SmLoopButton*)smButtons[c][1])->currChan = &(module->currChan);
			((SmLoopButton*)smButtons[c][1])->channels = module->channels;
		}
	}
	// GridX and range
	addChild(smLabels[18] = createWidgetCentered<GridXLabel>(mm2px(Vec(133.0f, belowDispY))));
	addChild(smLabels[19] = createWidgetCentered<RangeLabel>(mm2px(Vec(151.5f, belowDispY))));



	// Blocks
	static constexpr float knob0Y = 91.2f;
	static constexpr float knob1Y = 101.78f;
	static constexpr float knob2Y = 112.37f;
	
	static constexpr float rowTop = 92.27f - 0.25f;// for top row labels only
	static constexpr float rowMid = 102.85f - 0.25f;// for middle row labels only
	static constexpr float rowBot = 113.43f - 0.25f;// for bottom row labels only

	// Block 1 (sync, lock, trig mode, play mode)
	// sync and lock
	Vec syncPosVec = mm2px(Vec(37.77f, 92.27f));
	Vec lockPosVec = mm2px(Vec(49.64f, 92.27f));
	for (int c = 0; c < 8; c++) {
		int cp = c * NUM_CHAN_PARAMS;
		addParam(smButtons[c][4] = createParamCentered<SmSyncButton>(syncPosVec, module, SYNC_PARAM + cp));
		addParam(smButtons[c][0] = createParamCentered<SmLockButton>(lockPosVec, module, LOCK_PARAM + cp));
		if (module) {
			((SmSyncButton*)smButtons[c][4])->currChan = &(module->currChan);
			((SmSyncButton*)smButtons[c][4])->channels = module->channels;
			((SmSyncButton*)smButtons[c][4])->inClock = &(module->inputs[CLOCK_INPUT]);
			((SmSyncButton*)smButtons[c][4])->displayInfo = &displayInfo;
			
		}
	}
	// trig and play modes
	addChild(smLabels[13] = createWidgetCentered<TrigModeLabel>(mm2px(Vec(34.9f, rowMid))));
	addChild(smLabels[14] = createWidgetCentered<PlayModeLabel>(mm2px(Vec(34.9f + 12.12f, rowMid))));
	// repetitions
	addChild(smLabels[15] = createWidgetCentered<KnobLabelRepetitions>(mm2px(Vec(47.0f, rowBot))));
	for (int c = 0; c < 8; c++) {
		addParam(smKnobs[c][14] = createParamCentered<SmRepetitionsKnob>(mm2px(Vec(34.95f, knob2Y)), module, REPETITIONS_PARAM + c * NUM_CHAN_PARAMS));
	}


	// Block 2 (length, offset, swing)
	for (int c = 0; c < 8; c++) {
		int cp = c * NUM_CHAN_PARAMS;
		addParam(smKnobs[c][0] = createParamCentered<SmLengthSyncKnob>(mm2px(Vec(63.2f, knob0Y)), module, LENGTH_SYNC_PARAM + cp));
		addParam(smKnobs[c][1] = createParamCentered<SmLengthUnsyncKnob>(mm2px(Vec(63.2f, knob0Y)), module, LENGTH_UNSYNC_PARAM + cp));
		addParam(smKnobs[c][2] = createParamCentered<SmOffsetKnob>(mm2px(Vec(74.25f, knob1Y)), module, OFFSET_PARAM + cp));
		addParam(smKnobs[c][3] = createParamCentered<SmSwingKnob>(mm2px(Vec(63.2f, knob2Y)), module, SWING_PARAM + cp));
	}
	// labels
	addChild(smLabels[0] = createWidgetCentered<KnobLabelLength>(mm2px(Vec(74.88f - .3f, rowTop))));
	addChild(smLabels[1] = createWidgetCentered<KnobLabelOffset>(mm2px(Vec(62.56f, rowMid))));
	addChild(smLabels[2] = createWidgetCentered<KnobLabelSwing>(mm2px(Vec(74.88f, rowBot))));
	if (module) {
		((KnobLabelLength*)smLabels[0])->lengthSyncParamSrc = &(module->params[LENGTH_SYNC_PARAM]);
		((KnobLabelLength*)smLabels[0])->lengthUnsyncParamSrc = &(module->params[LENGTH_UNSYNC_PARAM]);
	}


	// Block 3 (phase, response, warp)
	for (int c = 0; c < 8; c++) {
		int cp = c * NUM_CHAN_PARAMS;
		addParam(smKnobs[c][4] = createParamCentered<SmPhaseKnob>(mm2px(Vec(91.0f, knob0Y)), module, PHASE_PARAM + cp));
		addParam(smKnobs[c][5] = createParamCentered<SmResponseKnob>(mm2px(Vec(102.04f, knob1Y)), module, RESPONSE_PARAM + cp));
		addParam(smKnobs[c][6] = createParamCentered<SmWarpKnob>(mm2px(Vec(91.0f, knob2Y)), module, WARP_PARAM + cp));
	}
	// labels
	addChild(smLabels[3] = createWidgetCentered<KnobLabelPhase>(mm2px(Vec(102.68f, rowTop))));
	addChild(smLabels[4] = createWidgetCentered<KnobLabelResponse>(mm2px(Vec(90.35f, rowMid))));
	addChild(smLabels[5] = createWidgetCentered<KnobLabelWarp>(mm2px(Vec(102.68f, rowBot))));


	// Block 4 (amount, slew, smooth)
	for (int c = 0; c < 8; c++) {
		int cp = c * NUM_CHAN_PARAMS;
		addParam(smKnobs[c][7] = createParamCentered<SmLevelKnob>(mm2px(Vec(118.79f, knob0Y)), module, AMOUNT_PARAM + cp));
		addParam(smKnobs[c][8] = createParamCentered<SmSlewKnob>(mm2px(Vec(129.83f, knob1Y)), module, SLEW_PARAM + cp));
		addParam(smKnobs[c][9] = createParamCentered<SmSmoothKnob>(mm2px(Vec(118.79f, knob2Y)), module, SMOOTH_PARAM + cp));
	}
	// labels
	addChild(smLabels[6] = createWidgetCentered<KnobLabelLevel>(mm2px(Vec(130.48f, rowTop))));
	addChild(smLabels[7] = createWidgetCentered<KnobLabelSlew>(mm2px(Vec(118.14f, rowMid))));
	addChild(smLabels[8] = createWidgetCentered<KnobLabelSmooth>(mm2px(Vec(130.48f, rowBot))));

	// Block 5 (crossover, high, low)
	for (int c = 0; c < 8; c++) {
		int cp = c * NUM_CHAN_PARAMS;
		addParam(smKnobs[c][10] = createParamCentered<SmCrossoverKnob>(mm2px(Vec(146.61f, knob0Y)), module, CROSSOVER_PARAM + cp));
		addParam(smKnobs[c][11] = createParamCentered<SmHighKnob>(mm2px(Vec(157.65f, knob1Y)), module, HIGH_PARAM + cp));
		addParam(smKnobs[c][12] = createParamCentered<SmLowKnob>(mm2px(Vec(146.61f, knob2Y)), module, LOW_PARAM + cp));
	}
	// labels
	addChild(smLabels[9] = createWidgetCentered<KnobLabelCrossover>(mm2px(Vec(158.3f, rowTop))));
	addChild(smLabels[10] = createWidgetCentered<KnobLabelHigh>(mm2px(Vec(145.96f, rowMid))));
	addChild(smLabels[11] = createWidgetCentered<KnobLabelLow>(mm2px(Vec(158.3f, rowBot))));

	// knob arcs
	#ifdef SM_PRO
	if (module) {
		for (int c = 0; c < 8; c++) {
			// warp knobs
			smKnobs[c][6]->paramWithCV = &(module->channels[c].warpPhaseResponseAmountWithCv[0]);
			smKnobs[c][6]->paramCvConnected = &(module->channels[c].warpPhaseResponseAmountCvConnected);
			// phase knobs
			smKnobs[c][4]->paramWithCV = &(module->channels[c].warpPhaseResponseAmountWithCv[1]);
			smKnobs[c][4]->paramCvConnected = &(module->channels[c].warpPhaseResponseAmountCvConnected);
			// response knobs
			smKnobs[c][5]->paramWithCV = &(module->channels[c].warpPhaseResponseAmountWithCv[2]);
			smKnobs[c][5]->paramCvConnected = &(module->channels[c].warpPhaseResponseAmountCvConnected);
			// amount knobs
			smKnobs[c][7]->paramWithCV = &(module->channels[c].warpPhaseResponseAmountWithCv[3]);
			smKnobs[c][7]->paramCvConnected = &(module->channels[c].warpPhaseResponseAmountCvConnected);
			// crossover knobs
			smKnobs[c][10]->paramWithCV = &(module->channels[c].xoverSlewWithCv[0]);
			smKnobs[c][10]->paramCvConnected = &(module->channels[c].xoverSlewCvConnected);
			// high knobs
			smKnobs[c][11]->paramWithCV = &(module->channels[c].xoverSlewWithCv[1]);
			smKnobs[c][11]->paramCvConnected = &(module->channels[c].xoverSlewCvConnected);
			// low knobs
			smKnobs[c][12]->paramWithCV = &(module->channels[c].xoverSlewWithCv[2]);
			smKnobs[c][12]->paramCvConnected = &(module->channels[c].xoverSlewCvConnected);
			// slew knobs
			smKnobs[c][8]->paramWithCV = &(module->channels[c].xoverSlewWithCv[3]);
			smKnobs[c][8]->paramCvConnected = &(module->channels[c].xoverSlewCvConnected);
			// offset knobs
			smKnobs[c][2]->paramWithCV = &(module->channels[c].getPlayHead()->offsetSwingLoopsWithCv[0]);
			smKnobs[c][2]->paramCvConnected = &(module->channels[c].getPlayHead()->offsetSwingLoopsCvConnected);
			// swing knobs
			smKnobs[c][3]->paramWithCV = &(module->channels[c].getPlayHead()->offsetSwingLoopsWithCv[1]);
			smKnobs[c][3]->paramCvConnected = &(module->channels[c].getPlayHead()->offsetSwingLoopsCvConnected);
			// length sync knobs
			smKnobs[c][0]->paramWithCV = &(module->channels[c].getPlayHead()->lengthSyncWithCv);
			smKnobs[c][0]->paramCvConnected = &(module->channels[c].getPlayHead()->lengthCvConnected);
			// length unsync knobs
			smKnobs[c][1]->paramWithCV = &(module->channels[c].getPlayHead()->lengthUnsyncWithCv);
			smKnobs[c][1]->paramCvConnected = &(module->channels[c].getPlayHead()->lengthCvConnected);
		}
	}		
	#endif


	// Right side (audio and cv outputs, sidechain section)
	static constexpr float outsY = 13.82f;
	static constexpr float outsDY = 10.5f;
	static constexpr float audioOutX = 172.72f;
	static constexpr float cvOutX = 185.42f;

	// outputs
	for (int i = 0; i < 8; i++) {
		addOutput(createOutputCentered<MmPort>(mm2px(Vec(audioOutX, outsY + outsDY * i)), module, OUT_OUTPUTS + i));
		addOutput(createOutputCentered<MmPort>(mm2px(Vec(cvOutX, outsY + outsDY * i)), module, CV_OUTPUTS + i));
	}

	// audition and sidechain
	for (int c = 0; c < 8; c++) {
		addParam(smButtons[c][5] = createParamCentered<SmAuditionScButton>(mm2px(Vec(170.89f, 101.87f)), module, AUDITION_PARAM + c * NUM_CHAN_PARAMS));
	}
	addInput(createInputCentered<MmPortGold>(mm2px(Vec(179.07f, 102.11f)), module, SIDECHAIN_INPUT));
	addChild(createLightCentered<TinyLight<GreenLight>>(mm2px(Vec(179.07f - 4.17f, 102.11f - 4.5f)), module, ShapeMaster::SC_HPF_LIGHT));
	addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(179.07f + 4.17f, 102.11f - 4.5f)), module, ShapeMaster::SC_LPF_LIGHT));
	SmSidechainSettingsButton* sidechainSettingsButton;
	addParam(sidechainSettingsButton = createParamCentered<SmSidechainSettingsButton>(mm2px(Vec(187.24f, 101.87f)), module, GEAR_PARAM));
	if (module) {
		sidechainSettingsButton->currChan = &(module->currChan);
		sidechainSettingsButton->channels = module->channels;
	}

	for (int c = 0; c < 8; c++) {
		addParam(smKnobs[c][13] = createParamCentered<SmTrigLevelKnob>(mm2px(Vec(173.05f, knob2Y)), module, TRIGLEV_PARAM + c * NUM_CHAN_PARAMS));
	}
	// label
	addChild(smLabels[12] = createWidgetCentered<KnobLabelTrigLevel>(mm2px(Vec(173.05f + 11.69f, rowBot))));


	if (module) {
		// all knobs common
		for (int c = 0; c < 8; c++) {
			for (int i = 0; i < NUM_KNOB_PARAMS; i++) {
				smKnobs[c][i]->currChan = &(module->currChan);// will likely need this for CV arcs
				smKnobs[c][i]->channels = module->channels;// will likely need this for CV arcs
				smKnobs[c][i]->detailsShowSrc = &(module->miscSettings.cc4[0]);
				smKnobs[c][i]->cloakedModeSrc = &(module->cloakedMode);
				smKnobs[c][i]->displayInfo = &displayInfo;
				smKnobs[c][i]->setVisible((c == 0) && (i != 1));
			}
		}
		// all buttons common
		for (int c = 0; c < 8; c++) {
			for (int i = 0; i < NUM_BUTTON_PARAMS; i++) {
				smButtons[c][i]->setVisible(c == 0);
			}
			for (int i = 0; i < NUM_ARROW_PARAMS; i++) {
				arrowButtons[c][i]->setVisible(c == 0);
			}
		}
		// all labels common
		for (int i = 0; i < NUM_SM_LABELS; i++) {
			smLabels[i]->knobLabelColorsSrc = &(module->miscSettings.cc4[1]);// unused in [18] and [19] since they are in the display color (GridX and Range)
			smLabels[i]->currChan = &(module->currChan);
			smLabels[i]->channels = module->channels;
		}
	}
	
	
	#ifdef SM_PRO
	ProSvgWithMessage* proSvgWithMessage;
	addChild(proSvgWithMessage = createWidgetCentered<ProSvgWithMessage>(mm2px(Vec(119.86f, 4.96f))));
	proSvgWithMessage->displayInfo = &displayInfo;
	if (!module) {
		addChild(createWidgetCentered<SyncSvg>(syncPosVec));
		addChild(createWidgetCentered<LockSvg>(lockPosVec));
	}
	#include "../Pro2.hpp"
	#endif
}


void ShapeMasterWidget::step() {
	ShapeMaster* module = (ShapeMaster*)(this->module);

	if (module) {
		int chan = module->currChan;

		bool syncedLengthVisible = module->params[SYNC_PARAM + chan * NUM_CHAN_PARAMS].getValue() >= 0.5f;// 1 for pro, 0 for non-pro
		smKnobs[chan][0]->setVisible(syncedLengthVisible);
		smKnobs[chan][1]->setVisible(!syncedLengthVisible);
		smButtons[chan][0]->setVisible(syncedLengthVisible);// lock button
		
		// update visibility for multi controls when user selects another channel
		if (oldVisibleChannel != chan) {
			// make new chan's knobs and buttons visible (length knobs done outside of guard since sync button can change)
			for (int i = 2; i < NUM_KNOB_PARAMS; i++) {
				smKnobs[chan][i]->setVisible(true);
			}
			for (int i = 1; i < NUM_BUTTON_PARAMS; i++) {
				smButtons[chan][i]->setVisible(true);
			}
			for (int i = 0; i < NUM_ARROW_PARAMS; i++) {
				arrowButtons[chan][i]->setVisible(true);
			}
			// make old chan's knobs and buttons invisible
			for (int i = 0; i < NUM_KNOB_PARAMS; i++) {
				smKnobs[oldVisibleChannel][i]->setVisible(false);
			}
			for (int i = 0; i < NUM_BUTTON_PARAMS; i++) {
				smButtons[oldVisibleChannel][i]->setVisible(false);
			}
			for (int i = 0; i < NUM_ARROW_PARAMS; i++) {
				arrowButtons[oldVisibleChannel][i]->setVisible(false);
			}
			oldVisibleChannel = chan;
		}
		
		// Preset dirty check (current channel only)
		if ((stepDivider & 0x7) == 0) {
			std::string currChanPresetPath = module->channels[chan].getPresetPath();
			std::string currChanShapePath = module->channels[chan].getShapePath();
			if (!currChanPresetPath.empty()) {
				if (!(module->channelDirtyCache.getPresetPath() == currChanPresetPath)) {
					if (!loadPresetOrShape(currChanPresetPath, &module->channelDirtyCache, true, &unsupportedSync, false)) {
						currChanPresetPath = "";
						module->channels[chan].setPresetPath(currChanPresetPath);
					}
				}
				if (!currChanPresetPath.empty()) {
					presetOrShapeDirty = module->channels[chan].isDirty(&module->channelDirtyCache);
				}
			}
			else if (!currChanShapePath.empty()) {
				if (!(module->channelDirtyCache.getShapePath() == currChanShapePath)) {
					if (!loadPresetOrShape(currChanShapePath, &module->channelDirtyCache, false, NULL, false)) {
						currChanShapePath = "";
						module->channels[chan].setShapePath(currChanShapePath);
					}	
				}
				if (!currChanShapePath.empty()) {
					presetOrShapeDirty = module->channels[chan].isDirtyShape(&module->channelDirtyCache);
				}
			}
			else {
				presetOrShapeDirty = false;
			}
		}
		
		// Borders	
		int newSizeAdd = 0;
		if (module->expPresentLeft) {
			newSizeAdd = 3;
		}
		if (module->expPresentRight) {
			newSizeAdd += 4;// needs to be different than left so triggers dirty and no zoom bug
		}
		if (panelBorder && panelBorder->box.size.x != (box.size.x + newSizeAdd)) {
			panelBorder->box.pos.x = (module->expPresentLeft ? -3 : 0);
			panelBorder->box.size.x = (box.size.x + newSizeAdd);
			SvgPanel* svgPanel = (SvgPanel*)getPanel();
			svgPanel->fb->dirty = true;
		}
	}
	
	stepDivider++;
	if (stepDivider == 64) {
		stepDivider = 0;
	}

	ModuleWidget::step();
}


#ifdef SM_PRO
Model *modelShapeMaster = createModel<ShapeMaster, ShapeMasterWidget>("ShapeMasterPro");
#else
Model *modelShapeMaster = createModel<ShapeMaster, ShapeMasterWidget>("ShapeMaster");
#endif
