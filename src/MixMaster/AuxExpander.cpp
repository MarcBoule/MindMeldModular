//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "MixerWidgets.hpp"


template<int N_TRK, int N_GRP>
struct AuxExpander : Module {
	
	enum ParamIds {
		ENUMS(TRACK_AUXSEND_PARAMS, N_TRK * 4), // trk 1 aux A, trk 1 aux B, ... 
		ENUMS(GROUP_AUXSEND_PARAMS, N_GRP * 4),// Mapping: 1A, 1B, 1C, 1D, 2A, etc; must be contiguous with TRACK_AUXSEND_PARAMS
		ENUMS(TRACK_AUXMUTE_PARAMS, N_TRK),
		ENUMS(GROUP_AUXMUTE_PARAMS, N_GRP),// must be contiguous with TRACK_AUXMUTE_PARAMS
		ENUMS(GLOBAL_AUXMUTE_PARAMS, 4),// must be contiguous with GROUP_AUXMUTE_PARAMS
		ENUMS(GLOBAL_AUXSOLO_PARAMS, 4),// must be contiguous with GLOBAL_AUXMUTE_PARAMS
		ENUMS(GLOBAL_AUXGROUP_PARAMS, 4),// must be contiguous with GLOBAL_AUXSOLO_PARAMS
		ENUMS(GLOBAL_AUXSEND_PARAMS, 4),// must be contiguous with GLOBAL_AUXGROUP_PARAMS
		ENUMS(GLOBAL_AUXPAN_PARAMS, 4),// must be contiguous with GLOBAL_AUXSEND_PARAMS
		ENUMS(GLOBAL_AUXRETURN_PARAMS, 4),// must be contiguous with GLOBAL_AUXPAN_PARAMS
		NUM_PARAMS
	};
	
	enum InputIds {
		ENUMS(RETURN_INPUTS, 2 * 4),// must be first element (see AuxspanderAux.construct()). Mapping: left A, right A, left B, right B, left C, right C, left D, right D
		ENUMS(POLY_AUX_AD_CV_INPUTS, N_GRP),// size happens to coincide with N_GRP
		POLY_AUX_M_CV_INPUT,
		POLY_GRPS_AD_CV_INPUT,// Mapping: 1A, 2A, 3A, 4A, 1B, etc
		POLY_GRPS_M_CV_INPUT,// not used in jr version, use POLY_AUX_M_CV_INPUT instead
		POLY_BUS_SND_PAN_RET_CV_INPUT,
		POLY_BUS_MUTE_SOLO_CV_INPUT,
		NUM_INPUTS
	};
	
	enum OutputIds {
		ENUMS(SEND_OUTPUTS, 2 * 4),// A left, B left, C left, D left, A right, B right, C right, D right
		NUM_OUTPUTS
	};
	
	enum LightIds {
		ENUMS(AUXSENDMUTE_GROUPED_RETURN_LIGHTS, N_GRP * 4),
		NUM_LIGHTS
	};
	
	typedef TAfmExpInterface<N_TRK, N_GRP> AfmExpInterface;
	
	
	#include "AuxExpander.hpp"
	

	// Expander
	AfmExpInterface leftMessages[2];// messages from mother (first index is page), see enum called AuxFromMotherIds in MixerCommon.hpp


	// Constants
	// none


	// Need to save, no reset
	// none
	
	// Need to save, with reset
	PackedBytes4 directOutsModeLocal;// must send back to main panel
	PackedBytes4 panLawStereoLocal;// must send back to main panel
	PackedBytes4 vuColorThemeLocal; // 0 to numthemes - 1; (when per-track choice)
	PackedBytes4 dispColorAuxLocal;
	float auxFadeRatesAndProfiles[8];// first 4 are fade rates, last 4 are fade profiles, all same standard as mixmaster
	alignas(4) char auxLabels[4 * 4 + 4];// 4 chars per label, 4 aux labels, null terminate the end the whole array only, pad with three extra chars for alignment
	AuxspanderAux aux[4];
	float panCvLevels[4];// 0 to 1.0f

	// No need to save, with reset
	int updateTrackLabelRequest;// 0 when nothing to do, 1 for read names in widget
	int updateAuxLabelRequest;// 0 when nothing to do, 1 for read names in widget
	int refreshCounter20;
	float srcLevelsVus[4][4];// first index is aux number, 2nd index is a vuValue (organized according to VuMeters::VuIds)
	float paramRetFaderWithCv[4];// for cv pointers in aux retrun faders 
	simd::float_4 globalSendsWithCV;
	bool globalSendsCvConnected;
	float indivTrackSendWithCv[N_TRK * 4];
	bool indivTrackSendCvConnected[4];// one for each aux
	float indivGroupSendWithCv[N_GRP * 4];
	bool indivGroupSendCvConnected;
	float globalRetPansWithCV[4];
	bool globalRetPansCvConnected;
	TSlewLimiterSingle<simd::float_4> sendMuteSlewers[N_TRK / 4 + 1];
	simd::float_4 trackSendVcaGains[N_TRK];
	simd::float_4 groupSendVcaGains[N_GRP];
	
