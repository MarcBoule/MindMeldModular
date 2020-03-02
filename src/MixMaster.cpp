//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include <time.h>
#include "MixerWidgets.hpp"


template<int N_TRK, int N_GRP>
struct MixMaster : Module {

	enum ParamIds {
		ENUMS(TRACK_FADER_PARAMS, N_TRK),
		ENUMS(GROUP_FADER_PARAMS, N_GRP),// must follow TRACK_FADER_PARAMS since code assumes contiguous
		ENUMS(TRACK_PAN_PARAMS, N_TRK),
		ENUMS(GROUP_PAN_PARAMS, N_GRP),
		ENUMS(TRACK_MUTE_PARAMS, N_TRK),
		ENUMS(GROUP_MUTE_PARAMS, N_GRP),// must follow TRACK_MUTE_PARAMS since code assumes contiguous
		ENUMS(TRACK_SOLO_PARAMS, N_TRK),// must follow GROUP_MUTE_PARAMS since code assumes contiguous
		ENUMS(GROUP_SOLO_PARAMS, N_GRP),// must follow TRACK_SOLO_PARAMS since code assumes contiguous
		MAIN_MUTE_PARAM,// must follow GROUP_SOLO_PARAMS since code assumes contiguous
		MAIN_DIM_PARAM,// must follow MAIN_MUTE_PARAM since code assumes contiguous
		MAIN_MONO_PARAM,// must follow MAIN_DIM_PARAM since code assumes contiguous
		MAIN_FADER_PARAM,
		ENUMS(GROUP_SELECT_PARAMS, N_TRK),
		NUM_PARAMS
	}; 

	enum InputIds {
		ENUMS(TRACK_SIGNAL_INPUTS, N_TRK * 2), // Track 0: 0 = L, 1 = R, Track 1: 2 = L, 3 = R, etc...
		ENUMS(TRACK_VOL_INPUTS, N_TRK),
		ENUMS(GROUP_VOL_INPUTS, N_GRP),
		ENUMS(TRACK_PAN_INPUTS, N_TRK), 
		ENUMS(GROUP_PAN_INPUTS, N_GRP), 
		ENUMS(CHAIN_INPUTS, 2),
		ENUMS(INSERT_TRACK_INPUTS, N_TRK / 8),
		INSERT_GRP_AUX_INPUT,
		ENUMS(TRACK_MUTESOLO_INPUTS, 2),
		GRPM_MUTESOLO_INPUT,// 1-4 Group mutes, 5-8 Group solos, 9 Master Mute, 10 Master Dim, 11 Master Mono, 12 Master VOL
		NUM_INPUTS
	};

	enum OutputIds {
		ENUMS(DIRECT_OUTPUTS, N_TRK / 8 + 1), // Track 1-8, (Track 9-16), Groups and Aux
		ENUMS(MAIN_OUTPUTS, 2),
		ENUMS(INSERT_TRACK_OUTPUTS, N_TRK / 8),
		INSERT_GRP_AUX_OUTPUT,
		FADE_CV_OUTPUT,
		NUM_OUTPUTS
	};

	enum LightIds {
		ENUMS(TRACK_HPF_LIGHTS, N_TRK),
		ENUMS(TRACK_LPF_LIGHTS, N_TRK),
		NUM_LIGHTS
	};

	typedef ExpansionInterface<N_TRK, N_GRP> Intf;


	#include "MixMaster.hpp"
	
	
	// Expander
	float rightMessages[2][Intf::MFA_NUM_VALUES] = {};// messages from aux-expander, see enum in MixerCommon.hpp

	// Constants
	int numChannels16 = 16;// avoids warning that happens when hardcode 16 (static const or directly use 16 in code below)

	// Need to save, no reset
	int panelTheme;
	
	// Need to save, with reset
	alignas(4) char trackLabels[4 * (N_TRK + N_GRP) + 1];// 4 chars per label, 16 (8) tracks and 4 (2) groups means 20 (10) labels, null terminate the end the whole array only
	GlobalInfo gInfo;
	MixerTrack tracks[N_TRK];
	MixerGroup groups[N_GRP];
	MixerAux aux[4];
	MixerMaster master;
	
	// No need to save, with reset
	int updateTrackLabelRequest;// 0 when nothing to do, 1 for read names in widget
	int refreshCounter8;
	int32_t trackMoveInAuxRequest;// 0 when nothing to do, {dest,src} packed when a move is requested
	float values20[20];
	dsp::SlewLimiter muteTrackWhenSoloAuxRetSlewer;

	// No need to save, no reset
	RefreshCounter refresh;	
	bool auxExpanderPresent = false;// can't be local to process() since widget must know in order to properly draw border
	float trackTaps[N_TRK * 2 * 4];// room for 4 taps for each of the 16 (8) stereo tracks. Trk0-tap0, Trk1-tap0 ... Trk15-tap0,  Trk0-tap1
	float trackInsertOuts[N_TRK * 2];// room for 16 (8) stereo track insert outs
	float groupTaps[N_GRP * 2 * 4];// room for 4 taps for each of the 4 stereo groups
	float auxTaps[4 * 2 * 4];// room for 4 taps for each of the 4 stereo aux
	float *auxSends;// index into correct page of messages from expander (avoid having separate buffers)
	float *auxReturns;// index into correct page of messages from expander (avoid having separate buffers)
	float *auxRetFadePan;// index into correct page of messages from expander (avoid having separate buffers)
	uint32_t muteAuxSendWhenReturnGrouped;// { ... g2-B, g2-A, g1-D, g1-C, g1-B, g1-A}
	PackedBytes4 directOutsModeLocalAux;
	PackedBytes4 stereoPanModeLocalAux;
	alignas(4) char auxLabels[4 * 4 + 1];
	PackedBytes4 auxVuColors;
	PackedBytes4 auxDispColors;
	TriggerRiseFall muteSoloCvTriggers[N_TRK * 2 + N_GRP * 2 + 3];// 16 (8) trk mute, 16 (8) trk solo, 4 (2) grp mute, 4 (2) grp solo, 3 mast (mute, dim, mono)
		
		
	void sendToMessageBus() { 
		int8_t vuColors[1 + 16 + 4 + 4];// room for global, tracks, groups, aux
		int8_t dispColors[1 + 16 + 4 + 4];// room for global, tracks, groups, aux
		vuColors[0] = gInfo.colorAndCloak.cc4[vuColorGlobal];
		dispColors[0] = gInfo.colorAndCloak.cc4[dispColorGlobal];
		if (vuColors[0] >= numVuThemes) {
			for (int t = 0; t < N_TRK; t++) {
				vuColors[1 + t] = tracks[t].vuColorThemeLocal;
			}
			for (int g = 0; g < N_GRP; g++) {
				vuColors[1 + 16 + g] = groups[g].vuColorThemeLocal;
			}
			for (int a = 0; a < 4; a++) {
				vuColors[1 + 16 + 4 + a] = auxVuColors.cc4[a];
			}
		}
		if (dispColors[0] >= numDispThemes) {
			for (int t = 0; t < N_TRK; t++) {
				dispColors[1 + t] = tracks[t].dispColorLocal;
			}
			for (int g = 0; g < N_GRP; g++) {
				dispColors[1 + 16 + g] = groups[g].dispColorLocal;
			}
			for (int a = 0; a < 4; a++) {
				dispColors[1 + 16 + 4 + a] = auxDispColors.cc4[a];
			}
		}
		
		if (N_TRK < 16) {
			mixerMessageBus.sendJr(id + 1, master.masterLabel, trackLabels, &(trackLabels[N_TRK * 4]), auxLabels, vuColors, dispColors);
		}
		else {
			mixerMessageBus.send(id + 1, master.masterLabel, trackLabels, auxLabels, vuColors, dispColors);
		}
	}
	
		
		
