//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "MixerWidgets.hpp"


struct AuxExpander : Module {
	enum ParamIds {
		ENUMS(TRACK_AUXSEND_PARAMS, 16 * 4), // trk 1 aux A, trk 1 aux B, ... 
		ENUMS(GROUP_AUXSEND_PARAMS, 4 * 4),// Mapping: 1A, 2A, 3A, 4A, 1B, etc
		ENUMS(TRACK_AUXMUTE_PARAMS, 16),
		ENUMS(GROUP_AUXMUTE_PARAMS, 4),// must be contiguous with TRACK_AUXMUTE_PARAMS
		ENUMS(GLOBAL_AUXMUTE_PARAMS, 4),// must be contiguous with GROUP_AUXMUTE_PARAMS
		ENUMS(GLOBAL_AUXSOLO_PARAMS, 4),// must be contiguous with GLOBAL_AUXMUTE_PARAMS
		ENUMS(GLOBAL_AUXGROUP_PARAMS, 4),// must be contiguous with GLOBAL_AUXSOLO_PARAMS
		ENUMS(GLOBAL_AUXSEND_PARAMS, 4),
		ENUMS(GLOBAL_AUXPAN_PARAMS, 4),
		ENUMS(GLOBAL_AUXRETURN_PARAMS, 4),
		NUM_PARAMS
	};
	enum InputIds {
		ENUMS(RETURN_INPUTS, 2 * 4),// must be first element (see AuxspanderAux.construct()). Mapping: left A, right A, left B, right B, left C, right C, left D, right D
		ENUMS(POLY_AUX_AD_CV_INPUTS, 4),
		POLY_AUX_M_CV_INPUT,
		POLY_GRPS_AD_CV_INPUT,// Mapping: 1A, 2A, 3A, 4A, 1B, etc
		POLY_GRPS_M_CV_INPUT,
		POLY_BUS_SND_PAN_RET_CV_INPUT,
		POLY_BUS_MUTE_SOLO_CV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(SEND_OUTPUTS, 2 * 4),// A left, B left, C left, D left, A right, B right, C right, D right
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(AUXSENDMUTE_GROUPED_RETURN_LIGHTS, 16),
		NUM_LIGHTS
	};

	// Expander
	float leftMessages[2][AFM_NUM_VALUES] = {};// messages from mother (first index is page), see enum called AuxFromMotherIds in Mixer.hpp


	// Constants
	// none


	// Need to save, no reset
	int panelTheme;
	
	// Need to save, with reset
	alignas(4) char auxLabels[4 * 4 + 1];// 4 chars per label, 4 aux labels, null terminate the end the whole array only
	PackedBytes4 vuColorThemeLocal; // 0 to numthemes - 1; (when per-track choice), no need to send back to main panel
	PackedBytes4 directOutsModeLocal;// must send back to main panel
	PackedBytes4 panLawStereoLocal;// must send back to main panel
	int8_t dispColorAuxLocal[4];
	AuxspanderAux aux[4];

	// No need to save, with reset
	int updateAuxLabelRequest;// 0 when nothing to do, 1 for read names in widget
	int refreshCounter12;
	VuMeterAllDual vu[4];
	float paramRetFaderWithCv[4];// for cv pointers in aux retrun faders 
	simd::float_4 globalSendsWithCV;
	float indivTrackSendWithCv[64];
	float indivGroupSendWithCv[16];
	float globalRetPansWithCV[4];
	dsp::TSlewLimiter<simd::float_4> sendMuteSlewers[5];
	simd::float_4 trackSendVcaGains[16];
	simd::float_4 groupSendVcaGains[4];
	
	// No need to save, no reset
	bool motherPresent = false;// can't be local to process() since widget must know in order to properly draw border
	alignas(4) char trackLabels[4 * 20 + 1] = "-01--02--03--04--05--06--07--08--09--10--11--12--13--14--15--16-GRP1GRP2GRP3GRP4";// 4 chars per label, 16 tracks and 4 groups means 20 labels, null terminate the end the whole array only
	PackedBytes4 colorAndCloak;
	PackedBytes4 directOutsAndStereoPanModes;// cc1[0] is direct out mode, cc1[1] is stereo pan mode
	int updateTrackLabelRequest = 0;// 0 when nothing to do, 1 for read names in widget
	float maxAGIndivSendFader;
	float maxAGGlobSendFader;
	uint32_t muteAuxSendWhenReturnGrouped = 0;// { ... g2-B, g2-A, g1-D, g1-C, g1-B, g1-A}
	simd::float_4 globalSends;
	simd::float_4 muteSends[5];
	Trigger muteSoloCvTriggers[28];
	int muteSoloCvTrigRefresh = 0;
	PackedBytes4 trackDispColsLocal[5];// 4 elements for 16 tracks, and 1 element for 4 groups
	uint16_t ecoMode;
	int32_t muteTrkAuxSendWhenTrkGrouped;
	
