//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.txt for all licenses
//***********************************************************************************************


#include "MixerWidgets.hpp"


struct MixMaster : Module {
	// Expander
	float rightMessages[2][MFA_NUM_VALUES] = {};// messages from aux-expander (first index is page), see enum called MotherFromAuxIds in Mixer.hpp


	// Constants
	int numChannels16 = 16;// avoids warning that happens when hardcode 16 (static const or directly use 16 in code below)

	// Need to save, no reset
	int panelTheme;
	
	// Need to save, with reset
	alignas(4) char trackLabels[4 * 20 + 1];// 4 chars per label, 16 tracks and 4 groups means 20 labels, null terminate the end the whole array only
	GlobalInfo gInfo;
	MixerTrack tracks[16];
	MixerGroup groups[4];
	MixerAux aux[4];
	MixerMaster master;
	
	// No need to save, with reset
	int updateTrackLabelRequest;// 0 when nothing to do, 1 for read names in widget
	float values80[80];
	float values20[20];

	// No need to save, no reset
	RefreshCounter refresh;	
	int panelThemeWithAuxPresent = 0;
	bool auxExpanderPresent = false;// can't be local to process() since widget must know in order to properly draw border
	float trackTaps[16 * 2 * 4];// room for 4 taps for each of the 16 stereo tracks
	float trackInsertOuts[16 * 2];// room for 16 stereo track insert outs
	float groupTaps[4 * 2 * 4];// room for 4 taps for each of the 4 stereo groups
	float groupAuxInsertOuts[8 * 2];// room for 4 stereo group insert outs and 4 stereo aux insert outs
	float auxTaps[4 * 2 * 4];// room for 4 taps for each of the 4 stereo aux
	float *auxSends;// index into correct page of messages from expander (avoid having separate buffers)
	float *auxReturns;// index into correct page of messages from expander (avoid having separate buffers)
	// std::string busId;

		
	MixMaster() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);		
		
		rightExpander.producerMessage = rightMessages[0];
		rightExpander.consumerMessage = rightMessages[1];

		char strBuf[32];
		// Track
		float maxTGFader = std::pow(GlobalInfo::trkAndGrpFaderMaxLinearGain, 1.0f / GlobalInfo::trkAndGrpFaderScalingExponent);
		for (int i = 0; i < 16; i++) {
			// Pan
			snprintf(strBuf, 32, "Track #%i pan", i + 1);
			configParam(TRACK_PAN_PARAMS + i, 0.0f, 1.0f, 0.5f, strBuf, "%", 0.0f, 200.0f, -100.0f);
			// Fader
			snprintf(strBuf, 32, "Track #%i level", i + 1);
			configParam(TRACK_FADER_PARAMS + i, 0.0f, maxTGFader, 1.0f, strBuf, " dB", -10, 20.0f * GlobalInfo::trkAndGrpFaderScalingExponent);
			// Mute
			snprintf(strBuf, 32, "Track #%i mute", i + 1);
			configParam(TRACK_MUTE_PARAMS + i, 0.0f, 1.0f, 0.0f, strBuf);
			// Solo
			snprintf(strBuf, 32, "Track #%i solo", i + 1);
			configParam(TRACK_SOLO_PARAMS + i, 0.0f, 1.0f, 0.0f, strBuf);
			// Group select
			snprintf(strBuf, 32, "Track #%i group", i + 1);
			configParam(GROUP_SELECT_PARAMS + i, 0.0f, 4.0f, 0.0f, strBuf);
		}
		// Group
		for (int i = 0; i < 4; i++) {
			// Pan
			snprintf(strBuf, 32, "Group #%i pan", i + 1);
			configParam(GROUP_PAN_PARAMS + i, 0.0f, 1.0f, 0.5f, strBuf, "%", 0.0f, 200.0f, -100.0f);
			// Fader
			snprintf(strBuf, 32, "Group #%i level", i + 1);
			configParam(GROUP_FADER_PARAMS + i, 0.0f, maxTGFader, 1.0f, strBuf, " dB", -10, 20.0f * GlobalInfo::trkAndGrpFaderScalingExponent);
			// Mute
			snprintf(strBuf, 32, "Group #%i mute", i + 1);
			configParam(GROUP_MUTE_PARAMS + i, 0.0f, 1.0f, 0.0f, strBuf);
			// Solo
			snprintf(strBuf, 32, "Group #%i solo", i + 1);
			configParam(GROUP_SOLO_PARAMS + i, 0.0f, 1.0f, 0.0f, strBuf);
		}
		float maxMFader = std::pow(MixerMaster::masterFaderMaxLinearGain, 1.0f / MixerMaster::masterFaderScalingExponent);
		configParam(MAIN_FADER_PARAM, 0.0f, maxMFader, 1.0f, "Master level", " dB", -10, 20.0f * MixerMaster::masterFaderScalingExponent);
		// Mute
		configParam(MAIN_MUTE_PARAM, 0.0f, 1.0f, 0.0f, "Master mute");
		// Dim
		configParam(MAIN_DIM_PARAM, 0.0f, 1.0f, 0.0f, "Master dim");
		// Solo
		configParam(MAIN_MONO_PARAM, 0.0f, 1.0f, 0.0f, "Master mono");
		

		gInfo.construct(&params[0]);
		for (int i = 0; i < 16; i++) {
			tracks[i].construct(i, &gInfo, &inputs[0], &params[0], &(trackLabels[4 * i]), &trackTaps[i << 1], &trackInsertOuts[i << 1]);
		}
		for (int i = 0; i < 4; i++) {
			groups[i].construct(i, &gInfo, &inputs[0], &params[0], &(trackLabels[4 * (16 + i)]), &groupTaps[i << 1], &groupAuxInsertOuts[i << 1]);
			aux[i].construct(i, &gInfo, &inputs[0], values20, &auxTaps[i << 1], &groupAuxInsertOuts[8 + (i << 1)]);
		}
		master.construct(&gInfo, &params[0], &inputs[0]);
		onReset();

		panelTheme = 0;//(loadDarkAsDefault() ? 1 : 0);

		// busId = messages->registerMember();
	}
  
	// ~MixMaster() {
		// messages->deregisterMember(busId);
	// }

	
	void onReset() override {
		snprintf(trackLabels, 4 * 20 + 1, "-01--02--03--04--05--06--07--08--09--10--11--12--13--14--15--16-GRP1GRP2GRP3GRP4");	
		gInfo.onReset();
		for (int i = 0; i < 16; i++) {
			tracks[i].onReset();
		}
		for (int i = 0; i < 4; i++) {
			groups[i].onReset();
			aux[i].onReset();
		}
		master.onReset();
		resetNonJson(false);
	}
	void resetNonJson(bool recurseNonJson) {
		updateTrackLabelRequest = 1;
		if (recurseNonJson) {
			gInfo.resetNonJson();
			for (int i = 0; i < 16; i++) {
				tracks[i].resetNonJson();
			}
			for (int i = 0; i < 4; i++) {
				groups[i].resetNonJson();
				aux[i].resetNonJson();
			}
			master.resetNonJson();
		}
		for (int i = 0; i < 80; i++) {
			values80[i] = 0.0f;
		}
		for (int i = 0; i < 20; i++) {
			values20[i] = 0.0f;
		}
	}


	void onRandomize() override {
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// trackLabels
		json_object_set_new(rootJ, "trackLabels", json_string(trackLabels));

		// gInfo
		gInfo.dataToJson(rootJ);

		// tracks
		for (int i = 0; i < 16; i++) {
			tracks[i].dataToJson(rootJ);
		}
		// groups/aux
		for (int i = 0; i < 4; i++) {
			groups[i].dataToJson(rootJ);
			aux[i].dataToJson(rootJ);
		}
		// master
		master.dataToJson(rootJ);
		
		return rootJ;
	}


	void dataFromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// trackLabels
		json_t *textJ = json_object_get(rootJ, "trackLabels");
		if (textJ)
			snprintf(trackLabels, 4 * 20 + 1, "%s", json_string_value(textJ));
		
		// gInfo
		gInfo.dataFromJson(rootJ);

		// tracks
		for (int i = 0; i < 16; i++) {
			tracks[i].dataFromJson(rootJ);
		}
		// groups/aux
		for (int i = 0; i < 4; i++) {
			groups[i].dataFromJson(rootJ);
			aux[i].dataFromJson(rootJ);
		}
		// master
		master.dataFromJson(rootJ);
		
		resetNonJson(true);
	}


	void onSampleRateChange() override {
		gInfo.sampleTime = APP->engine->getSampleTime();
		for (int trk = 0; trk < 16; trk++) {
			tracks[trk].onSampleRateChange();
		}
		master.onSampleRateChange();
	}
	

	void process(const ProcessArgs &args) override {
		
		auxExpanderPresent = (rightExpander.module && rightExpander.module->model == modelAuxExpander);
		
		//********** Inputs **********
		
		// From Aux-Expander
		if (auxExpanderPresent) {
			float *messagesFromExpander = (float*)rightExpander.consumerMessage;// could be invalid pointer when !expanderPresent, so read it only when expanderPresent
			
			auxReturns = &messagesFromExpander[AFM_AUX_RETURNS]; // contains 8 values of the returns from the aux panel
			
			int value80i = clamp((int)(messagesFromExpander[AFM_VALUE80_INDEX]), 0, 79);
			values80[value80i] = messagesFromExpander[AFM_VALUE80];
			
			int value20i = clamp((int)(messagesFromExpander[AFM_VALUE20_INDEX]), 0, 19);
			values20[value20i] = messagesFromExpander[AFM_VALUE20];
		}
		
		
		if (refresh.processInputs()) {
			panelThemeWithAuxPresent = (auxExpanderPresent ? panelTheme : -1);
			
			int trackToProcess = refresh.refreshCounter >> 4;// Corresponds to 172Hz refreshing of each track, at 44.1 kHz
			
			// Tracks
			gInfo.updateSoloBit(trackToProcess);
			tracks[trackToProcess].updateSlowValues();// a track is updated once every 16 passes in input proceesing
			// Groups/Aux
			if ( (trackToProcess & 0x3) == 0) {// a group is updated once every 16 passes in input proceesing
				gInfo.updateSoloBit(16 + (trackToProcess >> 2));
				groups[trackToProcess >> 2].updateSlowValues();
				aux[trackToProcess >> 2].updateSlowValues();
			}
			// Master
			if ((trackToProcess & 0x3) == 1) {// master updated once every 4 passes in input proceesing
				master.updateSlowValues();
			}
			
			// EQ Expander message bus test
			// Message<Payload> *message = messages->receive("1");	
			// if (message != NULL) {
				// params[TRACK_PAN_PARAMS + 0].setValue(message->value.values[0]);
				// delete message;
			// }
			
		}// userInputs refresh
		
		
		
		//********** Outputs **********

		float mix[10] = {0.0f};// room for main and groups
		
		// Tracks
		for (int i = 0; i < 16; i++) {
			tracks[i].process(mix);
		}
		memcpy(groupTaps, &mix[2], 8 * 4);// TODO: change memory layout so that this is not necessary
		// Groups
		for (int i = 0; i < 4; i++) {
			groups[i].process(mix);
		}
		// Master
		master.process(mix);
		// Aux
		if (auxExpanderPresent) {
			memcpy(auxTaps, auxReturns, 8 * 4);// TODO: change memory layout so that this is not necessary		
			for (int i = 0; i < 4; i++) {
				aux[i].process(mix);
			}
		}
		
		// Set master outputs
		outputs[MAIN_OUTPUTS + 0].setVoltage(mix[0]);
		outputs[MAIN_OUTPUTS + 1].setVoltage(mix[1]);
		
		// Direct outs
		SetDirectTrackOuts(0);// 1-8
		SetDirectTrackOuts(8);// 9-16
		SetDirectGroupOuts();
		SetDirectAuxOuts();// this uses one of the taps in the aux return signal flow (same signal flow as a group), and choice of tap is same as other diretct outs
				
		// Insert outs
		SetInsertTrackOuts(0);// 1-8
		SetInsertTrackOuts(8);// 9-16
		SetInsertGroupAuxOuts();	



		//********** Lights **********
		
		bool slowExpander = false;		
		if (refresh.processLights()) {
			slowExpander = true;
			for (int i = 0; i < 16; i++) {
				lights[TRACK_HPF_LIGHTS + i].setBrightness(tracks[i].getHPFCutoffFreq() >= MixerTrack::minHPFCutoffFreq ? 1.0f : 0.0f);
				lights[TRACK_LPF_LIGHTS + i].setBrightness(tracks[i].getLPFCutoffFreq() <= MixerTrack::maxLPFCutoffFreq ? 1.0f : 0.0f);
			}
		}
		
		
		//********** Expanders **********
		
		// To Aux-Expander
		if (auxExpanderPresent) {
			float *messageToExpander = (float*)(rightExpander.module->leftExpander.producerMessage);
			
			// Slow
			
			uint32_t* updateSlow = (uint32_t*)(&messageToExpander[AFM_UPDATE_SLOW]);
			if (slowExpander) {
				// Track names
				*updateSlow = 1;
				memcpy(&messageToExpander[AFM_TRACK_GROUP_NAMES], trackLabels, 4 * 20);
				int32_t tmp = panelTheme;
				// Panel theme
				memcpy(&messageToExpander[AFM_PANEL_THEME], &tmp, 4);
				// Color theme
				memcpy(&messageToExpander[AFM_COLOR_AND_CLOAK], &gInfo.colorAndCloak.cc1, 4);
			}
			else {
				*updateSlow = 0;
			}
			
			// Fast
			
			// Aux sends
			auxSends = &messageToExpander[AFM_AUX_SENDS];// room for 8 values of the sends to the aux panel (left A, right A, left B, right B ...)
			// populate auxSends[0..7]: Take the trackTaps/groupTaps indicated by the Aux sends mode (with per-track option) and combine with the 80 send floats to form the 4 stereo sends
			for (int i = 0; i < 8; i++) {
				auxSends[i] = 0.0f;
			}
			// accumulate tracks
			for (int trk = 0; trk < 16; trk++) {
				if ((int)(tracks[trk].paGroup->getValue() + 0.5f) != 0) continue;
				int tapIndex = gInfo.auxSendsMode < 4 ? gInfo.auxSendsMode : tracks[trk].auxSendsMode;
				tapIndex <<= 5;
				float valTap[2] = {trackTaps[tapIndex + (trk << 1) + 0], trackTaps[tapIndex + (trk << 1) + 1]};
				for (int aux = 0; aux < 4; aux++) {
					auxSends[(aux << 1) + 0] += values80[(trk << 2) + aux] * valTap[0];
					auxSends[(aux << 1) + 1] += values80[(trk << 2) + aux] * valTap[1];
				}
			}
			// accumulate groups
			for (int grp = 0; grp < 4; grp++) {
				int tapIndex = gInfo.auxSendsMode < 4 ? gInfo.auxSendsMode : groups[grp].auxSendsMode;
				tapIndex <<= 3;
				float valTap[2] = {groupTaps[tapIndex + (grp << 1) + 0], groupTaps[tapIndex + (grp << 1) + 1]};
				for (int aux = 0; aux < 4; aux++) {
					auxSends[(aux << 1) + 0] += values80[64 + (grp << 2) + aux] * valTap[0];
					auxSends[(aux << 1) + 1] += values80[64 + (grp << 2) + aux] * valTap[1];
				}
			}
						
			// Aux VUs
			for (int i = 0; i < 4; i++) {
				messageToExpander[AFM_AUX_VUS + (i << 1) + 0] = auxTaps[24 + (i << 1) + 0];// send tap 4 of the aux return signal flows
				messageToExpander[AFM_AUX_VUS + (i << 1) + 1] = auxTaps[24 + (i << 1) + 1];// send tap 4 of the aux return signal flows
			}
			
			rightExpander.module->leftExpander.messageFlipRequested = true;
		}

		
	}// process()
	
	
	void SetDirectTrackOuts(const int base) {// base is 0 or 8
		int outi = base >> 3;
		if (outputs[DIRECT_OUTPUTS + outi].isConnected()) {
			outputs[DIRECT_OUTPUTS + outi].setChannels(numChannels16);

			if (gInfo.directOutsMode < 4) {// global direct outs
				int tapIndex = gInfo.directOutsMode;
				memcpy(outputs[DIRECT_OUTPUTS + outi].getVoltages(), &trackTaps[(tapIndex << 5) + (base << 1)], 4 * 16);
			}
			else {// per track direct outs
				for (unsigned int i = 0; i < 8; i++) {
					int tapIndex = tracks[base + i].directOutsMode;
					int offset = (tapIndex << 5) + ((base + i) << 1);
					outputs[DIRECT_OUTPUTS + outi].setVoltage(trackTaps[offset + 0], 2 * i);
					outputs[DIRECT_OUTPUTS + outi].setVoltage(trackTaps[offset + 1], 2 * i + 1);
				}
			}
		}
	}
	
	void SetDirectGroupOuts() {
		if (outputs[DIRECT_OUTPUTS + 2].isConnected()) {
			outputs[DIRECT_OUTPUTS + 2].setChannels(8);

			if (gInfo.directOutsMode < 4) {// global direct outs
				int tapIndex = gInfo.directOutsMode;
				memcpy(outputs[DIRECT_OUTPUTS + 2].getVoltages(), &groupTaps[(tapIndex << 3)], 4 * 8);
			}
			else {// per group direct outs
				for (unsigned int i = 0; i < 4; i++) {
					int tapIndex = groups[i].directOutsMode;
					int offset = (tapIndex << 3) + (i << 1);
					outputs[DIRECT_OUTPUTS + 2].setVoltage(groupTaps[offset + 0], 2 * i);
					outputs[DIRECT_OUTPUTS + 2].setVoltage(groupTaps[offset + 1], 2 * i + 1);
				}
			}
		}
	}
	
	void SetDirectAuxOuts() {
		if (outputs[DIRECT_OUTPUTS + 3].isConnected()) {
			outputs[DIRECT_OUTPUTS + 3].setChannels(8);

			if (gInfo.directOutsMode < 4) {// global direct outs
				int tapIndex = gInfo.directOutsMode;
				memcpy(outputs[DIRECT_OUTPUTS + 3].getVoltages(), &auxTaps[(tapIndex << 3)], 4 * 8);
			}
			else {// per aux direct outs
				for (unsigned int i = 0; i < 4; i++) {
					int tapIndex = aux[i].directOutsMode;
					int offset = (tapIndex << 3) + (i << 1);
					outputs[DIRECT_OUTPUTS + 3].setVoltage(auxTaps[offset + 0], 2 * i);
					outputs[DIRECT_OUTPUTS + 3].setVoltage(auxTaps[offset + 1], 2 * i + 1);
				}
			}		
		}
	}

	
	void SetInsertTrackOuts(const int base) {// base is 0 or 8
		int outi = base >> 3;
		if (outputs[INSERT_TRACK_OUTPUTS + outi].isConnected()) {
			outputs[INSERT_TRACK_OUTPUTS + outi].setChannels(numChannels16);
			memcpy(outputs[INSERT_TRACK_OUTPUTS + outi].getVoltages(), &trackInsertOuts[(base << 1)], 4 * 16);
		}
	}

	void SetInsertGroupAuxOuts() {
		if (outputs[INSERT_GRP_AUX_OUTPUT].isConnected()) {
			outputs[INSERT_GRP_AUX_OUTPUT].setChannels(numChannels16);
			memcpy(outputs[INSERT_GRP_AUX_OUTPUT].getVoltages(), &groupAuxInsertOuts[0], 4 * 16);
		}
	}

};