	// No need to save, no reset
	RefreshCounter refresh;	
	bool motherPresent = false;// can't be local to process() since widget must know in order to properly draw border
	float maxAGIndivSendFader;
	float maxAGGlobSendFader;
	simd::float_4 globalSends;
	simd::float_4 muteSends[N_TRK / 4 + 1];
	TriggerRiseFall muteSoloCvTriggers[N_TRK + N_GRP + 4 + 4];
	// fast exp values
	//   srcLevelsVus is declared above since better to do a reset on it (to kill displays)
	//   auxSendsTrkGrp is declared locally, no need here
	// slow exp values that are saved and need an init in constructor since not guaranteed to be set in first pass of expander:
	PackedBytes4 colorAndCloak;
	PackedBytes4 directOutPanStereoMomentCvLinearVol;// cc1[0] is direct out mode, cc1[1] is stereo pan mode, [2] is momentaryCvButtons, [3] is linearVolCvInputs	
	uint32_t muteAuxSendWhenReturnGrouped;// { ... g2-B, g2-A, g1-D, g1-C, g1-B, g1-A}
	uint16_t ecoMode;
	alignas(4) char trackLabels[4 * (N_TRK + N_GRP) + 4];// 4 chars per label, 16 (8) tracks and 4 (2) groups means 20 (10) labels, null terminate the end the whole array only, pad with three extra chars for alignment
	PackedBytes4 trackDispColsLocal[N_TRK / 4 + 1];// 4 (2) elements for 16 (8) tracks, and 1 element for 4 (2) groups
	float auxRetFadeGains[4];// for return fades
	float srcMuteGhost[4];// index is aux number
	
	
	AuxExpander() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);		
		
		leftExpander.producerMessage = &leftMessages[0];
		leftExpander.consumerMessage = &leftMessages[1];
		
		char strBuf[32];
		maxAGIndivSendFader = std::pow(GlobalConst::individualAuxSendMaxLinearGain, 1.0f / GlobalConst::individualAuxSendScalingExponent);
		for (unsigned int i = 0; i < N_TRK; i++) {
			// Track send aux A
			snprintf(strBuf, 32, "-%02u-: send AUXA", i + 1);
			configParam(TRACK_AUXSEND_PARAMS + i * 4 + 0, 0.0f, maxAGIndivSendFader, 0.0f, strBuf, " dB", -10, 20.0f * GlobalConst::individualAuxSendScalingExponent);
			// Track send aux B
			snprintf(strBuf, 32, "-%02u-: send AUXB", i + 1);
			configParam(TRACK_AUXSEND_PARAMS + i * 4 + 1, 0.0f, maxAGIndivSendFader, 0.0f, strBuf, " dB", -10, 20.0f * GlobalConst::individualAuxSendScalingExponent);
			// Track send aux C
			snprintf(strBuf, 32, "-%02u-: send AUXC", i + 1);
			configParam(TRACK_AUXSEND_PARAMS + i * 4 + 2, 0.0f, maxAGIndivSendFader, 0.0f, strBuf, " dB", -10, 20.0f * GlobalConst::individualAuxSendScalingExponent);
			// Track send aux D
			snprintf(strBuf, 32, "-%02u-: send AUXD", i + 1);
			configParam(TRACK_AUXSEND_PARAMS + i * 4 + 3, 0.0f, maxAGIndivSendFader, 0.0f, strBuf, " dB", -10, 20.0f * GlobalConst::individualAuxSendScalingExponent);
			// Mute
			snprintf(strBuf, 32, "-%02u-: send mute", i + 1);
			configParam(TRACK_AUXMUTE_PARAMS + i, 0.0f, 1.0f, 0.0f, strBuf);
		}
		for (int grp = 0; grp < N_GRP; grp++) {
			// Group send aux A
			snprintf(strBuf, 32, "GRP%i: send AUXA", grp + 1);
			configParam(GROUP_AUXSEND_PARAMS + 0 + grp * 4, 0.0f, maxAGIndivSendFader, 0.0f, strBuf, " dB", -10, 20.0f * GlobalConst::individualAuxSendScalingExponent);
			// Group send aux B
			snprintf(strBuf, 32, "GRP%i: send AUXB", grp + 1);
			configParam(GROUP_AUXSEND_PARAMS + 1 + grp * 4, 0.0f, maxAGIndivSendFader, 0.0f, strBuf, " dB", -10, 20.0f * GlobalConst::individualAuxSendScalingExponent);
			// Group send aux C
			snprintf(strBuf, 32, "GRP%i: send AUXC", grp + 1);
			configParam(GROUP_AUXSEND_PARAMS + 2 + grp * 4, 0.0f, maxAGIndivSendFader, 0.0f, strBuf, " dB", -10, 20.0f * GlobalConst::individualAuxSendScalingExponent);
			// Group send aux D
			snprintf(strBuf, 32, "GRP%i: send AUXD", grp + 1);
			configParam(GROUP_AUXSEND_PARAMS + 3 + grp * 4, 0.0f, maxAGIndivSendFader, 0.0f, strBuf, " dB", -10, 20.0f * GlobalConst::individualAuxSendScalingExponent);
			// Mute
			snprintf(strBuf, 32, "GRP%i: send mute", grp + 1);
			configParam(GROUP_AUXMUTE_PARAMS + grp, 0.0f, 1.0f, 0.0f, strBuf);		
		}
		
		maxAGGlobSendFader = std::pow(GlobalConst::globalAuxSendMaxLinearGain, 1.0f / GlobalConst::globalAuxSendScalingExponent);
		float maxAGAuxRetFader = std::pow(GlobalConst::globalAuxReturnMaxLinearGain, 1.0f / GlobalConst::globalAuxReturnScalingExponent);
		for (int i = 0; i < 4; i++) {
			// Global send aux A-D
			snprintf(strBuf, 32, "AUX%c: global send", i + 0x41);
			configParam(GLOBAL_AUXSEND_PARAMS + i, 0.0f, maxAGGlobSendFader, 1.0f, strBuf, " dB", -10, 20.0f * GlobalConst::globalAuxSendScalingExponent);
			// Global pan return aux A-D
			snprintf(strBuf, 32, "AUX%c: return pan", i + 0x41);
			configParam(GLOBAL_AUXPAN_PARAMS + i, 0.0f, 1.0f, 0.5f, strBuf, "%", 0.0f, 200.0f, -100.0f);
			// Global return aux A-D
			snprintf(strBuf, 32, "AUX%c: return level", i + 0x41);
			configParam(GLOBAL_AUXRETURN_PARAMS + i, 0.0f, maxAGAuxRetFader, 1.0f, strBuf, " dB", -10, 20.0f * GlobalConst::globalAuxReturnScalingExponent);
			// Global mute
			snprintf(strBuf, 32, "AUX%c: return mute", i + 0x41);
			configParam(GLOBAL_AUXMUTE_PARAMS + i, 0.0f, 1.0f, 0.0f, strBuf);		
			// Global solo
			snprintf(strBuf, 32, "AUX%c: return solo", i + 0x41);
			configParam(GLOBAL_AUXSOLO_PARAMS + i, 0.0f, 1.0f, 0.0f, strBuf);		
			// Global group select
			snprintf(strBuf, 32, "AUX%c: return group", i + 0x41);
			configParam(GLOBAL_AUXGROUP_PARAMS + i, 0.0f, (float)N_GRP, 0.0f, strBuf);		
		}		
		
		// slow exp values that are saved and need an init in constructor since not guaranteed to be set in first pass of expander:
		colorAndCloak.cc1 = 0;
		directOutPanStereoMomentCvLinearVol.cc4[0] = 3; // directOutsMode: post-solo by default
		directOutPanStereoMomentCvLinearVol.cc4[1] = 1; // panLawStereo
		directOutPanStereoMomentCvLinearVol.cc4[2] = 1; // momentaryCvButtons: momentary by default
		directOutPanStereoMomentCvLinearVol.cc4[3] = 0; // linearVolCvInputs: 0 means powN, 1 means linear		
		muteAuxSendWhenReturnGrouped = 0;
		ecoMode = 0xFFFF;// all 1's means yes, 0 means no
		for (unsigned int trk = 0; trk < N_TRK; trk++) {
			snprintf(&trackLabels[trk << 2], 5, "-%02u-", trk + 1);
		}
		for (unsigned int grp = 0; grp < N_GRP; grp++) {
			snprintf(&trackLabels[(N_TRK + grp) << 2], 5, "GRP%1u", grp + 1);
		}
		for (int i = 0; i < (N_TRK / 4 + 1); i++) {
			trackDispColsLocal[i].cc1 = 0;
		}
		for (int i = 0; i < 4; i++) {
			auxRetFadeGains[i] = 1.0f;
			srcMuteGhost[i] = 0.0f;
		}
		
		
		for (int i = 0; i < (N_TRK / 4 + 1); i++) {
			sendMuteSlewers[i].setRiseFall(simd::float_4(GlobalConst::antipopSlewFast)); // slew rate is in input-units per second (ex: V/s)
		}
		for (int i = 0; i < N_TRK; i++) {
			trackSendVcaGains[i] = simd::float_4::zero();
		}
		for (int i = 0; i < N_GRP; i++) {
			groupSendVcaGains[i] = simd::float_4::zero();
		}
		auxLabels[4 * 4] = 0;
		for (int i = 0; i < 4; i++) {
			aux[i].construct(i, &inputs[0], &params[0], &(auxLabels[4 * i]), &vuColorThemeLocal.cc4[i], &directOutsModeLocal.cc4[i], &panLawStereoLocal.cc4[i], &dispColorAuxLocal.cc4[i], &panCvLevels[i], &auxFadeRatesAndProfiles[i]);
		}
		
		onReset();
	}
  
	void onReset() override {
		for (int i = 0; i < 4; i++) {
			aux[i].onReset();
		}
		resetNonJson(false);
	}
	void resetNonJson(bool recurseNonJson) {
		updateTrackLabelRequest = 1;
		updateAuxLabelRequest = 1;
		refreshCounter20 = 0;
		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				srcLevelsVus[i][j] = 0.0f;
			}
			paramRetFaderWithCv[i] = -100.0f;
			globalSendsWithCV[i] = 1.0f;
			globalRetPansWithCV[i] = 0.5f;
			aux[i].resetNonJson();
			indivTrackSendCvConnected[i] = false;
		}
		globalSendsCvConnected = false;
		indivGroupSendCvConnected = false;
		globalRetPansCvConnected = false;
		for (int i = 0; i < (N_TRK / 4 + 1); i++) {
			sendMuteSlewers[i].reset();
		}
		for (int i = 0; i < N_TRK * 4; i++) {
			indivTrackSendWithCv[i] = 0.0f;
		}
		for (int i = 0; i < N_GRP * 4; i++) {
			indivGroupSendWithCv[i] = 0.0f;
		}
	}


	void onRandomize() override {
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// directOutsModeLocal
		json_object_set_new(rootJ, "directOutsModeLocal", json_integer(directOutsModeLocal.cc1));

		// panLawStereoLocal
		json_object_set_new(rootJ, "panLawStereoLocal", json_integer(panLawStereoLocal.cc1));

		// vuColorThemeLocal
		json_object_set_new(rootJ, "vuColorThemeLocal", json_integer(vuColorThemeLocal.cc1));

		// dispColorAuxLocal
		json_t *dispColorAuxLocalJ = json_array();
		for (int c = 0; c < 4; c++)
			json_array_insert_new(dispColorAuxLocalJ, c, json_integer(dispColorAuxLocal.cc4[c]));// keep as array for legacy
		json_object_set_new(rootJ, "dispColorAuxLocal", dispColorAuxLocalJ);

		// auxFadeRatesAndProfiles
		json_t *auxFadeRatesAndProfilesJ = json_array();
		for (int c = 0; c < 8; c++)
			json_array_insert_new(auxFadeRatesAndProfilesJ, c, json_real(auxFadeRatesAndProfiles[c]));
		json_object_set_new(rootJ, "auxFadeRatesAndProfiles", auxFadeRatesAndProfilesJ);
		
		// auxLabels
		json_object_set_new(rootJ, "auxLabels", json_string(auxLabels));
		
		// aux
		for (int i = 0; i < 4; i++) {
			aux[i].dataToJson(rootJ);
		}

		// panCvLevels
		json_t *panCvLevelsJ = json_array();
		for (int c = 0; c < 4; c++)
			json_array_insert_new(panCvLevelsJ, c, json_real(panCvLevels[c]));
		json_object_set_new(rootJ, "panCvLevels", panCvLevelsJ);
		
		return rootJ;
	}


	void dataFromJson(json_t *rootJ) override {
		// directOutsModeLocal
		json_t *directOutsModeLocalJ = json_object_get(rootJ, "directOutsModeLocal");
		if (directOutsModeLocalJ)
			directOutsModeLocal.cc1 = json_integer_value(directOutsModeLocalJ);

		// panLawStereoLocal
		json_t *panLawStereoLocalJ = json_object_get(rootJ, "panLawStereoLocal");
		if (panLawStereoLocalJ)
			panLawStereoLocal.cc1 = json_integer_value(panLawStereoLocalJ);

		// vuColorThemeLocal
		json_t *vuColorThemeLocalJ = json_object_get(rootJ, "vuColorThemeLocal");
		if (vuColorThemeLocalJ)
			vuColorThemeLocal.cc1 = json_integer_value(vuColorThemeLocalJ);

		// dispColorAuxLocal
		json_t *dispColorAuxLocalJ = json_object_get(rootJ, "dispColorAuxLocal");
		if (dispColorAuxLocalJ) {
			for (int c = 0; c < 4; c++)
			{
				json_t *dispColorAuxLocalArrayJ = json_array_get(dispColorAuxLocalJ, c);
				if (dispColorAuxLocalArrayJ)
					dispColorAuxLocal.cc4[c] = json_integer_value(dispColorAuxLocalArrayJ);// kept as array for legacy
			}
		}
		
		// auxFadeRatesAndProfiles
		json_t *auxFadeRatesAndProfilesJ = json_object_get(rootJ, "auxFadeRatesAndProfiles");
		if (auxFadeRatesAndProfilesJ) {
			for (int c = 0; c < 8; c++)
			{
				json_t *auxFadeRatesAndProfilesArrayJ = json_array_get(auxFadeRatesAndProfilesJ, c);
				if (auxFadeRatesAndProfilesArrayJ)
					auxFadeRatesAndProfiles[c] = json_real_value(auxFadeRatesAndProfilesArrayJ);
			}
		}

		// auxLabels
		json_t *textJ = json_object_get(rootJ, "auxLabels");
		if (textJ) {
			snprintf(auxLabels, 4 * 4 + 1, "%s", json_string_value(textJ));
		}

		// aux
		for (int i = 0; i < 4; i++) {
			aux[i].dataFromJson(rootJ);
		}

		// panCvLevels
		json_t *panCvLevelsJ = json_object_get(rootJ, "panCvLevels");
		if (panCvLevelsJ) {
			for (int c = 0; c < 4; c++)
			{
				json_t *panCvLevelsArrayJ = json_array_get(panCvLevelsJ, c);
				if (panCvLevelsArrayJ)
					panCvLevels[c] = json_real_value(panCvLevelsArrayJ);
			}
		}

		resetNonJson(true);
	}


	void swapCopyToClipboard() {
		// auxspander
		json_t* auxspanderJ = json_object();
		
		// params
		//   first batch done this way:
		json_object_set_new(auxspanderJ, "TRACK_AUXSEND_PARAMS", paramArrayToJsonArray(TRACK_AUXSEND_PARAMS, N_TRK * 4));
		json_object_set_new(auxspanderJ, "GROUP_AUXSEND_PARAMS", paramArrayToJsonArray(GROUP_AUXSEND_PARAMS, N_GRP * 4));
		json_object_set_new(auxspanderJ, "TRACK_AUXMUTE_PARAMS", paramArrayToJsonArray(TRACK_AUXMUTE_PARAMS, N_TRK));
		json_object_set_new(auxspanderJ, "GROUP_AUXMUTE_PARAMS", paramArrayToJsonArray(GROUP_AUXMUTE_PARAMS, N_GRP));
		//   second batch done this other way:
		json_t* paramsJ = json_array();
		for (size_t i = GLOBAL_AUXMUTE_PARAMS; i < NUM_PARAMS; i++) {
			json_array_append_new(paramsJ, json_real(params[i].getValue()));
		}
		json_object_set_new(auxspanderJ, "params", paramsJ);
		
		// dataToJson data
		json_object_set_new(auxspanderJ, "dataToJson-data", dataToJson());
		
		// clipboard
		json_t* clipboardJ = json_object();		
		json_object_set_new(clipboardJ, "auxspander-swap", auxspanderJ);
		
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
			WARN("AuxSpander swap: error getting clipboard string");
			return;
		}

		json_error_t error;
		json_t* clipboardJ = json_loads(inerchangeClip, 0, &error);
		if (!clipboardJ) {
			WARN("AuxSpander swap: error json parsing clipboard");
			return;
		}
		DEFER({json_decref(clipboardJ);});

		// auxspander
		json_t* auxspanderJ = json_object_get(clipboardJ, "auxspander-swap");
		if (!auxspanderJ) {
			WARN("AuxSpander swap: error no auxspander-swap present in clipboard");
			return;
		}
		
		// params
		//   first batch done this way:
		jsonArrayToParamDirect(json_object_get(auxspanderJ, "TRACK_AUXSEND_PARAMS"), TRACK_AUXSEND_PARAMS, N_TRK * 4);		
		jsonArrayToParamDirect(json_object_get(auxspanderJ, "GROUP_AUXSEND_PARAMS"), GROUP_AUXSEND_PARAMS, N_GRP * 4);		
		jsonArrayToParamDirect(json_object_get(auxspanderJ, "TRACK_AUXMUTE_PARAMS"), TRACK_AUXMUTE_PARAMS, N_TRK);		
		jsonArrayToParamDirect(json_object_get(auxspanderJ, "GROUP_AUXMUTE_PARAMS"), GROUP_AUXMUTE_PARAMS, N_GRP);		
		//   second batch done this other way:
		json_t* paramsJ = json_object_get(auxspanderJ, "params");
		if ( !paramsJ || !json_is_array(paramsJ) ) {
			WARN("AuxSpander swap: error params array malformed or missing");
			return;
		}
		for (size_t i = 0; i < json_array_size(paramsJ); i++) {
			json_t* paramJ = json_array_get(paramsJ, i);
			if (!paramJ) {
				WARN("AuxSpander swap: error missing param in params array");
				return;		
			}
			params[GLOBAL_AUXMUTE_PARAMS + i].setValue(json_number_value(paramJ));
		}	
		
		// dataToJson data
		json_t* dataToJsonJ = json_object_get(auxspanderJ, "dataToJson-data");
		if (!dataToJsonJ) {
			WARN("AuxSpander swap: error dataToJson-data missing");
			return;
		}
		dataFromJson(dataToJsonJ);
	}
	void jsonArrayToParamDirect(json_t* paramJ, int baseParam, int numParam) {// numParam is of .this
		if ( !paramJ || !json_is_array(paramJ) ) {
			WARN("AuxSpander swap: error param array malformed or missing");
			return;
		}
		for (int i = 0; i < std::min((int)json_array_size(paramJ), numParam) ; i++) {
			json_t* paramItemJ = json_array_get(paramJ, i);
			if (!paramItemJ) {
				WARN("AuxSpander swap: error missing param value in param array");
				return;		
			}
			params[baseParam + i].setValue(json_number_value(paramItemJ));
		}	
	}
	

	void onSampleRateChange() override {
		for (int i = 0; i < 4; i++) {
			aux[i].onSampleRateChange();
		}
	}
	

	void process(const ProcessArgs &args) override {
		
		motherPresent = (leftExpander.module && leftExpander.module->model == (N_TRK == 16 ? modelMixMaster : modelMixMasterJr));
		
		AfmExpInterface *messagesFromMother = (AfmExpInterface*)leftExpander.consumerMessage;
		
		if (refresh.processInputs()) {
			processMuteSoloCvTriggers();
		}// userInputs refresh
		
		if (motherPresent) {
			// From Mother
			// ***********
			
			// Slow values from mother
			if (messagesFromMother->updateSlow) {
				colorAndCloak.cc1 = messagesFromMother->colorAndCloak.cc1;
				directOutPanStereoMomentCvLinearVol.cc1 = messagesFromMother->directOutPanStereoMomentCvLinearVol.cc1;
				muteAuxSendWhenReturnGrouped = messagesFromMother->muteAuxSendWhenReturnGrouped;
				for (int i = 0; i < N_TRK; i++) {
					lights[AUXSENDMUTE_GROUPED_RETURN_LIGHTS + i].setBrightness((muteAuxSendWhenReturnGrouped & (1 << i)) != 0 ? 1.0f : 0.0f);
				}
				ecoMode = messagesFromMother->ecoMode;
				if (messagesFromMother->trackMoveInAuxRequest != 0) {
					moveTrack(messagesFromMother->trackMoveInAuxRequest);
					messagesFromMother->trackMoveInAuxRequest = 0;
				}
				if (messagesFromMother->trackOrGroupResetInAux != -1) {
					resetTrackOrGroup(messagesFromMother->trackOrGroupResetInAux);
					messagesFromMother->trackOrGroupResetInAux = -1;
				}
				memcpy(trackLabels, messagesFromMother->trackLabels, 4 * (N_TRK + N_GRP));
				updateTrackLabelRequest = 1;
				memcpy(trackDispColsLocal, messagesFromMother->trackDispColsLocal, (N_TRK / 4 + 1) * 4);
				memcpy(auxRetFadeGains, messagesFromMother->auxRetFadeGains, 4 * 4);
				memcpy(srcMuteGhost, messagesFromMother->srcMuteGhost, 4 * 4);
			}
			
			// Fast values from mother
			// Vus 
			int value4i = clamp(messagesFromMother->vuIndex, 0, 4);
			memcpy(&srcLevelsVus[value4i][0], messagesFromMother->vuValues, 4 * 4);

						
			// Aux sends

			// Prepare values used to compute aux sends
			//   Global aux send knobs (4 instances)
			if (ecoMode == 0 || (refreshCounter20 & 0x3) == 0) {// stagger 0
				for (int gi = 0; gi < 4; gi++) {
					globalSends[gi] = params[GLOBAL_AUXSEND_PARAMS + gi].getValue();
				}
				globalSendsCvConnected = inputs[POLY_BUS_SND_PAN_RET_CV_INPUT].isConnected();
				if (globalSendsCvConnected) {
					// Knob CV (adding, pre-scaling)
					simd::float_4 cvVoltages(inputs[POLY_BUS_SND_PAN_RET_CV_INPUT].getVoltage(0), inputs[POLY_BUS_SND_PAN_RET_CV_INPUT].getVoltage(1),
					inputs[POLY_BUS_SND_PAN_RET_CV_INPUT].getVoltage(2), inputs[POLY_BUS_SND_PAN_RET_CV_INPUT].getVoltage(3));
					globalSends += cvVoltages * 0.1f * maxAGGlobSendFader;
					// lines above replace commented line below since templating AuxExpander broke it for some strange reason
					// globalSends += (inputs[POLY_BUS_SND_PAN_RET_CV_INPUT].getVoltageSimd<simd::float_4>(0)) * 0.1f * maxAGGlobSendFader;
					globalSends = clamp(globalSends, 0.0f, maxAGGlobSendFader);
					globalSendsWithCV = globalSends;// can put here since unused when cv disconnected
				}
				globalSends = simd::pow<simd::float_4>(globalSends, GlobalConst::globalAuxSendScalingExponent);
			
				//   Indiv mute sends (20 or 10 instances)				
				for (int gi = 0; gi < (N_TRK + N_GRP); gi++) {
					muteSends[gi >> 2][gi & 0x3] = params[TRACK_AUXMUTE_PARAMS + gi].getValue();
				}
				for (int gi = 0; gi < (N_TRK / 4 + 1); gi++) {
					muteSends[gi] = simd::ifelse(muteSends[gi] >= 0.5f, 0.0f, 1.0f);
					if (movemask(muteSends[gi] == sendMuteSlewers[gi].out) != 0xF) {// movemask returns 0xF when 4 floats are equal
						sendMuteSlewers[gi].process(args.sampleTime * (1 + (ecoMode & 0x3)), muteSends[gi]);
					}
				}
			}
	
			// Aux send VCAs
			float* auxSendsTrkGrp = messagesFromMother->auxSends;// 40 values of the sends (Trk1L, Trk1R, Trk2L, Trk2R ... Trk16L, Trk16R, Grp1L, Grp1R ... Grp4L, Grp4R))
			simd::float_4 auxSends[2] = {simd::float_4::zero(), simd::float_4::zero()};// [0] = ABCD left, [1] = ABCD right
			// accumulate tracks
			for (int trk = 0; trk < N_TRK; trk++) {
				// prepare trackSendVcaGains when needed
				if (ecoMode == 0 || (refreshCounter20 & 0x3) == 1) {// stagger 1			
					for (int auxi = 0; auxi < 4; auxi++) {
					// 64 (32) individual track aux send knobs
						float val = params[TRACK_AUXSEND_PARAMS + (trk << 2) + auxi].getValue();
						int inputNum = POLY_AUX_AD_CV_INPUTS + (auxi >> (N_GRP == 4 ? 0 : 1));
						indivTrackSendCvConnected[auxi] = inputs[inputNum].isConnected();
						if (inputs[inputNum].isConnected()) {
							// Knob CV (adding, pre-scaling)
							if (N_GRP == 4) {
								val += inputs[inputNum].getVoltage(trk) * 0.1f * maxAGIndivSendFader;
							}
							else {
								val += inputs[inputNum].getVoltage(trk + (((auxi & 0x1) != 0) ? 8 : 0)) * 0.1f * maxAGIndivSendFader;
							}
							val = clamp(val, 0.0f, maxAGIndivSendFader);
							indivTrackSendWithCv[(trk << 2) + auxi] = val;// can put here since unused when cv disconnected
						}
						trackSendVcaGains[trk][auxi] = val;
					}
					trackSendVcaGains[trk] = simd::pow<simd::float_4>(trackSendVcaGains[trk], GlobalConst::individualAuxSendScalingExponent);
					trackSendVcaGains[trk] *= globalSends * simd::float_4(sendMuteSlewers[trk >> 2].out[trk & 0x3]);
				}
				// vca the aux send knobs with the track's sound
				auxSends[0] += trackSendVcaGains[trk] * simd::float_4(auxSendsTrkGrp[(trk << 1) + 0]);// L
				auxSends[1] += trackSendVcaGains[trk] * simd::float_4(auxSendsTrkGrp[(trk << 1) + 1]);// R				
			}
			// accumulate groups
			for (int grp = 0; grp < N_GRP; grp++) {
				// prepare groupSendVcaGains when needed
				if (ecoMode == 0 || (refreshCounter20 & 0x3) == 2) {// stagger 2
					indivGroupSendCvConnected = inputs[POLY_GRPS_AD_CV_INPUT].isConnected();
					for (int auxi = 0; auxi < 4; auxi++) {
					// 16 (8) individual group aux send knobs
						float val = params[GROUP_AUXSEND_PARAMS + (grp << 2) + auxi].getValue();
						if (indivGroupSendCvConnected) {
							// Knob CV (adding, pre-scaling)
							int cvIndex = ((auxi << (N_GRP / 2)) + grp);// not the same order for the CVs
							val += inputs[POLY_GRPS_AD_CV_INPUT].getVoltage(cvIndex) * 0.1f * maxAGIndivSendFader;
							val = clamp(val, 0.0f, maxAGIndivSendFader);
							indivGroupSendWithCv[(grp << 2) + auxi] = val;// can put here since unused when cv disconnected
						}
						if ((muteAuxSendWhenReturnGrouped & (1 << ((grp << 2) + auxi))) == 0) {
							groupSendVcaGains[grp][auxi] = val;
						}
						else {
							groupSendVcaGains[grp][auxi] = 0.0f;
						}
					}
					groupSendVcaGains[grp] = simd::pow<simd::float_4>(groupSendVcaGains[grp], GlobalConst::individualAuxSendScalingExponent);
					groupSendVcaGains[grp] *= globalSends * simd::float_4(sendMuteSlewers[N_TRK >> 2].out[grp]);
				}
				// vca the aux send knobs with the group's sound
				auxSends[0] += groupSendVcaGains[grp] * simd::float_4(auxSendsTrkGrp[(grp << 1) + N_TRK * 2 + 0]);// L
				auxSends[1] += groupSendVcaGains[grp] * simd::float_4(auxSendsTrkGrp[(grp << 1) + N_TRK * 2 + 1]);// R				
			}			
			// Aux send outputs
			for (int i = 0; i < 4; i++) {
				if (outputs[SEND_OUTPUTS + i + 4].isConnected()) {
					// stereo send
					outputs[SEND_OUTPUTS + i + 0].setVoltage(auxSends[0][i]);// L ABCD
					outputs[SEND_OUTPUTS + i + 4].setVoltage(auxSends[1][i]);// R ABCD
				}
				else {
					// mono send (send (L+R)/2 into L send
					float mix = (auxSends[0][i] + auxSends[1][i]) * 0.5f;
					outputs[SEND_OUTPUTS + i + 0].setVoltage(mix);// L+R ABCD
					outputs[SEND_OUTPUTS + i + 4].setVoltage(0.0f);
				}
			}			
						
			
			
			// To Mother
			// ***********
			
			MfaExpInterface *messagesToMother = (MfaExpInterface*)leftExpander.module->rightExpander.producerMessage;
			
			messagesToMother->updateSlow = refresh.refreshCounter == 0;
			if (messagesToMother->updateSlow) {
				messagesToMother->directOutsModeLocalAux.cc1 = directOutsModeLocal.cc1;
				messagesToMother->stereoPanModeLocalAux.cc1 = panLawStereoLocal.cc1;				
				messagesToMother->auxVuColors.cc1 = vuColorThemeLocal.cc1;
				messagesToMother->auxDispColors.cc1 = dispColorAuxLocal.cc1;
				for (int i = 0; i < 12; i++) {// Aux mute, solo, group
					messagesToMother->values20[i] = params[GLOBAL_AUXMUTE_PARAMS + i].getValue();
				}
				memcpy(&messagesToMother->values20[12], auxFadeRatesAndProfiles, 4 * 8);
				memcpy(messagesToMother->auxLabels, &auxLabels, 4 * 4);
			}
			
			// Aux returns
			// left A, right A, left B, right B, left C, right C, left D, right D
			for (int i = 0; i < 4; i++) {
				aux[i].process(&messagesToMother->auxReturns[i << 1]);
			}
						
			// aux return pan
			globalRetPansCvConnected = inputs[POLY_BUS_SND_PAN_RET_CV_INPUT].isConnected();
			for (int i = 0; i < 4; i++) {
				float val = params[GLOBAL_AUXPAN_PARAMS + i].getValue();
				// cv for pan
				if (globalRetPansCvConnected) {
					val += inputs[POLY_BUS_SND_PAN_RET_CV_INPUT].getVoltage(4 + i) * 0.1f * panCvLevels[i];// Pan CV is a -5V to +5V input
					val = clamp(val, 0.0f, 1.0f);
					globalRetPansWithCV[i] = val;// can put here since unused when cv disconnected
				}
				messagesToMother->auxRetFaderPanFadercv[4 + i] = val;
			}
			
			// aux return fader
			for (int i = 0; i < 4; i++) {
				float fader = params[GLOBAL_AUXPAN_PARAMS + 4 + i].getValue();
				// cv for return fader
				bool isConnected = inputs[POLY_BUS_SND_PAN_RET_CV_INPUT].isConnected() && 
						(inputs[POLY_BUS_SND_PAN_RET_CV_INPUT].getChannels() >= (8 + i + 1));
				float volCv;
				if (isConnected) {
					volCv = clamp(inputs[POLY_BUS_SND_PAN_RET_CV_INPUT].getVoltage(8 + i) * 0.1f, 0.f, 1.0f);
					paramRetFaderWithCv[i] = fader * volCv;
					if (directOutPanStereoMomentCvLinearVol.cc4[2] == 0) {
						fader = paramRetFaderWithCv[i];
					}
				}
				else {
					volCv = 1.0f;
					paramRetFaderWithCv[i] = -100.0f;// do not show cv pointer
				}

				fader = std::pow(fader, GlobalConst::globalAuxReturnScalingExponent);// scaling
				messagesToMother->auxRetFaderPanFadercv[i] = fader;
				messagesToMother->auxRetFaderPanFadercv[8 + i] = volCv;// send back to mother in case linearVolCvInputs!=0
			}
				
			refreshCounter20++;
			if (refreshCounter20 >= 20) {
				refreshCounter20 = 0;
			}
			
			leftExpander.module->rightExpander.messageFlipRequested = true;
		}	
		else {// if (motherPresent)
			for (int i = 0; i < N_TRK; i++) {
				lights[AUXSENDMUTE_GROUPED_RETURN_LIGHTS + i].setBrightness(0.0f);
			}
			
		}

		// VUs
		if (!motherPresent || colorAndCloak.cc4[cloakedMode] != 0) {
			for (int i = 0; i < 4; i++) {
				for (int j = 0; j < 4; j++) {
					srcLevelsVus[i][j] = 0.0f;
				}
				srcMuteGhost[i] = 0.0f;
			}
		}
		
		refresh.processLights(); // none, but this advances the refresh counter
	}// process()


	void writeTrackParams(int trk, float* bufDest) {
		for (int aux = 0; aux < 4; aux++) {
			bufDest[aux] = params[TRACK_AUXSEND_PARAMS + (trk << 2) + aux].getValue();
		}
		bufDest[4] = params[TRACK_AUXMUTE_PARAMS + trk].getValue();
	}
	
	void readTrackParams(int trk, float* bufSrc) {
		for (int aux = 0; aux < 4; aux++) {
			params[TRACK_AUXSEND_PARAMS + (trk << 2) + aux].setValue(bufSrc[aux]);
		}
		params[TRACK_AUXMUTE_PARAMS + trk].setValue(bufSrc[4]);
	}
	
	void moveTrack(int destSrc) {
		const int trackNumDest = (destSrc >> 8);
		const int trackNumSrc = (destSrc & 0xFF);
		
		float buffer1[5];// bit0 = auxA, bit1 = auxB, bit2 = auxC, bit3 = auxD, bit4 = mute param
		float buffer2[5];
		
		writeTrackParams(trackNumSrc, buffer2);
		if (trackNumDest < trackNumSrc) {
			for (int trk = trackNumSrc - 1; trk >= trackNumDest; trk--) {
				writeTrackParams(trk, buffer1);
				readTrackParams(trk + 1, buffer1);
			}
		}
		else {// must automatically be bigger (equal is impossible)
			for (int trk = trackNumSrc; trk < trackNumDest; trk++) {
				writeTrackParams(trk + 1, buffer1);
				readTrackParams(trk, buffer1);
			}
		}
		readTrackParams(trackNumDest, buffer2);
	}
	
	void resetTrackOrGroup(int tg) {
		// tg: 0 to N_TRK-1 for track reset, N_TRK to N_TRK+N_GRP-1 for group reset 
		// since params are contiguous, no need to distinguish between track or group
		for (int auxi = 0; auxi < 4; auxi++) {
			params[TRACK_AUXSEND_PARAMS + (tg << 2) + auxi].setValue(0.0f);
		}
		params[TRACK_AUXMUTE_PARAMS + tg].setValue(0.0f);
	}
	
	void processMuteSoloCvTriggers() {
		int state;

		// track send mutes
		if (inputs[POLY_AUX_M_CV_INPUT].isConnected()) {
			for (int trk = 0; trk < N_TRK; trk++) {
				state = muteSoloCvTriggers[trk].process(inputs[POLY_AUX_M_CV_INPUT].getVoltage(trk));
				if (state != 0) {
					if (directOutPanStereoMomentCvLinearVol.cc4[2] != 0) {
						if (state == 1) {
							float newParam = 1.0f - params[TRACK_AUXMUTE_PARAMS + trk].getValue();// toggle
							params[TRACK_AUXMUTE_PARAMS + trk].setValue(newParam);
						};
					}
					else {
						params[TRACK_AUXMUTE_PARAMS + trk].setValue(state == 1 ? 1.0f : 0.0f);// gate level
					}
				}
			}
		}
		// group send mutes
		int inputNum = (N_TRK == 16 ? POLY_GRPS_M_CV_INPUT : POLY_AUX_M_CV_INPUT);		
		if (inputs[inputNum].isConnected()) {
			for (int grp = 0; grp < N_GRP; grp++) {
				state = muteSoloCvTriggers[grp + N_TRK].process(inputs[inputNum].getVoltage(grp + (N_TRK & 0xF)));
				if (state != 0) {
					if (directOutPanStereoMomentCvLinearVol.cc4[2] != 0) {
						if (state == 1) {
							float newParam = 1.0f - params[GROUP_AUXMUTE_PARAMS + grp].getValue();// toggle
							params[GROUP_AUXMUTE_PARAMS + grp].setValue(newParam);
						};
					}
					else {
						params[GROUP_AUXMUTE_PARAMS + grp].setValue(state == 1 ? 1.0f : 0.0f);// gate level
					}
				}
			}
		}
		// return mutes and solos
		if (inputs[POLY_BUS_MUTE_SOLO_CV_INPUT].isConnected()) {
			for (int aux = 0; aux < 4; aux++) {
				// mutes
				state = muteSoloCvTriggers[aux + (N_TRK + N_GRP)].process(inputs[POLY_BUS_MUTE_SOLO_CV_INPUT].getVoltage(aux));
				if (state != 0) {
					if (directOutPanStereoMomentCvLinearVol.cc4[2] != 0) {
						if (state == 1) {
							float newParam = 1.0f - params[GLOBAL_AUXMUTE_PARAMS + aux].getValue();// toggle
							params[GLOBAL_AUXMUTE_PARAMS + aux].setValue(newParam);
						};
					}
					else {
						params[GLOBAL_AUXMUTE_PARAMS + aux].setValue(state == 1 ? 1.0f : 0.0f);// gate level
					}
				}
				
				// solos
				state = muteSoloCvTriggers[aux + (N_TRK + N_GRP) + 4].process(inputs[POLY_BUS_MUTE_SOLO_CV_INPUT].getVoltage(aux + 4));
				if (state != 0) {
					if (directOutPanStereoMomentCvLinearVol.cc4[2] != 0) {
						if (state == 1) {
							float newParam = 1.0f - params[GLOBAL_AUXSOLO_PARAMS + aux].getValue();// toggle
							params[GLOBAL_AUXSOLO_PARAMS + aux].setValue(newParam);
						};
					}
					else {
						params[GLOBAL_AUXSOLO_PARAMS + aux].setValue(state == 1 ? 1.0f : 0.0f);// gate level
					}
				}
				
			}
		}
	}
};


