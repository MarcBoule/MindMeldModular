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
		ENUMS(TRACK_HPCUT_PARAMS, N_TRK),
		ENUMS(TRACK_LPCUT_PARAMS, N_TRK),
		ENUMS(GROUP_HPCUT_PARAMS, N_GRP),
		ENUMS(GROUP_LPCUT_PARAMS, N_GRP),
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
		ENUMS(GROUP_HPF_LIGHTS, N_GRP),
		ENUMS(GROUP_LPF_LIGHTS, N_GRP),
		NUM_LIGHTS
	};

	typedef TAfmExpInterface<N_TRK, N_GRP> AfmExpInterface;


	#include "MixMaster.hpp"
	
	
	// Expander
	MfaExpInterface rightMessages[2];// messages from aux-expander, see MixerCommon.hpp

	// Constants
	int numChannels16 = 16;// avoids warning that happens when hardcode 16 (static const or directly use 16 in code below)

	// Need to save, no reset
	// none
	
	// Need to save, with reset
	alignas(4) char trackLabels[4 * (N_TRK + N_GRP) + 4];// 4 chars per label, 16 (8) tracks and 4 (2) groups means 20 (10) labels, null terminate the end the whole array only, pad with three extra chars for alignment
	GlobalInfo gInfo;
	MixerTrack tracks[N_TRK];
	MixerGroup groups[N_GRP];
	MixerAux aux[4];
	MixerMaster master;
	
	// No need to save, with reset
	int updateTrackLabelRequest;// 0 when nothing to do, 1 for read names in widget
	int refreshCounter4;
	int32_t trackMoveInAuxRequest;// 0 when nothing to do, {dest,src} packed when a move is requested
	int8_t trackOrGroupResetInAux;// -1 when nothing to do, 0 to N_TRK-1 for track reset, N_TRK to N_TRK+N_GRP-1 for group reset 
	SlewLimiterSingle muteTrackWhenSoloAuxRetSlewer;

	// No need to save, no reset
	RefreshCounter refresh;	
	bool auxExpanderPresent = false;// can't be local to process() since widget must know in order to properly draw border
	float trackTaps[N_TRK * 2 * 4];// room for 4 taps for each of the 16 (8) stereo tracks. Trk0-tap0, Trk1-tap0 ... Trk15-tap0,  Trk0-tap1
	float trackInsertOuts[N_TRK * 2];// room for 16 (8) stereo track insert outs
	float groupTaps[N_GRP * 2 * 4];// room for 4 taps for each of the 4 stereo groups
	float groupInsertOuts[N_GRP * 2];// room for 4 (2) stereo group insert outs
	float auxTaps[4 * 2 * 4];// room for 4 taps for each of the 4 stereo aux
	uint32_t muteAuxSendWhenReturnGrouped;// { ... g2-B, g2-A, g1-D, g1-C, g1-B, g1-A}
	TriggerRiseFall muteSoloCvTriggers[N_TRK * 2 + N_GRP * 2 + 3];// 16 (8) trk mute, 16 (8) trk solo, 4 (2) grp mute, 4 (2) grp solo, 3 mast (mute, dim, mono)
	// fast exp values
	float *auxReturns;
	float *auxRetFadePanFadecv;
	// slow exp values that are saved and need an init in constructor since not guaranteed to be set in first pass of expander:
	PackedBytes4 directOutsModeLocalAux;// slow expander
	PackedBytes4 stereoPanModeLocalAux;// slow expander
	PackedBytes4 auxVuColors;// slow expander
	PackedBytes4 auxDispColors;// slow expander
	float values20[20];// slow expander
	alignas(4) char auxLabels[4 * 4 + 4];// slow expander
		
		
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
		
		rightExpander.producerMessage = &rightMessages[0];
		rightExpander.consumerMessage = &rightMessages[1];

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
			
			// HPF cutoff
			snprintf(strBuf, 32, "-%02i-: HPF cutoff", i + 1);
			configParam<HPFCutoffParamQuantity>(TRACK_HPCUT_PARAMS + i, 13.0f, 1000.0f, GlobalConst::defHPFCutoffFreq, strBuf); 
			// LPF cutoff
			snprintf(strBuf, 32, "-%02i-: LPF cutoff", i + 1);
			configParam<LPFCutoffParamQuantity>(TRACK_LPCUT_PARAMS + i, 1000.0f, 21000.0f, GlobalConst::defLPFCutoffFreq, strBuf, "", 0.0f, 0.001f);// diplay params are: base, mult, offset 
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
			
			// HPF cutoff
			snprintf(strBuf, 32, "GRP%i: HPF cutoff", i + 1);
			configParam<HPFCutoffParamQuantity>(GROUP_HPCUT_PARAMS + i, 13.0f, 1000.0f, GlobalConst::defHPFCutoffFreq, strBuf); 
			// LPF cutoff
			snprintf(strBuf, 32, "GRP%i: LPF cutoff", i + 1);
			configParam<LPFCutoffParamQuantity>(GROUP_LPCUT_PARAMS + i, 1000.0f, 21000.0f, GlobalConst::defLPFCutoffFreq, strBuf, "", 0.0f, 0.001f);// diplay params are: base, mult, offset 
		}
		float maxMFader = std::pow(GlobalConst::masterFaderMaxLinearGain, 1.0f / GlobalConst::masterFaderScalingExponent);
		configParam(MAIN_FADER_PARAM, 0.0f, maxMFader, 1.0f, "MASTER: level", " dB", -10, 20.0f * GlobalConst::masterFaderScalingExponent);
		// Mute
		configParam(MAIN_MUTE_PARAM, 0.0f, 1.0f, 0.0f, "MASTER: mute");
		// Dim
		configParam(MAIN_DIM_PARAM, 0.0f, 1.0f, 0.0f, "MASTER: dim");
		// Mono
		configParam(MAIN_MONO_PARAM, 0.0f, 1.0f, 0.0f, "MASTER: mono");
		
		// slow exp values that are saved and need an init in constructor since not guaranteed to be set in first pass of expander:
		directOutsModeLocalAux.cc1 = 0;
		stereoPanModeLocalAux.cc1 = 0;
		auxVuColors.cc1 = 0;
		auxDispColors.cc1 = 0;
		for (int i = 0; i < 20; i++) {
			values20[i] = 0.0f;
		}
		snprintf(auxLabels, 4 * 4 + 1, "AUXAAUXBAUXCAUXD");

		gInfo.construct(&params[0], values20);
		trackLabels[4 * (N_TRK + N_GRP)] = 0;
		for (int i = 0; i < N_TRK; i++) {
			tracks[i].construct(i, &gInfo, &inputs[0], &params[0], &(trackLabels[4 * i]), &trackTaps[i << 1], groupTaps, &trackInsertOuts[i << 1]);
		}
		for (int i = 0; i < N_GRP; i++) {
			groups[i].construct(i, &gInfo, &inputs[0], &params[0], &(trackLabels[4 * (N_TRK + i)]), &groupTaps[i << 1], &groupInsertOuts[i << 1]);
		}
		for (int i = 0; i < 4; i++) {
			aux[i].construct(i, &gInfo, &inputs[0], values20, &auxTaps[i << 1], &stereoPanModeLocalAux.cc4[i]);
		}
		master.construct(&gInfo, &params[0], &inputs[0]);
		muteTrackWhenSoloAuxRetSlewer.setRiseFall(GlobalConst::antipopSlewFast); // slew rate is in input-units per second 
		onReset();

		sendToMessageBus();// register by just writing data
	}
  
	~MixMaster() {
		if (id > -1) {
			mixerMessageBus.deregisterMember(id + 1);
		}
	}

	
	void onReset() override {
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
		refreshCounter4 = 0;
		trackMoveInAuxRequest = 0;
		trackOrGroupResetInAux = -1;
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
		dataFromJsonWithSize(rootJ, N_TRK, N_GRP);
	}
	void dataFromJsonWithSize(json_t *rootJ, int nTrkSrc, int nGrpSrc) {
		// trackLabels
		json_t *textJ = json_object_get(rootJ, "trackLabels");
		if (textJ) {
			const char* labels = json_string_value(textJ);
			memcpy(  trackLabels,               labels,               4 * std::min(N_TRK, nTrkSrc));
			memcpy(&(trackLabels[4 * N_TRK]), &(labels[4 * nTrkSrc]), 4 * std::min(N_GRP, nGrpSrc));		
		}
		
		// gInfo
		gInfo.dataFromJson(rootJ, nTrkSrc, nGrpSrc);

		// tracks
		for (int i = 0; i < std::min(N_TRK, nTrkSrc); i++) {
			tracks[i].dataFromJson(rootJ);
		}
		// groups
		for (int i = 0; i < std::min(N_GRP, nGrpSrc); i++) {
			groups[i].dataFromJson(rootJ);
		}
		// aux
		for (int i = 0; i < 4; i++) {
			aux[i].dataFromJson(rootJ);
		}
		// master
		master.dataFromJson(rootJ);
		
		resetNonJson(true);
	}
	

	void swapCopyToClipboard() {
		// mixer
		json_t* mixerJ = json_object();
		
		// dimensions
		json_object_set_new(mixerJ, "n-trk", json_integer(N_TRK));
		json_object_set_new(mixerJ, "n-grp", json_integer(N_GRP));
		
		// params
		json_object_set_new(mixerJ, "TRACK_FADER_PARAMS", paramArrayToJsonArray(TRACK_FADER_PARAMS, N_TRK));
		json_object_set_new(mixerJ, "GROUP_FADER_PARAMS", paramArrayToJsonArray(GROUP_FADER_PARAMS, N_GRP));
		json_object_set_new(mixerJ, "TRACK_PAN_PARAMS", paramArrayToJsonArray(TRACK_PAN_PARAMS, N_TRK));
		json_object_set_new(mixerJ, "GROUP_PAN_PARAMS", paramArrayToJsonArray(GROUP_PAN_PARAMS, N_GRP));
		json_object_set_new(mixerJ, "TRACK_MUTE_PARAMS", paramArrayToJsonArray(TRACK_MUTE_PARAMS, N_TRK));
		json_object_set_new(mixerJ, "GROUP_MUTE_PARAMS", paramArrayToJsonArray(GROUP_MUTE_PARAMS, N_GRP));
		json_object_set_new(mixerJ, "TRACK_SOLO_PARAMS", paramArrayToJsonArray(TRACK_SOLO_PARAMS, N_TRK));
		json_object_set_new(mixerJ, "GROUP_SOLO_PARAMS", paramArrayToJsonArray(GROUP_SOLO_PARAMS, N_GRP));
		json_object_set_new(mixerJ, "MAIN_MUTE_PARAM", json_real(params[MAIN_MUTE_PARAM].getValue()));
		json_object_set_new(mixerJ, "MAIN_DIM_PARAM", json_real(params[MAIN_DIM_PARAM].getValue()));
		json_object_set_new(mixerJ, "MAIN_MONO_PARAM", json_real(params[MAIN_MONO_PARAM].getValue()));
		json_object_set_new(mixerJ, "MAIN_FADER_PARAM", json_real(params[MAIN_FADER_PARAM].getValue()));
		json_object_set_new(mixerJ, "GROUP_SELECT_PARAMS", paramArrayToJsonArray(GROUP_SELECT_PARAMS, N_TRK));
		json_object_set_new(mixerJ, "TRACK_HPCUT_PARAMS", paramArrayToJsonArray(TRACK_HPCUT_PARAMS, N_TRK));
		json_object_set_new(mixerJ, "TRACK_LPCUT_PARAMS", paramArrayToJsonArray(TRACK_LPCUT_PARAMS, N_TRK));
		json_object_set_new(mixerJ, "GROUP_HPCUT_PARAMS", paramArrayToJsonArray(GROUP_HPCUT_PARAMS, N_GRP));
		json_object_set_new(mixerJ, "GROUP_LPCUT_PARAMS", paramArrayToJsonArray(GROUP_LPCUT_PARAMS, N_GRP));

		// dataToJson data
		json_object_set_new(mixerJ, "dataToJson-data", dataToJson());
		
		// clipboard
		json_t* clipboardJ = json_object();		
		json_object_set_new(clipboardJ, "mixmaster-swap", mixerJ);
		
		char* inerchangeClip = json_dumps(clipboardJ, JSON_INDENT(2) | JSON_REAL_PRECISION(9));
		json_decref(clipboardJ);
		glfwSetClipboardString(APP->window->win, inerchangeClip);
		free(inerchangeClip);
	}
	json_t* paramArrayToJsonArray(int baseParam, int numParam) {
		json_t* paramJ = json_array();
		for (int i = baseParam; i < (baseParam + numParam); i++) {
			json_array_append_new(paramJ, json_real(params[i].getValue()));
		}
		return paramJ;
	}


	void swapPasteFromClipboard() {
		// clipboard
		const char* inerchangeClip = glfwGetClipboardString(APP->window->win);

		if (!inerchangeClip) {
			WARN("MixMaster swap: error getting clipboard string");
			return;
		}

		json_error_t error;
		json_t* clipboardJ = json_loads(inerchangeClip, 0, &error);
		if (!clipboardJ) {
			WARN("MixMaster swap: error json parsing clipboard");
			return;
		}
		DEFER({json_decref(clipboardJ);});

		// mixer
		json_t* mixerJ = json_object_get(clipboardJ, "mixmaster-swap");
		if (!mixerJ) {
			WARN("MixMaster swap: error no mixmaster-swap present in clipboard");
			return;
		}

		// dimensions
		json_t* nTrkJ = json_object_get(mixerJ, "n-trk");
		if (!nTrkJ) {
			WARN("MixMaster swap: error num tracks missing");
			return;
		}
		int n_trk = json_integer_value(nTrkJ);
		
		json_t* nGrpJ = json_object_get(mixerJ, "n-grp");
		if (!nGrpJ) {
			WARN("MixMaster swap: error num groups missing");
			return;
		}
		int n_grp = json_integer_value(nGrpJ);
		
		// params
		jsonArrayToParamDirect(json_object_get(mixerJ, "TRACK_FADER_PARAMS"), TRACK_FADER_PARAMS, N_TRK);		
		jsonArrayToParamDirect(json_object_get(mixerJ, "GROUP_FADER_PARAMS"), GROUP_FADER_PARAMS, N_GRP);
		jsonArrayToParamDirect(json_object_get(mixerJ, "TRACK_PAN_PARAMS"), TRACK_PAN_PARAMS, N_TRK);
		jsonArrayToParamDirect(json_object_get(mixerJ, "GROUP_PAN_PARAMS"), GROUP_PAN_PARAMS, N_GRP);
		jsonArrayToParamDirect(json_object_get(mixerJ, "TRACK_MUTE_PARAMS"), TRACK_MUTE_PARAMS, N_TRK);
		jsonArrayToParamDirect(json_object_get(mixerJ, "GROUP_MUTE_PARAMS"), GROUP_MUTE_PARAMS, N_GRP);
		jsonArrayToParamDirect(json_object_get(mixerJ, "TRACK_SOLO_PARAMS"), TRACK_SOLO_PARAMS, N_TRK);
		jsonArrayToParamDirect(json_object_get(mixerJ, "GROUP_SOLO_PARAMS"), GROUP_SOLO_PARAMS, N_GRP);
		json_t* muteJ = json_object_get(mixerJ, "MAIN_MUTE_PARAM");
		if (muteJ) {
			params[MAIN_MUTE_PARAM].setValue(json_number_value(muteJ));
		}
		json_t* dimJ = json_object_get(mixerJ, "MAIN_DIM_PARAM");
		if (dimJ) {
			params[MAIN_DIM_PARAM].setValue(json_number_value(dimJ));
		}
		json_t* monoJ = json_object_get(mixerJ, "MAIN_MONO_PARAM");
		if (monoJ) {
			params[MAIN_MONO_PARAM].setValue(json_number_value(monoJ));
		}
		json_t* faderJ = json_object_get(mixerJ, "MAIN_FADER_PARAM");
		if (faderJ) {
			params[MAIN_FADER_PARAM].setValue(json_number_value(faderJ));
		}
		jsonArrayToParamDirect(json_object_get(mixerJ, "GROUP_SELECT_PARAMS"), GROUP_SELECT_PARAMS, N_TRK);
		jsonArrayToParamDirect(json_object_get(mixerJ, "TRACK_HPCUT_PARAMS"), TRACK_HPCUT_PARAMS, N_TRK);
		jsonArrayToParamDirect(json_object_get(mixerJ, "TRACK_LPCUT_PARAMS"), TRACK_LPCUT_PARAMS, N_TRK);
		jsonArrayToParamDirect(json_object_get(mixerJ, "GROUP_HPCUT_PARAMS"), GROUP_HPCUT_PARAMS, N_GRP);
		jsonArrayToParamDirect(json_object_get(mixerJ, "GROUP_LPCUT_PARAMS"), GROUP_LPCUT_PARAMS, N_GRP);
		
		// dataToJson data
		json_t* dataToJsonJ = json_object_get(mixerJ, "dataToJson-data");
		if (!dataToJsonJ) {
			WARN("MixMaster swap: error dataToJson-data missing");
			return;
		}
		dataFromJsonWithSize(dataToJsonJ, n_trk, n_grp);
	}
	void jsonArrayToParamDirect(json_t* paramJ, int baseParam, int numParam) {// numParam is of .this
		if ( !paramJ || !json_is_array(paramJ) ) {
			WARN("MixMaster swap: error param array malformed or missing");
			return;
		}
		for (int i = 0; i < std::min((int)json_array_size(paramJ), numParam) ; i++) {
			json_t* paramItemJ = json_array_get(paramJ, i);
			if (!paramItemJ) {
				WARN("MixMaster swap: error missing param value in param array");
				return;		
			}
			params[baseParam + i].setValue(json_number_value(paramItemJ));
		}	
	}


	void onSampleRateChange() override {
		gInfo.sampleTime = APP->engine->getSampleTime();
		for (int trk = 0; trk < N_TRK; trk++) {
			tracks[trk].onSampleRateChange();
		}
		for (int grp = 0; grp < N_GRP; grp++) {
			groups[grp].onSampleRateChange();
		}
		master.onSampleRateChange();
	}
	

	void process(const ProcessArgs &args) override {
		
		auxExpanderPresent = (rightExpander.module && rightExpander.module->model == (N_TRK == 16 ? modelAuxExpander : modelAuxExpanderJr));
		
		
		//********** Inputs **********
		
		// From Aux-Expander
		if (auxExpanderPresent) {
			MfaExpInterface *messagesFromExpander = (MfaExpInterface*)rightExpander.consumerMessage;// could be invalid pointer when !expanderPresent, so read it only when expanderPresent
			
			// Slow values from expander
			if (messagesFromExpander->updateSlow) {
				directOutsModeLocalAux.cc1 = messagesFromExpander->directOutsModeLocalAux.cc1;
				stereoPanModeLocalAux.cc1 = messagesFromExpander->stereoPanModeLocalAux.cc1;
				auxVuColors.cc1 = messagesFromExpander->auxVuColors.cc1;
				auxDispColors.cc1 = messagesFromExpander->auxDispColors.cc1;
				memcpy(values20, messagesFromExpander->values20, 4 * 20);
				memcpy(auxLabels, messagesFromExpander->auxLabels, 4 * 4);
			}
			
			// Aux returns
			auxReturns = messagesFromExpander->auxReturns; // contains 8 values of the returns from the aux panel
			auxRetFadePanFadecv = messagesFromExpander->auxRetFaderPanFadercv; // contains 12 values of the return faders and pan knobs and cvs for faders			
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
					aux[auxi].process(&groupTaps[auxGroup << 1], &auxRetFadePanFadecv[auxi], ecoStagger3);// stagger 3
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
					aux[auxi].process(mix, &auxRetFadePanFadecv[auxi], ecoStagger3);// stagger 3
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
				
		// Insert outs (uses trackInsertOuts, groupInsertOuts and aux tap0)
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
			for (int i = 0; i < N_GRP; i++) {
				lights[GROUP_HPF_LIGHTS + i].setBrightness(groups[i].getHPFCutoffFreq() >= GlobalConst::minHPFCutoffFreq ? 1.0f : 0.0f);
				lights[GROUP_LPF_LIGHTS + i].setBrightness(groups[i].getLPFCutoffFreq() <= GlobalConst::maxLPFCutoffFreq ? 1.0f : 0.0f);
			}
		}
		
		
		//********** Expander **********
		
		// To Aux-Expander
		if (auxExpanderPresent) {
			AfmExpInterface *messageToExpander = (AfmExpInterface*)(rightExpander.module->leftExpander.producerMessage);
			
			// Slow
			messageToExpander->updateSlow = refresh.refreshCounter == 0;
			if (messageToExpander->updateSlow) {
				messageToExpander->colorAndCloak.cc1 = gInfo.colorAndCloak.cc1;
				messageToExpander->directOutPanStereoMomentCvLinearVol.cc1 = gInfo.directOutPanStereoMomentCvLinearVol.cc1;
				messageToExpander->muteAuxSendWhenReturnGrouped = muteAuxSendWhenReturnGrouped;
				messageToExpander->ecoMode = gInfo.ecoMode;
				messageToExpander->trackMoveInAuxRequest = trackMoveInAuxRequest;
				trackMoveInAuxRequest = 0;
				messageToExpander->trackOrGroupResetInAux = trackOrGroupResetInAux;
				trackOrGroupResetInAux = -1;
				memcpy(messageToExpander->trackLabels, trackLabels, ((N_TRK + N_GRP) << 2));
				
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
				memcpy(messageToExpander->trackDispColsLocal, tmpDispCols, (N_TRK / 4 + 1) * 4);
				// auxFadeGains
				for (int auxi = 0; auxi < 4; auxi++) {
					messageToExpander->auxRetFadeGains[auxi] = aux[auxi].fadeGain;
				}
				// mute ghost
				for (int auxi = 0; auxi < 4; auxi++) {
					messageToExpander->srcMuteGhost[auxi] = aux[auxi].fadeGainScaledWithSolo;
				}
			}
			
			// Fast
			
			// 16+4 (8+2) stereo signals to be used to make sends in aux expander
			writeAuxSends(messageToExpander->auxSends);						
			// Aux VUs
			// a return VU related value; index 0-3 : quad vu floats of a given aux
			messageToExpander->vuIndex = refreshCounter4;
			memcpy(messageToExpander->vuValues, aux[refreshCounter4].vu.vuValues, 4 * 4);
			
			refreshCounter4++;
			if (refreshCounter4 >= 4) {
				refreshCounter4 = 0;
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
				
		float masterGain = gInfo.masterFaderScalesSends == 0 ? 1.0f : (master.gainMatrixSlewers.out[1] + master.gainMatrixSlewers.out[2]) * master.chainGainAndMuteSlewers.out[2];
		
		// tracks
		if ( gInfo.auxSendsMode < 4 && gInfo.masterFaderScalesSends == 0 && (gInfo.groupsControlTrackSendLevels == 0 || gInfo.groupUsage[N_GRP] == 0) ) {
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
				if (masterGain != 1.0f) {
					trackL *= masterGain;
					trackR *= masterGain;
				}
				auxSends[(trk << 1) + 0] = trackL;
				auxSends[(trk << 1) + 1] = trackR;
			}
		}
		
		// groups
		if (gInfo.auxSendsMode < 4 && gInfo.masterFaderScalesSends == 0) {
			memcpy(&auxSends[N_TRK * 2], &groupTaps[gInfo.auxSendsMode << (1 + N_GRP / 2)], (N_GRP * 2) * 4);
		}
		else {
			for (int grp = 0; grp < N_GRP; grp++) {
				int tapIndex = (groups[grp].auxSendsMode << (1 + N_GRP / 2));
				auxSends[(grp << 1) + N_TRK * 2] = groupTaps[tapIndex + (grp << 1) + 0] * masterGain;
				auxSends[(grp << 1) + N_TRK * 2] = groupTaps[tapIndex + (grp << 1) + 1] * masterGain;
			}
		}

	}
	
	
	void SetDirectTrackOuts(const int base) {// base is 0 or 8
		int outi = base >> 3;
		if (outputs[DIRECT_OUTPUTS + outi].isConnected()) {
			outputs[DIRECT_OUTPUTS + outi].setChannels(numChannels16);

			for (unsigned int i = 0; i < 8; i++) {
				if (gInfo.directOutsSkipGroupedTracks != 0 && tracks[base + i].paGroup->getValue() >= 0.5f) {
					outputs[DIRECT_OUTPUTS + outi].setVoltage(0.0f, 2 * i);
					outputs[DIRECT_OUTPUTS + outi].setVoltage(0.0f, 2 * i + 1);
				}
				else {
					int tapIndex = gInfo.directOutPanStereoMomentCvLinearVol.cc4[0] < 4 ? gInfo.directOutPanStereoMomentCvLinearVol.cc4[0] : tracks[base + i].directOutsMode;
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
	}
	
	void SetDirectGroupAuxOuts() {
		int outputNum = DIRECT_OUTPUTS + N_TRK / 8;
		if (outputs[outputNum].isConnected()) {
			outputs[outputNum].setChannels(auxExpanderPresent ? numChannels16 : 8);

			// Groups
			int tapIndex = gInfo.directOutPanStereoMomentCvLinearVol.cc4[0];			
			if (gInfo.directOutPanStereoMomentCvLinearVol.cc4[0] < 4) {// global direct outs
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
				if (gInfo.directOutPanStereoMomentCvLinearVol.cc4[0] < 4) {// global direct outs
					int tapIndex = gInfo.directOutPanStereoMomentCvLinearVol.cc4[0];
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
			memcpy(outputs[INSERT_GRP_AUX_OUTPUT].getVoltages(), groupInsertOuts, 4 *  N_GRP * 2);
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
					if (gInfo.directOutPanStereoMomentCvLinearVol.cc4[2]) {
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
					if (gInfo.directOutPanStereoMomentCvLinearVol.cc4[2]) {
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
					if (gInfo.directOutPanStereoMomentCvLinearVol.cc4[2]) {
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
					if (gInfo.directOutPanStereoMomentCvLinearVol.cc4[2]) {
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
				if (gInfo.directOutPanStereoMomentCvLinearVol.cc4[2]) {
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
				if (gInfo.directOutPanStereoMomentCvLinearVol.cc4[2]) {
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
				if (gInfo.directOutPanStereoMomentCvLinearVol.cc4[2]) {
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
		SvgPanel* svgPanel = (SvgPanel*)getPanel();
		panelBorder = findBorder(svgPanel->fb);
		
		// Inserts and CVs
		static const float xIns = 13.8;
		// Insert outputs
		addOutput(createOutputCentered<MmPortGold>(mm2px(Vec(xIns, 12.8)), module, TMixMaster::INSERT_TRACK_OUTPUTS + 0));
		addOutput(createOutputCentered<MmPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 1)), module, TMixMaster::INSERT_TRACK_OUTPUTS + 1));
		addOutput(createOutputCentered<MmPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 2)), module, TMixMaster::INSERT_GRP_AUX_OUTPUT));
		// Insert inputs
		addInput(createInputCentered<MmPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 3)), module, TMixMaster::INSERT_TRACK_INPUTS + 0));
		addInput(createInputCentered<MmPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 4)), module, TMixMaster::INSERT_TRACK_INPUTS + 1));
		addInput(createInputCentered<MmPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 5)), module, TMixMaster::INSERT_GRP_AUX_INPUT));
		// Mute, solo and other CVs
		addInput(createInputCentered<MmPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 7)), module, TMixMaster::TRACK_MUTESOLO_INPUTS + 0));
		addInput(createInputCentered<MmPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 8)), module, TMixMaster::TRACK_MUTESOLO_INPUTS + 1));
		addInput(createInputCentered<MmPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 9)), module, TMixMaster::GRPM_MUTESOLO_INPUT));
		
	
		// Tracks
		static const float xTrck1 = 11.43 + 20.32;
		for (int i = 0; i < N_TRK; i++) {
			// Labels
			addChild(trackDisplays[i] = createWidgetCentered<TrackDisplay<TMixMaster::MixerTrack>>(mm2px(Vec(xTrck1 + 12.7 * i + 2, 4.7))));
			if (module) {
				// trackDisplays[i]->tabNextFocus = // done after the for loop
				trackDisplays[i]->colorAndCloak = &(module->gInfo.colorAndCloak);
				trackDisplays[i]->dispColorLocal = &(module->tracks[i].dispColorLocal);				
				trackDisplays[i]->tracks = module->tracks;
				trackDisplays[i]->trackNumSrc = i;
				trackDisplays[i]->auxExpanderPresentPtr = &(module->auxExpanderPresent);
				trackDisplays[i]->numTracks = N_TRK;
				trackDisplays[i]->updateTrackLabelRequestPtr = &(module->updateTrackLabelRequest);
				trackDisplays[i]->trackOrGroupResetInAuxPtr = &(module->trackOrGroupResetInAux);
				trackDisplays[i]->trackMoveInAuxRequestPtr = &(module->trackMoveInAuxRequest);
				trackDisplays[i]->inputWidgets = inputWidgets;
				trackDisplays[i]->hpfParamQuantity = module->paramQuantities[TMixMaster::TRACK_HPCUT_PARAMS + i];
				trackDisplays[i]->lpfParamQuantity = module->paramQuantities[TMixMaster::TRACK_LPCUT_PARAMS + i];
			}
			// HPF lights
			addChild(createLightCentered<TinyLight<GreenLight>>(mm2px(Vec(xTrck1 - 4.17 + 12.7 * i, 8.3)), module, TMixMaster::TRACK_HPF_LIGHTS + i));	
			addParam(createParamCentered<FilterCutWidget>(mm2px(Vec(xTrck1 - 4.17 + 12.7 * i, 8.3)), module, TMixMaster::TRACK_HPCUT_PARAMS + i));				
			// LPF lights
			addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(xTrck1 + 4.17 + 12.7 * i, 8.3)), module, TMixMaster::TRACK_LPF_LIGHTS + i));	
			addParam(createParamCentered<FilterCutWidget>(mm2px(Vec(xTrck1 + 4.17 + 12.7 * i, 8.3)), module, TMixMaster::TRACK_LPCUT_PARAMS + i));	
			// Left inputs
			addInput(inputWidgets[i + 0] = createInputCentered<MmPort>(mm2px(Vec(xTrck1 + 12.7 * i, 12.8)), module, TMixMaster::TRACK_SIGNAL_INPUTS + 2 * i + 0));			
			// Right inputs
			addInput(inputWidgets[i + 16] = createInputCentered<MmPort>(mm2px(Vec(xTrck1 + 12.7 * i, 21.8)), module, TMixMaster::TRACK_SIGNAL_INPUTS + 2 * i + 1));	
			// Volume inputs
			addInput(inputWidgets[i + 16 * 2] = createInputCentered<MmPort>(mm2px(Vec(xTrck1 + 12.7 * i, 31.5)), module, TMixMaster::TRACK_VOL_INPUTS + i));			
			// Pan inputs
			addInput(inputWidgets[i + 16 * 3] = createInputCentered<MmPort>(mm2px(Vec(xTrck1 + 12.7 * i, 40.5)), module, TMixMaster::TRACK_PAN_INPUTS + i));			
			// Pan knobs
			MmSmallKnobGreyWithArc *panKnobTrack;
			addParam(panKnobTrack = createParamCentered<MmSmallKnobGreyWithArc>(mm2px(Vec(xTrck1 + 12.7 * i, 51.8)), module, TMixMaster::TRACK_PAN_PARAMS + i));
			if (module) {
				panKnobTrack->detailsShowSrc = &(module->gInfo.colorAndCloak.cc4[detailsShow]);
				panKnobTrack->cloakedModeSrc = &(module->gInfo.colorAndCloak.cc4[cloakedMode]);
				panKnobTrack->paramWithCV = &(module->tracks[i].pan);
				panKnobTrack->paramCvConnected = &(module->tracks[i].panCvConnected);
				panKnobTrack->dispColorGlobalSrc = &(module->gInfo.colorAndCloak.cc4[dispColorGlobal]);
				panKnobTrack->dispColorLocalSrc = &(module->tracks[i].dispColorLocal);
			}
			
			// Faders
			MmSmallFaderWithLink *newFader;
			addParam(newFader = createParamCentered<MmSmallFaderWithLink>(mm2px(Vec(xTrck1 + 3.67 + 12.7 * i, 81.2)), module, TMixMaster::TRACK_FADER_PARAMS + i));
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
			MmMuteFadeButtonWithClear* newMuteFade;
			addParam(newMuteFade = createParamCentered<MmMuteFadeButtonWithClear>(mm2px(Vec(xTrck1 + 12.7 * i, 109.8)), module, TMixMaster::TRACK_MUTE_PARAMS + i));
			if (module) {
				newMuteFade->type = module->tracks[i].fadeRate;
				newMuteFade->muteParams = &module->params[TMixMaster::TRACK_MUTE_PARAMS];
				newMuteFade->baseMuteParamId = TMixMaster::TRACK_MUTE_PARAMS;
				newMuteFade->numTracksAndGroups = N_TRK + N_GRP;
			}
			// Solos
			MmSoloButtonMutex *newSoloButton;
			addParam(newSoloButton = createParamCentered<MmSoloButtonMutex>(mm2px(Vec(xTrck1 + 12.7 * i, 116.1)), module, TMixMaster::TRACK_SOLO_PARAMS + i));
			newSoloButton->soloParams =  module ? &module->params[TMixMaster::TRACK_SOLO_PARAMS] : NULL;
			newSoloButton->baseSoloParamId = TMixMaster::TRACK_SOLO_PARAMS;
			newSoloButton->numTracks = N_TRK;
			newSoloButton->numGroups = N_GRP;
			// Group dec
			MmGroupMinusButtonNotify *newGrpMinusButton;
			addChild(newGrpMinusButton = createWidgetCentered<MmGroupMinusButtonNotify>(mm2px(Vec(xTrck1 - 3.73 + 12.7 * i - 0.75, 123.1))));
			if (module) {
				newGrpMinusButton->sourceParam = &(module->params[TMixMaster::GROUP_SELECT_PARAMS + i]);
				newGrpMinusButton->numGroups = (float)N_GRP;
			}
			// Group inc
			MmGroupPlusButtonNotify *newGrpPlusButton;
			addChild(newGrpPlusButton = createWidgetCentered<MmGroupPlusButtonNotify>(mm2px(Vec(xTrck1 + 3.77 + 12.7 * i + 0.75, 123.1))));
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
				addOutput(createOutputCentered<MmPortGold>(mm2px(Vec(xGrp1 + 12.7 * (i), 11.5)), module, TMixMaster::DIRECT_OUTPUTS + i - 1));
			}
			else {
				addOutput(createOutputCentered<MmPortGold>(mm2px(Vec(xGrp1 + 12.7 * (0), 11.5)), module, TMixMaster::FADE_CV_OUTPUT));				
			}
			// Labels
			addChild(groupDisplays[i] = createWidgetCentered<GroupDisplay<TMixMaster::MixerGroup>>(mm2px(Vec(xGrp1 + 12.7 * i + 2, 23.5))));
			if (module) {
				// groupDisplays[i]->tabNextFocus = // done after the for loop
				groupDisplays[i]->colorAndCloak = &(module->gInfo.colorAndCloak);
				groupDisplays[i]->dispColorLocal = &(module->groups[i].dispColorLocal);
				groupDisplays[i]->srcGroup = &(module->groups[i]);
				groupDisplays[i]->updateTrackLabelRequestPtr = &(module->updateTrackLabelRequest);
				groupDisplays[i]->trackOrGroupResetInAuxPtr = &(module->trackOrGroupResetInAux);
				groupDisplays[i]->auxExpanderPresentPtr = &(module->auxExpanderPresent);
				groupDisplays[i]->numTracks = N_TRK;
				groupDisplays[i]->hpfParamQuantity = module->paramQuantities[TMixMaster::GROUP_HPCUT_PARAMS + i];
				groupDisplays[i]->lpfParamQuantity = module->paramQuantities[TMixMaster::GROUP_LPCUT_PARAMS + i];
			}
			// HPF lights
			addChild(createLightCentered<TinyLight<GreenLight>>(mm2px(Vec(xGrp1 - 4.17 + 12.7 * i, 27.0)), module, TMixMaster::GROUP_HPF_LIGHTS + i));	
			addParam(createParamCentered<FilterCutWidget>(mm2px(Vec(xGrp1 - 4.17 + 12.7 * i, 27.0)), module, TMixMaster::GROUP_HPCUT_PARAMS + i));				
			// LPF lights
			addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(xGrp1 + 4.17 + 12.7 * i, 27.0)), module, TMixMaster::GROUP_LPF_LIGHTS + i));	
			addParam(createParamCentered<FilterCutWidget>(mm2px(Vec(xGrp1 + 4.17 + 12.7 * i, 27.0)), module, TMixMaster::GROUP_LPCUT_PARAMS + i));				
			// Volume inputs
			addInput(createInputCentered<MmPort>(mm2px(Vec(xGrp1 + 12.7 * i, 31.5)), module, TMixMaster::GROUP_VOL_INPUTS + i));			
			// Pan inputs
			addInput(createInputCentered<MmPort>(mm2px(Vec(xGrp1 + 12.7 * i, 40.5)), module, TMixMaster::GROUP_PAN_INPUTS + i));			
			// Pan knobs
			MmSmallKnobGreyWithArc *panKnobGroup;
			addParam(panKnobGroup = createParamCentered<MmSmallKnobGreyWithArc>(mm2px(Vec(xGrp1 + 12.7 * i, 51.8)), module, TMixMaster::GROUP_PAN_PARAMS + i));
			if (module) {
				panKnobGroup->detailsShowSrc = &(module->gInfo.colorAndCloak.cc4[detailsShow]);
				panKnobGroup->cloakedModeSrc = &(module->gInfo.colorAndCloak.cc4[cloakedMode]);
				panKnobGroup->paramWithCV = &(module->groups[i].pan);
				panKnobGroup->paramCvConnected = &(module->groups[i].panCvConnected);
				panKnobGroup->dispColorGlobalSrc = &(module->gInfo.colorAndCloak.cc4[dispColorGlobal]);
				panKnobGroup->dispColorLocalSrc = &(module->groups[i].dispColorLocal);
			}
			
			// Faders
			MmSmallFaderWithLink *newFader;
			addParam(newFader = createParamCentered<MmSmallFaderWithLink>(mm2px(Vec(xGrp1 + 3.67 + 12.7 * i, 81.2)), module, TMixMaster::GROUP_FADER_PARAMS + i));		
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
			MmMuteFadeButtonWithClear* newMuteFade;
			addParam(newMuteFade = createParamCentered<MmMuteFadeButtonWithClear>(mm2px(Vec(xGrp1 + 12.7 * i, 109.8)), module, TMixMaster::GROUP_MUTE_PARAMS + i));
			if (module) {
				newMuteFade->type = module->groups[i].fadeRate;
				newMuteFade->muteParams = &module->params[TMixMaster::TRACK_MUTE_PARAMS];
				newMuteFade->baseMuteParamId = TMixMaster::TRACK_MUTE_PARAMS;
				newMuteFade->numTracksAndGroups = N_TRK + N_GRP;
			}
			// Solos
			MmSoloButtonMutex* newSoloButton;
			addParam(newSoloButton = createParamCentered<MmSoloButtonMutex>(mm2px(Vec(xGrp1 + 12.7 * i, 116.1)), module, TMixMaster::GROUP_SOLO_PARAMS + i));
			newSoloButton->soloParams =  module ? &module->params[TMixMaster::TRACK_SOLO_PARAMS] : NULL;
			newSoloButton->baseSoloParamId = TMixMaster::TRACK_SOLO_PARAMS;
			newSoloButton->numTracks = N_TRK;
			newSoloButton->numGroups = N_GRP;
		}
		for (int i = 0; i < N_GRP; i++) {
			groupDisplays[i]->tabNextFocus = groupDisplays[(i + 1) % N_GRP];
		}
	
		// Master inputs
		addInput(createInputCentered<MmPort>(mm2px(Vec(289.62, 12.8)), module, TMixMaster::CHAIN_INPUTS + 0));			
		addInput(createInputCentered<MmPort>(mm2px(Vec(289.62, 21.8)), module, TMixMaster::CHAIN_INPUTS + 1));			
		
		// Master outputs
		addOutput(createOutputCentered<MmPort>(mm2px(Vec(300.12, 12.8)), module, TMixMaster::MAIN_OUTPUTS + 0));			
		addOutput(createOutputCentered<MmPort>(mm2px(Vec(300.12, 21.8)), module, TMixMaster::MAIN_OUTPUTS + 1));			
		
		// Master label
		addChild(masterDisplay = createWidgetCentered<MasterDisplay>(mm2px(Vec(294.82 + 2, 128.5 - 97.15))));
		if (module) {
			masterDisplay->dcBlock = &(module->master.dcBlock);
			masterDisplay->clipping = &(module->master.clipping);
			masterDisplay->fadeRate = &(module->master.fadeRate);
			masterDisplay->fadeProfile = &(module->master.fadeProfile);
			masterDisplay->vuColorThemeLocal = &(module->master.vuColorThemeLocal);
			masterDisplay->dispColorLocal = &(module->master.dispColorLocal);
			masterDisplay->chainOnly = &(module->master.chainOnly);
			masterDisplay->dimGain = &(module->master.dimGain);
			masterDisplay->masterLabel = module->master.masterLabel;
			masterDisplay->dimGainIntegerDB = &(module->master.dimGainIntegerDB);
			masterDisplay->colorAndCloak = &(module->gInfo.colorAndCloak);
			masterDisplay->idSrc = &(module->id);
			masterDisplay->masterFaderScalesSendsSrc = &(module->gInfo.masterFaderScalesSends);
		}
		
		// Master fader
		addParam(createParamCentered<MmBigFader>(mm2px(Vec(300.17, 70.3)), module, TMixMaster::MAIN_FADER_PARAM));
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
		MmMuteFadeButton* newMuteFade;
		addParam(newMuteFade = createParamCentered<MmMuteFadeButton>(mm2px(Vec(294.82, 109.8)), module, TMixMaster::MAIN_MUTE_PARAM));
		if (module) {
			newMuteFade->type = &(module->master.fadeRate);
		}
		
		// Master dim
		addParam(createParamCentered<MmDimButton>(mm2px(Vec(289.42, 116.1)), module, TMixMaster::MAIN_DIM_PARAM));
		
		// Master mono
		addParam(createParamCentered<MmMonoButton>(mm2px(Vec(300.22, 116.1)), module, TMixMaster::MAIN_MONO_PARAM));
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
		SvgPanel* svgPanel = (SvgPanel*)getPanel();
		panelBorder = findBorder(svgPanel->fb);
		
		// Inserts and CVs
		static const float xIns = 13.8;
		// Fade CV output
		addOutput(createOutputCentered<MmPortGold>(mm2px(Vec(xIns, 12.8)), module, TMixMaster::FADE_CV_OUTPUT));
		// Insert outputs
		addOutput(createOutputCentered<MmPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 2)), module, TMixMaster::INSERT_TRACK_OUTPUTS + 0));				
		addOutput(createOutputCentered<MmPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 3)), module, TMixMaster::INSERT_GRP_AUX_OUTPUT));
		// Insert inputs
		addInput(createInputCentered<MmPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 4)), module, TMixMaster::INSERT_TRACK_INPUTS + 0));
		addInput(createInputCentered<MmPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 5)), module, TMixMaster::INSERT_GRP_AUX_INPUT));
		// Mute, solo and other CVs
		addInput(createInputCentered<MmPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 7)), module, TMixMaster::TRACK_MUTESOLO_INPUTS + 0));
		addInput(createInputCentered<MmPortGold>(mm2px(Vec(xIns, 12.8 + 10.85 * 8)), module, TMixMaster::GRPM_MUTESOLO_INPUT));
		
	
		// Tracks
		static const float xTrck1 = 11.43 + 20.32;
		for (int i = 0; i < N_TRK; i++) {
			// Labels
			addChild(trackDisplays[i] = createWidgetCentered<TrackDisplay<TMixMaster::MixerTrack>>(mm2px(Vec(xTrck1 + 12.7 * i + 2, 4.7))));
			if (module) {
				// trackDisplays[i]->tabNextFocus = // done after the for loop
				trackDisplays[i]->colorAndCloak = &(module->gInfo.colorAndCloak);
				trackDisplays[i]->dispColorLocal = &(module->tracks[i].dispColorLocal);				
				trackDisplays[i]->tracks = module->tracks;
				trackDisplays[i]->trackNumSrc = i;
				trackDisplays[i]->auxExpanderPresentPtr = &(module->auxExpanderPresent);
				trackDisplays[i]->numTracks = N_TRK;
				trackDisplays[i]->updateTrackLabelRequestPtr = &(module->updateTrackLabelRequest);
				trackDisplays[i]->trackOrGroupResetInAuxPtr = &(module->trackOrGroupResetInAux);
				trackDisplays[i]->trackMoveInAuxRequestPtr = &(module->trackMoveInAuxRequest);
				trackDisplays[i]->inputWidgets = inputWidgets;
				trackDisplays[i]->hpfParamQuantity = module->paramQuantities[TMixMaster::TRACK_HPCUT_PARAMS + i];
				trackDisplays[i]->lpfParamQuantity = module->paramQuantities[TMixMaster::TRACK_LPCUT_PARAMS + i];
			}
			// HPF lights
			addChild(createLightCentered<TinyLight<GreenLight>>(mm2px(Vec(xTrck1 - 4.17 + 12.7 * i, 8.3)), module, TMixMaster::TRACK_HPF_LIGHTS + i));
			addParam(createParamCentered<FilterCutWidget>(mm2px(Vec(xTrck1 - 4.17 + 12.7 * i, 8.3)), module, TMixMaster::TRACK_HPCUT_PARAMS + i));	
			// LPF lights
			addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(xTrck1 + 4.17 + 12.7 * i, 8.3)), module, TMixMaster::TRACK_LPF_LIGHTS + i));	
			addParam(createParamCentered<FilterCutWidget>(mm2px(Vec(xTrck1 + 4.17 + 12.7 * i, 8.3)), module, TMixMaster::TRACK_LPCUT_PARAMS + i));	
			// Left inputs
			addInput(inputWidgets[i + 0] = createInputCentered<MmPort>(mm2px(Vec(xTrck1 + 12.7 * i, 12.8)), module, TMixMaster::TRACK_SIGNAL_INPUTS + 2 * i + 0));			
			// Right inputs
			addInput(inputWidgets[i + N_TRK] = createInputCentered<MmPort>(mm2px(Vec(xTrck1 + 12.7 * i, 21.8)), module, TMixMaster::TRACK_SIGNAL_INPUTS + 2 * i + 1));	
			// Volume inputs
			addInput(inputWidgets[i + N_TRK * 2] = createInputCentered<MmPort>(mm2px(Vec(xTrck1 + 12.7 * i, 31.5)), module, TMixMaster::TRACK_VOL_INPUTS + i));			
			// Pan inputs
			addInput(inputWidgets[i + N_TRK * 3] = createInputCentered<MmPort>(mm2px(Vec(xTrck1 + 12.7 * i, 40.5)), module, TMixMaster::TRACK_PAN_INPUTS + i));			
			// Pan knobs
			MmSmallKnobGreyWithArc *panKnobTrack;
			addParam(panKnobTrack = createParamCentered<MmSmallKnobGreyWithArc>(mm2px(Vec(xTrck1 + 12.7 * i, 51.8)), module, TMixMaster::TRACK_PAN_PARAMS + i));
			if (module) {
				panKnobTrack->detailsShowSrc = &(module->gInfo.colorAndCloak.cc4[detailsShow]);
				panKnobTrack->cloakedModeSrc = &(module->gInfo.colorAndCloak.cc4[cloakedMode]);
				panKnobTrack->paramWithCV = &(module->tracks[i].pan);
				panKnobTrack->paramCvConnected = &(module->tracks[i].panCvConnected);
				panKnobTrack->dispColorGlobalSrc = &(module->gInfo.colorAndCloak.cc4[dispColorGlobal]);
				panKnobTrack->dispColorLocalSrc = &(module->tracks[i].dispColorLocal);
			}
			
			// Faders
			MmSmallFaderWithLink *newFader;
			addParam(newFader = createParamCentered<MmSmallFaderWithLink>(mm2px(Vec(xTrck1 + 3.67 + 12.7 * i, 81.2)), module, TMixMaster::TRACK_FADER_PARAMS + i));
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
			MmMuteFadeButtonWithClear* newMuteFade;
			addParam(newMuteFade = createParamCentered<MmMuteFadeButtonWithClear>(mm2px(Vec(xTrck1 + 12.7 * i, 109.8)), module, TMixMaster::TRACK_MUTE_PARAMS + i));
			if (module) {
				newMuteFade->type = module->tracks[i].fadeRate;
				newMuteFade->muteParams = &module->params[TMixMaster::TRACK_MUTE_PARAMS];
				newMuteFade->baseMuteParamId = TMixMaster::TRACK_MUTE_PARAMS;
				newMuteFade->numTracksAndGroups = N_TRK + N_GRP;
			}
			// Solos
			MmSoloButtonMutex *newSoloButton;
			addParam(newSoloButton = createParamCentered<MmSoloButtonMutex>(mm2px(Vec(xTrck1 + 12.7 * i, 116.1)), module, TMixMaster::TRACK_SOLO_PARAMS + i));
			newSoloButton->soloParams =  module ? &module->params[TMixMaster::TRACK_SOLO_PARAMS] : NULL;
			newSoloButton->baseSoloParamId = TMixMaster::TRACK_SOLO_PARAMS;
			newSoloButton->numTracks = N_TRK;
			newSoloButton->numGroups = N_GRP;
			// Group dec
			MmGroupMinusButtonNotify *newGrpMinusButton;
			addChild(newGrpMinusButton = createWidgetCentered<MmGroupMinusButtonNotify>(mm2px(Vec(xTrck1 - 3.73 + 12.7 * i - 0.75, 123.1))));
			if (module) {
				newGrpMinusButton->sourceParam = &(module->params[TMixMaster::GROUP_SELECT_PARAMS + i]);
				newGrpMinusButton->numGroups = (float)N_GRP;
			}
			// Group inc
			MmGroupPlusButtonNotify *newGrpPlusButton;
			addChild(newGrpPlusButton = createWidgetCentered<MmGroupPlusButtonNotify>(mm2px(Vec(xTrck1 + 3.77 + 12.7 * i + 0.75, 123.1))));
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
			addOutput(createOutputCentered<MmPortGold>(mm2px(Vec(xGrp1 + 12.7 * (i), 11.5)), module, TMixMaster::DIRECT_OUTPUTS + i));
			// Labels
			addChild(groupDisplays[i] = createWidgetCentered<GroupDisplay<TMixMaster::MixerGroup>>(mm2px(Vec(xGrp1 + 12.7 * i + 2, 23.5))));
			if (module) {
				// groupDisplays[i]->tabNextFocus = // done after the for loop
				groupDisplays[i]->colorAndCloak = &(module->gInfo.colorAndCloak);
				groupDisplays[i]->dispColorLocal = &(module->groups[i].dispColorLocal);
				groupDisplays[i]->srcGroup = &(module->groups[i]);
				groupDisplays[i]->updateTrackLabelRequestPtr = &(module->updateTrackLabelRequest);
				groupDisplays[i]->trackOrGroupResetInAuxPtr = &(module->trackOrGroupResetInAux);
				groupDisplays[i]->auxExpanderPresentPtr = &(module->auxExpanderPresent);
				groupDisplays[i]->numTracks = N_TRK;
				groupDisplays[i]->hpfParamQuantity = module->paramQuantities[TMixMaster::GROUP_HPCUT_PARAMS + i];
				groupDisplays[i]->lpfParamQuantity = module->paramQuantities[TMixMaster::GROUP_LPCUT_PARAMS + i];
			}
			// HPF lights
			addChild(createLightCentered<TinyLight<GreenLight>>(mm2px(Vec(xGrp1 - 4.17 + 12.7 * i, 27.0)), module, TMixMaster::GROUP_HPF_LIGHTS + i));	
			addParam(createParamCentered<FilterCutWidget>(mm2px(Vec(xGrp1 - 4.17 + 12.7 * i, 27.0)), module, TMixMaster::GROUP_HPCUT_PARAMS + i));				
			// LPF lights
			addChild(createLightCentered<TinyLight<BlueLight>>(mm2px(Vec(xGrp1 + 4.17 + 12.7 * i, 27.0)), module, TMixMaster::GROUP_LPF_LIGHTS + i));	
			addParam(createParamCentered<FilterCutWidget>(mm2px(Vec(xGrp1 + 4.17 + 12.7 * i, 27.0)), module, TMixMaster::GROUP_LPCUT_PARAMS + i));				
			// Volume inputs
			addInput(createInputCentered<MmPort>(mm2px(Vec(xGrp1 + 12.7 * i, 31.5)), module, TMixMaster::GROUP_VOL_INPUTS + i));			
			// Pan inputs
			addInput(createInputCentered<MmPort>(mm2px(Vec(xGrp1 + 12.7 * i, 40.5)), module, TMixMaster::GROUP_PAN_INPUTS + i));			
			// Pan knobs
			MmSmallKnobGreyWithArc *panKnobGroup;
			addParam(panKnobGroup = createParamCentered<MmSmallKnobGreyWithArc>(mm2px(Vec(xGrp1 + 12.7 * i, 51.8)), module, TMixMaster::GROUP_PAN_PARAMS + i));
			if (module) {
				panKnobGroup->detailsShowSrc = &(module->gInfo.colorAndCloak.cc4[detailsShow]);
				panKnobGroup->cloakedModeSrc = &(module->gInfo.colorAndCloak.cc4[cloakedMode]);
				panKnobGroup->paramWithCV = &(module->groups[i].pan);
				panKnobGroup->paramCvConnected = &(module->groups[i].panCvConnected);
				panKnobGroup->dispColorGlobalSrc = &(module->gInfo.colorAndCloak.cc4[dispColorGlobal]);
				panKnobGroup->dispColorLocalSrc = &(module->groups[i].dispColorLocal);
			}
			
			// Faders
			MmSmallFaderWithLink *newFader;
			addParam(newFader = createParamCentered<MmSmallFaderWithLink>(mm2px(Vec(xGrp1 + 3.67 + 12.7 * i, 81.2)), module, TMixMaster::GROUP_FADER_PARAMS + i));		
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
			MmMuteFadeButtonWithClear* newMuteFade;
			addParam(newMuteFade = createParamCentered<MmMuteFadeButtonWithClear>(mm2px(Vec(xGrp1 + 12.7 * i, 109.8)), module, TMixMaster::GROUP_MUTE_PARAMS + i));
			if (module) {
				newMuteFade->type = module->groups[i].fadeRate;
				newMuteFade->muteParams = &module->params[TMixMaster::TRACK_MUTE_PARAMS];
				newMuteFade->baseMuteParamId = TMixMaster::TRACK_MUTE_PARAMS;
				newMuteFade->numTracksAndGroups = N_TRK + N_GRP;
			}
			// Solos
			MmSoloButtonMutex* newSoloButton;
			addParam(newSoloButton = createParamCentered<MmSoloButtonMutex>(mm2px(Vec(xGrp1 + 12.7 * i, 116.1)), module, TMixMaster::GROUP_SOLO_PARAMS + i));
			newSoloButton->soloParams =  module ? &module->params[TMixMaster::TRACK_SOLO_PARAMS] : NULL;
			newSoloButton->baseSoloParamId = TMixMaster::TRACK_SOLO_PARAMS;
			newSoloButton->numTracks = N_TRK;
			newSoloButton->numGroups = N_GRP;
		}
		for (int i = 0; i < N_GRP; i++) {
			groupDisplays[i]->tabNextFocus = groupDisplays[(i + 1) % N_GRP];
		}
	
		// Master inputs
		addInput(createInputCentered<MmPort>(mm2px(Vec(289.62 - 12.7 * 10, 12.8)), module, TMixMaster::CHAIN_INPUTS + 0));			
		addInput(createInputCentered<MmPort>(mm2px(Vec(289.62 - 12.7 * 10, 21.8)), module, TMixMaster::CHAIN_INPUTS + 1));			
		
		// Master outputs
		addOutput(createOutputCentered<MmPort>(mm2px(Vec(300.12 - 12.7 * 10, 12.8)), module, TMixMaster::MAIN_OUTPUTS + 0));			
		addOutput(createOutputCentered<MmPort>(mm2px(Vec(300.12 - 12.7 * 10, 21.8)), module, TMixMaster::MAIN_OUTPUTS + 1));			
		
		// Master label
		addChild(masterDisplay = createWidgetCentered<MasterDisplay>(mm2px(Vec(294.81 + 2 - 12.7 * 10, 128.5 - 97.15))));
		if (module) {
			masterDisplay->dcBlock = &(module->master.dcBlock);
			masterDisplay->clipping = &(module->master.clipping);
			masterDisplay->fadeRate = &(module->master.fadeRate);
			masterDisplay->fadeProfile = &(module->master.fadeProfile);
			masterDisplay->vuColorThemeLocal = &(module->master.vuColorThemeLocal);
			masterDisplay->dispColorLocal = &(module->master.dispColorLocal);
			masterDisplay->chainOnly = &(module->master.chainOnly);
			masterDisplay->dimGain = &(module->master.dimGain);
			masterDisplay->masterLabel = module->master.masterLabel;
			masterDisplay->dimGainIntegerDB = &(module->master.dimGainIntegerDB);
			masterDisplay->colorAndCloak = &(module->gInfo.colorAndCloak);
			masterDisplay->idSrc = &(module->id);
			masterDisplay->masterFaderScalesSendsSrc = &(module->gInfo.masterFaderScalesSends);
		}
		
		// Master fader
		addParam(createParamCentered<MmBigFader>(mm2px(Vec(300.17 - 12.7 * 10, 70.3)), module, TMixMaster::MAIN_FADER_PARAM));
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
		MmMuteFadeButton* newMuteFade;
		addParam(newMuteFade = createParamCentered<MmMuteFadeButton>(mm2px(Vec(294.82 - 12.7 * 10, 109.8)), module, TMixMaster::MAIN_MUTE_PARAM));
		if (module) {
			newMuteFade->type = &(module->master.fadeRate);
		}
		
		// Master dim
		addParam(createParamCentered<MmDimButton>(mm2px(Vec(289.42 - 12.7 * 10, 116.1)), module, TMixMaster::MAIN_DIM_PARAM));
		
		// Master mono
		addParam(createParamCentered<MmMonoButton>(mm2px(Vec(300.22 - 12.7 * 10, 116.1)), module, TMixMaster::MAIN_MONO_PARAM));
	}
};



Model *modelMixMaster = createModel<MixMaster<16, 4>, MixMasterWidget>("MixMaster");
Model *modelMixMasterJr = createModel<MixMaster<8, 2>, MixMasterJrWidget>("MixMasterJr");