struct MixMasterWidget : ModuleWidget {
	TrackDisplay* trackDisplays[16];
	GroupDisplay* groupDisplays[4];
	PortWidget* inputWidgets[16 * 4];// Left, Right, Volume, Pan
	PanelBorder* panelBorder;
	bool oldAuxExpanderPresent = false;
	


	// Module's context menu
	// --------------------

	void appendContextMenu(Menu *menu) override {
		MixMaster *module = dynamic_cast<MixMaster*>(this->module);
		assert(module);

		menu->addChild(new MenuLabel());// empty line
		
		MenuLabel *settingsALabel = new MenuLabel();
		settingsALabel->text = "Settings (audio)";
		menu->addChild(settingsALabel);
		
		TapModeItem *directOutsItem = createMenuItem<TapModeItem>("Direct outs", RIGHT_ARROW);
		directOutsItem->tapModePtr = &(module->gInfo.directOutsMode);
		directOutsItem->isGlobal = true;
		menu->addChild(directOutsItem);
		
		if (module->auxExpanderPresent) {
			TapModeItem *auxSendsItem = createMenuItem<TapModeItem>("Aux sends", RIGHT_ARROW);
			auxSendsItem->tapModePtr = &(module->gInfo.auxSendsMode);
			auxSendsItem->isGlobal = true;
			menu->addChild(auxSendsItem);
		}
		
		ChainItem *chainItem = createMenuItem<ChainItem>("Chain input", RIGHT_ARROW);
		chainItem->gInfo = &(module->gInfo);
		menu->addChild(chainItem);
		
		PanLawMonoItem *panLawMonoItem = createMenuItem<PanLawMonoItem>("Mono pan law", RIGHT_ARROW);
		panLawMonoItem->gInfo = &(module->gInfo);
		menu->addChild(panLawMonoItem);
		
		PanLawStereoItem *panLawStereoItem = createMenuItem<PanLawStereoItem>("Stereo pan mode", RIGHT_ARROW);
		panLawStereoItem->panLawStereoSrc = &(module->gInfo.panLawStereo);
		panLawStereoItem->isGlobal = true;
		menu->addChild(panLawStereoItem);
		
		SymmetricalFadeItem *symItem = createMenuItem<SymmetricalFadeItem>("Symmetrical fade", CHECKMARK(module->gInfo.symmetricalFade));
		symItem->gInfo = &(module->gInfo);
		menu->addChild(symItem);
		
		menu->addChild(new MenuLabel());// empty line
		
		MenuLabel *settingsVLabel = new MenuLabel();
		settingsVLabel->text = "Settings (visual)";
		menu->addChild(settingsVLabel);
		
		DispColorItem *dispColItem = createMenuItem<DispColorItem>("Display colour", RIGHT_ARROW);
		dispColItem->srcColor = &(module->gInfo.colorAndCloak.cc4[dispColor]);
		menu->addChild(dispColItem);
		
		VuColorItem *vuColItem = createMenuItem<VuColorItem>("VU colour", RIGHT_ARROW);
		vuColItem->srcColor = &(module->gInfo.colorAndCloak.cc4[vuColor]);
		vuColItem->isGlobal = true;
		menu->addChild(vuColItem);
		
		CloakedModeItem *nightItem = createMenuItem<CloakedModeItem>("Cloaked mode", CHECKMARK(module->gInfo.colorAndCloak.cc4[cloakedMode]));
		nightItem->gInfo = &(module->gInfo);
		menu->addChild(nightItem);
	}