//-----------------------------------------------------------------------------


struct AuxExpanderWidget : ModuleWidget {
	static const int N_TRK = 16;
	static const int N_GRP = 4;
	
	
	#include "AuxExpanderWidget.hpp"
	
	
	AuxExpanderWidget(TAuxExpander *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/auxspander.svg")));
		SvgPanel* svgPanel = (SvgPanel*)getPanel();
		panelBorder = findBorder(svgPanel->fb);


		// Left side (globals)
		for (int i = 0; i < 4; i++) {
			// Labels
			addChild(auxDisplays[i] = createWidgetCentered<AuxDisplay<TAuxExpander::AuxspanderAux>>(mm2px(Vec(6.35 + 12.7 * i + 2, 4.7))));
			if (module) {
				// auxDisplays[i]->tabNextFocus = // done after the for loop
				auxDisplays[i]->colorAndCloak = &(module->colorAndCloak);
				auxDisplays[i]->dispColorLocal = &(module->dispColorAuxLocal.cc4[i]);
				auxDisplays[i]->srcAux = &(module->aux[i]);
				auxDisplays[i]->srcVuColor = &(module->vuColorThemeLocal.cc4[i]);
				auxDisplays[i]->srcDirectOutsModeLocal = &(module->directOutsModeLocal.cc4[i]);
				auxDisplays[i]->srcPanLawStereoLocal = &(module->panLawStereoLocal.cc4[i]);
				auxDisplays[i]->srcDirectOutsModeGlobal = &(module->directOutPanStereoMomentCvLinearVol.cc4[0]);
				auxDisplays[i]->srcPanLawStereoGlobal = &(module->directOutPanStereoMomentCvLinearVol.cc4[1]);
				auxDisplays[i]->srcPanCvLevel = &(module->panCvLevels[i]);
				auxDisplays[i]->srcFadeRatesAndProfiles = &(module->auxFadeRatesAndProfiles[i]);
				auxDisplays[i]->auxName = &(module->auxLabels[i * 4]);
				auxDisplays[i]->auxNumber = i;
				auxDisplays[i]->numTracks = N_TRK;
				auxDisplays[i]->numGroups = N_GRP;
				auxDisplays[i]->updateAuxLabelRequestPtr = &(module->updateAuxLabelRequest);
			}
			// Y is 4.7, same X as below
			
			// Left sends
			addOutput(createOutputCentered<MmPort>(mm2px(Vec(6.35 + 12.7 * i, 12.8)), module, TAuxExpander::SEND_OUTPUTS + i + 0));			
			// Right sends
			addOutput(createOutputCentered<MmPort>(mm2px(Vec(6.35 + 12.7 * i, 21.8)), module, TAuxExpander::SEND_OUTPUTS + i + 4));

			// Left returns
			addInput(createInputCentered<MmPort>(mm2px(Vec(6.35 + 12.7 * i, 31.5)), module, TAuxExpander::RETURN_INPUTS + i * 2 + 0));			
			// Right returns
			addInput(createInputCentered<MmPort>(mm2px(Vec(6.35 + 12.7 * i, 40.5)), module, TAuxExpander::RETURN_INPUTS + i * 2 + 1));			
			
			// Pan knobs
			MmSmallKnobGreyWithArc *panKnobAux;
			addParam(panKnobAux = createParamCentered<MmSmallKnobGreyWithArc>(mm2px(Vec(6.35 + 12.7 * i, 62.83)), module, TAuxExpander::GLOBAL_AUXPAN_PARAMS + i));
			if (module) {
				panKnobAux->detailsShowSrc = &(module->colorAndCloak.cc4[detailsShow]);
				panKnobAux->cloakedModeSrc = &(module->colorAndCloak.cc4[cloakedMode]);
				panKnobAux->paramWithCV = &(module->globalRetPansWithCV[i]);
				panKnobAux->paramCvConnected = &(module->globalRetPansCvConnected);
				panKnobAux->dispColorGlobalSrc = &(module->colorAndCloak.cc4[dispColorGlobal]);
				panKnobAux->dispColorLocalSrc = &(module->dispColorAuxLocal.cc4[i]);
			}
			
			// Return faders
			addParam(createParamCentered<MmSmallerFader>(mm2px(Vec(6.35 + 3.67 + 12.7 * i, 87.2)), module, TAuxExpander::GLOBAL_AUXRETURN_PARAMS + i));
			if (module) {
				// VU meters
				VuMeterAux *newVU = createWidgetCentered<VuMeterAux>(mm2px(Vec(6.35 + 12.7 * i, 87.2)));
				newVU->srcLevels = &(module->srcLevelsVus[i][0]);
				newVU->srcMuteGhost = &(module->srcMuteGhost[i]);
				newVU->colorThemeGlobal = &(module->colorAndCloak.cc4[vuColorGlobal]);
				newVU->colorThemeLocal = &(module->vuColorThemeLocal.cc4[i]);
				addChild(newVU);
				// Fade pointers
				CvAndFadePointerAuxRet *newFP = createWidgetCentered<CvAndFadePointerAuxRet>(mm2px(Vec(6.35 - 2.95 + 12.7 * i, 87.2)));
				newFP->srcParam = &(module->params[TAuxExpander::GLOBAL_AUXRETURN_PARAMS + i]);
				newFP->srcParamWithCV = &(module->paramRetFaderWithCv[i]);
				newFP->colorAndCloak = &(module->colorAndCloak);
				newFP->srcFadeGain = &(module->auxRetFadeGains[i]);
				newFP->srcFadeRate = &(module->auxFadeRatesAndProfiles[i]);
				newFP->dispColorLocalPtr = &(module->dispColorAuxLocal.cc4[i]);
				addChild(newFP);				
			}				
			
			// Global mute buttons
			MmMuteFadeButton* newMuteFade;
			addParam(newMuteFade = createParamCentered<MmMuteFadeButton>(mm2px(Vec(6.35  + 12.7 * i, 109.8)), module, TAuxExpander::GLOBAL_AUXMUTE_PARAMS + i));
			if (module) {
				newMuteFade->type = &(module->auxFadeRatesAndProfiles[i]);
			}
			
			// Global solo buttons
			addParam(createParamCentered<MmSoloButton>(mm2px(Vec(6.35  + 12.7 * i, 116.1)), module, TAuxExpander::GLOBAL_AUXSOLO_PARAMS + i));		

			// Group dec
			MmGroupMinusButtonNotify *newGrpMinusButton;
			addChild(newGrpMinusButton = createWidgetCentered<MmGroupMinusButtonNotify>(mm2px(Vec(6.35 - 3.73 + 12.7 * i - 0.75, 123.1))));
			if (module) {
				newGrpMinusButton->sourceParam = &(module->params[TAuxExpander::GLOBAL_AUXGROUP_PARAMS + i]);
				newGrpMinusButton->numGroups = (float)N_GRP;
			}
			// Group inc
			MmGroupPlusButtonNotify *newGrpPlusButton;
			addChild(newGrpPlusButton = createWidgetCentered<MmGroupPlusButtonNotify>(mm2px(Vec(6.35 + 3.77 + 12.7 * i + 0.75, 123.1))));
			if (module) {
				newGrpPlusButton->sourceParam = &(module->params[TAuxExpander::GLOBAL_AUXGROUP_PARAMS + i]);
				newGrpPlusButton->numGroups = (float)N_GRP;
			}
			// Group select displays
			GroupSelectDisplay* groupSelectDisplay;
			addParam(groupSelectDisplay = createParamCentered<GroupSelectDisplay>(mm2px(Vec(6.35 + 12.7 * i, 123.1)), module, TAuxExpander::GLOBAL_AUXGROUP_PARAMS + i));
			if (module) {
				groupSelectDisplay->srcColor = &(module->colorAndCloak);
				groupSelectDisplay->srcColorLocal = &(module->dispColorAuxLocal.cc4[i]);
				groupSelectDisplay->numGroups = N_GRP;
			}
		}
		for (int i = 0; i < 4; i++) {
			auxDisplays[i]->tabNextFocus = auxDisplays[(i + 1) % 4];
		}

		// Global send knobs
		MmKnobWithArc* sendKnobs[4];
		addParam(sendKnobs[0] = createParamCentered<MmSmallKnobRedWithArcTopCentered>(mm2px(Vec(6.35 + 12.7 * 0, 51.8)), module, TAuxExpander::GLOBAL_AUXSEND_PARAMS + 0));
		addParam(sendKnobs[1] = createParamCentered<MmSmallKnobOrangeWithArcTopCentered>(mm2px(Vec(6.35 + 12.7 * 1, 51.8)), module, TAuxExpander::GLOBAL_AUXSEND_PARAMS + 1));
		addParam(sendKnobs[2] = createParamCentered<MmSmallKnobBlueWithArcTopCentered>(mm2px(Vec(6.35 + 12.7 * 2, 51.8)), module, TAuxExpander::GLOBAL_AUXSEND_PARAMS + 2));
		addParam(sendKnobs[3] = createParamCentered<MmSmallKnobPurpleWithArcTopCentered>(mm2px(Vec(6.35 + 12.7 * 3, 51.8)), module, TAuxExpander::GLOBAL_AUXSEND_PARAMS + 3));
		if (module) {
			for (int k = 0; k < 4; k++) {
				sendKnobs[k]->paramWithCV = &module->globalSendsWithCV[k];
				sendKnobs[k]->paramCvConnected = &module->globalSendsCvConnected;
				sendKnobs[k]->detailsShowSrc = &(module->colorAndCloak.cc4[detailsShow]);
				sendKnobs[k]->cloakedModeSrc = &(module->colorAndCloak.cc4[cloakedMode]);
			}
		}


		// Right side (individual tracks)
		MmKnobWithArc* newArcKnob;
		for (int i = 0; i < 8; i++) {
			// Labels for tracks 1 to 8
			addChild(trackAndGroupLabels[i] = createWidgetCentered<TrackAndGroupLabel>(mm2px(Vec(67.31 + 2 + 12.7 * i, 4.7))));
			if (module) {
				trackAndGroupLabels[i]->dispColorPtr = &(module->colorAndCloak.cc4[dispColorGlobal]);
				trackAndGroupLabels[i]->dispColorLocalPtr = &(module->trackDispColsLocal[i >> 2].cc4[i & 0x3]);
			}
			// aux A send for tracks 1 to 8
			addParam(newArcKnob = createParamCentered<MmSmallKnobRedWithArc>(mm2px(Vec(67.31 + 12.7 * i, 14)), module, TAuxExpander::TRACK_AUXSEND_PARAMS + i * 4 + 0));
			if (module) {
				newArcKnob->paramWithCV = &module->indivTrackSendWithCv[i * 4 + 0];
				newArcKnob->paramCvConnected = &module->indivTrackSendCvConnected[0];
				newArcKnob->detailsShowSrc = &(module->colorAndCloak.cc4[detailsShow]);
				newArcKnob->cloakedModeSrc = &(module->colorAndCloak.cc4[cloakedMode]);
			}				
			// aux B send for tracks 1 to 8
			addParam(newArcKnob = createParamCentered<MmSmallKnobOrangeWithArc>(mm2px(Vec(67.31 + 12.7 * i, 24.85)), module, TAuxExpander::TRACK_AUXSEND_PARAMS + i * 4 + 1));
			if (module) {
				newArcKnob->paramWithCV = &module->indivTrackSendWithCv[i * 4 + 1];
				newArcKnob->paramCvConnected = &module->indivTrackSendCvConnected[1];
				newArcKnob->detailsShowSrc = &(module->colorAndCloak.cc4[detailsShow]);
				newArcKnob->cloakedModeSrc = &(module->colorAndCloak.cc4[cloakedMode]);
			}				
			// aux C send for tracks 1 to 8
			addParam(newArcKnob = createParamCentered<MmSmallKnobBlueWithArc>(mm2px(Vec(67.31 + 12.7 * i, 35.7)), module, TAuxExpander::TRACK_AUXSEND_PARAMS + i * 4 + 2));
			if (module) {
				newArcKnob->paramWithCV = &module->indivTrackSendWithCv[i * 4 + 2];
				newArcKnob->paramCvConnected = &module->indivTrackSendCvConnected[2];
				newArcKnob->detailsShowSrc = &(module->colorAndCloak.cc4[detailsShow]);
				newArcKnob->cloakedModeSrc = &(module->colorAndCloak.cc4[cloakedMode]);
			}				
			// aux D send for tracks 1 to 8
			addParam(newArcKnob = createParamCentered<MmSmallKnobPurpleWithArc>(mm2px(Vec(67.31 + 12.7 * i, 46.55)), module, TAuxExpander::TRACK_AUXSEND_PARAMS + i * 4 + 3));
			if (module) {
				newArcKnob->paramWithCV = &module->indivTrackSendWithCv[i * 4 + 3];
				newArcKnob->paramCvConnected = &module->indivTrackSendCvConnected[3];
				newArcKnob->detailsShowSrc = &(module->colorAndCloak.cc4[detailsShow]);
				newArcKnob->cloakedModeSrc = &(module->colorAndCloak.cc4[cloakedMode]);
			}				
			// mute for tracks 1 to 8
			addParam(createParamCentered<MmMuteButton>(mm2px(Vec(67.31  + 12.7 * i, 55.7)), module, TAuxExpander::TRACK_AUXMUTE_PARAMS + i));
			
			
			// Labels for tracks 9 to 16
			addChild(trackAndGroupLabels[i + 8] = createWidgetCentered<TrackAndGroupLabel>(mm2px(Vec(67.31 + 2 + 12.7 * i, 65.08))));
			if (module) {
				trackAndGroupLabels[i + 8]->dispColorPtr = &(module->colorAndCloak.cc4[dispColorGlobal]);
				trackAndGroupLabels[i + 8]->dispColorLocalPtr = &(module->trackDispColsLocal[(i + 8) >> 2].cc4[i & 0x3]);
			}

			// aux A send for tracks 9 to 16
			addParam(newArcKnob = createParamCentered<MmSmallKnobRedWithArc>(mm2px(Vec(67.31 + 12.7 * i, 74.5)), module, TAuxExpander::TRACK_AUXSEND_PARAMS + (i + 8) * 4 + 0));			
			if (module) {
				newArcKnob->paramWithCV = &module->indivTrackSendWithCv[(i + 8) * 4 + 0];
				newArcKnob->paramCvConnected = &module->indivTrackSendCvConnected[0];
				newArcKnob->detailsShowSrc = &(module->colorAndCloak.cc4[detailsShow]);
				newArcKnob->cloakedModeSrc = &(module->colorAndCloak.cc4[cloakedMode]);
			}				
			// aux B send for tracks 9 to 16
			addParam(newArcKnob = createParamCentered<MmSmallKnobOrangeWithArc>(mm2px(Vec(67.31 + 12.7 * i, 85.35)), module, TAuxExpander::TRACK_AUXSEND_PARAMS + (i + 8) * 4 + 1));
			if (module) {
				newArcKnob->paramWithCV = &module->indivTrackSendWithCv[(i + 8) * 4 + 1];
				newArcKnob->paramCvConnected = &module->indivTrackSendCvConnected[1];
				newArcKnob->detailsShowSrc = &(module->colorAndCloak.cc4[detailsShow]);
				newArcKnob->cloakedModeSrc = &(module->colorAndCloak.cc4[cloakedMode]);
			}				
			// aux C send for tracks 9 to 16
			addParam(newArcKnob = createParamCentered<MmSmallKnobBlueWithArc>(mm2px(Vec(67.31 + 12.7 * i, 96.2)), module, TAuxExpander::TRACK_AUXSEND_PARAMS + (i + 8) * 4 + 2));
			if (module) {
				newArcKnob->paramWithCV = &module->indivTrackSendWithCv[(i + 8) * 4 + 2];
				newArcKnob->paramCvConnected = &module->indivTrackSendCvConnected[2];
				newArcKnob->detailsShowSrc = &(module->colorAndCloak.cc4[detailsShow]);
				newArcKnob->cloakedModeSrc = &(module->colorAndCloak.cc4[cloakedMode]);
			}				
			// aux D send for tracks 9 to 16
			addParam(newArcKnob = createParamCentered<MmSmallKnobPurpleWithArc>(mm2px(Vec(67.31 + 12.7 * i, 107.05)), module, TAuxExpander::TRACK_AUXSEND_PARAMS + (i + 8) * 4 + 3));
			if (module) {
				newArcKnob->paramWithCV = &module->indivTrackSendWithCv[(i + 8) * 4 + 3];
				newArcKnob->paramCvConnected = &module->indivTrackSendCvConnected[3];
				newArcKnob->detailsShowSrc = &(module->colorAndCloak.cc4[detailsShow]);
				newArcKnob->cloakedModeSrc = &(module->colorAndCloak.cc4[cloakedMode]);
			}				
			// mute for tracks 9 to 16
			addParam(createParamCentered<MmMuteButton>(mm2px(Vec(67.31  + 12.7 * i, 116.1)), module, TAuxExpander::TRACK_AUXMUTE_PARAMS + i + 8));
		}

		// Right side (individual groups)
		static constexpr float redO = 3.85f;
		static constexpr float redOx = 0.58f;
		for (int i = 0; i < 2; i++) {
			// Labels for groups 1 to 2
			addChild(trackAndGroupLabels[i + 16] = createWidgetCentered<TrackAndGroupLabel>(mm2px(Vec(171.45 + 2 + 12.7 * i, 4.7))));
			if (module) {
				trackAndGroupLabels[i + 16]->dispColorPtr = &(module->colorAndCloak.cc4[dispColorGlobal]);
				trackAndGroupLabels[i + 16]->dispColorLocalPtr = &(module->trackDispColsLocal[(i + 16) >> 2].cc4[i & 0x3]);			
			}

			// aux A send for groups 1 to 2
			addParam(newArcKnob = createParamCentered<MmSmallKnobRedWithArc>(mm2px(Vec(171.45 + 12.7 * i, 14)), module, TAuxExpander::GROUP_AUXSEND_PARAMS + i * 4 + 0));
			addChild(createLightCentered<TinyLight<RedLight>>(mm2px(Vec(171.45 + 12.7 * i - redO - redOx, 14 + redO)), module, TAuxExpander::AUXSENDMUTE_GROUPED_RETURN_LIGHTS + i * 4 + 0));	
			if (module) {
				newArcKnob->paramWithCV = &module->indivGroupSendWithCv[i * 4 + 0];
				newArcKnob->paramCvConnected = &module->indivGroupSendCvConnected;
				newArcKnob->detailsShowSrc = &(module->colorAndCloak.cc4[detailsShow]);
				newArcKnob->cloakedModeSrc = &(module->colorAndCloak.cc4[cloakedMode]);
			}				
			// aux B send for groups 1 to 2
			addParam(newArcKnob = createParamCentered<MmSmallKnobOrangeWithArc>(mm2px(Vec(171.45 + 12.7 * i, 24.85)), module, TAuxExpander::GROUP_AUXSEND_PARAMS + i * 4 + 1));
			addChild(createLightCentered<TinyLight<RedLight>>(mm2px(Vec(171.45 + 12.7 * i - redO - redOx, 24.85 + redO)), module, TAuxExpander::AUXSENDMUTE_GROUPED_RETURN_LIGHTS + i * 4 + 1));	
			if (module) {
				newArcKnob->paramWithCV = &module->indivGroupSendWithCv[i * 4 + 1];
				newArcKnob->paramCvConnected = &module->indivGroupSendCvConnected;
				newArcKnob->detailsShowSrc = &(module->colorAndCloak.cc4[detailsShow]);
				newArcKnob->cloakedModeSrc = &(module->colorAndCloak.cc4[cloakedMode]);
			}				
			// aux C send for groups 1 to 2
			addParam(newArcKnob = createParamCentered<MmSmallKnobBlueWithArc>(mm2px(Vec(171.45 + 12.7 * i, 35.7)), module, TAuxExpander::GROUP_AUXSEND_PARAMS + i * 4 + 2));
			addChild(createLightCentered<TinyLight<RedLight>>(mm2px(Vec(171.45 + 12.7 * i - redO - redOx, 35.7 + redO)), module, TAuxExpander::AUXSENDMUTE_GROUPED_RETURN_LIGHTS + i * 4 + 2));	
			if (module) {
				newArcKnob->paramWithCV = &module->indivGroupSendWithCv[i * 4 + 2];
				newArcKnob->paramCvConnected = &module->indivGroupSendCvConnected;
				newArcKnob->detailsShowSrc = &(module->colorAndCloak.cc4[detailsShow]);
				newArcKnob->cloakedModeSrc = &(module->colorAndCloak.cc4[cloakedMode]);
			}				
			// aux D send for groups 1 to 2
			addParam(newArcKnob = createParamCentered<MmSmallKnobPurpleWithArc>(mm2px(Vec(171.45 + 12.7 * i, 46.55)), module, TAuxExpander::GROUP_AUXSEND_PARAMS + i * 4 + 3));
			addChild(createLightCentered<TinyLight<RedLight>>(mm2px(Vec(171.45 + 12.7 * i - redO - redOx, 46.55 + redO)), module, TAuxExpander::AUXSENDMUTE_GROUPED_RETURN_LIGHTS + i * 4 + 3));	
			if (module) {
				newArcKnob->paramWithCV = &module->indivGroupSendWithCv[i * 4 + 3];
				newArcKnob->paramCvConnected = &module->indivGroupSendCvConnected;
				newArcKnob->detailsShowSrc = &(module->colorAndCloak.cc4[detailsShow]);
				newArcKnob->cloakedModeSrc = &(module->colorAndCloak.cc4[cloakedMode]);
			}				
			// mute for groups 1 to 2
			addParam(createParamCentered<MmMuteButton>(mm2px(Vec(171.45  + 12.7 * i, 55.7)), module, TAuxExpander::GROUP_AUXMUTE_PARAMS + i));
			
			
			// Labels for groups 3 to 4
			addChild(trackAndGroupLabels[i + 18] = createWidgetCentered<TrackAndGroupLabel>(mm2px(Vec(171.45 + 2 + 12.7 * i, 65.08))));
			if (module) {
				trackAndGroupLabels[i + 18]->dispColorPtr = &(module->colorAndCloak.cc4[dispColorGlobal]);
				trackAndGroupLabels[i + 18]->dispColorLocalPtr = &(module->trackDispColsLocal[(i + 18) >> 2].cc4[(i + 18) & 0x3]);
			}

			// aux A send for groups 3 to 4
			addParam(newArcKnob = createParamCentered<MmSmallKnobRedWithArc>(mm2px(Vec(171.45 + 12.7 * i, 74.5)), module, TAuxExpander::GROUP_AUXSEND_PARAMS + (i + 2) * 4 + 0));			
			addChild(createLightCentered<TinyLight<RedLight>>(mm2px(Vec(171.45 + 12.7 * i - redO - redOx, 74.5 + redO)), module, TAuxExpander::AUXSENDMUTE_GROUPED_RETURN_LIGHTS + (i + 2) * 4 + 0));	
			if (module) {
				newArcKnob->paramWithCV = &module->indivGroupSendWithCv[(i + 2) * 4 + 0];
				newArcKnob->paramCvConnected = &module->indivGroupSendCvConnected;
				newArcKnob->detailsShowSrc = &(module->colorAndCloak.cc4[detailsShow]);
				newArcKnob->cloakedModeSrc = &(module->colorAndCloak.cc4[cloakedMode]);
			}				
			// aux B send for groups 3 to 4
			addParam(newArcKnob = createParamCentered<MmSmallKnobOrangeWithArc>(mm2px(Vec(171.45 + 12.7 * i, 85.35)), module, TAuxExpander::GROUP_AUXSEND_PARAMS + (i + 2) * 4 + 1));
			addChild(createLightCentered<TinyLight<RedLight>>(mm2px(Vec(171.45 + 12.7 * i - redO - redOx, 85.35 + redO)), module, TAuxExpander::AUXSENDMUTE_GROUPED_RETURN_LIGHTS + (i + 2) * 4 + 1));	
			if (module) {
				newArcKnob->paramWithCV = &module->indivGroupSendWithCv[(i + 2) * 4 + 1];
				newArcKnob->paramCvConnected = &module->indivGroupSendCvConnected;
				newArcKnob->detailsShowSrc = &(module->colorAndCloak.cc4[detailsShow]);
				newArcKnob->cloakedModeSrc = &(module->colorAndCloak.cc4[cloakedMode]);
			}				
			// aux C send for groups 3 to 4
			addParam(newArcKnob = createParamCentered<MmSmallKnobBlueWithArc>(mm2px(Vec(171.45 + 12.7 * i, 96.2)), module, TAuxExpander::GROUP_AUXSEND_PARAMS + (i + 2) * 4 + 2));
			addChild(createLightCentered<TinyLight<RedLight>>(mm2px(Vec(171.45 + 12.7 * i - redO - redOx, 96.2 + redO)), module, TAuxExpander::AUXSENDMUTE_GROUPED_RETURN_LIGHTS + (i + 2) * 4 + 2));	
			if (module) {
				newArcKnob->paramWithCV = &module->indivGroupSendWithCv[(i + 2) * 4 + 2];
				newArcKnob->paramCvConnected = &module->indivGroupSendCvConnected;
				newArcKnob->detailsShowSrc = &(module->colorAndCloak.cc4[detailsShow]);
				newArcKnob->cloakedModeSrc = &(module->colorAndCloak.cc4[cloakedMode]);
			}				
			// aux D send for groups 3 to 4
			addParam(newArcKnob = createParamCentered<MmSmallKnobPurpleWithArc>(mm2px(Vec(171.45 + 12.7 * i, 107.05)), module, TAuxExpander::GROUP_AUXSEND_PARAMS + (i + 2) * 4 + 3));
			addChild(createLightCentered<TinyLight<RedLight>>(mm2px(Vec(171.45 + 12.7 * i - redO - redOx, 107.05 + redO)), module, TAuxExpander::AUXSENDMUTE_GROUPED_RETURN_LIGHTS + (i + 2) * 4 + 3));	
			if (module) {
				newArcKnob->paramWithCV = &module->indivGroupSendWithCv[(i + 2) * 4 + 3];
				newArcKnob->paramCvConnected = &module->indivGroupSendCvConnected;
				newArcKnob->detailsShowSrc = &(module->colorAndCloak.cc4[detailsShow]);
				newArcKnob->cloakedModeSrc = &(module->colorAndCloak.cc4[cloakedMode]);
			}				
			// mute for groups 3 to 4
			addParam(createParamCentered<MmMuteButton>(mm2px(Vec(171.45  + 12.7 * i, 116.1)), module, TAuxExpander::GROUP_AUXMUTE_PARAMS + i + 2));
		}
		
		static constexpr float cvx = 198.6f;
		
		// CV inputs A-D
		addInput(createInputCentered<MmPortGold>(mm2px(Vec(cvx, 13.8)), module, TAuxExpander::POLY_AUX_AD_CV_INPUTS + 0));			
		addInput(createInputCentered<MmPortGold>(mm2px(Vec(cvx, 13.8 + 10.85 * 1)), module, TAuxExpander::POLY_AUX_AD_CV_INPUTS + 1));			
		addInput(createInputCentered<MmPortGold>(mm2px(Vec(cvx, 13.8 + 10.85 * 2)), module, TAuxExpander::POLY_AUX_AD_CV_INPUTS + 2));			
		addInput(createInputCentered<MmPortGold>(mm2px(Vec(cvx, 13.8 + 10.85 * 3)), module, TAuxExpander::POLY_AUX_AD_CV_INPUTS + 3));	
		
		// CV input M
		addInput(createInputCentered<MmPortGold>(mm2px(Vec(cvx, 13.8 + 10.85 * 4)), module, TAuxExpander::POLY_AUX_M_CV_INPUT));	
		
		// CV input grp A-D
		addInput(createInputCentered<MmPortGold>(mm2px(Vec(cvx, 13.8 + 10.85 * 5)), module, TAuxExpander::POLY_GRPS_AD_CV_INPUT));	
		
		// CV input M grp
		addInput(createInputCentered<MmPortGold>(mm2px(Vec(cvx, 13.8 + 10.85 * 6)), module, TAuxExpander::POLY_GRPS_M_CV_INPUT));	
		
		// CV input bus send, pan, return
		addInput(createInputCentered<MmPortGold>(mm2px(Vec(cvx, 13.8 + 10.85 * 7)), module, TAuxExpander::POLY_BUS_SND_PAN_RET_CV_INPUT));	
	
		// CV input bus mute, solo
		addInput(createInputCentered<MmPortGold>(mm2px(Vec(cvx, 13.8 + 10.85 * 8)), module, TAuxExpander::POLY_BUS_MUTE_SOLO_CV_INPUT));	
	}
};