	AuxExpander() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);		
		
		leftExpander.producerMessage = leftMessages[0];
		leftExpander.consumerMessage = leftMessages[1];
		
		char strBuf[32];

		maxAGIndivSendFader = std::pow(GlobalInfo::individualAuxSendMaxLinearGain, 1.0f / GlobalInfo::individualAuxSendScalingExponent);
		for (int i = 0; i < 16; i++) {
			// Track send aux A
			snprintf(strBuf, 32, "Track #%i aux send A", i + 1);
			configParam(TRACK_AUXSEND_PARAMS + i * 4 + 0, 0.0f, maxAGIndivSendFader, 0.0f, strBuf, " dB", -10, 20.0f * GlobalInfo::individualAuxSendScalingExponent);
			// Track send aux B
			snprintf(strBuf, 32, "Track #%i aux send B", i + 1);
			configParam(TRACK_AUXSEND_PARAMS + i * 4 + 1, 0.0f, maxAGIndivSendFader, 0.0f, strBuf, " dB", -10, 20.0f * GlobalInfo::individualAuxSendScalingExponent);
			// Track send aux C
			snprintf(strBuf, 32, "Track #%i aux send C", i + 1);
			configParam(TRACK_AUXSEND_PARAMS + i * 4 + 2, 0.0f, maxAGIndivSendFader, 0.0f, strBuf, " dB", -10, 20.0f * GlobalInfo::individualAuxSendScalingExponent);
			// Track send aux D
			snprintf(strBuf, 32, "Track #%i aux send D", i + 1);
			configParam(TRACK_AUXSEND_PARAMS + i * 4 + 3, 0.0f, maxAGIndivSendFader, 0.0f, strBuf, " dB", -10, 20.0f * GlobalInfo::individualAuxSendScalingExponent);
			// Mute
			snprintf(strBuf, 32, "Track #%i aux send mute", i + 1);
			configParam(TRACK_AUXMUTE_PARAMS + i, 0.0f, 1.0f, 0.0f, strBuf);
		}
		for (int grp = 0; grp < 4; grp++) {
			// Group send aux A
			snprintf(strBuf, 32, "Group #%i aux send A", grp + 1);
			configParam(GROUP_AUXSEND_PARAMS + 0 + grp, 0.0f, maxAGIndivSendFader, 0.0f, strBuf, " dB", -10, 20.0f * GlobalInfo::individualAuxSendScalingExponent);
			// Group send aux B
			snprintf(strBuf, 32, "Group #%i aux send B", grp + 1);
			configParam(GROUP_AUXSEND_PARAMS + 4 + grp, 0.0f, maxAGIndivSendFader, 0.0f, strBuf, " dB", -10, 20.0f * GlobalInfo::individualAuxSendScalingExponent);
			// Group send aux C
			snprintf(strBuf, 32, "Group #%i aux send C", grp + 1);
			configParam(GROUP_AUXSEND_PARAMS + 8 + grp, 0.0f, maxAGIndivSendFader, 0.0f, strBuf, " dB", -10, 20.0f * GlobalInfo::individualAuxSendScalingExponent);
			// Group send aux D
			snprintf(strBuf, 32, "Group #%i aux send D", grp + 1);
			configParam(GROUP_AUXSEND_PARAMS + 12 + grp, 0.0f, maxAGIndivSendFader, 0.0f, strBuf, " dB", -10, 20.0f * GlobalInfo::individualAuxSendScalingExponent);
			// Mute
			snprintf(strBuf, 32, "Group #%i aux send mute", grp + 1);
			configParam(GROUP_AUXMUTE_PARAMS + grp, 0.0f, 1.0f, 0.0f, strBuf);		
		}

		// Global send aux A-D
		maxAGGlobSendFader = std::pow(GlobalInfo::globalAuxSendMaxLinearGain, 1.0f / GlobalInfo::globalAuxSendScalingExponent);
		configParam(GLOBAL_AUXSEND_PARAMS + 0, 0.0f, maxAGGlobSendFader, 1.0f, "Global aux send A", " dB", -10, 20.0f * GlobalInfo::globalAuxSendScalingExponent);
		configParam(GLOBAL_AUXSEND_PARAMS + 1, 0.0f, maxAGGlobSendFader, 1.0f, "Global aux send B", " dB", -10, 20.0f * GlobalInfo::globalAuxSendScalingExponent);
		configParam(GLOBAL_AUXSEND_PARAMS + 2, 0.0f, maxAGGlobSendFader, 1.0f, "Global aux send C", " dB", -10, 20.0f * GlobalInfo::globalAuxSendScalingExponent);
		configParam(GLOBAL_AUXSEND_PARAMS + 3, 0.0f, maxAGGlobSendFader, 1.0f, "Global aux send D", " dB", -10, 20.0f * GlobalInfo::globalAuxSendScalingExponent);

		// Global pan return aux A-D
		configParam(GLOBAL_AUXPAN_PARAMS + 0, 0.0f, 1.0f, 0.5f, "Global aux return pan A", "%", 0.0f, 200.0f, -100.0f);
		configParam(GLOBAL_AUXPAN_PARAMS + 1, 0.0f, 1.0f, 0.5f, "Global aux return pan B", "%", 0.0f, 200.0f, -100.0f);
		configParam(GLOBAL_AUXPAN_PARAMS + 2, 0.0f, 1.0f, 0.5f, "Global aux return pan C", "%", 0.0f, 200.0f, -100.0f);
		configParam(GLOBAL_AUXPAN_PARAMS + 3, 0.0f, 1.0f, 0.5f, "Global aux return pan D", "%", 0.0f, 200.0f, -100.0f);

		// Global return aux A-D
		float maxAGAuxRetFader = std::pow(GlobalInfo::globalAuxReturnMaxLinearGain, 1.0f / GlobalInfo::globalAuxReturnScalingExponent);
		configParam(GLOBAL_AUXRETURN_PARAMS + 0, 0.0f, maxAGAuxRetFader, 1.0f, "Global aux return A", " dB", -10, 20.0f * GlobalInfo::globalAuxReturnScalingExponent);
		configParam(GLOBAL_AUXRETURN_PARAMS + 1, 0.0f, maxAGAuxRetFader, 1.0f, "Global aux return B", " dB", -10, 20.0f * GlobalInfo::globalAuxReturnScalingExponent);
		configParam(GLOBAL_AUXRETURN_PARAMS + 2, 0.0f, maxAGAuxRetFader, 1.0f, "Global aux return C", " dB", -10, 20.0f * GlobalInfo::globalAuxReturnScalingExponent);
		configParam(GLOBAL_AUXRETURN_PARAMS + 3, 0.0f, maxAGAuxRetFader, 1.0f, "Global aux return D", " dB", -10, 20.0f * GlobalInfo::globalAuxReturnScalingExponent);

		// Global mute
		configParam(GLOBAL_AUXMUTE_PARAMS + 0, 0.0f, 1.0f, 0.0f, "Global aux send mute A");		
		configParam(GLOBAL_AUXMUTE_PARAMS + 1, 0.0f, 1.0f, 0.0f, "Global aux send mute B");		
		configParam(GLOBAL_AUXMUTE_PARAMS + 2, 0.0f, 1.0f, 0.0f, "Global aux send mute C");		
		configParam(GLOBAL_AUXMUTE_PARAMS + 3, 0.0f, 1.0f, 0.0f, "Global aux send mute D");		

		// Global solo
		configParam(GLOBAL_AUXSOLO_PARAMS + 0, 0.0f, 1.0f, 0.0f, "Global aux send solo A");		
		configParam(GLOBAL_AUXSOLO_PARAMS + 1, 0.0f, 1.0f, 0.0f, "Global aux send solo B");		
		configParam(GLOBAL_AUXSOLO_PARAMS + 2, 0.0f, 1.0f, 0.0f, "Global aux send solo C");		
		configParam(GLOBAL_AUXSOLO_PARAMS + 3, 0.0f, 1.0f, 0.0f, "Global aux send solo D");		


		colorAndCloak.cc1 = 0;
		directOutsAndStereoPanModes.cc1 = 0;
		for (int i = 0; i < 5; i++) {
			trackDispColsLocal[i].cc1 = 0;
			sendMuteSlewers[i].setRiseFall(simd::float_4(GlobalInfo::antipopSlewFast), simd::float_4(GlobalInfo::antipopSlewFast)); // slew rate is in input-units per second (ex: V/s)
		}
		for (int i = 0; i < 16; i++) {
			trackSendVcaGains[i] = simd::float_4::zero();
		}
		for (int i = 0; i < 4; i++) {
			groupSendVcaGains[i] = simd::float_4::zero();
			aux[i].construct(i, &inputs[0]);
		}
		ecoMode = 0xFFFF;// all 1's means yes, 0 means no
		muteTrkAuxSendWhenTrkGrouped = 0;
		
		onReset();

		panelTheme = 0;//(loadDarkAsDefault() ? 1 : 0);

	}
  
	void onReset() override {
		snprintf(auxLabels, 4 * 4 + 1, "AUXAAUXBAUXCAUXD");	
		for (int i = 0; i < 4; i++) {
			vuColorThemeLocal.cc4[i] = 0;
			directOutsModeLocal.cc4[i] = 3;// post-solo should be default
			panLawStereoLocal.cc4[i] = 1;
			dispColorAuxLocal[i] = 0;	
			aux[i].onReset();
		}
		resetNonJson(false);
	}
	void resetNonJson(bool recurseNonJson) {
		updateAuxLabelRequest = 1;
		refreshCounter12 = 0;
		for (int i = 0; i < 4; i++) {
			vu[i].reset();
			paramRetFaderWithCv[i] = -1.0f;
			globalSendsWithCV[i] = -1.0f;
			globalRetPansWithCV[i] = -1.0f;
			aux[i].resetNonJson();
		}
		for (int i = 0; i < 5; i++) {
			sendMuteSlewers[i].reset();
		}
		for (int i = 0; i < 64; i++) {
			indivTrackSendWithCv[i] = -1.0f;
		}
		for (int i = 0; i < 16; i++) {
			indivGroupSendWithCv[i] = -1.0f;
		}
	}


	void onRandomize() override {
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// vuColorThemeLocal
		json_object_set_new(rootJ, "vuColorThemeLocal", json_integer(vuColorThemeLocal.cc1));

		// directOutsModeLocal
		json_object_set_new(rootJ, "directOutsModeLocal", json_integer(directOutsModeLocal.cc1));

		// panLawStereoLocal
		json_object_set_new(rootJ, "panLawStereoLocal", json_integer(panLawStereoLocal.cc1));

		// dispColorAuxLocal
		json_t *dispColorAuxLocalJ = json_array();
		for (int c = 0; c < 4; c++)
			json_array_insert_new(dispColorAuxLocalJ, c, json_integer(dispColorAuxLocal[c]));
		json_object_set_new(rootJ, "dispColorAuxLocal", dispColorAuxLocalJ);

		// auxLabels
		json_object_set_new(rootJ, "auxLabels", json_string(auxLabels));
		
		// aux
		for (int i = 0; i < 4; i++) {
			aux[i].dataToJson(rootJ);
		}

		return rootJ;
	}


	void dataFromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// vuColorThemeLocal
		json_t *vuColorThemeLocalJ = json_object_get(rootJ, "vuColorThemeLocal");
		if (vuColorThemeLocalJ)
			vuColorThemeLocal.cc1 = json_integer_value(vuColorThemeLocalJ);

		// directOutsModeLocal
		json_t *directOutsModeLocalJ = json_object_get(rootJ, "directOutsModeLocal");
		if (directOutsModeLocalJ)
			directOutsModeLocal.cc1 = json_integer_value(directOutsModeLocalJ);

		// panLawStereoLocal
		json_t *panLawStereoLocalJ = json_object_get(rootJ, "panLawStereoLocal");
		if (panLawStereoLocalJ)
			panLawStereoLocal.cc1 = json_integer_value(panLawStereoLocalJ);

		// dispColorAuxLocal
		json_t *dispColorAuxLocalJ = json_object_get(rootJ, "dispColorAuxLocal");
		if (dispColorAuxLocalJ) {
			for (int c = 0; c < 4; c++)
			{
				json_t *dispColorAuxLocalArrayJ = json_array_get(dispColorAuxLocalJ, c);
				if (dispColorAuxLocalArrayJ)
					dispColorAuxLocal[c] = json_integer_value(dispColorAuxLocalArrayJ);
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

		resetNonJson(true);
	}



	void process(const ProcessArgs &args) override {
		motherPresent = (leftExpander.module && leftExpander.module->model == modelMixMaster);
		float *messagesFromMother = (float*)leftExpander.consumerMessage;
		
		processMuteSoloCvTriggers();
		
		if (motherPresent) {
			// From Mother
			// ***********
			
			// Slow values from mother
			uint32_t* updateSlow = (uint32_t*)(&messagesFromMother[AFM_UPDATE_SLOW]);
			if (*updateSlow != 0) {
				// Track labels
				memcpy(trackLabels, &messagesFromMother[AFM_TRACK_GROUP_NAMES], 4 * 20);
				updateTrackLabelRequest = 1;
				// Panel theme
				int32_t tmp;
				memcpy(&tmp, &messagesFromMother[AFM_PANEL_THEME], 4);
				panelTheme = tmp;
				// Color theme
				memcpy(&colorAndCloak.cc1, &messagesFromMother[AFM_COLOR_AND_CLOAK], 4);
				// Direct outs mode global and Stereo pan mode global
				memcpy(&directOutsAndStereoPanModes.cc1, &messagesFromMother[AFM_DIRECT_AND_PAN_MODES], 4);			
				// Track move
				memcpy(&tmp, &messagesFromMother[AFM_TRACK_MOVE], 4);
				if (tmp != 0) {
					moveTrack(tmp);
					tmp = 0;
					memcpy(&messagesFromMother[AFM_TRACK_MOVE], &tmp, 4);
				}
				// Aux send mute when grouped return lights
				muteAuxSendWhenReturnGrouped = (uint32_t)messagesFromMother[AFM_AUXSENDMUTE_GROUPED_RETURN];
				for (int i = 0; i < 16; i++) {
					lights[AUXSENDMUTE_GROUPED_RETURN_LIGHTS + i].setBrightness((muteAuxSendWhenReturnGrouped & (1 << i)) != 0 ? 1.0f : 0.0f);
				}
				// Display colors (when per track)
				memcpy(trackDispColsLocal, &messagesFromMother[AFM_TRK_DISP_COL], 5 * 4);
				// Eco mode
				memcpy(&tmp, &messagesFromMother[AFM_ECO_MODE], 4);
				ecoMode = (uint16_t)tmp;
				// mute aux send of track when it is grouped and that group is muted
				memcpy(&muteTrkAuxSendWhenTrkGrouped, &messagesFromMother[AFM_TRK_AUX_SEND_MUTED_WHEN_GROUPED], 4);
			}
			
			// Fast values from mother
			// Vus handled below outside the block
						
			// Aux sends

			// Prepare values used to compute aux sends
			//   Global aux send knobs (4 instances)
			if (ecoMode == 0 || (refreshCounter12 & 0x3) == 0) {// stagger 0
				for (int gi = 0; gi < 4; gi++) {
					globalSends[gi] = params[GLOBAL_AUXSEND_PARAMS + gi].getValue();
				}
				if (inputs[POLY_BUS_SND_PAN_RET_CV_INPUT].isConnected()) {
					// Knob CV (adding, pre-scaling)
					globalSends += inputs[POLY_BUS_SND_PAN_RET_CV_INPUT].getVoltageSimd<simd::float_4>(0) * 0.1f * maxAGGlobSendFader;
					globalSends = clamp(globalSends, 0.0f, maxAGGlobSendFader);
					globalSendsWithCV = globalSends;
				}
				else {
					globalSendsWithCV = simd::float_4(-1.0f);
				}
				globalSends = simd::pow<simd::float_4>(globalSends, GlobalInfo::globalAuxSendScalingExponent);
			
				//   Indiv mute sends (20 instances)				
				for (int gi = 0; gi < 16; gi++) {
					muteSends[gi >> 2][gi & 0x3] = ( ((muteTrkAuxSendWhenTrkGrouped & (1 << gi)) == 0) && 
													 (params[TRACK_AUXMUTE_PARAMS + gi].getValue() < 0.5f) ? 1.0f : 0.0f );
				}
				for (int gi = 0; gi < 4; gi++) {
					muteSends[4][gi] = params[GROUP_AUXMUTE_PARAMS + gi].getValue();
				}	
				muteSends[4] = simd::ifelse(muteSends[4] > 0.5f, 0.0f, 1.0f);
				for (int gi = 0; gi < 5; gi++) {
					if (movemask(muteSends[gi] == sendMuteSlewers[gi].out) != 0xF) {// movemask returns 0xF when 4 floats are equal
						sendMuteSlewers[gi].process(args.sampleTime * (1 + (ecoMode & 0x3)), muteSends[gi]);
					}
				}
			}
	
			// Aux send VCAs
			float* auxSendsTrkGrp = &messagesFromMother[AFM_AUX_SENDS];// 40 values of the sends (Trk1L, Trk1R, Trk2L, Trk2R ... Trk16L, Trk16R, Grp1L, Grp1R ... Grp4L, Grp4R))
			simd::float_4 auxSends[2] = {simd::float_4::zero(), simd::float_4::zero()};// [0] = ABCD left, [1] = ABCD right
			// accumulate tracks
			for (int trk = 0; trk < 16; trk++) {		
				// prepare trackSendVcaGains when needed
				if (ecoMode == 0 || (refreshCounter12 & 0x3) == 1) {// stagger 1			
					for (int auxi = 0; auxi < 4; auxi++) {
					// 64 individual track aux send knobs
						float val = params[TRACK_AUXSEND_PARAMS + (trk << 2) + auxi].getValue();
						int inputNum = POLY_AUX_AD_CV_INPUTS + auxi;
						if (inputs[inputNum].isConnected()) {
							// Knob CV (adding, pre-scaling)
							val += inputs[inputNum].getVoltage(trk) * 0.1f * maxAGIndivSendFader;
							val = clamp(val, 0.0f, maxAGIndivSendFader);
							indivTrackSendWithCv[(trk << 2) + auxi] = val;
						}
						else {
							indivTrackSendWithCv[(trk << 2) + auxi] = -1.0f;
						}
						trackSendVcaGains[trk][auxi] = val;
					}
					trackSendVcaGains[trk] = simd::pow<simd::float_4>(trackSendVcaGains[trk], GlobalInfo::individualAuxSendScalingExponent);
					trackSendVcaGains[trk] *= globalSends * simd::float_4(sendMuteSlewers[trk >> 2].out[trk & 0x3]);
				}
				// vca the aux send knobs with the track's sound
				auxSends[0] += trackSendVcaGains[trk] * simd::float_4(auxSendsTrkGrp[(trk << 1) + 0]);// L
				auxSends[1] += trackSendVcaGains[trk] * simd::float_4(auxSendsTrkGrp[(trk << 1) + 1]);// R				
			}
			// accumulate groups
			for (int grp = 0; grp < 4; grp++) {
				// prepare groupSendVcaGains when needed
				if (ecoMode == 0 || (refreshCounter12 & 0x3) == 2) {// stagger 2
					for (int auxi = 0; auxi < 4; auxi++) {
						if ((muteAuxSendWhenReturnGrouped & (1 << ((grp << 2) + auxi))) == 0) {
						// 16 individual group aux send knobs
							float val = params[GROUP_AUXSEND_PARAMS + (grp << 2) + auxi].getValue();
							if (inputs[POLY_GRPS_AD_CV_INPUT].isConnected()) {
								// Knob CV (adding, pre-scaling)
								int cvIndex = ((auxi << 2) + grp);// not the same order for the CVs
								val += inputs[POLY_GRPS_AD_CV_INPUT].getVoltage(cvIndex) * 0.1f * maxAGIndivSendFader;
								val = clamp(val, 0.0f, maxAGIndivSendFader);
								indivGroupSendWithCv[(grp << 2) + auxi] = val;
							}
							else {
								indivGroupSendWithCv[(grp << 2) + auxi] = -1.0f;							
							}
							groupSendVcaGains[grp][auxi] = val;
						}
						else {
							groupSendVcaGains[grp][auxi] = 0.0f;
						}
					}
					groupSendVcaGains[grp] = simd::pow<simd::float_4>(groupSendVcaGains[grp], GlobalInfo::individualAuxSendScalingExponent);
					groupSendVcaGains[grp] *= globalSends * simd::float_4(sendMuteSlewers[4].out[grp]);
				}
				// vca the aux send knobs with the group's sound
				auxSends[0] += groupSendVcaGains[grp] * simd::float_4(auxSendsTrkGrp[(grp << 1) + 32]);// L
				auxSends[1] += groupSendVcaGains[grp] * simd::float_4(auxSendsTrkGrp[(grp << 1) + 33]);// R				
			}			
			// Aux send outputs
			for (int i = 0; i < 4; i++) {
				outputs[SEND_OUTPUTS + i + 0].setVoltage(auxSends[0][i]);// L ABCD
				outputs[SEND_OUTPUTS + i + 4].setVoltage(auxSends[1][i]);// R ABCD
			}			
						
			
			
			// To Mother
			// ***********
			
			float *messagesToMother = (float*)leftExpander.module->rightExpander.producerMessage;
			
			// Aux returns
			// left A, right A, left B, right B, left C, right C, left D, right D
			for (int i = 0; i < 4; i++) {
				aux[i].process(&messagesToMother[MFA_AUX_RETURNS + (i << 1)]);
			}
			
			// values for returns, 12 such values (mute, solo, group)
			messagesToMother[MFA_VALUE12_INDEX] = (float)refreshCounter12;
			messagesToMother[MFA_VALUE12] = params[GLOBAL_AUXMUTE_PARAMS + refreshCounter12].getValue();;
			
			// Direct outs and Stereo pan for each aux (could be SLOW but not worth setting up for just two floats)
			memcpy(&messagesToMother[MFA_AUX_DIR_OUTS], &directOutsModeLocal, 4);
			memcpy(&messagesToMother[MFA_AUX_STEREO_PANS], &panLawStereoLocal, 4);
			
			// aux return pan
			for (int i = 0; i < 4; i++) {
				float val = params[GLOBAL_AUXPAN_PARAMS + i].getValue();
				// cv for pan
				if (inputs[POLY_BUS_SND_PAN_RET_CV_INPUT].isConnected()) {
					val += inputs[POLY_BUS_SND_PAN_RET_CV_INPUT].getVoltage(4 + i) * 0.1f;// Pan CV is a -5V to +5V input
					val = clamp(val, 0.0f, 1.0f);
					globalRetPansWithCV[i] = val;
				}
				else {
					globalRetPansWithCV[i] = -1.0f;
				}
				messagesToMother[MFA_AUX_RET_PAN + i] = val;
			}
			
			// aux return fader
			for (int i = 0; i < 4; i++) {
				float val = params[GLOBAL_AUXPAN_PARAMS + 4 + i].getValue();
				// cv for return fader
				bool isConnected = inputs[POLY_BUS_SND_PAN_RET_CV_INPUT].isConnected() && 
						(inputs[POLY_BUS_SND_PAN_RET_CV_INPUT].getChannels() >= (8 + i + 1));
				if (isConnected) {
					val *= clamp(inputs[POLY_BUS_SND_PAN_RET_CV_INPUT].getVoltage(8 + i) * 0.1f, 0.f, 1.0f);//(multiplying, pre-scaling)
					paramRetFaderWithCv[i] = val;
				}
				else {
					paramRetFaderWithCv[i] = -1.0f;// do not show cv pointer
				}
				// scaling done in mother
				messagesToMother[MFA_AUX_RET_FADER + i] = val;
			}
						
			refreshCounter12++;
			if (refreshCounter12 >= 12) {
				refreshCounter12 = 0;
			}
			
			leftExpander.module->rightExpander.messageFlipRequested = true;
		}	
		else {// if (motherPresent)
			for (int i = 0; i < 16; i++) {
				lights[AUXSENDMUTE_GROUPED_RETURN_LIGHTS + i].setBrightness(0.0f);
			}
			
		}

		// VUs
		if (!motherPresent || colorAndCloak.cc4[cloakedMode] != 0) {
			for (int i = 0; i < 4; i++) {
				vu[i].reset();
			}
		}
		else {// here mother is present and we are not cloaked
			if (ecoMode == 0 || (refreshCounter12 & 0x3) == 3) {// stagger 3
				for (int i = 0; i < 4; i++) { 
					vu[i].process(args.sampleTime, &messagesFromMother[AFM_AUX_VUS + (i << 1) + 0]);
				}
			}
		}
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
	
	void processMuteSoloCvTriggers() {
		// mute and solo cv triggers
		bool toggle = false;
		//   track send mutes
		if (muteSoloCvTrigRefresh < 16) {
			if (muteSoloCvTriggers[muteSoloCvTrigRefresh].process(inputs[POLY_AUX_M_CV_INPUT].getVoltage(muteSoloCvTrigRefresh))) {
				toggle = true;
			}
		}
		//   group send mutes
		else if (muteSoloCvTrigRefresh < 20) {
			if (muteSoloCvTriggers[muteSoloCvTrigRefresh].process(inputs[POLY_GRPS_M_CV_INPUT].getVoltage(muteSoloCvTrigRefresh - 16))) {
				toggle = true;				
			}
		}
		//   return mutes and solos
		else {
			if (muteSoloCvTriggers[muteSoloCvTrigRefresh].process(inputs[POLY_BUS_MUTE_SOLO_CV_INPUT].getVoltage(muteSoloCvTrigRefresh - 20))) {
				toggle = true;				
			}
		}
		if (toggle) {
			float newParam = 1.0f - params[TRACK_AUXMUTE_PARAMS + muteSoloCvTrigRefresh].getValue();
			params[TRACK_AUXMUTE_PARAMS + muteSoloCvTrigRefresh].setValue(newParam);
		}
		
		muteSoloCvTrigRefresh++;
		if (muteSoloCvTrigRefresh >= 28) {
			muteSoloCvTrigRefresh = 0;
		}
	}
};


struct AuxExpanderWidget : ModuleWidget {
	PanelBorder* panelBorder;
	bool oldMotherPresent = false;
	TrackAndGroupLabel* trackAndGroupLabels[20];
	AuxDisplay* auxDisplays[4];
	time_t oldTime = 0;

	AuxExpanderWidget(AuxExpander *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/auxspander.svg")));
		panelBorder = findBorder(panel);


		// Left side (globals)
		for (int i = 0; i < 4; i++) {
			// Labels
			addChild(auxDisplays[i] = createWidgetCentered<AuxDisplay>(mm2px(Vec(6.35 + 12.7 * i + 0.4, 4.7))));
			if (module) {
				auxDisplays[i]->srcAux = &(module->aux[i]);
				auxDisplays[i]->colorAndCloak = &(module->colorAndCloak);
				auxDisplays[i]->srcVuColor = &(module->vuColorThemeLocal.cc4[i]);
				auxDisplays[i]->srcDispColor = &(module->dispColorAuxLocal[i]);
				auxDisplays[i]->srcDirectOutsModeLocal = &(module->directOutsModeLocal.cc4[i]);
				auxDisplays[i]->srcPanLawStereoLocal = &(module->panLawStereoLocal.cc4[i]);
				auxDisplays[i]->srcDirectOutsModeGlobal = &(module->directOutsAndStereoPanModes.cc4[0]);
				auxDisplays[i]->srcPanLawStereoGlobal = &(module->directOutsAndStereoPanModes.cc4[1]);
				auxDisplays[i]->auxName = &(module->auxLabels[i * 4]);
				auxDisplays[i]->auxNumber = i;
				auxDisplays[i]->dispColorLocal = &(module->dispColorAuxLocal[i]);
			}
			// Y is 4.7, same X as below
			
			// Left sends
			addOutput(createDynamicPortCentered<DynPort>(mm2px(Vec(6.35 + 12.7 * i, 12.8)), false, module, AuxExpander::SEND_OUTPUTS + i + 0, module ? &module->panelTheme : NULL));			
			// Right sends
			addOutput(createDynamicPortCentered<DynPort>(mm2px(Vec(6.35 + 12.7 * i, 21.8)), false, module, AuxExpander::SEND_OUTPUTS + i + 4, module ? &module->panelTheme : NULL));

			// Left returns
			addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(6.35 + 12.7 * i, 31.5)), true, module, AuxExpander::RETURN_INPUTS + i * 2 + 0, module ? &module->panelTheme : NULL));			
			// Right returns
			addInput(createDynamicPortCentered<DynPort>(mm2px(Vec(6.35 + 12.7 * i, 40.5)), true, module, AuxExpander::RETURN_INPUTS + i * 2 + 1, module ? &module->panelTheme : NULL));			
			
			// Pan knobs
			DynSmallKnobGreyWithArc *panKnobAux;
			addParam(panKnobAux = createDynamicParamCentered<DynSmallKnobGreyWithArc>(mm2px(Vec(6.35 + 12.7 * i, 62.83)), module, AuxExpander::GLOBAL_AUXPAN_PARAMS + i, module ? &module->panelTheme : NULL));
			if (module) {
				panKnobAux->colorAndCloakPtr = &(module->colorAndCloak);
				panKnobAux->paramWithCV = &(module->globalRetPansWithCV[i]);
				panKnobAux->dispColorLocal = &(module->dispColorAuxLocal[i]);
			}
			
			// Return faders
			addParam(createDynamicParamCentered<DynSmallerFader>(mm2px(Vec(6.35 + 3.67 + 12.7 * i, 87.2)), module, AuxExpander::GLOBAL_AUXRETURN_PARAMS + i, module ? &module->panelTheme : NULL));
			if (module) {
				// VU meters
				VuMeterAux *newVU = createWidgetCentered<VuMeterAux>(mm2px(Vec(6.35 + 12.7 * i, 87.2)));
				newVU->srcLevels = &(module->vu[i]);
				newVU->colorThemeGlobal = &(module->colorAndCloak.cc4[vuColorGlobal]);
				newVU->colorThemeLocal = &(module->vuColorThemeLocal.cc4[i]);
				addChild(newVU);
				// Fade pointers
				CvAndFadePointerAuxRet *newFP = createWidgetCentered<CvAndFadePointerAuxRet>(mm2px(Vec(6.35 - 2.95 + 12.7 * i, 87.2)));
				newFP->srcParam = &(module->params[AuxExpander::GLOBAL_AUXRETURN_PARAMS + i]);
				newFP->srcParamWithCV = &(module->paramRetFaderWithCv[i]);
				newFP->colorAndCloak = &(module->colorAndCloak);
				newFP->dispColorLocalPtr = &(module->dispColorAuxLocal[i]);
				addChild(newFP);				
			}				
			
			// Global mute buttons
			addParam(createDynamicParamCentered<DynMuteButton>(mm2px(Vec(6.35  + 12.7 * i, 109.8)), module, AuxExpander::GLOBAL_AUXMUTE_PARAMS + i, module ? &module->panelTheme : NULL));
			
			// Global solo buttons
			addParam(createDynamicParamCentered<DynSoloButton>(mm2px(Vec(6.35  + 12.7 * i, 116.1)), module, AuxExpander::GLOBAL_AUXSOLO_PARAMS + i, module ? &module->panelTheme : NULL));		

			// Group dec
			DynGroupMinusButtonNotify *newGrpMinusButton;
			addChild(newGrpMinusButton = createDynamicWidgetCentered<DynGroupMinusButtonNotify>(mm2px(Vec(6.35 - 3.73 + 12.7 * i - 0.75, 123.1)), module ? &module->panelTheme : NULL));
			if (module) {
				newGrpMinusButton->sourceParam = &(module->params[AuxExpander::GLOBAL_AUXGROUP_PARAMS + i]);
			}
			// Group inc
			DynGroupPlusButtonNotify *newGrpPlusButton;
			addChild(newGrpPlusButton = createDynamicWidgetCentered<DynGroupPlusButtonNotify>(mm2px(Vec(6.35 + 3.77 + 12.7 * i + 0.75, 123.1)), module ? &module->panelTheme : NULL));
			if (module) {
				newGrpPlusButton->sourceParam = &(module->params[AuxExpander::GLOBAL_AUXGROUP_PARAMS + i]);
			}
			// Group select displays
			GroupSelectDisplay* groupSelectDisplay;
			addParam(groupSelectDisplay = createParamCentered<GroupSelectDisplay>(mm2px(Vec(6.35 + 12.7 * i - 0.1, 123.1)), module, AuxExpander::GLOBAL_AUXGROUP_PARAMS + i));
			if (module) {
				groupSelectDisplay->srcColor = &(module->colorAndCloak);
				groupSelectDisplay->srcColorLocal = &(module->dispColorAuxLocal[i]);
			}
		}

		// Global send knobs
		DynKnobWithArc* sendKnobs[4];
		addParam(sendKnobs[0] = createDynamicParamCentered<DynSmallKnobAuxAWithArc>(mm2px(Vec(6.35 + 12.7 * 0, 51.8)), module, AuxExpander::GLOBAL_AUXSEND_PARAMS + 0, module ? &module->panelTheme : NULL));
		addParam(sendKnobs[1] = createDynamicParamCentered<DynSmallKnobAuxBWithArc>(mm2px(Vec(6.35 + 12.7 * 1, 51.8)), module, AuxExpander::GLOBAL_AUXSEND_PARAMS + 1, module ? &module->panelTheme : NULL));
		addParam(sendKnobs[2] = createDynamicParamCentered<DynSmallKnobAuxCWithArc>(mm2px(Vec(6.35 + 12.7 * 2, 51.8)), module, AuxExpander::GLOBAL_AUXSEND_PARAMS + 2, module ? &module->panelTheme : NULL));
		addParam(sendKnobs[3] = createDynamicParamCentered<DynSmallKnobAuxDWithArc>(mm2px(Vec(6.35 + 12.7 * 3, 51.8)), module, AuxExpander::GLOBAL_AUXSEND_PARAMS + 3, module ? &module->panelTheme : NULL));
		if (module) {
			for (int k = 0; k < 4; k++) {
				sendKnobs[k]->paramWithCV = &module->globalSendsWithCV[k];
				sendKnobs[k]->colorAndCloakPtr = &(module->colorAndCloak);
			}
		}


		// Right side (individual tracks)
		DynKnobWithArc* newArcKnob;
		for (int i = 0; i < 8; i++) {
			// Labels for tracks 1 to 8
			addChild(trackAndGroupLabels[i] = createWidgetCentered<TrackAndGroupLabel>(mm2px(Vec(67.31 + 12.7 * i, 4.7))));
			if (module) {
				trackAndGroupLabels[i]->dispColorPtr = &(module->colorAndCloak.cc4[dispColor]);
				trackAndGroupLabels[i]->dispColorLocalPtr = &(module->trackDispColsLocal[i >> 2].cc4[i & 0x3]);
			}
			// aux A send for tracks 1 to 8
			addParam(newArcKnob = createDynamicParamCentered<DynSmallKnobAuxAWithArc>(mm2px(Vec(67.31 + 12.7 * i, 14)), module, AuxExpander::TRACK_AUXSEND_PARAMS + i * 4 + 0, module ? &module->panelTheme : NULL));
			if (module) {
				newArcKnob->paramWithCV = &module->indivTrackSendWithCv[i * 4 + 0];
				newArcKnob->colorAndCloakPtr = &(module->colorAndCloak);		
			}				
			// aux B send for tracks 1 to 8
			addParam(newArcKnob = createDynamicParamCentered<DynSmallKnobAuxBWithArc>(mm2px(Vec(67.31 + 12.7 * i, 24.85)), module, AuxExpander::TRACK_AUXSEND_PARAMS + i * 4 + 1, module ? &module->panelTheme : NULL));
			if (module) {
				newArcKnob->paramWithCV = &module->indivTrackSendWithCv[i * 4 + 1];
				newArcKnob->colorAndCloakPtr = &(module->colorAndCloak);		
			}				
			// aux C send for tracks 1 to 8
			addParam(newArcKnob = createDynamicParamCentered<DynSmallKnobAuxCWithArc>(mm2px(Vec(67.31 + 12.7 * i, 35.7)), module, AuxExpander::TRACK_AUXSEND_PARAMS + i * 4 + 2, module ? &module->panelTheme : NULL));
			if (module) {
				newArcKnob->paramWithCV = &module->indivTrackSendWithCv[i * 4 + 2];
				newArcKnob->colorAndCloakPtr = &(module->colorAndCloak);		
			}				
			// aux D send for tracks 1 to 8
			addParam(newArcKnob = createDynamicParamCentered<DynSmallKnobAuxDWithArc>(mm2px(Vec(67.31 + 12.7 * i, 46.55)), module, AuxExpander::TRACK_AUXSEND_PARAMS + i * 4 + 3, module ? &module->panelTheme : NULL));
			if (module) {
				newArcKnob->paramWithCV = &module->indivTrackSendWithCv[i * 4 + 3];
				newArcKnob->colorAndCloakPtr = &(module->colorAndCloak);		
			}				
			// mute for tracks 1 to 8
			addParam(createDynamicParamCentered<DynMuteButton>(mm2px(Vec(67.31  + 12.7 * i, 55.7)), module, AuxExpander::TRACK_AUXMUTE_PARAMS + i, module ? &module->panelTheme : NULL));
			
			
			// Labels for tracks 9 to 16
			addChild(trackAndGroupLabels[i + 8] = createWidgetCentered<TrackAndGroupLabel>(mm2px(Vec(67.31 + 12.7 * i, 65.08))));
			if (module) {
				trackAndGroupLabels[i + 8]->dispColorPtr = &(module->colorAndCloak.cc4[dispColor]);
				trackAndGroupLabels[i + 8]->dispColorLocalPtr = &(module->trackDispColsLocal[(i + 8) >> 2].cc4[i & 0x3]);
			}

			// aux A send for tracks 9 to 16
			addParam(newArcKnob = createDynamicParamCentered<DynSmallKnobAuxAWithArc>(mm2px(Vec(67.31 + 12.7 * i, 74.5)), module, AuxExpander::TRACK_AUXSEND_PARAMS + (i + 8) * 4 + 0, module ? &module->panelTheme : NULL));			
			if (module) {
				newArcKnob->paramWithCV = &module->indivTrackSendWithCv[(i + 8) * 4 + 0];
				newArcKnob->colorAndCloakPtr = &(module->colorAndCloak);		
			}				
			// aux B send for tracks 9 to 16
			addParam(newArcKnob = createDynamicParamCentered<DynSmallKnobAuxBWithArc>(mm2px(Vec(67.31 + 12.7 * i, 85.35)), module, AuxExpander::TRACK_AUXSEND_PARAMS + (i + 8) * 4 + 1, module ? &module->panelTheme : NULL));
			if (module) {
				newArcKnob->paramWithCV = &module->indivTrackSendWithCv[(i + 8) * 4 + 1];
				newArcKnob->colorAndCloakPtr = &(module->colorAndCloak);		
			}				
			// aux C send for tracks 9 to 16
			addParam(newArcKnob = createDynamicParamCentered<DynSmallKnobAuxCWithArc>(mm2px(Vec(67.31 + 12.7 * i, 96.2)), module, AuxExpander::TRACK_AUXSEND_PARAMS + (i + 8) * 4 + 2, module ? &module->panelTheme : NULL));
			if (module) {
				newArcKnob->paramWithCV = &module->indivTrackSendWithCv[(i + 8) * 4 + 2];
				newArcKnob->colorAndCloakPtr = &(module->colorAndCloak);		
			}				
			// aux D send for tracks 9 to 16
			addParam(newArcKnob = createDynamicParamCentered<DynSmallKnobAuxDWithArc>(mm2px(Vec(67.31 + 12.7 * i, 107.05)), module, AuxExpander::TRACK_AUXSEND_PARAMS + (i + 8) * 4 + 3, module ? &module->panelTheme : NULL));
			if (module) {
				newArcKnob->paramWithCV = &module->indivTrackSendWithCv[(i + 8) * 4 + 3];
				newArcKnob->colorAndCloakPtr = &(module->colorAndCloak);		
			}				
			// mute for tracks 1 to 8
			addParam(createDynamicParamCentered<DynMuteButton>(mm2px(Vec(67.31  + 12.7 * i, 116.1)), module, AuxExpander::TRACK_AUXMUTE_PARAMS + i + 8, module ? &module->panelTheme : NULL));
		}

		// Right side (individual groups)
		static constexpr float redO = 3.85f;
		static constexpr float redOx = 0.58f;
		for (int i = 0; i < 2; i++) {
			// Labels for groups 1 to 2
			addChild(trackAndGroupLabels[i + 16] = createWidgetCentered<TrackAndGroupLabel>(mm2px(Vec(171.45 + 12.7 * i, 4.7))));
			if (module) {
				trackAndGroupLabels[i + 16]->dispColorPtr = &(module->colorAndCloak.cc4[dispColor]);
				trackAndGroupLabels[i + 16]->dispColorLocalPtr = &(module->trackDispColsLocal[(i + 16) >> 2].cc4[i & 0x3]);			
			}

			// aux A send for groups 1 to 2
			addParam(newArcKnob = createDynamicParamCentered<DynSmallKnobAuxAWithArc>(mm2px(Vec(171.45 + 12.7 * i, 14)), module, AuxExpander::GROUP_AUXSEND_PARAMS + i * 4 + 0, module ? &module->panelTheme : NULL));
			addChild(createLightCentered<TinyLight<RedLight>>(mm2px(Vec(171.45 + 12.7 * i - redO - redOx, 14 + redO)), module, AuxExpander::AUXSENDMUTE_GROUPED_RETURN_LIGHTS + i * 4 + 0));	
			if (module) {
				newArcKnob->paramWithCV = &module->indivGroupSendWithCv[i * 4 + 0];
				newArcKnob->colorAndCloakPtr = &(module->colorAndCloak);		
			}				
			// aux B send for groups 1 to 2
			addParam(newArcKnob = createDynamicParamCentered<DynSmallKnobAuxBWithArc>(mm2px(Vec(171.45 + 12.7 * i, 24.85)), module, AuxExpander::GROUP_AUXSEND_PARAMS + i * 4 + 1, module ? &module->panelTheme : NULL));
			addChild(createLightCentered<TinyLight<RedLight>>(mm2px(Vec(171.45 + 12.7 * i - redO - redOx, 24.85 + redO)), module, AuxExpander::AUXSENDMUTE_GROUPED_RETURN_LIGHTS + i * 4 + 1));	
			if (module) {
				newArcKnob->paramWithCV = &module->indivGroupSendWithCv[i * 4 + 1];
				newArcKnob->colorAndCloakPtr = &(module->colorAndCloak);		
			}				
			// aux C send for groups 1 to 2
			addParam(newArcKnob = createDynamicParamCentered<DynSmallKnobAuxCWithArc>(mm2px(Vec(171.45 + 12.7 * i, 35.7)), module, AuxExpander::GROUP_AUXSEND_PARAMS + i * 4 + 2, module ? &module->panelTheme : NULL));
			addChild(createLightCentered<TinyLight<RedLight>>(mm2px(Vec(171.45 + 12.7 * i - redO - redOx, 35.7 + redO)), module, AuxExpander::AUXSENDMUTE_GROUPED_RETURN_LIGHTS + i * 4 + 2));	
			if (module) {
				newArcKnob->paramWithCV = &module->indivGroupSendWithCv[i * 4 + 2];
				newArcKnob->colorAndCloakPtr = &(module->colorAndCloak);		
			}				
			// aux D send for groups 1 to 2
			addParam(newArcKnob = createDynamicParamCentered<DynSmallKnobAuxDWithArc>(mm2px(Vec(171.45 + 12.7 * i, 46.55)), module, AuxExpander::GROUP_AUXSEND_PARAMS + i * 4 + 3, module ? &module->panelTheme : NULL));
			addChild(createLightCentered<TinyLight<RedLight>>(mm2px(Vec(171.45 + 12.7 * i - redO - redOx, 46.55 + redO)), module, AuxExpander::AUXSENDMUTE_GROUPED_RETURN_LIGHTS + i * 4 + 3));	
			if (module) {
				newArcKnob->paramWithCV = &module->indivGroupSendWithCv[i * 4 + 3];
				newArcKnob->colorAndCloakPtr = &(module->colorAndCloak);		
			}				
			// mute for groups 1 to 2
			addParam(createDynamicParamCentered<DynMuteButton>(mm2px(Vec(171.45  + 12.7 * i, 55.7)), module, AuxExpander::GROUP_AUXMUTE_PARAMS + i, module ? &module->panelTheme : NULL));
			
			
			// Labels for groups 3 to 4
			addChild(trackAndGroupLabels[i + 18] = createWidgetCentered<TrackAndGroupLabel>(mm2px(Vec(171.45 + 12.7 * i, 65.08))));
			if (module) {
				trackAndGroupLabels[i + 18]->dispColorPtr = &(module->colorAndCloak.cc4[dispColor]);
				trackAndGroupLabels[i + 18]->dispColorLocalPtr = &(module->trackDispColsLocal[(i + 18) >> 2].cc4[(i + 18) & 0x3]);
			}

			// aux A send for groups 3 to 4
			addParam(newArcKnob = createDynamicParamCentered<DynSmallKnobAuxAWithArc>(mm2px(Vec(171.45 + 12.7 * i, 74.5)), module, AuxExpander::GROUP_AUXSEND_PARAMS + (i + 2) * 4 + 0, module ? &module->panelTheme : NULL));			
			addChild(createLightCentered<TinyLight<RedLight>>(mm2px(Vec(171.45 + 12.7 * i - redO - redOx, 74.5 + redO)), module, AuxExpander::AUXSENDMUTE_GROUPED_RETURN_LIGHTS + (i + 2) * 4 + 0));	
			if (module) {
				newArcKnob->paramWithCV = &module->indivGroupSendWithCv[(i + 2) * 4 + 0];
				newArcKnob->colorAndCloakPtr = &(module->colorAndCloak);		
			}				
			// aux B send for groups 3 to 4
			addParam(newArcKnob = createDynamicParamCentered<DynSmallKnobAuxBWithArc>(mm2px(Vec(171.45 + 12.7 * i, 85.35)), module, AuxExpander::GROUP_AUXSEND_PARAMS + (i + 2) * 4 + 1, module ? &module->panelTheme : NULL));
			addChild(createLightCentered<TinyLight<RedLight>>(mm2px(Vec(171.45 + 12.7 * i - redO - redOx, 85.35 + redO)), module, AuxExpander::AUXSENDMUTE_GROUPED_RETURN_LIGHTS + (i + 2) * 4 + 1));	
			if (module) {
				newArcKnob->paramWithCV = &module->indivGroupSendWithCv[(i + 2) * 4 + 1];
				newArcKnob->colorAndCloakPtr = &(module->colorAndCloak);		
			}				
			// aux C send for groups 3 to 4
			addParam(newArcKnob = createDynamicParamCentered<DynSmallKnobAuxCWithArc>(mm2px(Vec(171.45 + 12.7 * i, 96.2)), module, AuxExpander::GROUP_AUXSEND_PARAMS + (i + 2) * 4 + 2, module ? &module->panelTheme : NULL));
			addChild(createLightCentered<TinyLight<RedLight>>(mm2px(Vec(171.45 + 12.7 * i - redO - redOx, 96.2 + redO)), module, AuxExpander::AUXSENDMUTE_GROUPED_RETURN_LIGHTS + (i + 2) * 4 + 2));	
			if (module) {
				newArcKnob->paramWithCV = &module->indivGroupSendWithCv[(i + 2) * 4 + 2];
				newArcKnob->colorAndCloakPtr = &(module->colorAndCloak);		
			}				
			// aux D send for groups 3 to 4
			addParam(newArcKnob = createDynamicParamCentered<DynSmallKnobAuxDWithArc>(mm2px(Vec(171.45 + 12.7 * i, 107.05)), module, AuxExpander::GROUP_AUXSEND_PARAMS + (i + 2) * 4 + 3, module ? &module->panelTheme : NULL));
			addChild(createLightCentered<TinyLight<RedLight>>(mm2px(Vec(171.45 + 12.7 * i - redO - redOx, 107.05 + redO)), module, AuxExpander::AUXSENDMUTE_GROUPED_RETURN_LIGHTS + (i + 2) * 4 + 3));	
			if (module) {
				newArcKnob->paramWithCV = &module->indivGroupSendWithCv[(i + 2) * 4 + 3];
				newArcKnob->colorAndCloakPtr = &(module->colorAndCloak);		
			}				
			// mute for groups 3 to 4
			addParam(createDynamicParamCentered<DynMuteButton>(mm2px(Vec(171.45  + 12.7 * i, 116.1)), module, AuxExpander::GROUP_AUXMUTE_PARAMS + i + 2, module ? &module->panelTheme : NULL));
		}
		
		static constexpr float cvx = 198.6f;
		
		// CV inputs A-D
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(cvx, 13.8)), true, module, AuxExpander::POLY_AUX_AD_CV_INPUTS + 0, module ? &module->panelTheme : NULL));			
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(cvx, 13.8 + 10.85 * 1)), true, module, AuxExpander::POLY_AUX_AD_CV_INPUTS + 1, module ? &module->panelTheme : NULL));			
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(cvx, 13.8 + 10.85 * 2)), true, module, AuxExpander::POLY_AUX_AD_CV_INPUTS + 2, module ? &module->panelTheme : NULL));			
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(cvx, 13.8 + 10.85 * 3)), true, module, AuxExpander::POLY_AUX_AD_CV_INPUTS + 3, module ? &module->panelTheme : NULL));	
		
		// CV input M
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(cvx, 13.8 + 10.85 * 4)), true, module, AuxExpander::POLY_AUX_M_CV_INPUT, module ? &module->panelTheme : NULL));	
		
		// CV input grp A-D
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(cvx, 13.8 + 10.85 * 5)), true, module, AuxExpander::POLY_GRPS_AD_CV_INPUT, module ? &module->panelTheme : NULL));	
		
		// CV input M grp
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(cvx, 13.8 + 10.85 * 6)), true, module, AuxExpander::POLY_GRPS_M_CV_INPUT, module ? &module->panelTheme : NULL));	
		
		// CV input bus send, pan, return
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(cvx, 13.8 + 10.85 * 7)), true, module, AuxExpander::POLY_BUS_SND_PAN_RET_CV_INPUT, module ? &module->panelTheme : NULL));	
	
		// CV input bus mute, solo
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(cvx, 13.8 + 10.85 * 8)), true, module, AuxExpander::POLY_BUS_MUTE_SOLO_CV_INPUT, module ? &module->panelTheme : NULL));	
	
	}

	void step() override {
		if (module) {
			AuxExpander* moduleA = (AuxExpander*)module;
			
			// Labels (pull from module)
			if (moduleA->updateAuxLabelRequest != 0) {// pull request from module
				// aux displays
				for (int aux = 0; aux < 4; aux++) {
					auxDisplays[aux]->text = std::string(&(moduleA->auxLabels[aux * 4]), 4);
				}
				moduleA->updateAuxLabelRequest = 0;// all done pulling
			}
			if (moduleA->updateTrackLabelRequest != 0) {// pull request from module
				// track and group labels
				for (int trk = 0; trk < 20; trk++) {
					trackAndGroupLabels[trk]->text = std::string(&(moduleA->trackLabels[trk * 4]), 4);
				}
				moduleA->updateTrackLabelRequest = 0;// all done pulling
			}
			
			// Borders			
			if ( moduleA->motherPresent != oldMotherPresent ) {
				oldMotherPresent = moduleA->motherPresent;
				if (oldMotherPresent) {
					panelBorder->box.pos.x = -3;
					panelBorder->box.size.x = box.size.x + 3;
				}
				else {
					panelBorder->box.pos.x = 0;
					panelBorder->box.size.x = box.size.x;
				}
				((SvgPanel*)panel)->dirty = true;// weird zoom bug: if the if/else above is commented, zoom bug when this executes
			}
			
			// Update param tooltips at 1Hz
			time_t currentTime = time(0);
			if (currentTime != oldTime) {
				oldTime = currentTime;
				char strBuf[32];
				std::string auxLabels[4];
				for (int i = 0; i < 4; i++) {
					auxLabels[i] = std::string(&(moduleA->auxLabels[i * 4]), 4);
				}
				
				// Track and group indiv sends
				for (int i = 0; i < 20; i++) {
					std::string trackLabel = std::string(&(moduleA->trackLabels[i * 4]), 4);
					// Aux A-D
					for (int auxi = 0; auxi < 4; auxi++) {
						snprintf(strBuf, 32, "%s: send %s", trackLabel.c_str(), auxLabels[auxi].c_str());
						moduleA->paramQuantities[AuxExpander::TRACK_AUXSEND_PARAMS + i * 4 + auxi]->label = strBuf;
					}
					// Mutes
					snprintf(strBuf, 32, "%s: aux mute", trackLabel.c_str());
					moduleA->paramQuantities[AuxExpander::TRACK_AUXMUTE_PARAMS + i]->label = strBuf;
				}

				// Global send aux A-D
				for (int auxi = 0; auxi < 4; auxi++) {
					snprintf(strBuf, 32, "%s: global send", auxLabels[auxi].c_str());
					moduleA->paramQuantities[AuxExpander::GLOBAL_AUXSEND_PARAMS + auxi]->label = strBuf;
				}

				// Global pan return aux A-D
				for (int auxi = 0; auxi < 4; auxi++) {
					snprintf(strBuf, 32, "%s: return pan", auxLabels[auxi].c_str());
					moduleA->paramQuantities[AuxExpander::GLOBAL_AUXPAN_PARAMS + auxi]->label = strBuf;
				}

				// Global return aux A-D
				for (int auxi = 0; auxi < 4; auxi++) {
					snprintf(strBuf, 32, "%s: return level", auxLabels[auxi].c_str());
					moduleA->paramQuantities[AuxExpander::GLOBAL_AUXRETURN_PARAMS + auxi]->label = strBuf;
				}

				// Global mute
				for (int auxi = 0; auxi < 4; auxi++) {
					snprintf(strBuf, 32, "%s: return mute", auxLabels[auxi].c_str());
					moduleA->paramQuantities[AuxExpander::GLOBAL_AUXMUTE_PARAMS + auxi]->label = strBuf;
				}

				// Global solo
				for (int auxi = 0; auxi < 4; auxi++) {
					snprintf(strBuf, 32, "%s: return solo", auxLabels[auxi].c_str());
					moduleA->paramQuantities[AuxExpander::GLOBAL_AUXSOLO_PARAMS + auxi]->label = strBuf;
				}
			}
		}
		Widget::step();
	}
	
};

Model *modelAuxExpander = createModel<AuxExpander, AuxExpanderWidget>("AuxExpander");