	MixMaster() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);		
		
		rightExpander.producerMessage = rightMessages[0];
		rightExpander.consumerMessage = rightMessages[1];

		char strBuf[32];
		// Track
		float maxTGFader = std::pow(GlobalConst::trkAndGrpFaderMaxLinearGain, 1.0f / GlobalConst::trkAndGrpFaderScalingExponent);
		for (int i = 0; i < N_TRK; i++) {
			// Pan
			snprintf(strBuf, 32, "-%02i-: pan", i + 1);
			configParam(TRACK_PAN_PARAMS + i, 0.0f, 1.0f, 0.5f, strBuf, "%", 0.0f, 200.0f, -100.0f);
			// Fader
			snprintf(strBuf, 32, "-%02i-: level", i + 1);
			configParam(TRACK_FADER_PARAMS + i, 0.0f, maxTGFader, 1.0f, strBuf, " dB", -10, 20.0f * GlobalConst::trkAndGrpFaderScalingExponent);
			// Mute
			snprintf(strBuf, 32, "-%02i-: mute", i + 1);
			configParam(TRACK_MUTE_PARAMS + i, 0.0f, 1.0f, 0.0f, strBuf);
			// Solo
			snprintf(strBuf, 32, "-%02i-: solo", i + 1);
			configParam(TRACK_SOLO_PARAMS + i, 0.0f, 1.0f, 0.0f, strBuf);
			// Group select
			snprintf(strBuf, 32, "-%02i-: group", i + 1);
			configParam(GROUP_SELECT_PARAMS + i, 0.0f, (float)N_GRP, 0.0f, strBuf);
		}
		// Group
		for (int i = 0; i < N_GRP; i++) {
			// Pan
			snprintf(strBuf, 32, "GRP%i: pan", i + 1);
			configParam(GROUP_PAN_PARAMS + i, 0.0f, 1.0f, 0.5f, strBuf, "%", 0.0f, 200.0f, -100.0f);
			// Fader
			snprintf(strBuf, 32, "GRP%i: level", i + 1);
			configParam(GROUP_FADER_PARAMS + i, 0.0f, maxTGFader, 1.0f, strBuf, " dB", -10, 20.0f * GlobalConst::trkAndGrpFaderScalingExponent);
			// Mute
			snprintf(strBuf, 32, "GRP%i: mute", i + 1);
			configParam(GROUP_MUTE_PARAMS + i, 0.0f, 1.0f, 0.0f, strBuf);
			// Solo
			snprintf(strBuf, 32, "GRP%i: solo", i + 1);
			configParam(GROUP_SOLO_PARAMS + i, 0.0f, 1.0f, 0.0f, strBuf);
		}
		float maxMFader = std::pow(GlobalConst::masterFaderMaxLinearGain, 1.0f / GlobalConst::masterFaderScalingExponent);
		configParam(MAIN_FADER_PARAM, 0.0f, maxMFader, 1.0f, "MASTER: level", " dB", -10, 20.0f * GlobalConst::masterFaderScalingExponent);
		// Mute
		configParam(MAIN_MUTE_PARAM, 0.0f, 1.0f, 0.0f, "MASTER: mute");
		// Dim
		configParam(MAIN_DIM_PARAM, 0.0f, 1.0f, 0.0f, "MASTER: dim");
		// Mono
		configParam(MAIN_MONO_PARAM, 0.0f, 1.0f, 0.0f, "MASTER: mono");
		

		directOutsModeLocalAux.cc1 = 0;
		stereoPanModeLocalAux.cc1 = 0;
		snprintf(auxLabels, 16 + 1, "AUXAAUXBAUXCAUXD");
		auxVuColors.cc1 = 0;
		auxDispColors.cc1 = 0;

		gInfo.construct(&params[0], values20);
		for (int i = 0; i < N_TRK; i++) {
			tracks[i].construct(i, &gInfo, &inputs[0], &params[0], &(trackLabels[4 * i]), &trackTaps[i << 1], groupTaps, &trackInsertOuts[i << 1]);
		}
		for (int i = 0; i < N_GRP; i++) {
			groups[i].construct(i, &gInfo, &inputs[0], &params[0], &(trackLabels[4 * (N_TRK + i)]), &groupTaps[i << 1]);
		}
		for (int i = 0; i < 4; i++) {
			aux[i].construct(i, &gInfo, &inputs[0], values20, &auxTaps[i << 1], &stereoPanModeLocalAux.cc4[i]);
		}
		master.construct(&gInfo, &params[0], &inputs[0]);
		muteTrackWhenSoloAuxRetSlewer.setRiseFall(GlobalConst::antipopSlewFast, GlobalConst::antipopSlewFast); // slew rate is in input-units per second 
		onReset();

		panelTheme = 0;//(loadDarkAsDefault() ? 1 : 0);
		sendToMessageBus();// register by just writing data
	}
  
	~MixMaster() {
		if (id > -1) {
			mixerMessageBus.deregisterMember(id + 1);
		}
	}

	
	void onReset() override {
		for (int trk = 0; trk < N_TRK; trk++) {
			snprintf(&trackLabels[trk << 2], 5, "-%02i-", trk + 1);
		}
		for (int grp = 0; grp < N_GRP; grp++) {
			snprintf(&trackLabels[(N_TRK + grp) << 2], 5, "GRP%1i", grp + 1);
		}
		gInfo.onReset();
		for (int i = 0; i < N_TRK; i++) {
			tracks[i].onReset();
		}
		for (int i = 0; i < N_GRP; i++) {
			groups[i].onReset();
		}
		for (int i = 0; i < 4; i++) {
			aux[i].onReset();
		}
		master.onReset();
		resetNonJson(false);
	}
	void resetNonJson(bool recurseNonJson) {
		updateTrackLabelRequest = 1;
		refreshCounter8 = 0;
		trackMoveInAuxRequest = 0;
		if (recurseNonJson) {
			gInfo.resetNonJson();
			for (int i = 0; i < N_TRK; i++) {
				tracks[i].resetNonJson();
			}
			for (int i = 0; i < N_GRP; i++) {
				groups[i].resetNonJson();
			}
			for (int i = 0; i < 4; i++) {
				aux[i].resetNonJson();
			}
			master.resetNonJson();
		}
		for (int i = 0; i < 20; i++) {
			values20[i] = 0.0f;
		}
		muteTrackWhenSoloAuxRetSlewer.reset();
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
		for (int i = 0; i < N_TRK; i++) {
			tracks[i].dataToJson(rootJ);
		}
		// groups
		for (int i = 0; i < N_GRP; i++) {
			groups[i].dataToJson(rootJ);
		}
		// aux
		for (int i = 0; i < 4; i++) {
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
			snprintf(trackLabels, 4 * (N_TRK + N_GRP) + 1, "%s", json_string_value(textJ));
		
		// gInfo
		gInfo.dataFromJson(rootJ);

		// tracks
		for (int i = 0; i < N_TRK; i++) {
			tracks[i].dataFromJson(rootJ);
		}
		// groups/aux
		for (int i = 0; i < N_GRP; i++) {
			groups[i].dataFromJson(rootJ);
			aux[i].dataFromJson(rootJ);
		}
		// master
		master.dataFromJson(rootJ);
		
		resetNonJson(true);
	}


	void onSampleRateChange() override {
		gInfo.sampleTime = APP->engine->getSampleTime();
		for (int trk = 0; trk < N_TRK; trk++) {
			tracks[trk].onSampleRateChange();
		}
		master.onSampleRateChange();
	}
	

	void process(const ProcessArgs &args) override {
		
		auxExpanderPresent = (rightExpander.module && rightExpander.module->model == (N_TRK == 16 ? modelAuxExpander : modelAuxExpanderJr));
		
		
		//********** Inputs **********
		
		// From Aux-Expander
		if (auxExpanderPresent) {
			float *messagesFromExpander = (float*)rightExpander.consumerMessage;// could be invalid pointer when !expanderPresent, so read it only when expanderPresent
			
			auxReturns = &messagesFromExpander[Intf::MFA_AUX_RETURNS]; // contains 8 values of the returns from the aux panel
			auxRetFadePan = &messagesFromExpander[Intf::MFA_AUX_RET_FADER]; // contains 8 values of the return faders and pan knobs
						
			int value20i = clamp((int)(messagesFromExpander[Intf::MFA_VALUE20_INDEX]), 0, 19);// mute, solo, group, fadeRate, fadeProfile for aux returns
			values20[value20i] = messagesFromExpander[Intf::MFA_VALUE20];
			
			// Slow values from expander
			uint32_t* mfaUpdateSlow = (uint32_t*)(&messagesFromExpander[Intf::MFA_UPDATE_SLOW]);
			if (*mfaUpdateSlow != 0) {
				// Direct outs and Stereo pan for each aux
				memcpy(&directOutsModeLocalAux, &messagesFromExpander[Intf::MFA_AUX_DIR_OUTS], 4);
				memcpy(&stereoPanModeLocalAux, &messagesFromExpander[Intf::MFA_AUX_STEREO_PANS], 4);
				// Aux labels
				memcpy(&auxLabels, &messagesFromExpander[Intf::AFM_AUX_NAMES], 4 * 4);
				// Colors
				memcpy(&auxVuColors, &messagesFromExpander[Intf::AFM_AUX_VUCOL], 4);
				memcpy(&auxDispColors, &messagesFromExpander[Intf::AFM_AUX_DISPCOL], 4);
			}
		}
		else {
			muteTrackWhenSoloAuxRetSlewer.reset();
		}

		if (refresh.processInputs()) {
			int sixteenCount = refresh.refreshCounter >> 4;// Corresponds to 172Hz refreshing of each track, at 44.1 kHz
			
			// Tracks
			int trackToProcess = (N_TRK == 16 ? sixteenCount : (sixteenCount & 0x7));
			gInfo.updateSoloBit(trackToProcess);
			tracks[trackToProcess].updateSlowValues();// a track is updated once every 16 passes in input proceesing
			// Groups
			if ( (sixteenCount & 0x3) == 0) {// a group is updated once every 16 passes in input proceesing
				int groupToProcess = sixteenCount >> (4 - N_GRP / 2);
				gInfo.updateSoloBit(N_TRK + groupToProcess);
				groups[groupToProcess].updateSlowValues();
			}
			// Aux
			if ( auxExpanderPresent && ((sixteenCount & 0x3) == 1) ) {// an aux is updated once every 16 passes in input proceesing
				gInfo.updateReturnSoloBits();
				aux[sixteenCount >> 2].updateSlowValues();
			}
			// Master
			if ((sixteenCount & 0x3) == 2) {// master and groupUsage updated once every 4 passes in input proceesing
				master.updateSlowValues();
				gInfo.updateGroupUsage();
			}
			
			processMuteSoloCvTriggers();
		}// userInputs refresh
		
		
		// ecoCode: cycles from 0 to 3 in eco mode, stuck at 0 when full power mode
		uint16_t ecoCode = (refresh.refreshCounter & 0x3 & gInfo.ecoMode);
		bool ecoStagger4 = (gInfo.ecoMode == 0 || ecoCode == 3);
				
	
		//********** Outputs **********

		float mix[2] = {0.0f};// room for main (groups will automatically be stored into groups taps 0 by tracks)
		for (int i = 0; i < (N_GRP << 1); i++) {
			groupTaps[i] = 0.0f;
		}
		
		// GlobalInfo
		gInfo.process();
		
		// Tracks
		for (int trk = 0; trk < N_TRK; trk++) {
			tracks[trk].process(mix, ecoCode == 0);// stagger 1
		}
		// Aux return when group
		if (auxExpanderPresent) {
			muteAuxSendWhenReturnGrouped = 0;
			bool ecoStagger3 = (gInfo.ecoMode == 0 || ecoCode == 2);
			for (int auxi = 0; auxi < 4; auxi++) {
				int auxGroup = aux[auxi].getAuxGroup();
				if (auxGroup != 0) {
					auxGroup--;
					aux[auxi].process(&groupTaps[auxGroup << 1], &auxRetFadePan[auxi], ecoStagger3);// stagger 3
					if (gInfo.groupedAuxReturnFeedbackProtection != 0) {
						muteAuxSendWhenReturnGrouped |= (0x1 << ((auxGroup << 2) + auxi));
					}
				}
			}
		}
		
		// Groups (at this point, all groups's tap0 are setup and ready)
		bool ecoStagger2 = (gInfo.ecoMode == 0 || ecoCode == 1);
		for (int i = 0; i < N_GRP; i++) {
			groups[i].process(mix, ecoStagger2);// stagger 2
		}
		
		// Aux
		if (auxExpanderPresent) {
			memcpy(auxTaps, auxReturns, 8 * 4);		
			
			// Mute tracks/groups when soloing aux returns
			float newMuteTrackWhenSoloAuxRet = (gInfo.returnSoloBitMask != 0 && gInfo.auxReturnsSolosMuteDry != 0) ? 0.0f : 1.0f;
			muteTrackWhenSoloAuxRetSlewer.process(args.sampleTime, newMuteTrackWhenSoloAuxRet);
			mix[0] *= muteTrackWhenSoloAuxRetSlewer.out;
			mix[1] *= muteTrackWhenSoloAuxRetSlewer.out;
			
			// Aux returns when no group
			bool ecoStagger3 = (gInfo.ecoMode == 0 || ecoCode == 2);
			for (int auxi = 0; auxi < 4; auxi++) {
				if (aux[auxi].getAuxGroup() == 0) {
					aux[auxi].process(mix, &auxRetFadePan[auxi], ecoStagger3);// stagger 3
				}
			}
		}
		// Master
		master.process(mix, ecoStagger4);// stagger 4
		
		// Set master outputs
		outputs[MAIN_OUTPUTS + 0].setVoltage(mix[0]);
		outputs[MAIN_OUTPUTS + 1].setVoltage(mix[1]);
		
		// Direct outs (uses trackTaps, groupTaps and auxTaps)
		SetDirectTrackOuts(0);// 1-8
		if (N_TRK == 16) {
			SetDirectTrackOuts(8);// 9-16
		}
		SetDirectGroupAuxOuts();
				
		// Insert outs (uses trackInsertOuts and group tap0 and aux tap0)
		SetInsertTrackOuts(0);// 1-8
		if (N_TRK == 16) {
			SetInsertTrackOuts(8);// 9-16
		}
		SetInsertGroupAuxOuts();	

		setFadeCvOuts();


		//********** Lights **********
		
		if (refresh.processLights()) {
			for (int i = 0; i < N_TRK; i++) {
				lights[TRACK_HPF_LIGHTS + i].setBrightness(tracks[i].getHPFCutoffFreq() >= GlobalConst::minHPFCutoffFreq ? 1.0f : 0.0f);
				lights[TRACK_LPF_LIGHTS + i].setBrightness(tracks[i].getLPFCutoffFreq() <= GlobalConst::maxLPFCutoffFreq ? 1.0f : 0.0f);
			}
		}
		
		
		//********** Expander **********
		
		// To Aux-Expander
		if (auxExpanderPresent) {
			float *messageToExpander = (float*)(rightExpander.module->leftExpander.producerMessage);
			
			// Slow
			
			uint32_t* afmUpdateSlow = (uint32_t*)(&messageToExpander[Intf::AFM_UPDATE_SLOW]);
			*afmUpdateSlow = 0;
			if (refresh.refreshCounter == 0) {
				// Track names
				*afmUpdateSlow = 1;
				memcpy(&messageToExpander[Intf::AFM_TRACK_GROUP_NAMES], trackLabels, ((N_TRK + N_GRP) << 2));
				// Panel theme
				int32_t tmp = panelTheme;
				memcpy(&messageToExpander[Intf::AFM_PANEL_THEME], &tmp, 4);
				// Color theme
				memcpy(&messageToExpander[Intf::AFM_COLOR_AND_CLOAK], &gInfo.colorAndCloak.cc1, 4);
				// Direct outs mode global and Stereo pan mode global
				PackedBytes4 directAndPan;
				directAndPan.cc4[0] = gInfo.directOutsMode;
				directAndPan.cc4[1] = gInfo.panLawStereo;
				memcpy(&messageToExpander[Intf::AFM_DIRECT_AND_PAN_MODES], &directAndPan.cc1, 4);
				// Track move
				memcpy(&messageToExpander[Intf::AFM_TRACK_MOVE], &trackMoveInAuxRequest, 4);
				trackMoveInAuxRequest = 0;
				// Aux send mute when grouped return lights
				messageToExpander[Intf::AFM_AUXSENDMUTE_GROUPED_RETURN] = (float)(muteAuxSendWhenReturnGrouped);
				// Display colors (when per track)
				PackedBytes4 tmpDispCols[N_TRK / 4 + 1];
				if (gInfo.colorAndCloak.cc4[dispColorGlobal] < numDispThemes) {
					for (int i = 0; i < (N_TRK / 4 + 1); i++) {
						tmpDispCols[i].cc1 = 0;
					}
				}
				else {
					for (int i = 0; i < (N_TRK / 4); i++) {
						for (int j = 0; j < 4; j++) {
							tmpDispCols[i].cc4[j] = tracks[ (i << 2) + j ].dispColorLocal;
						}	
					}
					for (int j = 0; j < N_GRP; j++) {
						tmpDispCols[N_TRK / 4].cc4[j] = groups[ j ].dispColorLocal;
					}
				}	
				memcpy(&messageToExpander[Intf::AFM_TRK_DISP_COL], tmpDispCols, (N_TRK / 4 + 1) * 4);
				// Eco mode
				tmp = gInfo.ecoMode;
				memcpy(&messageToExpander[Intf::AFM_ECO_MODE], &tmp, 4);
				// auxFadeGains
				for (int auxi = 0; auxi < 4; auxi++) {
					messageToExpander[Intf::AFM_FADE_GAINS + auxi] = aux[auxi].fadeGain;
				}
				// momentaryCvButtons
				tmp = gInfo.momentaryCvButtons;
				memcpy(&messageToExpander[Intf::AFM_MOMENTARY_CVBUTTONS], &tmp, 4);			
			}
			
			// Fast
			
			// 16+4 (8+2) stereo signals to be used to make sends in aux expander
			writeAuxSends(&messageToExpander[Intf::AFM_AUX_SENDS]);						
			// Aux VUs
			// a return VU related value; index 0-3 : quad vu floats of a given aux, 4-7 : mute ghost of given aux (in [AFM_VU_VALUES + 0] only)
			messageToExpander[Intf::AFM_VU_INDEX] = (float)refreshCounter8;
			if (refreshCounter8 < 4) {
				memcpy(&messageToExpander[Intf::AFM_VU_VALUES], aux[refreshCounter8].vu.vuValues, 4 * 4);
			}
			else {
				messageToExpander[Intf::AFM_VU_VALUES + 0] = aux[refreshCounter8 & 0x3].fadeGainScaledWithSolo;
			}	
			
			refreshCounter8++;
			if (refreshCounter8 >= 8) {
				refreshCounter8 = 0;
			}
			
			rightExpander.module->leftExpander.messageFlipRequested = true;
		}// if (auxExpanderPresent)

		
	}// process()
	
	
	void setFadeCvOuts() {
		if (outputs[FADE_CV_OUTPUT].isConnected()) {
			outputs[FADE_CV_OUTPUT].setChannels(N_TRK == 16 ? numChannels16 : 8);
			for (int trk = 0; trk < N_TRK; trk++) {
				float outV = tracks[trk].fadeGain * 10.0f;
				if (gInfo.fadeCvOutsWithVolCv) {
					outV *= tracks[trk].volCv;
				}
				outputs[FADE_CV_OUTPUT].setVoltage(outV, trk);
			}
		}
	}
	
	void writeAuxSends(float* auxSends) {
		// Aux sends (send track and group audio (16+4 or 8+2 stereo signals) to auxspander
		// auxSends[] has room for 16+4 or 8+2 stereo values of the sends to the aux panel (Trk1L, Trk1R, Trk2L, Trk2R ... Trk16L, Trk16R, Grp1L, Grp1R ... Grp4L, Grp4R)
		// populate auxSends[0..39]: Take the trackTaps/groupTaps indicated by the Aux sends mode (with per-track option)
		
		// tracks
		if ( gInfo.auxSendsMode < 4 && (gInfo.groupsControlTrackSendLevels == 0 || gInfo.groupUsage[N_GRP] == 0) ) {
			memcpy(auxSends, &trackTaps[gInfo.auxSendsMode << (3 + N_TRK / 8)], (N_TRK * 2) * 4);
		}
		else {
			int trkGroup;
			for (int trk = 0; trk < N_TRK; trk++) {
				// tracks should have aux sends even when grouped
				int tapIndex = (tracks[trk].auxSendsMode << (3 + N_TRK / 8));
				float trackL = trackTaps[tapIndex + (trk << 1) + 0];
				float trackR = trackTaps[tapIndex + (trk << 1) + 1];
				if (gInfo.groupsControlTrackSendLevels != 0 && (trkGroup = (int)(tracks[trk].paGroup->getValue() + 0.5f)) != 0) {
					trkGroup--;
					simd::float_4 sigs(trackL, trackR, trackR, trackL);
					sigs = sigs * groups[trkGroup].gainMatrixSlewers.out;
					trackL = sigs[0] + sigs[2];
					trackR = sigs[1] + sigs[3];
					trackL *= groups[trkGroup].muteSoloGainSlewer.out;
					trackR *= groups[trkGroup].muteSoloGainSlewer.out;
				}					
				auxSends[(trk << 1) + 0] = trackL;
				auxSends[(trk << 1) + 1] = trackR;
			}
		}
		
		// groups
		if (gInfo.auxSendsMode < 4) {
			memcpy(&auxSends[N_TRK * 2], &groupTaps[gInfo.auxSendsMode << (1 + N_GRP / 2)], (N_GRP * 2) * 4);
		}
		else {
			for (int grp = 0; grp < N_GRP; grp++) {
				int tapIndex = (groups[grp].auxSendsMode << (1 + N_GRP / 2));
				auxSends[(grp << 1) + N_TRK * 2] = groupTaps[tapIndex + (grp << 1) + 0];
				auxSends[(grp << 1) + N_TRK * 2] = groupTaps[tapIndex + (grp << 1) + 1];
			}
		}

	}
	
	
	void SetDirectTrackOuts(const int base) {// base is 0 or 8
		int outi = base >> 3;
		if (outputs[DIRECT_OUTPUTS + outi].isConnected()) {
			outputs[DIRECT_OUTPUTS + outi].setChannels(numChannels16);

			for (unsigned int i = 0; i < 8; i++) {
				int tapIndex = gInfo.directOutsMode < 4 ? gInfo.directOutsMode : tracks[base + i].directOutsMode;
				int offset = (tapIndex << (3 + N_TRK / 8)) + ((base + i) << 1);
				float leftSig = trackTaps[offset + 0];
				float rightSig = (tapIndex < 2 && !inputs[((base + i) << 1) + 1].isConnected()) ? 0.0f : trackTaps[offset + 1];
				if (auxExpanderPresent && gInfo.auxReturnsSolosMuteDry != 0 && tapIndex == 3) {
					leftSig *= muteTrackWhenSoloAuxRetSlewer.out;
					rightSig *= muteTrackWhenSoloAuxRetSlewer.out;
				}
				outputs[DIRECT_OUTPUTS + outi].setVoltage(leftSig, 2 * i);
				outputs[DIRECT_OUTPUTS + outi].setVoltage(rightSig, 2 * i + 1);
			}
		}
	}
	
	void SetDirectGroupAuxOuts() {
		int outputNum = DIRECT_OUTPUTS + N_TRK / 8;
		if (outputs[outputNum].isConnected()) {
			outputs[outputNum].setChannels(auxExpanderPresent ? numChannels16 : 8);

			// Groups
			int tapIndex = gInfo.directOutsMode;			
			if (gInfo.directOutsMode < 4) {// global direct outs
				if (auxExpanderPresent && gInfo.auxReturnsSolosMuteDry != 0 && tapIndex == 3) {
					for (unsigned int i = 0; i < N_GRP; i++) {
						int offset = (tapIndex << (1 + N_GRP / 2)) + (i << 1);
						outputs[outputNum].setVoltage(groupTaps[offset + 0] * muteTrackWhenSoloAuxRetSlewer.out, 2 * i);
						outputs[outputNum].setVoltage(groupTaps[offset + 1] * muteTrackWhenSoloAuxRetSlewer.out, 2 * i + 1);
					}
				}
				else {
					memcpy(outputs[outputNum].getVoltages(), &groupTaps[(tapIndex << (1 + N_GRP / 2))], 4 * N_GRP * 2);
				}
			}
			else {// per group direct outs
				for (unsigned int i = 0; i < N_GRP; i++) {
					tapIndex = groups[i].directOutsMode;
					int offset = (tapIndex << (1 + N_GRP / 2)) + (i << 1);
					if (auxExpanderPresent && gInfo.auxReturnsSolosMuteDry != 0 && tapIndex == 3) {
						outputs[outputNum].setVoltage(groupTaps[offset + 0] * muteTrackWhenSoloAuxRetSlewer.out, 2 * i);
						outputs[outputNum].setVoltage(groupTaps[offset + 1] * muteTrackWhenSoloAuxRetSlewer.out, 2 * i + 1);
					}
					else {
						outputs[outputNum].setVoltage(groupTaps[offset + 0], 2 * i);
						outputs[outputNum].setVoltage(groupTaps[offset + 1], 2 * i + 1);
					}
				}
			}
			
			// Aux
			// this uses one of the taps in the aux return signal flow (same signal flow as a group), and choice of tap is same as other diretct outs
			if (auxExpanderPresent) {
				if (gInfo.directOutsMode < 4) {// global direct outs
					int tapIndex = gInfo.directOutsMode;
					memcpy(outputs[outputNum].getVoltages(8), &auxTaps[(tapIndex << 3)], 4 * 8);
				}
				else {// per aux direct outs
					for (unsigned int i = 0; i < 4; i++) {
						int tapIndex = directOutsModeLocalAux.cc4[i];
						int offset = (tapIndex << 3) + (i << 1);
						outputs[outputNum].setVoltage(auxTaps[offset + 0], 8 + 2 * i);
						outputs[outputNum].setVoltage(auxTaps[offset + 1], 8 + 2 * i + 1);
					}
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
			outputs[INSERT_GRP_AUX_OUTPUT].setChannels(auxExpanderPresent ? numChannels16 : 8);
			memcpy(outputs[INSERT_GRP_AUX_OUTPUT].getVoltages(), groupTaps, 4 *  N_GRP * 2);// insert out for groups is directly tap0
			if (auxExpanderPresent) {
				memcpy(outputs[INSERT_GRP_AUX_OUTPUT].getVoltages(8), auxTaps, 4 * 8);// insert out for aux is directly tap0
			}
		}
	}


	void processMuteSoloCvTriggers() {
		int state;
		if (inputs[TRACK_MUTESOLO_INPUTS + 0].isConnected()) {
			for (int trk = 0; trk < N_TRK; trk++) {
				// track mutes
				state = muteSoloCvTriggers[trk].process(inputs[TRACK_MUTESOLO_INPUTS + 0].getVoltage(trk));
				if (state != 0) {
					if (gInfo.momentaryCvButtons) {
						if (state == 1) {
							float newParam = 1.0f - params[TRACK_MUTE_PARAMS + trk].getValue();// toggle
							params[TRACK_MUTE_PARAMS + trk].setValue(newParam);
						};
					}
					else {
						params[TRACK_MUTE_PARAMS + trk].setValue(state == 1 ? 1.0f : 0.0f);// gate level
					}
				}
			}
		}
		int soloInputNum = TRACK_MUTESOLO_INPUTS + N_TRK / 16;
		if (inputs[soloInputNum].isConnected()) {
			for (int trk = 0; trk < N_TRK; trk++) {
				// track solos
				state = muteSoloCvTriggers[trk + N_TRK].process(inputs[soloInputNum].getVoltage((N_TRK == 16 ? 0 : 8) + trk));
				if (state != 0) {
					if (gInfo.momentaryCvButtons) {
						if (state == 1) {
							float newParam = 1.0f - params[TRACK_SOLO_PARAMS + trk].getValue();// toggle
							params[TRACK_SOLO_PARAMS + trk].setValue(newParam);
						};
					}
					else {
						params[TRACK_SOLO_PARAMS + trk].setValue(state == 1 ? 1.0f : 0.0f);// gate level
					}
				}
			}
		}
		if (inputs[GRPM_MUTESOLO_INPUT].isConnected()) {
			for (int grp = 0; grp < N_GRP; grp++) {
				// group mutes
				state = muteSoloCvTriggers[grp + N_TRK * 2].process(inputs[GRPM_MUTESOLO_INPUT].getVoltage(grp));
				if (state != 0) {
					if (gInfo.momentaryCvButtons) {
						if (state == 1) {
							float newParam = 1.0f - params[GROUP_MUTE_PARAMS + grp].getValue();// toggle
							params[GROUP_MUTE_PARAMS + grp].setValue(newParam);
						};
					}
					else {
						params[GROUP_MUTE_PARAMS + grp].setValue(state == 1 ? 1.0f : 0.0f);// gate level
					}
				}
				// group solos 
				state = muteSoloCvTriggers[grp + N_TRK * 2 + N_GRP].process(inputs[GRPM_MUTESOLO_INPUT].getVoltage(grp + N_GRP));
				if (state != 0) {
					if (gInfo.momentaryCvButtons) {
						if (state == 1) {
							float newParam = 1.0f - params[GROUP_SOLO_PARAMS + grp].getValue();// toggle
							params[GROUP_SOLO_PARAMS + grp].setValue(newParam);
						};
					}
					else {
						params[GROUP_SOLO_PARAMS + grp].setValue(state == 1 ? 1.0f : 0.0f);// gate level
					}
				}
			}	
			// master mute
			state = muteSoloCvTriggers[N_TRK * 2 + N_GRP * 2].process(inputs[GRPM_MUTESOLO_INPUT].getVoltage(N_GRP * 2));
			if (state != 0) {
				if (gInfo.momentaryCvButtons) {
					if (state == 1) {
						float newParam = 1.0f - params[MAIN_MUTE_PARAM].getValue();// toggle
						params[MAIN_MUTE_PARAM].setValue(newParam);
					};
				}
				else {
					params[MAIN_MUTE_PARAM].setValue(state == 1 ? 1.0f : 0.0f);// gate level
				}
			}
			// master dim
			state = muteSoloCvTriggers[N_TRK * 2 + N_GRP * 2 + 1].process(inputs[GRPM_MUTESOLO_INPUT].getVoltage(N_GRP * 2 + 1));
			if (state != 0) {
				if (gInfo.momentaryCvButtons) {
					if (state == 1) {
						float newParam = 1.0f - params[MAIN_DIM_PARAM].getValue();// toggle
						params[MAIN_DIM_PARAM].setValue(newParam);
					};
				}
				else {
					params[MAIN_DIM_PARAM].setValue(state == 1 ? 1.0f : 0.0f);// gate level
				}
			}
			// master mono
			state = muteSoloCvTriggers[N_TRK * 2 + N_GRP * 2 + 2].process(inputs[GRPM_MUTESOLO_INPUT].getVoltage(N_GRP * 2 + 2));
			if (state != 0) {
				if (gInfo.momentaryCvButtons) {
					if (state == 1) {
						float newParam = 1.0f - params[MAIN_MONO_PARAM].getValue();// toggle
						params[MAIN_MONO_PARAM].setValue(newParam);
					};
				}
				else {
					params[MAIN_MONO_PARAM].setValue(state == 1 ? 1.0f : 0.0f);// gate level
				}
			}
		}
	}
};


//-----------------------------------------------------------------------------


struct MixMasterWidget : ModuleWidget {
	static const int N_TRK = 16;
	static const int N_GRP = 4;


	#include "MixMasterWidget.hpp"


	// Module's widget
	// --------------------

	MixMasterWidget(TMixMaster *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/mixmaster.svg")));
		panelBorder = findBorder(panel);		
		
		// Inserts and CVs
		static const float xIns = 13.8;
		// Insert outputs
		addOutput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xIns, 12.8)), false, module, TMixMaster::INSERT_TRACK_OUTPUTS + 0, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 1)), false, module, TMixMaster::INSERT_TRACK_OUTPUTS + 1, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 2)), false, module, TMixMaster::INSERT_GRP_AUX_OUTPUT, module ? &module->panelTheme : NULL));
		// Insert inputs
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 3)), true, module, TMixMaster::INSERT_TRACK_INPUTS + 0, module ? &module->panelTheme : NULL));
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 4)), true, module, TMixMaster::INSERT_TRACK_INPUTS + 1, module ? &module->panelTheme : NULL));
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 5)), true, module, TMixMaster::INSERT_GRP_AUX_INPUT, module ? &module->panelTheme : NULL));
		// Mute, solo and other CVs
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 7)), true, module, TMixMaster::TRACK_MUTESOLO_INPUTS + 0, module ? &module->panelTheme : NULL));
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 8)), true, module, TMixMaster::TRACK_MUTESOLO_INPUTS + 1, module ? &module->panelTheme : NULL));
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 9)), true, module, TMixMaster::GRPM_MUTESOLO_INPUT, module ? &module->panelTheme : NULL));
		
	
		// Tracks
		static const float xTrck1 = 11.43 + 20.32;
		for (int i = 0; i < N_TRK; i++) {
			// Labels
			addChild(trackDisplays[i] = createWidgetCentered<TrackDisplay<TMixMaster::MixerTrack>>(mm2px(Vec(xTrck1 + 12.7 * i, 4.7))));
			if (module) {
				// trackDisplays[i]->tabNextFocus = // done after the for loop
				trackDisplays[i]->colorAndCloak = &(module->gInfo.colorAndCloak);
				trackDisplays[i]->dispColorLocal = &(module->tracks[i].dispColorLocal);				
				trackDisplays[i]->tracks = module->tracks;
				trackDisplays[i]->trackNumSrc = i;
				trackDisplays[i]->auxExpanderPresentPtr = &(module->auxExpanderPresent);
				trackDisplays[i]->numTracks = N_TRK;
				trackDisplays[i]->updateTrackLabelRequestPtr = &(module->updateTrackLabelRequest);
				trackDisplays[i]->trackMoveInAuxRequestPtr = &(module->trackMoveInAuxRequest);
				trackDisplays[i]->inputWidgets = inputWidgets;
			}
			// HPF lights
			addChild(createLightCentered<TinyLight<GreenLight>>(mm2px(Vec(xTrck1 - 4.17 + 12.7 * i, 8.3)), module, TMixMaster::TRACK_HPF_LIGHTS + i));	
			// LPF lights
			addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(xTrck1 + 4.17 + 12.7 * i, 8.3)), module, TMixMaster::TRACK_LPF_LIGHTS + i));	
			// Left inputs
			addInput(inputWidgets[i + 0] = createDynamicPortCentered<DynPort>(mm2px(Vec(xTrck1 + 12.7 * i, 12.8)), true, module, TMixMaster::TRACK_SIGNAL_INPUTS + 2 * i + 0, module ? &module->panelTheme : NULL));			
			// Right inputs
			addInput(inputWidgets[i + 16] = createDynamicPortCentered<DynPort>(mm2px(Vec(xTrck1 + 12.7 * i, 21.8)), true, module, TMixMaster::TRACK_SIGNAL_INPUTS + 2 * i + 1, module ? &module->panelTheme : NULL));	
			// Volume inputs
			addInput(inputWidgets[i + 16 * 2] = createDynamicPortCentered<DynPort>(mm2px(Vec(xTrck1 + 12.7 * i, 31.5)), true, module, TMixMaster::TRACK_VOL_INPUTS + i, module ? &module->panelTheme : NULL));			
			// Pan inputs
			addInput(inputWidgets[i + 16 * 3] = createDynamicPortCentered<DynPort>(mm2px(Vec(xTrck1 + 12.7 * i, 40.5)), true, module, TMixMaster::TRACK_PAN_INPUTS + i, module ? &module->panelTheme : NULL));			
			// Pan knobs
			DynSmallKnobGreyWithArc *panKnobTrack;
			addParam(panKnobTrack = createDynamicParamCentered<DynSmallKnobGreyWithArc>(mm2px(Vec(xTrck1 + 12.7 * i, 51.8)), module, TMixMaster::TRACK_PAN_PARAMS + i, module ? &module->panelTheme : NULL));
			if (module) {
				panKnobTrack->detailsShowSrc = &(module->gInfo.colorAndCloak.cc4[detailsShow]);
				panKnobTrack->cloakedModeSrc = &(module->gInfo.colorAndCloak.cc4[cloakedMode]);
				panKnobTrack->paramWithCV = &(module->tracks[i].pan);
				panKnobTrack->paramCvConnected = &(module->tracks[i].panCvConnected);
				panKnobTrack->dispColorGlobalSrc = &(module->gInfo.colorAndCloak.cc4[dispColorGlobal]);
				panKnobTrack->dispColorLocalSrc = &(module->tracks[i].dispColorLocal);
			}
			
			// Faders
			DynSmallFaderWithLink *newFader;
			addParam(newFader = createDynamicParamCentered<DynSmallFaderWithLink>(mm2px(Vec(xTrck1 + 3.67 + 12.7 * i, 81.2)), module, TMixMaster::TRACK_FADER_PARAMS + i, module ? &module->panelTheme : NULL));
			if (module) {
				newFader->linkBitMaskSrc = &(module->gInfo.linkBitMask);
				newFader->baseFaderParamId = TMixMaster::TRACK_FADER_PARAMS;
				// VU meters
				VuMeterTrack *newVU = createWidgetCentered<VuMeterTrack>(mm2px(Vec(xTrck1 + 12.7 * i, 81.2)));
				newVU->srcLevels = module->tracks[i].vu.vuValues;
				newVU->srcMuteGhost = &(module->tracks[i].fadeGainScaledWithSolo);
				newVU->colorThemeGlobal = &(module->gInfo.colorAndCloak.cc4[vuColorGlobal]);
				newVU->colorThemeLocal = &(module->tracks[i].vuColorThemeLocal);
				addChild(newVU);
				// Fade pointers
				CvAndFadePointerTrack *newFP = createWidgetCentered<CvAndFadePointerTrack>(mm2px(Vec(xTrck1 - 2.95 + 12.7 * i, 81.2)));
				newFP->srcParam = &(module->params[TMixMaster::TRACK_FADER_PARAMS + i]);
				newFP->srcParamWithCV = &(module->tracks[i].paramWithCV);
				newFP->colorAndCloak = &(module->gInfo.colorAndCloak);
				newFP->srcFadeGain = &(module->tracks[i].fadeGain);
				newFP->srcFadeRate = module->tracks[i].fadeRate;
				newFP->dispColorLocalPtr = &(module->tracks[i].dispColorLocal);
				addChild(newFP);				
			}
			
			
			// Mutes
			DynMuteFadeButtonWithClear* newMuteFade;
			addParam(newMuteFade = createDynamicParamCentered<DynMuteFadeButtonWithClear>(mm2px(Vec(xTrck1 + 12.7 * i, 109.8)), module, TMixMaster::TRACK_MUTE_PARAMS + i, module ? &module->panelTheme : NULL));
			if (module) {
				newMuteFade->type = module->tracks[i].fadeRate;
				newMuteFade->muteParams = &module->params[TMixMaster::TRACK_MUTE_PARAMS];
				newMuteFade->baseMuteParamId = TMixMaster::TRACK_MUTE_PARAMS;
				newMuteFade->numTracksAndGroups = N_TRK + N_GRP;
			}
			// Solos
			DynSoloButtonMutex *newSoloButton;
			addParam(newSoloButton = createDynamicParamCentered<DynSoloButtonMutex>(mm2px(Vec(xTrck1 + 12.7 * i, 116.1)), module, TMixMaster::TRACK_SOLO_PARAMS + i, module ? &module->panelTheme : NULL));
			newSoloButton->soloParams =  module ? &module->params[TMixMaster::TRACK_SOLO_PARAMS] : NULL;
			newSoloButton->baseSoloParamId = TMixMaster::TRACK_SOLO_PARAMS;
			newSoloButton->numTracks = N_TRK;
			newSoloButton->numGroups = N_GRP;
			// Group dec
			DynGroupMinusButtonNotify *newGrpMinusButton;
			addChild(newGrpMinusButton = createDynamicWidgetCentered<DynGroupMinusButtonNotify>(mm2px(Vec(xTrck1 - 3.73 + 12.7 * i - 0.75, 123.1)), module ? &module->panelTheme : NULL));
			if (module) {
				newGrpMinusButton->sourceParam = &(module->params[TMixMaster::GROUP_SELECT_PARAMS + i]);
				newGrpMinusButton->numGroups = (float)N_GRP;
			}
			// Group inc
			DynGroupPlusButtonNotify *newGrpPlusButton;
			addChild(newGrpPlusButton = createDynamicWidgetCentered<DynGroupPlusButtonNotify>(mm2px(Vec(xTrck1 + 3.77 + 12.7 * i + 0.75, 123.1)), module ? &module->panelTheme : NULL));
			if (module) {
				newGrpPlusButton->sourceParam = &(module->params[TMixMaster::GROUP_SELECT_PARAMS + i]);
				newGrpPlusButton->numGroups = (float)N_GRP;
			}
			// Group select displays
			GroupSelectDisplay* groupSelectDisplay;
			addParam(groupSelectDisplay = createParamCentered<GroupSelectDisplay>(mm2px(Vec(xTrck1 + 12.7 * i, 123.1)), module, TMixMaster::GROUP_SELECT_PARAMS + i));
			if (module) {
				groupSelectDisplay->srcColor = &(module->gInfo.colorAndCloak);
				groupSelectDisplay->srcColorLocal = &(module->tracks[i].dispColorLocal);
				groupSelectDisplay->numGroups = N_GRP;
			}
		}
		for (int i = 0; i < N_TRK; i++) {
			trackDisplays[i]->tabNextFocus = trackDisplays[(i + 1) % N_TRK];
		}
		
		// Monitor outputs and groups
		static const float xGrp1 = 217.17 + 20.32;
		for (int i = 0; i < N_GRP; i++) {
			// Monitor outputs
			if (i > 0) {
				addOutput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xGrp1 + 12.7 * (i), 11.5)), false, module, TMixMaster::DIRECT_OUTPUTS + i - 1, module ? &module->panelTheme : NULL));
			}
			else {
				addOutput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xGrp1 + 12.7 * (0), 11.5)), false, module, TMixMaster::FADE_CV_OUTPUT, module ? &module->panelTheme : NULL));				
			}
			// Labels
			addChild(groupDisplays[i] = createWidgetCentered<GroupDisplay<TMixMaster::MixerGroup>>(mm2px(Vec(xGrp1 + 12.7 * i, 23.5))));
			if (module) {
				// groupDisplays[i]->tabNextFocus = // done after the for loop
				groupDisplays[i]->colorAndCloak = &(module->gInfo.colorAndCloak);
				groupDisplays[i]->dispColorLocal = &(module->groups[i].dispColorLocal);
				groupDisplays[i]->srcGroup = &(module->groups[i]);
				groupDisplays[i]->auxExpanderPresentPtr = &(module->auxExpanderPresent);
				groupDisplays[i]->numTracks = N_TRK;
			}
			
			// Volume inputs
			addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(xGrp1 + 12.7 * i, 31.5)), true, module, TMixMaster::GROUP_VOL_INPUTS + i, module ? &module->panelTheme : NULL));			
			// Pan inputs
			addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(xGrp1 + 12.7 * i, 40.5)), true, module, TMixMaster::GROUP_PAN_INPUTS + i, module ? &module->panelTheme : NULL));			
			// Pan knobs
			DynSmallKnobGreyWithArc *panKnobGroup;
			addParam(panKnobGroup = createDynamicParamCentered<DynSmallKnobGreyWithArc>(mm2px(Vec(xGrp1 + 12.7 * i, 51.8)), module, TMixMaster::GROUP_PAN_PARAMS + i, module ? &module->panelTheme : NULL));
			if (module) {
				panKnobGroup->detailsShowSrc = &(module->gInfo.colorAndCloak.cc4[detailsShow]);
				panKnobGroup->cloakedModeSrc = &(module->gInfo.colorAndCloak.cc4[cloakedMode]);
				panKnobGroup->paramWithCV = &(module->groups[i].pan);
				panKnobGroup->paramCvConnected = &(module->groups[i].panCvConnected);
				panKnobGroup->dispColorGlobalSrc = &(module->gInfo.colorAndCloak.cc4[dispColorGlobal]);
				panKnobGroup->dispColorLocalSrc = &(module->groups[i].dispColorLocal);
			}
			
			// Faders
			DynSmallFaderWithLink *newFader;
			addParam(newFader = createDynamicParamCentered<DynSmallFaderWithLink>(mm2px(Vec(xGrp1 + 3.67 + 12.7 * i, 81.2)), module, TMixMaster::GROUP_FADER_PARAMS + i, module ? &module->panelTheme : NULL));		
			if (module) {
				newFader->linkBitMaskSrc = &(module->gInfo.linkBitMask);
				newFader->baseFaderParamId = TMixMaster::TRACK_FADER_PARAMS;
				// VU meters
				VuMeterTrack *newVU = createWidgetCentered<VuMeterTrack>(mm2px(Vec(xGrp1 + 12.7 * i, 81.2)));
				newVU->srcLevels = module->groups[i].vu.vuValues;
				newVU->srcMuteGhost = &(module->groups[i].fadeGainScaled);
				newVU->colorThemeGlobal = &(module->gInfo.colorAndCloak.cc4[vuColorGlobal]);
				newVU->colorThemeLocal = &(module->groups[i].vuColorThemeLocal);
				addChild(newVU);
				// Fade pointers
				CvAndFadePointerGroup *newFP = createWidgetCentered<CvAndFadePointerGroup>(mm2px(Vec(xGrp1 - 2.95 + 12.7 * i, 81.2)));
				newFP->srcParam = &(module->params[TMixMaster::GROUP_FADER_PARAMS + i]);
				newFP->srcParamWithCV = &(module->groups[i].paramWithCV);
				newFP->colorAndCloak = &(module->gInfo.colorAndCloak);
				newFP->srcFadeGain = &(module->groups[i].fadeGain);
				newFP->srcFadeRate = module->groups[i].fadeRate;
				newFP->dispColorLocalPtr = &(module->groups[i].dispColorLocal);
				addChild(newFP);				
			}

			// Mutes
			DynMuteFadeButtonWithClear* newMuteFade;
			addParam(newMuteFade = createDynamicParamCentered<DynMuteFadeButtonWithClear>(mm2px(Vec(xGrp1 + 12.7 * i, 109.8)), module, TMixMaster::GROUP_MUTE_PARAMS + i, module ? &module->panelTheme : NULL));
			if (module) {
				newMuteFade->type = module->groups[i].fadeRate;
				newMuteFade->muteParams = &module->params[TMixMaster::TRACK_MUTE_PARAMS];
				newMuteFade->baseMuteParamId = TMixMaster::TRACK_MUTE_PARAMS;
				newMuteFade->numTracksAndGroups = N_TRK + N_GRP;
			}
			// Solos
			DynSoloButtonMutex* newSoloButton;
			addParam(newSoloButton = createDynamicParamCentered<DynSoloButtonMutex>(mm2px(Vec(xGrp1 + 12.7 * i, 116.1)), module, TMixMaster::GROUP_SOLO_PARAMS + i, module ? &module->panelTheme : NULL));
			newSoloButton->soloParams =  module ? &module->params[TMixMaster::TRACK_SOLO_PARAMS] : NULL;
			newSoloButton->baseSoloParamId = TMixMaster::TRACK_SOLO_PARAMS;
			newSoloButton->numTracks = N_TRK;
			newSoloButton->numGroups = N_GRP;
		}
		for (int i = 0; i < N_GRP; i++) {
			groupDisplays[i]->tabNextFocus = groupDisplays[(i + 1) % N_GRP];
		}
	
		// Master inputs
		addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(289.62, 12.8)), true, module, TMixMaster::CHAIN_INPUTS + 0, module ? &module->panelTheme : NULL));			
		addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(289.62, 21.8)), true, module, TMixMaster::CHAIN_INPUTS + 1, module ? &module->panelTheme : NULL));			
		
		// Master outputs
		addOutput(createDynamicPortCentered<DynPort>(mm2px(Vec(300.12, 12.8)), false, module, TMixMaster::MAIN_OUTPUTS + 0, module ? &module->panelTheme : NULL));			
		addOutput(createDynamicPortCentered<DynPort>(mm2px(Vec(300.12, 21.8)), false, module, TMixMaster::MAIN_OUTPUTS + 1, module ? &module->panelTheme : NULL));			
		
		// Master label
		addChild(masterDisplay = createWidgetCentered<MasterDisplay>(mm2px(Vec(294.82, 128.5 - 97.15))));
		if (module) {
			masterDisplay->dcBlock = &(module->master.dcBlock);
			masterDisplay->clipping = &(module->master.clipping);
			masterDisplay->fadeRate = &(module->master.fadeRate);
			masterDisplay->fadeProfile = &(module->master.fadeProfile);
			masterDisplay->vuColorThemeLocal = &(module->master.vuColorThemeLocal);
			masterDisplay->dispColorLocal = &(module->master.dispColorLocal);
			masterDisplay->dimGain = &(module->master.dimGain);
			masterDisplay->masterLabel = module->master.masterLabel;
			masterDisplay->dimGainIntegerDB = &(module->master.dimGainIntegerDB);
			masterDisplay->colorAndCloak = &(module->gInfo.colorAndCloak);
			masterDisplay->idSrc = &(module->id);
		}
		
		// Master fader
		addParam(createDynamicParamCentered<DynBigFader>(mm2px(Vec(300.17, 70.3)), module, TMixMaster::MAIN_FADER_PARAM, module ? &module->panelTheme : NULL));
		if (module) {
			// VU meter
			VuMeterMaster *newVU = createWidgetCentered<VuMeterMaster>(mm2px(Vec(294.82, 70.3)));
			newVU->srcLevels = module->master.vu.vuValues;
			newVU->srcMuteGhost = &(module->master.fadeGainScaled);
			newVU->colorThemeGlobal = &(module->gInfo.colorAndCloak.cc4[vuColorGlobal]);
			newVU->colorThemeLocal = &(module->master.vuColorThemeLocal);
			newVU->clippingPtr = &(module->master.clipping);
			addChild(newVU);
			// Fade pointer
			CvAndFadePointerMaster *newFP = createWidgetCentered<CvAndFadePointerMaster>(mm2px(Vec(294.82 - 3.4, 70.3)));
			newFP->srcParam = &(module->params[TMixMaster::MAIN_FADER_PARAM]);
			newFP->srcParamWithCV = &(module->master.paramWithCV);
			newFP->colorAndCloak = &(module->gInfo.colorAndCloak);
			newFP->srcFadeGain = &(module->master.fadeGain);
			newFP->srcFadeRate = &(module->master.fadeRate);
			addChild(newFP);				
		}
		
		// Master mute
		DynMuteFadeButton* newMuteFade;
		addParam(newMuteFade = createDynamicParamCentered<DynMuteFadeButton>(mm2px(Vec(294.82, 109.8)), module, TMixMaster::MAIN_MUTE_PARAM, module ? &module->panelTheme : NULL));
		if (module) {
			newMuteFade->type = &(module->master.fadeRate);
		}
		
		// Master dim
		addParam(createDynamicParamCentered<DynDimButton>(mm2px(Vec(289.42, 116.1)), module, TMixMaster::MAIN_DIM_PARAM, module ? &module->panelTheme : NULL));
		
		// Master mono
		addParam(createDynamicParamCentered<DynMonoButton>(mm2px(Vec(300.22, 116.1)), module, TMixMaster::MAIN_MONO_PARAM, module ? &module->panelTheme : NULL));
	}
};


