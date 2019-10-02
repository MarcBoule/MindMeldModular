//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.txt for all licenses
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
		ENUMS(RETURN_INPUTS, 2 * 4),
		ENUMS(POLY_AUX_AD_CV_INPUTS, 4),
		POLY_AUX_M_CV_INPUT,
		POLY_GRPS_AD_CV_INPUT,// Mapping: 1A, 2A, 3A, 4A, 1B, etc
		POLY_GRPS_M_CV_INPUT,
		POLY_BUS_SND_PAN_RET_CV_INPUT,
		POLY_BUS_MUTE_SOLO_CV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(SEND_OUTPUTS, 2 * 4),
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
	PackedBytes4 vuColorThemeLocal; // 0 to numthemes - 1; (when per-track choice), no need to send back to main panel
	PackedBytes4 directOutsModeLocal;// must send back to main panel
	PackedBytes4 panLawStereoLocal;// must send back to main panel
	dsp::SlewLimiter sendMuteSlewers[20];
	
	// No need to save, with reset
	int refreshCounter12;
	VuMeterAllDual vu[4];
	float paramRetFaderWithCv[4];// for cv pointers in aux retrun faders 
	simd::float_4 globalSendsWithCV;
	float indivTrackSendWithCv[64];
	float indivGroupSendWithCv[16];
	float globalRetPansWithCV[4];
	
	// No need to save, no reset
	bool motherPresent = false;// can't be local to process() since widget must know in order to properly draw border
	alignas(4) char trackLabels[4 * 20 + 1] = "-01--02--03--04--05--06--07--08--09--10--11--12--13--14--15--16-GRP1GRP2GRP3GRP4";// 4 chars per label, 16 tracks and 4 groups means 20 labels, null terminate the end the whole array only
	PackedBytes4 colorAndCloak;
	PackedBytes4 directOutsAndStereoPanModes;// cc1[0] is direct out mode, cc1[1] is stereo pan mode
	int updateTrackLabelRequest = 0;// 0 when nothing to do, 1 for read names in widget
	int resetAuxLabelRequest = 0;// 0 when nothing to do, 1 for reset names in widget	
	float maxAGIndivSendFader;
	float maxAGGlobSendFader;
	uint32_t muteAuxSendWhenReturnGrouped = 0;// { ... g2-B, g2-A, g1-D, g1-C, g1-B, g1-A}
	simd::float_4 globalSends;
	Trigger muteSoloCvTriggers[28];
	int muteSoloCvTrigRefresh = 0;

	
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
		for (int i = 0; i < 20; i++) {
			sendMuteSlewers[i].setRiseFall(GlobalInfo::antipopSlew, GlobalInfo::antipopSlew); // slew rate is in input-units per second (ex: V/s)
		}
		
		onReset();

		panelTheme = 0;//(loadDarkAsDefault() ? 1 : 0);

	}
  
	void onReset() override {
		for (int i = 0; i < 4; i++) {
			vuColorThemeLocal.cc4[i] = 0;
			directOutsModeLocal.cc4[i] = 3;// post-solo should be default
			panLawStereoLocal.cc4[i] = 0;
			
		}
		resetAuxLabelRequest = 1;
		resetNonJson(false);
	}
	void resetNonJson(bool recurseNonJson) {
		refreshCounter12 = 0;
		for (int i = 0; i < 4; i++) {
			vu[i].reset();
			paramRetFaderWithCv[i] = -1.0f;
			globalSendsWithCV[i] = -1.0f;
			globalRetPansWithCV[i] = -1.0f;
		}
		for (int i = 0; i < 20; i++) {
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
			}
			
			// Fast values from mother
			// Vus handled below outside the block
						
			// Aux sends

			// Prepare values used to compute aux sends
			//   Global aux send knobs (4 instances)
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
			for (int gi = 0; gi < 20; gi++) {
				float val = params[TRACK_AUXMUTE_PARAMS + gi].getValue();
				val = (val > 0.5f ? 0.0f : 1.0f);
				if (sendMuteSlewers[gi].out != val) {
					sendMuteSlewers[gi].process(args.sampleTime, val);
				}
			}
	
			// Aux send VCAs
			float* auxSendsTrkGrp = &messagesFromMother[AFM_AUX_SENDS];// 40 values of the sends (Trk1L, Trk1R, Trk2L, Trk2R ... Trk16L, Trk16R, Grp1L, Grp1R ... Grp4L, Grp4R))
			// populate auxSends[0..7]: Take the trackTaps/groupTaps indicated by the Aux sends mode (with per-track option) and combine with the 80 send floats to form the 4 stereo sends
			float auxSends[8] = {0.0f};// send outputs (left A, right A, left B, right B, left C, right C, left D, right D)
			// accumulate tracks
			for (int trk = 0; trk < 16; trk++) {
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
					val = std::pow(val, GlobalInfo::individualAuxSendScalingExponent);
					val *= globalSends[auxi] * sendMuteSlewers[trk].out;
					// vca the aux send knob with the track's sound
					auxSends[(auxi << 1) + 0] += val * auxSendsTrkGrp[(trk << 1) + 0];
					auxSends[(auxi << 1) + 1] += val * auxSendsTrkGrp[(trk << 1) + 1];
				}
			}
			// accumulate groups
			for (int grp = 0; grp < 4; grp++) {
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
						val = std::pow(val, GlobalInfo::individualAuxSendScalingExponent);
						val *= globalSends[auxi] * sendMuteSlewers[16 + grp].out;
						// vca the aux send knob with the track's sound
						auxSends[(auxi << 1) + 0] += val * auxSendsTrkGrp[(grp << 1) + 32];
						auxSends[(auxi << 1) + 1] += val * auxSendsTrkGrp[(grp << 1) + 33];
					}
				}
			}			
			// Aux send outputs
			for (int i = 0; i < 8; i++) {
				outputs[SEND_OUTPUTS + i].setVoltage(auxSends[i]);
			}			
						
			
			
			// To Mother
			// ***********
			
			// Aux returns
			float *messagesToMother = (float*)leftExpander.module->rightExpander.producerMessage;
			for (int i = 0; i < 8; i++) {
				messagesToMother[MFA_AUX_RETURNS + i] = inputs[RETURN_INPUTS + i].getVoltage();// left A, right A, left B, right B, left C, right C, left D, right D
			}
			leftExpander.module->rightExpander.messageFlipRequested = true;
			
			// values for returns, 12 such values (mute, solo, group)
			messagesToMother[MFA_VALUE12_INDEX] = (float)refreshCounter12;
			float val = params[GLOBAL_AUXMUTE_PARAMS + refreshCounter12].getValue();
			//   return mutes
			if (refreshCounter12 < 4) {
				val = (val > 0.5f ? 0.0f : 1.0f);
			}
			//   return solos
			// else if (refreshCounter12 < 8)   
			//    no compare needed here, will do a compare in GobalInfo::updateReturnSoloBits()
			//   return group
			// else 
			//    nothing to do
			messagesToMother[MFA_VALUE12] = val;
			
			// Direct outs and Stereo pan for each aux (could be SLOW but not worth setting up for just two floats)
			memcpy(&messagesToMother[MFA_AUX_DIR_OUTS], &directOutsModeLocal, 4);
			memcpy(&messagesToMother[MFA_AUX_STEREO_PANS], &panLawStereoLocal, 4);
			
			// aux return pan
			for (int i = 0; i < 4; i++) {
				val = params[GLOBAL_AUXPAN_PARAMS + i].getValue();
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
				val = params[GLOBAL_AUXPAN_PARAMS + 4 + i].getValue();
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
				// scaling
				val = std::pow(val, GlobalInfo::globalAuxReturnScalingExponent);
				messagesToMother[MFA_AUX_RET_FADER + i] = val;
			}
			
			refreshCounter12++;
			if (refreshCounter12 >= 12) {
				refreshCounter12 = 0;
			}
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
		else {
			for (int i = 0; i < 4; i++) { 
				vu[i].process(args.sampleTime, &messagesFromMother[AFM_AUX_VUS + (i << 1) + 0]);
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

	std::string getInitAuxLabel(int aux) {
		std::string ret = "AUXA";
		ret[3] += aux;
		return ret;
	}


	AuxExpanderWidget(AuxExpander *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/auxspander.svg")));
		panelBorder = findBorder(panel);


		// Left side (globals)
		for (int i = 0; i < 4; i++) {
			// Labels
			addChild(auxDisplays[i] = createWidgetCentered<AuxDisplay>(mm2px(Vec(6.35 + 12.7 * i, 4.7))));
			if (module) {
				auxDisplays[i]->colorAndCloak = &(module->colorAndCloak);
				auxDisplays[i]->srcColor = &(module->vuColorThemeLocal.cc4[i]);
				auxDisplays[i]->srcDirectOutsModeLocal = &(module->directOutsModeLocal.cc4[i]);
				auxDisplays[i]->srcPanLawStereoLocal = &(module->panLawStereoLocal.cc4[i]);
				auxDisplays[i]->srcDirectOutsModeGlobal = &(module->directOutsAndStereoPanModes.cc4[0]);
				auxDisplays[i]->srcPanLawStereoGlobal = &(module->directOutsAndStereoPanModes.cc4[1]);
				auxDisplays[i]->auxNumber = i;
				auxDisplays[i]->text = getInitAuxLabel(i);
			}
			// Y is 4.7, same X as below
			
			// Left sends
			addOutput(createDynamicPortCentered<DynPort>(mm2px(Vec(6.35 + 12.7 * i, 12.8)), false, module, AuxExpander::SEND_OUTPUTS + i * 2 + 0, module ? &module->panelTheme : NULL));			
			// Right sends
			addOutput(createDynamicPortCentered<DynPort>(mm2px(Vec(6.35 + 12.7 * i, 21.8)), false, module, AuxExpander::SEND_OUTPUTS + i * 2 + 1, module ? &module->panelTheme : NULL));

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
				newFP->dispColor = &(module->colorAndCloak.cc4[dispColor]);
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
				trackAndGroupLabels[i]->dispColor = &(module->colorAndCloak.cc4[dispColor]);
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
				trackAndGroupLabels[i + 8]->dispColor = &(module->colorAndCloak.cc4[dispColor]);
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
				trackAndGroupLabels[i + 16]->dispColor = &(module->colorAndCloak.cc4[dispColor]);
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
				trackAndGroupLabels[i + 18]->dispColor = &(module->colorAndCloak.cc4[dispColor]);
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
	
	
	json_t* toJson() override {
		json_t* rootJ = ModuleWidget::toJson();

		// aux0 label
		json_object_set_new(rootJ, "aux0label", json_string(auxDisplays[0]->text.c_str()));
		// aux1 label
		json_object_set_new(rootJ, "aux1label", json_string(auxDisplays[1]->text.c_str()));
		// aux2 label
		json_object_set_new(rootJ, "aux2label", json_string(auxDisplays[2]->text.c_str()));
		// aux3 label
		json_object_set_new(rootJ, "aux3label", json_string(auxDisplays[3]->text.c_str()));

		return rootJ;
	}

	void fromJson(json_t* rootJ) override {
		ModuleWidget::fromJson(rootJ);

		// aux0 label
		json_t* aux0J = json_object_get(rootJ, "aux0label");
		if (aux0J)
			auxDisplays[0]->text = json_string_value(aux0J);
		
		// aux1 label
		json_t* aux1J = json_object_get(rootJ, "aux1label");
		if (aux1J)
			auxDisplays[1]->text = json_string_value(aux1J);

		// aux2 label
		json_t* aux2J = json_object_get(rootJ, "aux2label");
		if (aux2J)
			auxDisplays[2]->text = json_string_value(aux2J);
		
		// aux3 label
		json_t* aux3J = json_object_get(rootJ, "aux3label");
		if (aux3J)
			auxDisplays[3]->text = json_string_value(aux3J);
	}



	void step() override {
		if (module) {
			AuxExpander* moduleA = (AuxExpander*)module;
			
			// Labels (pull from module)
			if (moduleA->resetAuxLabelRequest != 0) {
				// aux labels
				for (int aux = 0; aux < 4; aux++) {
					auxDisplays[aux]->text = getInitAuxLabel(aux);
				}
				
				moduleA->resetAuxLabelRequest = 0;
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
		}
		Widget::step();
	}
	
};

Model *modelAuxExpander = createModel<AuxExpander, AuxExpanderWidget>("AuxExpander");