//-----------------------------------------------------------------------------



struct AuxExpanderJrWidget : ModuleWidget {
	static const int N_TRK = 8;
	static const int N_GRP = 2;
	
	
	#include "AuxExpanderWidget.hpp"
	
	
	AuxExpanderJrWidget(TAuxExpander *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/auxspander-jr.svg")));
		SvgPanel* svgPanel = (SvgPanel*)getPanel();
		panelBorder = findBorder(svgPanel->fb);


		// Left side (globals)
		for (int i = 0; i < 4; i++) {
			// Labels
			addChild(auxDisplays[i] = createWidgetCentered<AuxDisplay<TAuxExpander::AuxspanderAux>>(mm2px(Vec(6.35 + 12.7 * i + 2, 4.7))));
			if (module) {
				// auxDisplays[i]->tabNextFocus = // done after the for loop
				auxDisplays[i]->colorAndCloak = &(module->colorAndCloak);
				auxDisplays[i]->dispColorLocal = &(module->dispColorAuxLocal.cc4[i]);
				auxDisplays[i]->srcAux = &(module->aux[i]);
				auxDisplays[i]->srcVuColor = &(module->vuColorThemeLocal.cc4[i]);
				auxDisplays[i]->srcDirectOutsModeLocal = &(module->directOutsModeLocal.cc4[i]);
				auxDisplays[i]->srcPanLawStereoLocal = &(module->panLawStereoLocal.cc4[i]);
				auxDisplays[i]->srcDirectOutsModeGlobal = &(module->directOutPanStereoMomentCvLinearVol.cc4[0]);
				auxDisplays[i]->srcPanLawStereoGlobal = &(module->directOutPanStereoMomentCvLinearVol.cc4[1]);
				auxDisplays[i]->srcPanCvLevel = &(module->panCvLevels[i]);
				auxDisplays[i]->srcFadeRatesAndProfiles = &(module->auxFadeRatesAndProfiles[i]);
				auxDisplays[i]->auxName = &(module->auxLabels[i * 4]);
				auxDisplays[i]->auxNumber = i;
				auxDisplays[i]->numTracks = N_TRK;
				auxDisplays[i]->numGroups = N_GRP;
				auxDisplays[i]->updateAuxLabelRequestPtr = &(module->updateAuxLabelRequest);
			}
			// Y is 4.7, same X as below
			
			// Left sends
			addOutput(createOutputCentered<MmPort>(mm2px(Vec(6.35 + 12.7 * i, 12.8)), module, TAuxExpander::SEND_OUTPUTS + i + 0));			
			// Right sends
			addOutput(createOutputCentered<MmPort>(mm2px(Vec(6.35 + 12.7 * i, 21.8)), module, TAuxExpander::SEND_OUTPUTS + i + 4));

			// Left returns
			addInput(createInputCentered<MmPort>(mm2px(Vec(6.35 + 12.7 * i, 31.5)), module, TAuxExpander::RETURN_INPUTS + i * 2 + 0));			
			// Right returns
			addInput(createInputCentered<MmPort>(mm2px(Vec(6.35 + 12.7 * i, 40.5)), module, TAuxExpander::RETURN_INPUTS + i * 2 + 1));			
			
			// Pan knobs
			MmSmallKnobGreyWithArc *panKnobAux;
			addParam(panKnobAux = createParamCentered<MmSmallKnobGreyWithArc>(mm2px(Vec(6.35 + 12.7 * i, 62.83)), module, TAuxExpander::GLOBAL_AUXPAN_PARAMS + i));
			if (module) {
				panKnobAux->detailsShowSrc = &(module->colorAndCloak.cc4[detailsShow]);
				panKnobAux->cloakedModeSrc = &(module->colorAndCloak.cc4[cloakedMode]);
				panKnobAux->paramWithCV = &(module->globalRetPansWithCV[i]);
				panKnobAux->paramCvConnected = &(module->globalRetPansCvConnected);
				panKnobAux->dispColorGlobalSrc = &(module->colorAndCloak.cc4[dispColorGlobal]);
				panKnobAux->dispColorLocalSrc = &(module->dispColorAuxLocal.cc4[i]);
			}
			
			// Return faders
			addParam(createParamCentered<MmSmallerFader>(mm2px(Vec(6.35 + 3.67 + 12.7 * i, 87.2)), module, TAuxExpander::GLOBAL_AUXRETURN_PARAMS + i));
			if (module) {
				// VU meters
				VuMeterAux *newVU = createWidgetCentered<VuMeterAux>(mm2px(Vec(6.35 + 12.7 * i, 87.2)));
				newVU->srcLevels = &(module->srcLevelsVus[i][0]);
				newVU->srcMuteGhost = &(module->srcMuteGhost[i]);
				newVU->colorThemeGlobal = &(module->colorAndCloak.cc4[vuColorGlobal]);
				newVU->colorThemeLocal = &(module->vuColorThemeLocal.cc4[i]);
				addChild(newVU);
				// Fade pointers
				CvAndFadePointerAuxRet *newFP = createWidgetCentered<CvAndFadePointerAuxRet>(mm2px(Vec(6.35 - 2.95 + 12.7 * i, 87.2)));
				newFP->srcParam = &(module->params[TAuxExpander::GLOBAL_AUXRETURN_PARAMS + i]);
				newFP->srcParamWithCV = &(module->paramRetFaderWithCv[i]);
				newFP->colorAndCloak = &(module->colorAndCloak);
				newFP->srcFadeGain = &(module->auxRetFadeGains[i]);
				newFP->srcFadeRate = &(module->auxFadeRatesAndProfiles[i]);
				newFP->dispColorLocalPtr = &(module->dispColorAuxLocal.cc4[i]);
				addChild(newFP);				
			}				
			
			// Global mute buttons
			MmMuteFadeButton* newMuteFade;
			addParam(newMuteFade = createParamCentered<MmMuteFadeButton>(mm2px(Vec(6.35  + 12.7 * i, 109.8)), module, TAuxExpander::GLOBAL_AUXMUTE_PARAMS + i));
			if (module) {
				newMuteFade->type = &(module->auxFadeRatesAndProfiles[i]);
			}
			
			// Global solo buttons
			addParam(createParamCentered<MmSoloButton>(mm2px(Vec(6.35  + 12.7 * i, 116.1)), module, TAuxExpander::GLOBAL_AUXSOLO_PARAMS + i));		

			// Group dec
			MmGroupMinusButtonNotify *newGrpMinusButton;
			addChild(newGrpMinusButton = createWidgetCentered<MmGroupMinusButtonNotify>(mm2px(Vec(6.35 - 3.73 + 12.7 * i - 0.75, 123.1))));
			if (module) {
				newGrpMinusButton->sourceParam = &(module->params[TAuxExpander::GLOBAL_AUXGROUP_PARAMS + i]);
				newGrpMinusButton->numGroups = (float)N_GRP;
			}
			// Group inc
			MmGroupPlusButtonNotify *newGrpPlusButton;
			addChild(newGrpPlusButton = createWidgetCentered<MmGroupPlusButtonNotify>(mm2px(Vec(6.35 + 3.77 + 12.7 * i + 0.75, 123.1))));
			if (module) {
				newGrpPlusButton->sourceParam = &(module->params[TAuxExpander::GLOBAL_AUXGROUP_PARAMS + i]);
				newGrpPlusButton->numGroups = (float)N_GRP;
			}
			// Group select displays
			GroupSelectDisplay* groupSelectDisplay;
			addParam(groupSelectDisplay = createParamCentered<GroupSelectDisplay>(mm2px(Vec(6.35 + 12.7 * i, 123.1)), module, TAuxExpander::GLOBAL_AUXGROUP_PARAMS + i));
			if (module) {
				groupSelectDisplay->srcColor = &(module->colorAndCloak);
				groupSelectDisplay->srcColorLocal = &(module->dispColorAuxLocal.cc4[i]);
				groupSelectDisplay->numGroups = N_GRP;
			}
		}
		for (int i = 0; i < 4; i++) {
			auxDisplays[i]->tabNextFocus = auxDisplays[(i + 1) % 4];
		}

		// Global send knobs
		MmKnobWithArc* sendKnobs[4];
		addParam(sendKnobs[0] = createParamCentered<MmSmallKnobRedWithArcTopCentered>(mm2px(Vec(6.35 + 12.7 * 0, 51.8)), module, TAuxExpander::GLOBAL_AUXSEND_PARAMS + 0));
		addParam(sendKnobs[1] = createParamCentered<MmSmallKnobOrangeWithArcTopCentered>(mm2px(Vec(6.35 + 12.7 * 1, 51.8)), module, TAuxExpander::GLOBAL_AUXSEND_PARAMS + 1));
		addParam(sendKnobs[2] = createParamCentered<MmSmallKnobBlueWithArcTopCentered>(mm2px(Vec(6.35 + 12.7 * 2, 51.8)), module, TAuxExpander::GLOBAL_AUXSEND_PARAMS + 2));
		addParam(sendKnobs[3] = createParamCentered<MmSmallKnobPurpleWithArcTopCentered>(mm2px(Vec(6.35 + 12.7 * 3, 51.8)), module, TAuxExpander::GLOBAL_AUXSEND_PARAMS + 3));
		if (module) {
			for (int k = 0; k < 4; k++) {
				sendKnobs[k]->paramWithCV = &module->globalSendsWithCV[k];
				sendKnobs[k]->paramCvConnected = &module->globalSendsCvConnected;
				sendKnobs[k]->detailsShowSrc = &(module->colorAndCloak.cc4[detailsShow]);
				sendKnobs[k]->cloakedModeSrc = &(module->colorAndCloak.cc4[cloakedMode]);
			}
		}


		// Right side (individual tracks)
		MmKnobWithArc* newArcKnob;
		for (int i = 0; i < 4; i++) {
			// Labels for tracks 1 to 4
			addChild(trackAndGroupLabels[i] = createWidgetCentered<TrackAndGroupLabel>(mm2px(Vec(67.31 + 2 + 12.7 * i, 4.7))));
			if (module) {
				trackAndGroupLabels[i]->dispColorPtr = &(module->colorAndCloak.cc4[dispColorGlobal]);
				trackAndGroupLabels[i]->dispColorLocalPtr = &(module->trackDispColsLocal[i >> 2].cc4[i & 0x3]);
			}
			// aux A send for tracks 1 to 4
			addParam(newArcKnob = createParamCentered<MmSmallKnobRedWithArc>(mm2px(Vec(67.31 + 12.7 * i, 14)), module, TAuxExpander::TRACK_AUXSEND_PARAMS + i * 4 + 0));
			if (module) {
				newArcKnob->paramWithCV = &module->indivTrackSendWithCv[i * 4 + 0];
				newArcKnob->paramCvConnected = &module->indivTrackSendCvConnected[0];
				newArcKnob->detailsShowSrc = &(module->colorAndCloak.cc4[detailsShow]);
				newArcKnob->cloakedModeSrc = &(module->colorAndCloak.cc4[cloakedMode]);
			}				
			// aux B send for tracks 1 to 4
			addParam(newArcKnob = createParamCentered<MmSmallKnobOrangeWithArc>(mm2px(Vec(67.31 + 12.7 * i, 24.85)), module, TAuxExpander::TRACK_AUXSEND_PARAMS + i * 4 + 1));
			if (module) {
				newArcKnob->paramWithCV = &module->indivTrackSendWithCv[i * 4 + 1];
				newArcKnob->paramCvConnected = &module->indivTrackSendCvConnected[1];
				newArcKnob->detailsShowSrc = &(module->colorAndCloak.cc4[detailsShow]);
				newArcKnob->cloakedModeSrc = &(module->colorAndCloak.cc4[cloakedMode]);
			}				
			// aux C send for tracks 1 to 4
			addParam(newArcKnob = createParamCentered<MmSmallKnobBlueWithArc>(mm2px(Vec(67.31 + 12.7 * i, 35.7)), module, TAuxExpander::TRACK_AUXSEND_PARAMS + i * 4 + 2));
			if (module) {
				newArcKnob->paramWithCV = &module->indivTrackSendWithCv[i * 4 + 2];
				newArcKnob->paramCvConnected = &module->indivTrackSendCvConnected[2];
				newArcKnob->detailsShowSrc = &(module->colorAndCloak.cc4[detailsShow]);
				newArcKnob->cloakedModeSrc = &(module->colorAndCloak.cc4[cloakedMode]);
			}				
			// aux D send for tracks 1 to 4
			addParam(newArcKnob = createParamCentered<MmSmallKnobPurpleWithArc>(mm2px(Vec(67.31 + 12.7 * i, 46.55)), module, TAuxExpander::TRACK_AUXSEND_PARAMS + i * 4 + 3));
			if (module) {
				newArcKnob->paramWithCV = &module->indivTrackSendWithCv[i * 4 + 3];
				newArcKnob->paramCvConnected = &module->indivTrackSendCvConnected[3];
				newArcKnob->detailsShowSrc = &(module->colorAndCloak.cc4[detailsShow]);
				newArcKnob->cloakedModeSrc = &(module->colorAndCloak.cc4[cloakedMode]);
			}				
			// mute for tracks 1 to 4
			addParam(createParamCentered<MmMuteButton>(mm2px(Vec(67.31  + 12.7 * i, 55.7)), module, TAuxExpander::TRACK_AUXMUTE_PARAMS + i));



			// Labels for tracks 5 to 8
			addChild(trackAndGroupLabels[i + 4] = createWidgetCentered<TrackAndGroupLabel>(mm2px(Vec(67.31 + 2 + 12.7 * i, 65.08))));
			if (module) {
				trackAndGroupLabels[i + 4]->dispColorPtr = &(module->colorAndCloak.cc4[dispColorGlobal]);
				trackAndGroupLabels[i + 4]->dispColorLocalPtr = &(module->trackDispColsLocal[(i + 4) >> 2].cc4[i & 0x3]);
			}

			// aux A send for tracks 5 to 8
			addParam(newArcKnob = createParamCentered<MmSmallKnobRedWithArc>(mm2px(Vec(67.31 + 12.7 * i, 74.5)), module, TAuxExpander::TRACK_AUXSEND_PARAMS + (i + 4) * 4 + 0));			
			if (module) {
				newArcKnob->paramWithCV = &module->indivTrackSendWithCv[(i + 4) * 4 + 0];
				newArcKnob->paramCvConnected = &module->indivTrackSendCvConnected[0];
				newArcKnob->detailsShowSrc = &(module->colorAndCloak.cc4[detailsShow]);
				newArcKnob->cloakedModeSrc = &(module->colorAndCloak.cc4[cloakedMode]);
			}				
			// aux B send for tracks 5 to 8
			addParam(newArcKnob = createParamCentered<MmSmallKnobOrangeWithArc>(mm2px(Vec(67.31 + 12.7 * i, 85.35)), module, TAuxExpander::TRACK_AUXSEND_PARAMS + (i + 4) * 4 + 1));
			if (module) {
				newArcKnob->paramWithCV = &module->indivTrackSendWithCv[(i + 4) * 4 + 1];
				newArcKnob->paramCvConnected = &module->indivTrackSendCvConnected[1];
				newArcKnob->detailsShowSrc = &(module->colorAndCloak.cc4[detailsShow]);
				newArcKnob->cloakedModeSrc = &(module->colorAndCloak.cc4[cloakedMode]);
			}				
			// aux C send for tracks 5 to 8
			addParam(newArcKnob = createParamCentered<MmSmallKnobBlueWithArc>(mm2px(Vec(67.31 + 12.7 * i, 96.2)), module, TAuxExpander::TRACK_AUXSEND_PARAMS + (i + 4) * 4 + 2));
			if (module) {
				newArcKnob->paramWithCV = &module->indivTrackSendWithCv[(i + 4) * 4 + 2];
				newArcKnob->paramCvConnected = &module->indivTrackSendCvConnected[2];
				newArcKnob->detailsShowSrc = &(module->colorAndCloak.cc4[detailsShow]);
				newArcKnob->cloakedModeSrc = &(module->colorAndCloak.cc4[cloakedMode]);
			}				
			// aux D send for tracks 5 to 8
			addParam(newArcKnob = createParamCentered<MmSmallKnobPurpleWithArc>(mm2px(Vec(67.31 + 12.7 * i, 107.05)), module, TAuxExpander::TRACK_AUXSEND_PARAMS + (i + 4) * 4 + 3));
			if (module) {
				newArcKnob->paramWithCV = &module->indivTrackSendWithCv[(i + 4) * 4 + 3];
				newArcKnob->paramCvConnected = &module->indivTrackSendCvConnected[3];
				newArcKnob->detailsShowSrc = &(module->colorAndCloak.cc4[detailsShow]);
				newArcKnob->cloakedModeSrc = &(module->colorAndCloak.cc4[cloakedMode]);
			}				
			// mute for tracks 5 to 8
			addParam(createParamCentered<MmMuteButton>(mm2px(Vec(67.31  + 12.7 * i, 116.1)), module, TAuxExpander::TRACK_AUXMUTE_PARAMS + i + 4));
		}

		// Right side (individual groups)
		static constexpr float redO = 3.85f;
		static constexpr float redOx = 0.58f;
		static const float xGrp1 = 171.45 + 12.7 - 12.7 * 5;
		
		// Labels for group 1
		addChild(trackAndGroupLabels[8] = createWidgetCentered<TrackAndGroupLabel>(mm2px(Vec(xGrp1 + 2, 4.7))));
		if (module) {
			trackAndGroupLabels[8]->dispColorPtr = &(module->colorAndCloak.cc4[dispColorGlobal]);
			trackAndGroupLabels[8]->dispColorLocalPtr = &(module->trackDispColsLocal[(8) >> 2].cc4[0]);			
		}

		// aux A send for group 1
		addParam(newArcKnob = createParamCentered<MmSmallKnobRedWithArc>(mm2px(Vec(xGrp1, 14)), module, TAuxExpander::GROUP_AUXSEND_PARAMS + 0));
		addChild(createLightCentered<TinyLight<RedLight>>(mm2px(Vec(xGrp1 - redO - redOx, 14 + redO)), module, TAuxExpander::AUXSENDMUTE_GROUPED_RETURN_LIGHTS + 0));	
		if (module) {
			newArcKnob->paramWithCV = &module->indivGroupSendWithCv[0];
			newArcKnob->paramCvConnected = &module->indivGroupSendCvConnected;
			newArcKnob->detailsShowSrc = &(module->colorAndCloak.cc4[detailsShow]);
			newArcKnob->cloakedModeSrc = &(module->colorAndCloak.cc4[cloakedMode]);
		}				
		// aux B send for group 1
		addParam(newArcKnob = createParamCentered<MmSmallKnobOrangeWithArc>(mm2px(Vec(xGrp1, 24.85)), module, TAuxExpander::GROUP_AUXSEND_PARAMS + 1));
		addChild(createLightCentered<TinyLight<RedLight>>(mm2px(Vec(xGrp1 - redO - redOx, 24.85 + redO)), module, TAuxExpander::AUXSENDMUTE_GROUPED_RETURN_LIGHTS + 1));	
		if (module) {
			newArcKnob->paramWithCV = &module->indivGroupSendWithCv[1];
			newArcKnob->paramCvConnected = &module->indivGroupSendCvConnected;
			newArcKnob->detailsShowSrc = &(module->colorAndCloak.cc4[detailsShow]);
			newArcKnob->cloakedModeSrc = &(module->colorAndCloak.cc4[cloakedMode]);
		}				
		// aux C send for group 1
		addParam(newArcKnob = createParamCentered<MmSmallKnobBlueWithArc>(mm2px(Vec(xGrp1, 35.7)), module, TAuxExpander::GROUP_AUXSEND_PARAMS + 2));
		addChild(createLightCentered<TinyLight<RedLight>>(mm2px(Vec(xGrp1 - redO - redOx, 35.7 + redO)), module, TAuxExpander::AUXSENDMUTE_GROUPED_RETURN_LIGHTS + 2));	
		if (module) {
			newArcKnob->paramWithCV = &module->indivGroupSendWithCv[2];
			newArcKnob->paramCvConnected = &module->indivGroupSendCvConnected;
			newArcKnob->detailsShowSrc = &(module->colorAndCloak.cc4[detailsShow]);
			newArcKnob->cloakedModeSrc = &(module->colorAndCloak.cc4[cloakedMode]);
		}				
		// aux D send for group 1
		addParam(newArcKnob = createParamCentered<MmSmallKnobPurpleWithArc>(mm2px(Vec(xGrp1, 46.55)), module, TAuxExpander::GROUP_AUXSEND_PARAMS + 3));
		addChild(createLightCentered<TinyLight<RedLight>>(mm2px(Vec(xGrp1 - redO - redOx, 46.55 + redO)), module, TAuxExpander::AUXSENDMUTE_GROUPED_RETURN_LIGHTS + 3));	
		if (module) {
			newArcKnob->paramWithCV = &module->indivGroupSendWithCv[3];
			newArcKnob->paramCvConnected = &module->indivGroupSendCvConnected;
			newArcKnob->detailsShowSrc = &(module->colorAndCloak.cc4[detailsShow]);
			newArcKnob->cloakedModeSrc = &(module->colorAndCloak.cc4[cloakedMode]);
		}				
		// mute for group 1
		addParam(createParamCentered<MmMuteButton>(mm2px(Vec(xGrp1, 55.7)), module, TAuxExpander::GROUP_AUXMUTE_PARAMS + 0));
			
			
		// Labels for group 2
		addChild(trackAndGroupLabels[9] = createWidgetCentered<TrackAndGroupLabel>(mm2px(Vec(xGrp1 + 2, 65.08))));
		if (module) {
			trackAndGroupLabels[9]->dispColorPtr = &(module->colorAndCloak.cc4[dispColorGlobal]);
			trackAndGroupLabels[9]->dispColorLocalPtr = &(module->trackDispColsLocal[(9) >> 2].cc4[1]);
		}

		// aux A send for group 2
		addParam(newArcKnob = createParamCentered<MmSmallKnobRedWithArc>(mm2px(Vec(xGrp1, 74.5)), module, TAuxExpander::GROUP_AUXSEND_PARAMS + 4));			
		addChild(createLightCentered<TinyLight<RedLight>>(mm2px(Vec(xGrp1 - redO - redOx, 74.5 + redO)), module, TAuxExpander::AUXSENDMUTE_GROUPED_RETURN_LIGHTS + 4));	
		if (module) {
			newArcKnob->paramWithCV = &module->indivGroupSendWithCv[4];
			newArcKnob->paramCvConnected = &module->indivGroupSendCvConnected;
			newArcKnob->detailsShowSrc = &(module->colorAndCloak.cc4[detailsShow]);
			newArcKnob->cloakedModeSrc = &(module->colorAndCloak.cc4[cloakedMode]);
		}				
		// aux B send for group 2
		addParam(newArcKnob = createParamCentered<MmSmallKnobOrangeWithArc>(mm2px(Vec(xGrp1, 85.35)), module, TAuxExpander::GROUP_AUXSEND_PARAMS + 5));
		addChild(createLightCentered<TinyLight<RedLight>>(mm2px(Vec(xGrp1 - redO - redOx, 85.35 + redO)), module, TAuxExpander::AUXSENDMUTE_GROUPED_RETURN_LIGHTS + 5));	
		if (module) {
			newArcKnob->paramWithCV = &module->indivGroupSendWithCv[5];
			newArcKnob->paramCvConnected = &module->indivGroupSendCvConnected;
			newArcKnob->detailsShowSrc = &(module->colorAndCloak.cc4[detailsShow]);
			newArcKnob->cloakedModeSrc = &(module->colorAndCloak.cc4[cloakedMode]);
		}				
		// aux C send for group 2
		addParam(newArcKnob = createParamCentered<MmSmallKnobBlueWithArc>(mm2px(Vec(xGrp1, 96.2)), module, TAuxExpander::GROUP_AUXSEND_PARAMS + 6));
		addChild(createLightCentered<TinyLight<RedLight>>(mm2px(Vec(xGrp1 - redO - redOx, 96.2 + redO)), module, TAuxExpander::AUXSENDMUTE_GROUPED_RETURN_LIGHTS + 6));	
		if (module) {
			newArcKnob->paramWithCV = &module->indivGroupSendWithCv[6];
			newArcKnob->paramCvConnected = &module->indivGroupSendCvConnected;
			newArcKnob->detailsShowSrc = &(module->colorAndCloak.cc4[detailsShow]);
			newArcKnob->cloakedModeSrc = &(module->colorAndCloak.cc4[cloakedMode]);
		}				
		// aux D send for group 2
		addParam(newArcKnob = createParamCentered<MmSmallKnobPurpleWithArc>(mm2px(Vec(xGrp1, 107.05)), module, TAuxExpander::GROUP_AUXSEND_PARAMS + 7));
		addChild(createLightCentered<TinyLight<RedLight>>(mm2px(Vec(xGrp1 - redO - redOx, 107.05 + redO)), module, TAuxExpander::AUXSENDMUTE_GROUPED_RETURN_LIGHTS + 7));	
		if (module) {
			newArcKnob->paramWithCV = &module->indivGroupSendWithCv[7];
			newArcKnob->paramCvConnected = &module->indivGroupSendCvConnected;
			newArcKnob->detailsShowSrc = &(module->colorAndCloak.cc4[detailsShow]);
			newArcKnob->cloakedModeSrc = &(module->colorAndCloak.cc4[cloakedMode]);
		}				
		// mute for group 2
		addParam(createParamCentered<MmMuteButton>(mm2px(Vec(xGrp1, 116.1)), module, TAuxExpander::GROUP_AUXMUTE_PARAMS + 1));
		
		
		
		static constexpr float cvx = 198.6 - 12.7 * 5 + 1.27;
		
		// CV inputs A-D
		addInput(createInputCentered<MmPortGold>(mm2px(Vec(cvx, 13.8)), module, TAuxExpander::POLY_AUX_AD_CV_INPUTS + 0));			
		addInput(createInputCentered<MmPortGold>(mm2px(Vec(cvx, 13.8 + 10.85 * 1)), module, TAuxExpander::POLY_AUX_AD_CV_INPUTS + 1));			
		
		// CV input grp A-D
		addInput(createInputCentered<MmPortGold>(mm2px(Vec(cvx, 13.8 + 10.85 * 2)), module, TAuxExpander::POLY_GRPS_AD_CV_INPUT));	
		
		// CV input M
		addInput(createInputCentered<MmPortGold>(mm2px(Vec(cvx, 13.8 + 10.85 * 3)), module, TAuxExpander::POLY_AUX_M_CV_INPUT));	
		
		// CV input bus send, pan, return
		addInput(createInputCentered<MmPortGold>(mm2px(Vec(cvx, 13.8 + 10.85 * 5)), module, TAuxExpander::POLY_BUS_SND_PAN_RET_CV_INPUT));	
	
		// CV input bus mute, solo
		addInput(createInputCentered<MmPortGold>(mm2px(Vec(cvx, 13.8 + 10.85 * 6)), module, TAuxExpander::POLY_BUS_MUTE_SOLO_CV_INPUT));	
	}
};


Model *modelAuxExpander = createModel<AuxExpander<16, 4>, AuxExpanderWidget>("AuxExpander");
Model *modelAuxExpanderJr = createModel<AuxExpander<8, 2>, AuxExpanderJrWidget>("AuxExpanderJr");