//-----------------------------------------------------------------------------


struct MixMasterJrWidget : ModuleWidget {
	static const int N_TRK = 8;
	static const int N_GRP = 2;


	#include "MixMasterWidget.hpp"


	// Module's widget
	// --------------------

	MixMasterJrWidget(TMixMaster *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/mixmaster-jr.svg")));
		panelBorder = findBorder(panel);		
		
		// Inserts and CVs
		static const float xIns = 13.8;
		// Fade CV output
		addOutput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xIns, 12.8)), false, module, TMixMaster::FADE_CV_OUTPUT, module ? &module->panelTheme : NULL));
		// Insert outputs
		addOutput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 2)), false, module, TMixMaster::INSERT_TRACK_OUTPUTS + 0, module ? &module->panelTheme : NULL));				
		addOutput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 3)), false, module, TMixMaster::INSERT_GRP_AUX_OUTPUT, module ? &module->panelTheme : NULL));
		// Insert inputs
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 4)), true, module, TMixMaster::INSERT_TRACK_INPUTS + 0, module ? &module->panelTheme : NULL));
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 5)), true, module, TMixMaster::INSERT_GRP_AUX_INPUT, module ? &module->panelTheme : NULL));
		// Mute, solo and other CVs
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 7)), true, module, TMixMaster::TRACK_MUTESOLO_INPUTS + 0, module ? &module->panelTheme : NULL));
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 8)), true, module, TMixMaster::GRPM_MUTESOLO_INPUT, module ? &module->panelTheme : NULL));
		
	
		// Tracks
		static const float xTrck1 = 11.43 + 20.32;
		for (int i = 0; i < N_TRK; i++) {
			// Labels
			addChild(trackDisplays[i] = createWidgetCentered<TrackDisplay<TMixMaster::MixerTrack>>(mm2px(Vec(xTrck1 + 12.7 * i, 4.7))));
			if (module) {
				// trackDisplays[i]->tabNextFocus = // done after the for loop
				trackDisplays[i]->colorAndCloak = &(module->gInfo.colorAndCloak);
				trackDisplays[i]->dispColorLocal = &(module->tracks[i].dispColorLocal);				
				trackDisplays[i]->tracks = module->tracks;
				trackDisplays[i]->trackNumSrc = i;
				trackDisplays[i]->auxExpanderPresentPtr = &(module->auxExpanderPresent);
				trackDisplays[i]->numTracks = N_TRK;
				trackDisplays[i]->updateTrackLabelRequestPtr = &(module->updateTrackLabelRequest);
				trackDisplays[i]->trackMoveInAuxRequestPtr = &(module->trackMoveInAuxRequest);
				trackDisplays[i]->inputWidgets = inputWidgets;
			}
			// HPF lights
			addChild(createLightCentered<TinyLight<GreenLight>>(mm2px(Vec(xTrck1 - 4.17 + 12.7 * i, 8.3)), module, TMixMaster::TRACK_HPF_LIGHTS + i));	
			// LPF lights
			addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(xTrck1 + 4.17 + 12.7 * i, 8.3)), module, TMixMaster::TRACK_LPF_LIGHTS + i));	
			// Left inputs
			addInput(inputWidgets[i + 0] = createDynamicPortCentered<DynPort>(mm2px(Vec(xTrck1 + 12.7 * i, 12.8)), true, module, TMixMaster::TRACK_SIGNAL_INPUTS + 2 * i + 0, module ? &module->panelTheme : NULL));			
			// Right inputs
			addInput(inputWidgets[i + N_TRK] = createDynamicPortCentered<DynPort>(mm2px(Vec(xTrck1 + 12.7 * i, 21.8)), true, module, TMixMaster::TRACK_SIGNAL_INPUTS + 2 * i + 1, module ? &module->panelTheme : NULL));	
			// Volume inputs
			addInput(inputWidgets[i + N_TRK * 2] = createDynamicPortCentered<DynPort>(mm2px(Vec(xTrck1 + 12.7 * i, 31.5)), true, module, TMixMaster::TRACK_VOL_INPUTS + i, module ? &module->panelTheme : NULL));			
			// Pan inputs
			addInput(inputWidgets[i + N_TRK * 3] = createDynamicPortCentered<DynPort>(mm2px(Vec(xTrck1 + 12.7 * i, 40.5)), true, module, TMixMaster::TRACK_PAN_INPUTS + i, module ? &module->panelTheme : NULL));			
			// Pan knobs
			DynSmallKnobGreyWithArc *panKnobTrack;
			addParam(panKnobTrack = createDynamicParamCentered<DynSmallKnobGreyWithArc>(mm2px(Vec(xTrck1 + 12.7 * i, 51.8)), module, TMixMaster::TRACK_PAN_PARAMS + i, module ? &module->panelTheme : NULL));
			if (module) {
				panKnobTrack->detailsShowSrc = &(module->gInfo.colorAndCloak.cc4[detailsShow]);
				panKnobTrack->cloakedModeSrc = &(module->gInfo.colorAndCloak.cc4[cloakedMode]);
				panKnobTrack->paramWithCV = &(module->tracks[i].pan);
				panKnobTrack->paramCvConnected = &(module->tracks[i].panCvConnected);
				panKnobTrack->dispColorGlobalSrc = &(module->gInfo.colorAndCloak.cc4[dispColorGlobal]);
				panKnobTrack->dispColorLocalSrc = &(module->tracks[i].dispColorLocal);
			}
			
			// Faders
			DynSmallFaderWithLink *newFader;
			addParam(newFader = createDynamicParamCentered<DynSmallFaderWithLink>(mm2px(Vec(xTrck1 + 3.67 + 12.7 * i, 81.2)), module, TMixMaster::TRACK_FADER_PARAMS + i, module ? &module->panelTheme : NULL));
			if (module) {
				newFader->linkBitMaskSrc = &(module->gInfo.linkBitMask);
				newFader->baseFaderParamId = TMixMaster::TRACK_FADER_PARAMS;
				// VU meters
				VuMeterTrack *newVU = createWidgetCentered<VuMeterTrack>(mm2px(Vec(xTrck1 + 12.7 * i, 81.2)));
				newVU->srcLevels = module->tracks[i].vu.vuValues;
				newVU->srcMuteGhost = &(module->tracks[i].fadeGainScaledWithSolo);
				newVU->colorThemeGlobal = &(module->gInfo.colorAndCloak.cc4[vuColorGlobal]);
				newVU->colorThemeLocal = &(module->tracks[i].vuColorThemeLocal);
				addChild(newVU);
				// Fade pointers
				CvAndFadePointerTrack *newFP = createWidgetCentered<CvAndFadePointerTrack>(mm2px(Vec(xTrck1 - 2.95 + 12.7 * i, 81.2)));
				newFP->srcParam = &(module->params[TMixMaster::TRACK_FADER_PARAMS + i]);
				newFP->srcParamWithCV = &(module->tracks[i].paramWithCV);
				newFP->colorAndCloak = &(module->gInfo.colorAndCloak);
				newFP->srcFadeGain = &(module->tracks[i].fadeGain);
				newFP->srcFadeRate = module->tracks[i].fadeRate;
				newFP->dispColorLocalPtr = &(module->tracks[i].dispColorLocal);
				addChild(newFP);				
			}
			
			
			// Mutes
			DynMuteFadeButtonWithClear* newMuteFade;
			addParam(newMuteFade = createDynamicParamCentered<DynMuteFadeButtonWithClear>(mm2px(Vec(xTrck1 + 12.7 * i, 109.8)), module, TMixMaster::TRACK_MUTE_PARAMS + i, module ? &module->panelTheme : NULL));
			if (module) {
				newMuteFade->type = module->tracks[i].fadeRate;
				newMuteFade->muteParams = &module->params[TMixMaster::TRACK_MUTE_PARAMS];
				newMuteFade->baseMuteParamId = TMixMaster::TRACK_MUTE_PARAMS;
				newMuteFade->numTracksAndGroups = N_TRK + N_GRP;
			}
			// Solos
			DynSoloButtonMutex *newSoloButton;
			addParam(newSoloButton = createDynamicParamCentered<DynSoloButtonMutex>(mm2px(Vec(xTrck1 + 12.7 * i, 116.1)), module, TMixMaster::TRACK_SOLO_PARAMS + i, module ? &module->panelTheme : NULL));
			newSoloButton->soloParams =  module ? &module->params[TMixMaster::TRACK_SOLO_PARAMS] : NULL;
			newSoloButton->baseSoloParamId = TMixMaster::TRACK_SOLO_PARAMS;
			newSoloButton->numTracks = N_TRK;
			newSoloButton->numGroups = N_GRP;
			// Group dec
			DynGroupMinusButtonNotify *newGrpMinusButton;
			addChild(newGrpMinusButton = createDynamicWidgetCentered<DynGroupMinusButtonNotify>(mm2px(Vec(xTrck1 - 3.73 + 12.7 * i - 0.75, 123.1)), module ? &module->panelTheme : NULL));
			if (module) {
				newGrpMinusButton->sourceParam = &(module->params[TMixMaster::GROUP_SELECT_PARAMS + i]);
				newGrpMinusButton->numGroups = (float)N_GRP;
			}
			// Group inc
			DynGroupPlusButtonNotify *newGrpPlusButton;
			addChild(newGrpPlusButton = createDynamicWidgetCentered<DynGroupPlusButtonNotify>(mm2px(Vec(xTrck1 + 3.77 + 12.7 * i + 0.75, 123.1)), module ? &module->panelTheme : NULL));
			if (module) {
				newGrpPlusButton->sourceParam = &(module->params[TMixMaster::GROUP_SELECT_PARAMS + i]);
				newGrpPlusButton->numGroups = (float)N_GRP;
			}
			// Group select displays
			GroupSelectDisplay* groupSelectDisplay;
			addParam(groupSelectDisplay = createParamCentered<GroupSelectDisplay>(mm2px(Vec(xTrck1 + 12.7 * i, 123.1)), module, TMixMaster::GROUP_SELECT_PARAMS + i));
			if (module) {
				groupSelectDisplay->srcColor = &(module->gInfo.colorAndCloak);
				groupSelectDisplay->srcColorLocal = &(module->tracks[i].dispColorLocal);
				groupSelectDisplay->numGroups = N_GRP;
			}
		}
		for (int i = 0; i < N_TRK; i++) {
			trackDisplays[i]->tabNextFocus = trackDisplays[(i + 1) % N_TRK];
		}
		
		// Monitor outputs and groups
		static const float xGrp1 = 217.17 - 12.7 * 8 + 20.32;
		for (int i = 0; i < N_GRP; i++) {
			// Monitor outputs
			addOutput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(xGrp1 + 12.7 * (i), 11.5)), false, module, TMixMaster::DIRECT_OUTPUTS + i, module ? &module->panelTheme : NULL));
			// Labels
			addChild(groupDisplays[i] = createWidgetCentered<GroupDisplay<TMixMaster::MixerGroup>>(mm2px(Vec(xGrp1 + 12.7 * i, 23.5))));
			if (module) {
				// groupDisplays[i]->tabNextFocus = // done after the for loop
				groupDisplays[i]->colorAndCloak = &(module->gInfo.colorAndCloak);
				groupDisplays[i]->dispColorLocal = &(module->groups[i].dispColorLocal);
				groupDisplays[i]->srcGroup = &(module->groups[i]);
				groupDisplays[i]->auxExpanderPresentPtr = &(module->auxExpanderPresent);
				groupDisplays[i]->numTracks = N_TRK;
			}
			
			// Volume inputs
			addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(xGrp1 + 12.7 * i, 31.5)), true, module, TMixMaster::GROUP_VOL_INPUTS + i, module ? &module->panelTheme : NULL));			
			// Pan inputs
			addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(xGrp1 + 12.7 * i, 40.5)), true, module, TMixMaster::GROUP_PAN_INPUTS + i, module ? &module->panelTheme : NULL));			
			// Pan knobs
			DynSmallKnobGreyWithArc *panKnobGroup;
			addParam(panKnobGroup = createDynamicParamCentered<DynSmallKnobGreyWithArc>(mm2px(Vec(xGrp1 + 12.7 * i, 51.8)), module, TMixMaster::GROUP_PAN_PARAMS + i, module ? &module->panelTheme : NULL));
			if (module) {
				panKnobGroup->detailsShowSrc = &(module->gInfo.colorAndCloak.cc4[detailsShow]);
				panKnobGroup->cloakedModeSrc = &(module->gInfo.colorAndCloak.cc4[cloakedMode]);
				panKnobGroup->paramWithCV = &(module->groups[i].pan);
				panKnobGroup->paramCvConnected = &(module->groups[i].panCvConnected);
				panKnobGroup->dispColorGlobalSrc = &(module->gInfo.colorAndCloak.cc4[dispColorGlobal]);
				panKnobGroup->dispColorLocalSrc = &(module->groups[i].dispColorLocal);
			}
			
			// Faders
			DynSmallFaderWithLink *newFader;
			addParam(newFader = createDynamicParamCentered<DynSmallFaderWithLink>(mm2px(Vec(xGrp1 + 3.67 + 12.7 * i, 81.2)), module, TMixMaster::GROUP_FADER_PARAMS + i, module ? &module->panelTheme : NULL));		
			if (module) {
				newFader->linkBitMaskSrc = &(module->gInfo.linkBitMask);
				newFader->baseFaderParamId = TMixMaster::TRACK_FADER_PARAMS;
				// VU meters
				VuMeterTrack *newVU = createWidgetCentered<VuMeterTrack>(mm2px(Vec(xGrp1 + 12.7 * i, 81.2)));
				newVU->srcLevels = module->groups[i].vu.vuValues;
				newVU->srcMuteGhost = &(module->groups[i].fadeGainScaled);
				newVU->colorThemeGlobal = &(module->gInfo.colorAndCloak.cc4[vuColorGlobal]);
				newVU->colorThemeLocal = &(module->groups[i].vuColorThemeLocal);
				addChild(newVU);
				// Fade pointers
				CvAndFadePointerGroup *newFP = createWidgetCentered<CvAndFadePointerGroup>(mm2px(Vec(xGrp1 - 2.95 + 12.7 * i, 81.2)));
				newFP->srcParam = &(module->params[TMixMaster::GROUP_FADER_PARAMS + i]);
				newFP->srcParamWithCV = &(module->groups[i].paramWithCV);
				newFP->colorAndCloak = &(module->gInfo.colorAndCloak);
				newFP->srcFadeGain = &(module->groups[i].fadeGain);
				newFP->srcFadeRate = module->groups[i].fadeRate;
				newFP->dispColorLocalPtr = &(module->groups[i].dispColorLocal);
				addChild(newFP);				
			}

			// Mutes
			DynMuteFadeButtonWithClear* newMuteFade;
			addParam(newMuteFade = createDynamicParamCentered<DynMuteFadeButtonWithClear>(mm2px(Vec(xGrp1 + 12.7 * i, 109.8)), module, TMixMaster::GROUP_MUTE_PARAMS + i, module ? &module->panelTheme : NULL));
			if (module) {
				newMuteFade->type = module->groups[i].fadeRate;
				newMuteFade->muteParams = &module->params[TMixMaster::TRACK_MUTE_PARAMS];
				newMuteFade->baseMuteParamId = TMixMaster::TRACK_MUTE_PARAMS;
				newMuteFade->numTracksAndGroups = N_TRK + N_GRP;
			}
			// Solos
			DynSoloButtonMutex* newSoloButton;
			addParam(newSoloButton = createDynamicParamCentered<DynSoloButtonMutex>(mm2px(Vec(xGrp1 + 12.7 * i, 116.1)), module, TMixMaster::GROUP_SOLO_PARAMS + i, module ? &module->panelTheme : NULL));
			newSoloButton->soloParams =  module ? &module->params[TMixMaster::TRACK_SOLO_PARAMS] : NULL;
			newSoloButton->baseSoloParamId = TMixMaster::TRACK_SOLO_PARAMS;
			newSoloButton->numTracks = N_TRK;
			newSoloButton->numGroups = N_GRP;
		}
		for (int i = 0; i < N_GRP; i++) {
			groupDisplays[i]->tabNextFocus = groupDisplays[(i + 1) % N_GRP];
		}
	
		// Master inputs
		addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(289.62 - 12.7 * 10, 12.8)), true, module, TMixMaster::CHAIN_INPUTS + 0, module ? &module->panelTheme : NULL));			
		addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(289.62 - 12.7 * 10, 21.8)), true, module, TMixMaster::CHAIN_INPUTS + 1, module ? &module->panelTheme : NULL));			
		
		// Master outputs
		addOutput(createDynamicPortCentered<DynPort>(mm2px(Vec(300.12 - 12.7 * 10, 12.8)), false, module, TMixMaster::MAIN_OUTPUTS + 0, module ? &module->panelTheme : NULL));			
		addOutput(createDynamicPortCentered<DynPort>(mm2px(Vec(300.12 - 12.7 * 10, 21.8)), false, module, TMixMaster::MAIN_OUTPUTS + 1, module ? &module->panelTheme : NULL));			
		
		// Master label
		addChild(masterDisplay = createWidgetCentered<MasterDisplay>(mm2px(Vec(294.81 - 12.7 * 10, 128.5 - 97.15))));
		if (module) {
			masterDisplay->dcBlock = &(module->master.dcBlock);
			masterDisplay->clipping = &(module->master.clipping);
			masterDisplay->fadeRate = &(module->master.fadeRate);
			masterDisplay->fadeProfile = &(module->master.fadeProfile);
			masterDisplay->vuColorThemeLocal = &(module->master.vuColorThemeLocal);
			masterDisplay->dispColorLocal = &(module->master.dispColorLocal);
			masterDisplay->dimGain = &(module->master.dimGain);
			masterDisplay->masterLabel = module->master.masterLabel;
			masterDisplay->dimGainIntegerDB = &(module->master.dimGainIntegerDB);
			masterDisplay->colorAndCloak = &(module->gInfo.colorAndCloak);
			masterDisplay->idSrc = &(module->id);
		}
		
		// Master fader
		addParam(createDynamicParamCentered<DynBigFader>(mm2px(Vec(300.17 - 12.7 * 10, 70.3)), module, TMixMaster::MAIN_FADER_PARAM, module ? &module->panelTheme : NULL));
		if (module) {
			// VU meter
			VuMeterMaster *newVU = createWidgetCentered<VuMeterMaster>(mm2px(Vec(294.82 - 12.7 * 10, 70.3)));
			newVU->srcLevels = module->master.vu.vuValues;
			newVU->srcMuteGhost = &(module->master.fadeGainScaled);
			newVU->colorThemeGlobal = &(module->gInfo.colorAndCloak.cc4[vuColorGlobal]);
			newVU->colorThemeLocal = &(module->master.vuColorThemeLocal);
			newVU->clippingPtr = &(module->master.clipping);
			addChild(newVU);
			// Fade pointer
			CvAndFadePointerMaster *newFP = createWidgetCentered<CvAndFadePointerMaster>(mm2px(Vec(294.82 - 12.7 * 10 - 3.4, 70.3)));
			newFP->srcParam = &(module->params[TMixMaster::MAIN_FADER_PARAM]);
			newFP->srcParamWithCV = &(module->master.paramWithCV);
			newFP->colorAndCloak = &(module->gInfo.colorAndCloak);
			newFP->srcFadeGain = &(module->master.fadeGain);
			newFP->srcFadeRate = &(module->master.fadeRate);
			addChild(newFP);				
		}
		
		// Master mute
		DynMuteFadeButton* newMuteFade;
		addParam(newMuteFade = createDynamicParamCentered<DynMuteFadeButton>(mm2px(Vec(294.82 - 12.7 * 10, 109.8)), module, TMixMaster::MAIN_MUTE_PARAM, module ? &module->panelTheme : NULL));
		if (module) {
			newMuteFade->type = &(module->master.fadeRate);
		}
		
		// Master dim
		addParam(createDynamicParamCentered<DynDimButton>(mm2px(Vec(289.42 - 12.7 * 10, 116.1)), module, TMixMaster::MAIN_DIM_PARAM, module ? &module->panelTheme : NULL));
		
		// Master mono
		addParam(createDynamicParamCentered<DynMonoButton>(mm2px(Vec(300.22 - 12.7 * 10, 116.1)), module, TMixMaster::MAIN_MONO_PARAM, module ? &module->panelTheme : NULL));
	}
};



Model *modelMixMaster = createModel<MixMaster<16, 4>, MixMasterWidget>("MixMaster");
Model *modelMixMasterJr = createModel<MixMaster<8, 2>, MixMasterJrWidget>("MixMasterJr");
