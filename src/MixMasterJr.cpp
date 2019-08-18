//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.txt for all licenses
//***********************************************************************************************


#include "Mixer.hpp"
#include "MixerWidgets.hpp"
#include "MixerMenus.hpp"


struct MixMasterJr : Module {
	// Expander
	// float rightMessages[2][5] = {};// messages from expander


	// Constants
	int numChannelsDirectOuts = 16;// avoids warning when hardcode 16 (static const or directly use 16 in code below)

	// Need to save, no reset
	int panelTheme;
	
	// Need to save, with reset
	char trackLabels[4 * 20 + 1];// 4 chars per label, 16 tracks and 4 groups means 20 labels, null terminate the end the whole array only
	GlobalInfo gInfo;
	MixerTrack tracks[16];
	MixerGroup groups[4];
	MixerMaster master;
	
	// No need to save, with reset
	int resetTrackLabelRequest;// -1 when nothing to do, 0 to 15 for incremental read in widget


	// No need to save, no reset
	RefreshCounter refresh;	

		
	MixMasterJr() {
		config(NUM_PARAMS_JR, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);		
		
		char strBuf[32];
		// Track
		float maxTFader = std::pow(MixerTrack::trackFaderMaxLinearGain, 1.0f / MixerTrack::trackFaderScalingExponent);
		for (int i = 0; i < 16; i++) {
			// Pan
			snprintf(strBuf, 32, "Track #%i pan", i + 1);
			configParam(TRACK_PAN_PARAMS + i, 0.0f, 1.0f, 0.5f, strBuf, "%", 0.0f, 200.0f, -100.0f);
			// Fader
			snprintf(strBuf, 32, "Track #%i level", i + 1);
			configParam(TRACK_FADER_PARAMS + i, 0.0f, maxTFader, 1.0f, strBuf, " dB", -10, 20.0f * MixerTrack::trackFaderScalingExponent);
			// Mute
			snprintf(strBuf, 32, "Track #%i mute", i + 1);
			configParam(TRACK_MUTE_PARAMS + i, 0.0f, 1.0f, 0.0f, strBuf);
			// Solo
			snprintf(strBuf, 32, "Track #%i solo", i + 1);
			configParam(TRACK_SOLO_PARAMS + i, 0.0f, 1.0f, 0.0f, strBuf);
			// Group decrement
			snprintf(strBuf, 32, "Track #%i group -", i + 1);
			configParam(GRP_DEC_PARAMS + i, 0.0f, 1.0f, 0.0f, strBuf);
			// Group increment
			snprintf(strBuf, 32, "Track #%i group +", i + 1);
			configParam(GRP_INC_PARAMS + i, 0.0f, 1.0f, 0.0f, strBuf);
		}
		// Group
		float maxGFader = std::pow(MixerGroup::groupFaderMaxLinearGain, 1.0f / MixerGroup::groupFaderScalingExponent);
		for (int i = 0; i < 4; i++) {
			// Pan
			snprintf(strBuf, 32, "Group #%i pan", i + 1);
			configParam(GROUP_PAN_PARAMS + i, 0.0f, 1.0f, 0.5f, strBuf, "%", 0.0f, 200.0f, -100.0f);
			// Fader
			snprintf(strBuf, 32, "Group #%i level", i + 1);
			configParam(GROUP_FADER_PARAMS + i, 0.0f, maxGFader, 1.0f, strBuf, " dB", -10, 20.0f * MixerGroup::groupFaderScalingExponent);
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
			tracks[i].construct(i, &gInfo, &inputs[0], &params[0], &(trackLabels[4 * i]));
		}
		for (int i = 0; i < 4; i++) {
			groups[i].construct(i, &gInfo, &inputs[0], &params[0], &(trackLabels[4 * (16 + i)]));
		}
		master.construct(&gInfo, &params[0]);
		onReset();

		panelTheme = 0;//(loadDarkAsDefault() ? 1 : 0);
	}

	
	void onReset() override {
		snprintf(trackLabels, 4 * 20 + 1, "-01--02--03--04--05--06--07--08--09--10--11--12--13--14--15--16-GRP1GRP2GRP3GRP4");	
		gInfo.onReset();
		for (int i = 0; i < 16; i++) {
			tracks[i].onReset();
		}
		for (int i = 0; i < 4; i++) {
			groups[i].onReset();
		}
		master.onReset();
		resetNonJson(false);
	}
	void resetNonJson(bool recurseNonJson) {
		resetTrackLabelRequest = 0;// setting to 0 will trigger 1, 2, 3 etc on each video frame afterwards
		if (recurseNonJson) {
			gInfo.resetNonJson();
			for (int i = 0; i < 16; i++) {
				tracks[i].resetNonJson();
			}
			for (int i = 0; i < 4; i++) {
				groups[i].resetNonJson();
			}
			master.resetNonJson();
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
		// groups
		for (int i = 0; i < 4; i++) {
			groups[i].dataToJson(rootJ);
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
		// groups
		for (int i = 0; i < 4; i++) {
			groups[i].dataFromJson(rootJ);
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
		
		//********** Inputs **********
		
		if (refresh.processInputs()) {
			int trackToProcess = refresh.refreshCounter >> 4;// Corresponds to 172Hz refreshing of each track, at 44.1 kHz
			
			// Tracks
			gInfo.updateSoloBit(trackToProcess);
			tracks[trackToProcess].updateSlowValues();// a track is updated once every 16 passes in input proceesing
			// Groups
			if ( (trackToProcess & 0x3) == 0) {// a group is updated once every 16 passes in input proceesing
				gInfo.updateSoloBit(16 + (trackToProcess >> 2));
				groups[trackToProcess >> 2].updateSlowValues();
			}
			// Master
			if ((trackToProcess & 0x3) == 1) {// master updated once every 4 passes in input proceesing
				master.updateSlowValues();
			}
			
		}// userInputs refresh
		
		
		
		//********** Outputs **********

		float mix[10] = {0.0f};// room for main and groups
		
		// Tracks
		for (int i = 0; i < 16; i++) {
			tracks[i].process(mix);
		}
		// Groups
		for (int i = 0; i < 4; i++) {
			groups[i].process(mix);
		}
		// Master
		master.process(mix);
		
		// Set master outputs
		outputs[MAIN_OUTPUTS + 0].setVoltage(mix[0]);
		outputs[MAIN_OUTPUTS + 1].setVoltage(mix[1]);
		
		// Direct outs
		SetDirectTrackOuts(0);// P1-8
		SetDirectTrackOuts(8);// P9-16
		SetDirectGroupOuts(mix);// PGrp
				
				

		//********** Lights **********
				
		if (refresh.processLights()) {
			for (int i = 0; i < 16; i++) {
				lights[TRACK_HPF_LIGHTS + i].setBrightness(tracks[i].getHPFCutoffFreq() >= MixerTrack::minHPFCutoffFreq ? 1.0f : 0.0f);
				lights[TRACK_LPF_LIGHTS + i].setBrightness(tracks[i].getLPFCutoffFreq() <= MixerTrack::maxLPFCutoffFreq ? 1.0f : 0.0f);
			}
		}
		
	}// process()
	
	
	void SetDirectTrackOuts(const int base) {// base is 0 or 8
		int outi = base >> 3;
		if (outputs[DIRECT_OUTPUTS + outi].isConnected()) {
			outputs[DIRECT_OUTPUTS + outi].setChannels(numChannelsDirectOuts);

			for (unsigned int i = 0; i < 8; i++) {
				if (gInfo.directOutsMode == 1 || (gInfo.directOutsMode == 2 && tracks[base + i].directOutsMode == 1) ) {// post-fader
					outputs[DIRECT_OUTPUTS + outi].setVoltage(tracks[base + i].post[0], 2 * i);
					outputs[DIRECT_OUTPUTS + outi].setVoltage(tracks[base + i].post[1], 2 * i + 1);
				}
				else// pre-fader
				{
					outputs[DIRECT_OUTPUTS + outi].setVoltage(tracks[base + i].pre[0], 2 * i);
					outputs[DIRECT_OUTPUTS + outi].setVoltage(tracks[base + i].pre[1], 2 * i + 1);
				}
			}
		}
	}
	
	void SetDirectGroupOuts(float *mix) {
		if (outputs[DIRECT_OUTPUTS + 2].isConnected()) {
			outputs[DIRECT_OUTPUTS + 2].setChannels(8);

			for (unsigned int i = 0; i < 4; i++) {
				if (gInfo.directOutsMode == 1 || (gInfo.directOutsMode == 2 && groups[i].directOutsMode == 1) ) {// post-fader
					outputs[DIRECT_OUTPUTS + 2].setVoltage(groups[i].post[0], 2 * i);
					outputs[DIRECT_OUTPUTS + 2].setVoltage(groups[i].post[1], 2 * i + 1);
				}
				else// pre-fader
				{
					outputs[DIRECT_OUTPUTS + 2].setVoltage(mix[2 * i + 2], 2 * i);
					outputs[DIRECT_OUTPUTS + 2].setVoltage(mix[2 * i + 3], 2 * i + 1);
				}
			}
		}
	}

};



struct MixMasterJrWidget : ModuleWidget {
	TrackDisplay* trackDisplays[16];
	GroupDisplay* groupDisplays[4];
	GroupSelectDisplay* groupSelectDisplays[16];
	PortWidget* inputWidgets[16 * 4];// Left, Right, Volume, Pan


	// Module's context menu
	// --------------------

	void appendContextMenu(Menu *menu) override {
		MixMasterJr *module = dynamic_cast<MixMasterJr*>(this->module);
		assert(module);

		menu->addChild(new MenuLabel());// empty line
		
		MenuLabel *settingsALabel = new MenuLabel();
		settingsALabel->text = "Settings (audio)";
		menu->addChild(settingsALabel);
		
		DirectOutsItem *directOutsItem = createMenuItem<DirectOutsItem>("Direct outs", RIGHT_ARROW);
		directOutsItem->gInfo = &(module->gInfo);
		menu->addChild(directOutsItem);
		
		PanLawMonoItem *panLawMonoItem = createMenuItem<PanLawMonoItem>("Mono pan law", RIGHT_ARROW);
		panLawMonoItem->gInfo = &(module->gInfo);
		menu->addChild(panLawMonoItem);
		
		PanLawStereoItem *panLawStereoItem = createMenuItem<PanLawStereoItem>("Stereo pan mode", RIGHT_ARROW);
		panLawStereoItem->gInfo = &(module->gInfo);
		menu->addChild(panLawStereoItem);
		
		menu->addChild(new MenuLabel());// empty line
		
		MenuLabel *settingsVLabel = new MenuLabel();
		settingsVLabel->text = "Settings (visual)";
		menu->addChild(settingsVLabel);
		
		NightModeItem *nightItem = createMenuItem<NightModeItem>("Cloaked mode", CHECKMARK(module->gInfo.nightMode));
		nightItem->gInfo = &(module->gInfo);
		menu->addChild(nightItem);
		
		VuColorItem *vuColItem = createMenuItem<VuColorItem>("VU Colour", RIGHT_ARROW);
		vuColItem->srcColor = &(module->gInfo.vuColor);
		vuColItem->isGlobal = true;
		menu->addChild(vuColItem);
	}

	// Module's widget
	// --------------------

	MixMasterJrWidget(MixMasterJr *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/mixmaster-jr.svg")));
		
		// Tracks
		for (int i = 0; i < 16; i++) {
			// Labels
			addChild(trackDisplays[i] = createWidgetCentered<TrackDisplay>(mm2px(Vec(11.43 + 12.7 * i + 0.4, 4.2 + 0.5))));
			if (module) {
				trackDisplays[i]->tracks = &(module->tracks[0]);
				trackDisplays[i]->trackNumSrc = i;
				trackDisplays[i]->resetTrackLabelRequestPtr = &(module->resetTrackLabelRequest);
				trackDisplays[i]->inputWidgets = inputWidgets;
			}
			// HPF lights
			addChild(createLightCentered<TinyLight<GreenLight>>(mm2px(Vec(11.43 - 4.17 + 12.7 * i, 7.8 + 0.5)), module, TRACK_HPF_LIGHTS + i));	
			// LPF lights
			addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(11.43 + 4.17 + 12.7 * i, 7.8 + 0.5)), module, TRACK_LPF_LIGHTS + i));	
			// Left inputs
			addInput(inputWidgets[i + 0] = createDynamicPortCentered<DynPort>(mm2px(Vec(11.43 + 12.7 * i, 12 + 0.8)), true, module, TRACK_SIGNAL_INPUTS + 2 * i + 0, module ? &module->panelTheme : NULL));			
			// Right inputs
			addInput(inputWidgets[i + 16] = createDynamicPortCentered<DynPort>(mm2px(Vec(11.43 + 12.7 * i, 21 + 0.8)), true, module, TRACK_SIGNAL_INPUTS + 2 * i + 1, module ? &module->panelTheme : NULL));	
			// Volume inputs
			addInput(inputWidgets[i + 16 * 2] = createDynamicPortCentered<DynPort>(mm2px(Vec(11.43 + 12.7 * i, 30.7 + 0.8)), true, module, TRACK_VOL_INPUTS + i, module ? &module->panelTheme : NULL));			
			// Pan inputs
			addInput(inputWidgets[i + 16 * 3] = createDynamicPortCentered<DynPort>(mm2px(Vec(11.43 + 12.7 * i, 39.7 + 0.8)), true, module, TRACK_PAN_INPUTS + i, module ? &module->panelTheme : NULL));			
			// Pan knobs
			addParam(createDynamicParamCentered<DynSmallKnob>(mm2px(Vec(11.43 + 12.7 * i, 51 + 0.8)), module, TRACK_PAN_PARAMS + i, module ? &module->panelTheme : NULL));
			
			// Faders
			addParam(createDynamicParamCentered<DynSmallFader>(mm2px(Vec(15.1 + 12.7 * i, 80.4 + 0.8)), module, TRACK_FADER_PARAMS + i, module ? &module->panelTheme : NULL));
			if (module) {
				// VU meters
				VuMeterTrack *newVU = createWidgetCentered<VuMeterTrack>(mm2px(Vec(11.43 + 12.7 * i, 80.4 + 0.8)));
				newVU->srcLevels = &(module->tracks[i].vu[0]);
				newVU->colorThemeGlobal = &(module->gInfo.vuColor);
				addChild(newVU);
				// Fade pointers
				FadePointerTrack *newFP = createWidgetCentered<FadePointerTrack>(mm2px(Vec(11.43 - 2.95 + 12.7 * i, 80.4 + 0.8)));
				newFP->srcParam = &(module->params[TRACK_FADER_PARAMS + i]);
				newFP->srcFadeGain = &(module->tracks[i].fadeGain);
				newFP->srcFadeRate = &(module->tracks[i].fadeRate);
				addChild(newFP);				
			}
			
			
			// Mutes
			DynMuteFadeButton* newMuteFade;
			addParam(newMuteFade = createDynamicParamCentered<DynMuteFadeButton>(mm2px(Vec(11.43 + 12.7 * i, 109 + 0.8)), module, TRACK_MUTE_PARAMS + i, module ? &module->panelTheme : NULL));
			if (module) {
				newMuteFade->type = &(module->tracks[i].fadeRate);
			}
			// Solos
			addParam(createDynamicParamCentered<DynSoloButton>(mm2px(Vec(11.43 + 12.7 * i, 115.3 + 0.8)), module, TRACK_SOLO_PARAMS + i, module ? &module->panelTheme : NULL));
			// Group dec
			DynGroupMinusButtonNotify *newGrpMinusButton;
			addParam(newGrpMinusButton = createDynamicParamCentered<DynGroupMinusButtonNotify>(mm2px(Vec(7.7 + 12.7 * i - 0.75, 122.6 + 0.5)), module, GRP_DEC_PARAMS + i, module ? &module->panelTheme : NULL));
			if (module) {
				newGrpMinusButton->srcTrack = &(module->tracks[i]);
			}
			// Group inc
			DynGroupPlusButtonNotify *newGrpPlusButton;
			addParam(newGrpPlusButton = createDynamicParamCentered<DynGroupPlusButtonNotify>(mm2px(Vec(15.2 + 12.7 * i + 0.75, 122.6 + 0.5)), module, GRP_INC_PARAMS + i, module ? &module->panelTheme : NULL));
			if (module) {
				newGrpPlusButton->srcTrack = &(module->tracks[i]);
			}
			// Group select displays
			addChild(groupSelectDisplays[i] = createWidgetCentered<GroupSelectDisplay>(mm2px(Vec(11.43 + 12.7 * i - 0.1, 122.6 + 0.5))));
			if (module) {
				groupSelectDisplays[i]->srcTrack = &(module->tracks[i]);
			}
		}
		
		// Monitor outputs and groups
		for (int i = 0; i < 4; i++) {
			// Monitor outputs
			addOutput(createDynamicPortCentered<DynPort>(mm2px(Vec(217.17 + 12.7 * i, 12 + 0.8)), false, module, DIRECT_OUTPUTS + i, module ? &module->panelTheme : NULL));			

			// Labels
			addChild(groupDisplays[i] = createWidgetCentered<GroupDisplay>(mm2px(Vec(217.17 + 12.7 * i + 0.4, 22.7 + 0.8))));
			if (module) {
				groupDisplays[i]->srcGroup = &(module->groups[i]);
			}
			
			// Volume inputs
			addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(217.17 + 12.7 * i, 30.7 + 0.8)), true, module, GROUP_VOL_INPUTS + i, module ? &module->panelTheme : NULL));			
			// Pan inputs
			addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(217.17 + 12.7 * i, 39.7 + 0.8)), true, module, GROUP_PAN_INPUTS + i, module ? &module->panelTheme : NULL));			
			// Pan knobs
			addParam(createDynamicParamCentered<DynSmallKnob>(mm2px(Vec(217.17 + 12.7 * i, 51.0 + 0.8)), module, GROUP_PAN_PARAMS + i, module ? &module->panelTheme : NULL));
			
			// Faders
			addParam(createDynamicParamCentered<DynSmallFader>(mm2px(Vec(220.84 + 12.7 * i, 80.4 + 0.8)), module, GROUP_FADER_PARAMS + i, module ? &module->panelTheme : NULL));		
			if (module) {
				// VU meters
				VuMeterTrack *newVU = createWidgetCentered<VuMeterTrack>(mm2px(Vec(217.17 + 12.7 * i, 80.4 + 0.8)));
				newVU->srcLevels = &(module->groups[i].vu[0]);
				newVU->colorThemeGlobal = &(module->gInfo.vuColor);
				addChild(newVU);
				// Fade pointers
				FadePointerGroup *newFP = createWidgetCentered<FadePointerGroup>(mm2px(Vec(217.17 - 2.95 + 12.7 * i, 80.4 + 0.8)));
				newFP->srcParam = &(module->params[GROUP_FADER_PARAMS + i]);
				newFP->srcFadeGain = &(module->groups[i].fadeGain);
				newFP->srcFadeRate = &(module->groups[i].fadeRate);
				addChild(newFP);				
			}

			// Mutes
			DynMuteFadeButton* newMuteFade;
			addParam(newMuteFade = createDynamicParamCentered<DynMuteFadeButton>(mm2px(Vec(217.17 + 12.7 * i, 109 + 0.8)), module, GROUP_MUTE_PARAMS + i, module ? &module->panelTheme : NULL));
			if (module) {
				newMuteFade->type = &(module->groups[i].fadeRate);
			}
			// Solos
			addParam(createDynamicParamCentered<DynSoloButton>(mm2px(Vec(217.17 + 12.7 * i, 115.3 + 0.8)), module, GROUP_SOLO_PARAMS + i, module ? &module->panelTheme : NULL));
		}
		
		// Master outputs
		addOutput(createDynamicPortCentered<DynPort>(mm2px(Vec(273.8, 12 + 0.8)), false, module, MAIN_OUTPUTS + 0, module ? &module->panelTheme : NULL));			
		addOutput(createDynamicPortCentered<DynPort>(mm2px(Vec(273.8, 21 + 0.8)), false, module, MAIN_OUTPUTS + 1, module ? &module->panelTheme : NULL));			
		
		// Master label
		MasterDisplay *mastLabel;
		addChild(mastLabel = createWidgetCentered<MasterDisplay>(mm2px(Vec(273.8, 128.5 - 97.2))));
		if (module) {
			mastLabel->srcMaster = &(module->master);
		}
		
		// Master fader
		addParam(createDynamicParamCentered<DynBigFader>(mm2px(Vec(277.65, 69.5 + 0.8)), module, MAIN_FADER_PARAM, module ? &module->panelTheme : NULL));
		if (module) {
			// VU meter
			VuMeterMaster *newVU = createWidgetCentered<VuMeterMaster>(mm2px(Vec(272.3, 69.5 + 0.8)));
			newVU->srcLevels = &(module->master.vu[0]);
			newVU->colorThemeGlobal = &(module->gInfo.vuColor);
			addChild(newVU);
			// Fade pointer
			FadePointerMaster *newFP = createWidgetCentered<FadePointerMaster>(mm2px(Vec(272.3 - 3.4, 69.5 + 0.8)));
			newFP->srcParam = &(module->params[MAIN_FADER_PARAM]);
			newFP->srcFadeGain = &(module->master.fadeGain);
			newFP->srcFadeRate = &(module->master.fadeRate);
			addChild(newFP);				
		}
		
		// Master mute
		DynMuteFadeButton* newMuteFade;
		addParam(newMuteFade = createDynamicParamCentered<DynMuteFadeButton>(mm2px(Vec(272.3, 109.0 + 0.8)), module, MAIN_MUTE_PARAM, module ? &module->panelTheme : NULL));
		if (module) {
			newMuteFade->type = &(module->master.fadeRate);
		}
		
		// Master dim
		addParam(createDynamicParamCentered<DynDimButton>(mm2px(Vec(266.9, 115.3 + 0.8)), module, MAIN_DIM_PARAM, module ? &module->panelTheme : NULL));
		
		// Master mono
		addParam(createDynamicParamCentered<DynMonoButton>(mm2px(Vec(277.7, 115.3 + 0.8)), module, MAIN_MONO_PARAM, module ? &module->panelTheme : NULL));
	}
	
	void step() override {
		MixMasterJr* moduleM = (MixMasterJr*)module;
		if (moduleM) {
			// Track labels (pull from module)
			if (moduleM->resetTrackLabelRequest >= 0) {// pull request from module
				// track displays
				for (int trk = 0; trk < 16; trk++) {
					trackDisplays[trk]->text = std::string(&(moduleM->trackLabels[trk * 4]), 4);
				}
				// group displays
				for (int grp = 0; grp < 4; grp++) {
					groupDisplays[grp]->text = std::string(&(moduleM->trackLabels[(16 + grp) * 4]), 4);
				}
				moduleM->resetTrackLabelRequest = -1;// all done pulling
			}
		}
		Widget::step();
	}
};

Model *modelMixMasterJr = createModel<MixMasterJr, MixMasterJrWidget>("MixMaster-Jr");