	// Module's widget
	// --------------------

	MixMasterWidget(MixMaster *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/mixmaster.svg")));
		panelBorder = findBorder(panel);		
		
		// Inserts and CVs
		static const float xIns = 13.8;
		// Insert outputs
		addOutput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xIns, 12.8)), false, module, INSERT_TRACK_OUTPUTS + 0, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 1)), false, module, INSERT_TRACK_OUTPUTS + 1, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 2)), false, module, INSERT_GRP_AUX_OUTPUT, module ? &module->panelTheme : NULL));
		// Insert inputs
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 3)), true, module, INSERT_TRACK_INPUTS + 0, module ? &module->panelTheme : NULL));
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 4)), true, module, INSERT_TRACK_INPUTS + 1, module ? &module->panelTheme : NULL));
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 5)), true, module, INSERT_GRP_AUX_INPUT, module ? &module->panelTheme : NULL));
		// Insert inputs
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 7)), true, module, TRACK_MUTE_INPUT, module ? &module->panelTheme : NULL));
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 8)), true, module, TRACK_SOLO_INPUT, module ? &module->panelTheme : NULL));
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 9)), true, module, GRPM_MUTESOLO_INPUT, module ? &module->panelTheme : NULL));
		
		
		// Tracks
		static const float xTrck1 = 11.43 + 20.32;
		for (int i = 0; i < 16; i++) {
			// Labels
			addChild(trackDisplays[i] = createWidgetCentered<TrackDisplay>(mm2px(Vec(xTrck1 + 12.7 * i + 0.4, 4.7))));
			if (module) {
				trackDisplays[i]->gInfo = &(module->gInfo);
				trackDisplays[i]->colorAndCloak = &(module->gInfo.colorAndCloak);
				trackDisplays[i]->tracks = &(module->tracks[0]);
				trackDisplays[i]->trackNumSrc = i;
				trackDisplays[i]->updateTrackLabelRequestPtr = &(module->updateTrackLabelRequest);
				trackDisplays[i]->inputWidgets = inputWidgets;
				trackDisplays[i]->auxExpanderPresentPtr = &(module->auxExpanderPresent);
			}
			// HPF lights
			addChild(createLightCentered<TinyLight<GreenLight>>(mm2px(Vec(xTrck1 - 4.17 + 12.7 * i, 8.3)), module, TRACK_HPF_LIGHTS + i));	
			// LPF lights
			addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(xTrck1 + 4.17 + 12.7 * i, 8.3)), module, TRACK_LPF_LIGHTS + i));	
			// Left inputs
			addInput(inputWidgets[i + 0] = createDynamicPortCentered<DynPort>(mm2px(Vec(xTrck1 + 12.7 * i, 12.8)), true, module, TRACK_SIGNAL_INPUTS + 2 * i + 0, module ? &module->panelTheme : NULL));			
			// Right inputs
			addInput(inputWidgets[i + 16] = createDynamicPortCentered<DynPort>(mm2px(Vec(xTrck1 + 12.7 * i, 21.8)), true, module, TRACK_SIGNAL_INPUTS + 2 * i + 1, module ? &module->panelTheme : NULL));	
			// Volume inputs
			addInput(inputWidgets[i + 16 * 2] = createDynamicPortCentered<DynPort>(mm2px(Vec(xTrck1 + 12.7 * i, 31.5)), true, module, TRACK_VOL_INPUTS + i, module ? &module->panelTheme : NULL));			
			// Pan inputs
			addInput(inputWidgets[i + 16 * 3] = createDynamicPortCentered<DynPort>(mm2px(Vec(xTrck1 + 12.7 * i, 40.5)), true, module, TRACK_PAN_INPUTS + i, module ? &module->panelTheme : NULL));			
			// Pan knobs
			DynSmallKnobGreyWithPanCol *panKnobTrack;
			addParam(panKnobTrack = createDynamicParamCentered<DynSmallKnobGreyWithPanCol>(mm2px(Vec(xTrck1 + 12.7 * i, 51.8)), module, TRACK_PAN_PARAMS + i, module ? &module->panelTheme : NULL));
			if (module) {
				panKnobTrack->dispColorPtr = &(module->gInfo.colorAndCloak.cc4[dispColor]);
			}
			
			// Faders
			DynSmallFaderWithLink *newFader;
			addParam(newFader = createDynamicParamCentered<DynSmallFaderWithLink>(mm2px(Vec(xTrck1 + 3.67 + 12.7 * i, 81.2)), module, TRACK_FADER_PARAMS + i, module ? &module->panelTheme : NULL));
			if (module) {
				newFader->gInfo = &(module->gInfo);
				newFader->faderParams = &module->params[TRACK_FADER_PARAMS];
				// VU meters
				VuMeterTrack *newVU = createWidgetCentered<VuMeterTrack>(mm2px(Vec(xTrck1 + 12.7 * i, 81.2)));
				newVU->srcLevels = &(module->tracks[i].vu);
				newVU->colorThemeGlobal = &(module->gInfo.colorAndCloak.cc4[vuColor]);
				newVU->colorThemeLocal = &(module->tracks[i].vuColorThemeLocal);
				addChild(newVU);
				// Fade pointers
				FadePointerTrack *newFP = createWidgetCentered<FadePointerTrack>(mm2px(Vec(xTrck1 - 2.95 + 12.7 * i, 81.2)));
				newFP->srcParam = &(module->params[TRACK_FADER_PARAMS + i]);
				newFP->srcFadeGain = &(module->tracks[i].fadeGain);
				newFP->srcFadeRate = &(module->tracks[i].fadeRate);
				addChild(newFP);				
			}
			
			
			// Mutes
			DynMuteFadeButton* newMuteFade;
			addParam(newMuteFade = createDynamicParamCentered<DynMuteFadeButton>(mm2px(Vec(xTrck1 + 12.7 * i, 109.8)), module, TRACK_MUTE_PARAMS + i, module ? &module->panelTheme : NULL));
			if (module) {
				newMuteFade->type = &(module->tracks[i].fadeRate);
			}
			// Solos
			DynSoloButtonMutex *newSoloButton;
			addParam(newSoloButton = createDynamicParamCentered<DynSoloButtonMutex>(mm2px(Vec(xTrck1 + 12.7 * i, 116.1)), module, TRACK_SOLO_PARAMS + i, module ? &module->panelTheme : NULL));
			newSoloButton->soloParams =  module ? &module->params[TRACK_SOLO_PARAMS] : NULL;
			// Group dec
			DynGroupMinusButtonNotify *newGrpMinusButton;
			addChild(newGrpMinusButton = createDynamicWidgetCentered<DynGroupMinusButtonNotify>(mm2px(Vec(xTrck1 - 3.73 + 12.7 * i - 0.75, 123.1)), module ? &module->panelTheme : NULL));
			if (module) {
				newGrpMinusButton->sourceParam = &(module->params[GROUP_SELECT_PARAMS + i]);
			}
			// Group inc
			DynGroupPlusButtonNotify *newGrpPlusButton;
			addChild(newGrpPlusButton = createDynamicWidgetCentered<DynGroupPlusButtonNotify>(mm2px(Vec(xTrck1 + 3.77 + 12.7 * i + 0.75, 123.1)), module ? &module->panelTheme : NULL));
			if (module) {
				newGrpPlusButton->sourceParam = &(module->params[GROUP_SELECT_PARAMS + i]);
			}
			// Group select displays
			GroupSelectDisplay* groupSelectDisplay;
			addParam(groupSelectDisplay = createParamCentered<GroupSelectDisplay>(mm2px(Vec(xTrck1 + 12.7 * i - 0.1, 123.1)), module, GROUP_SELECT_PARAMS + i));
			if (module) {
				groupSelectDisplay->srcColor = &(module->gInfo.colorAndCloak);
			}
		}
		
		// Monitor outputs and groups
		static const float xGrp1 = 217.17 + 20.32;
		for (int i = 0; i < 4; i++) {
			// Monitor outputs
			if (i == 3) {
				addOutput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xGrp1 + 12.7 * i, 11.8)), false, module, DIRECT_OUTPUTS + i, module ? &module->panelThemeWithAuxPresent : NULL));
			}
			else {
				addOutput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xGrp1 + 12.7 * i, 11.8)), false, module, DIRECT_OUTPUTS + i, module ? &module->panelTheme : NULL));
			}
			// Labels
			addChild(groupDisplays[i] = createWidgetCentered<GroupDisplay>(mm2px(Vec(xGrp1 + 12.7 * i + 0.4, 23.5))));
			if (module) {
				groupDisplays[i]->gInfo = &(module->gInfo);
				groupDisplays[i]->colorAndCloak = &(module->gInfo.colorAndCloak);
				groupDisplays[i]->srcGroup = &(module->groups[i]);
				groupDisplays[i]->auxExpanderPresentPtr = &(module->auxExpanderPresent);
			}
			
			// Volume inputs
			addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(xGrp1 + 12.7 * i, 31.5)), true, module, GROUP_VOL_INPUTS + i, module ? &module->panelTheme : NULL));			
			// Pan inputs
			addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(xGrp1 + 12.7 * i, 40.5)), true, module, GROUP_PAN_INPUTS + i, module ? &module->panelTheme : NULL));			
			// Pan knobs
			DynSmallKnobGreyWithPanCol *panKnobGroup;
			addParam(panKnobGroup = createDynamicParamCentered<DynSmallKnobGreyWithPanCol>(mm2px(Vec(xGrp1 + 12.7 * i, 51.8)), module, GROUP_PAN_PARAMS + i, module ? &module->panelTheme : NULL));
			if (module) {
				panKnobGroup->dispColorPtr = &(module->gInfo.colorAndCloak.cc4[dispColor]);
			}
			
			// Faders
			DynSmallFaderWithLink *newFader;
			addParam(newFader = createDynamicParamCentered<DynSmallFaderWithLink>(mm2px(Vec(xGrp1 + 3.67 + 12.7 * i, 81.2)), module, GROUP_FADER_PARAMS + i, module ? &module->panelTheme : NULL));		
			if (module) {
				newFader->gInfo = &(module->gInfo);
				newFader->faderParams = &module->params[TRACK_FADER_PARAMS];
				// VU meters
				VuMeterTrack *newVU = createWidgetCentered<VuMeterTrack>(mm2px(Vec(xGrp1 + 12.7 * i, 81.2)));
				newVU->srcLevels = &(module->groups[i].vu);
				newVU->colorThemeGlobal = &(module->gInfo.colorAndCloak.cc4[vuColor]);
				newVU->colorThemeLocal = &(module->groups[i].vuColorThemeLocal);
				addChild(newVU);
				// Fade pointers
				FadePointerGroup *newFP = createWidgetCentered<FadePointerGroup>(mm2px(Vec(xGrp1 - 2.95 + 12.7 * i, 81.2)));
				newFP->srcParam = &(module->params[GROUP_FADER_PARAMS + i]);
				newFP->srcFadeGain = &(module->groups[i].fadeGain);
				newFP->srcFadeRate = &(module->groups[i].fadeRate);
				addChild(newFP);				
			}

			// Mutes
			DynMuteFadeButton* newMuteFade;
			addParam(newMuteFade = createDynamicParamCentered<DynMuteFadeButton>(mm2px(Vec(xGrp1 + 12.7 * i, 109.8)), module, GROUP_MUTE_PARAMS + i, module ? &module->panelTheme : NULL));
			if (module) {
				newMuteFade->type = &(module->groups[i].fadeRate);
			}
			// Solos
			DynSoloButtonMutex* newSoloButton;
			addParam(newSoloButton = createDynamicParamCentered<DynSoloButtonMutex>(mm2px(Vec(xGrp1 + 12.7 * i, 116.1)), module, GROUP_SOLO_PARAMS + i, module ? &module->panelTheme : NULL));
			newSoloButton->soloParams =  module ? &module->params[TRACK_SOLO_PARAMS] : NULL;
		}
		
		// Master inputs
		addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(289.62, 12.8)), true, module, CHAIN_INPUTS + 0, module ? &module->panelTheme : NULL));			
		addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(289.62, 21.8)), true, module, CHAIN_INPUTS + 1, module ? &module->panelTheme : NULL));			
		
		// Master outputs
		addOutput(createDynamicPortCentered<DynPort>(mm2px(Vec(300.12, 12.8)), false, module, MAIN_OUTPUTS + 0, module ? &module->panelTheme : NULL));			
		addOutput(createDynamicPortCentered<DynPort>(mm2px(Vec(300.12, 21.8)), false, module, MAIN_OUTPUTS + 1, module ? &module->panelTheme : NULL));			
		
		// Master label
		MasterDisplay *mastLabel;
		addChild(mastLabel = createWidgetCentered<MasterDisplay>(mm2px(Vec(294.82, 128.5 - 97.2 + 0.2))));
		if (module) {
			mastLabel->srcMaster = &(module->master);
		}
		
		// Master fader
		addParam(createDynamicParamCentered<DynBigFader>(mm2px(Vec(300.17, 70.3)), module, MAIN_FADER_PARAM, module ? &module->panelTheme : NULL));
		if (module) {
			// VU meter
			VuMeterMaster *newVU = createWidgetCentered<VuMeterMaster>(mm2px(Vec(294.82, 70.3)));
			newVU->srcLevels = &(module->master.vu);
			newVU->colorThemeGlobal = &(module->gInfo.colorAndCloak.cc4[vuColor]);
			newVU->colorThemeLocal = &(module->master.vuColorThemeLocal);
			addChild(newVU);
			// Fade pointer
			FadePointerMaster *newFP = createWidgetCentered<FadePointerMaster>(mm2px(Vec(294.82 - 3.4, 70.3)));
			newFP->srcParam = &(module->params[MAIN_FADER_PARAM]);
			newFP->srcFadeGain = &(module->master.fadeGain);
			newFP->srcFadeRate = &(module->master.fadeRate);
			addChild(newFP);				
		}
		
		// Master mute
		DynMuteFadeButton* newMuteFade;
		addParam(newMuteFade = createDynamicParamCentered<DynMuteFadeButton>(mm2px(Vec(294.82, 109.8)), module, MAIN_MUTE_PARAM, module ? &module->panelTheme : NULL));
		if (module) {
			newMuteFade->type = &(module->master.fadeRate);
		}
		
		// Master dim
		addParam(createDynamicParamCentered<DynDimButton>(mm2px(Vec(289.42, 116.1)), module, MAIN_DIM_PARAM, module ? &module->panelTheme : NULL));
		
		// Master mono
		addParam(createDynamicParamCentered<DynMonoButton>(mm2px(Vec(300.22, 116.1)), module, MAIN_MONO_PARAM, module ? &module->panelTheme : NULL));
	}
	
	void step() override {
		MixMaster* moduleM = (MixMaster*)module;
		if (moduleM) {
			// Track labels (pull from module)
			if (moduleM->updateTrackLabelRequest != 0) {// pull request from module
				// track displays
				for (int trk = 0; trk < 16; trk++) {
					trackDisplays[trk]->text = std::string(&(moduleM->trackLabels[trk * 4]), 4);
				}
				// group displays
				for (int grp = 0; grp < 4; grp++) {
					groupDisplays[grp]->text = std::string(&(moduleM->trackLabels[(16 + grp) * 4]), 4);
				}
				moduleM->updateTrackLabelRequest = 0;// all done pulling
			}
			
			// Borders
			if ( moduleM->auxExpanderPresent != oldAuxExpanderPresent ) {
				oldAuxExpanderPresent = moduleM->auxExpanderPresent;
			
				if (oldAuxExpanderPresent) {
					//panelBorder->box.pos.x = 0;
					panelBorder->box.size.x = box.size.x + 3;
				}
				else {
					//panelBorder->box.pos.x = 0;
					panelBorder->box.size.x = box.size.x;
				}
				((SvgPanel*)panel)->dirty = true;// weird zoom bug: if the if/else above is commented, zoom bug when this executes
			}			
		}			
		
		Widget::step();
	}
};

Model *modelMixMaster = createModel<MixMaster, MixMasterWidget>("MixMaster");
