//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************



// managed by Mixer, not by tracks (tracks read only)
struct GlobalInfo {
	// constants
	// none
	
	// need to save, no reset
	// none
	
	// need to save, with reset
	PackedBytes4 directOutPanStereoMomentCvLinearVol;
		// directOutsMode: 0 is pre-insert, 1 is post-insert, 2 is post-fader, 3 is post-solo, 4 is per-track choice
		// panLawStereo: Stereo balance linear (default), Stereo balance equal power (+3dB boost since one channel lost),  True pan (linear redistribution but is not equal power), Per-track
		// momentaryCvButtons: 1 = yes (original rising edge only version), 0 = level sensitive (emulated with rising and falling detection)
		// linearVolCvInputs: 0 means powN, 1 means linear
	int panLawMono;// +0dB (no compensation),  +3 (equal power),  +4.5 (compromize),  +6dB (linear, default)
	int8_t directOutsSkipGroupedTracks;// 0 no (default), 1 is yes
	int8_t auxSendsMode;// 0 is pre-insert, 1 is post-insert, 2 is post-fader, 3 is post-solo, 4 is per-track choice
	int groupsControlTrackSendLevels;//  0 = no, 1 = yes
	int auxReturnsMutedWhenMainSolo;
	int auxReturnsSolosMuteDry;
	int chainMode;// 0 is pre-master, 1 is post-master
	PackedBytes4 colorAndCloak;// see enum called ccIds for fields
	bool symmetricalFade;
	bool fadeCvOutsWithVolCv;
	unsigned long linkBitMask;// 20 bits for 16 trks (trk1 = lsb) + 4 grps (grp4 = msb), or 10 bits for 8trk + 2grp
	int8_t filterPos;// 0 = pre insert, 1 = post insert, 2 = per track
	int8_t groupedAuxReturnFeedbackProtection;
	uint16_t ecoMode;// all 1's means yes, 0 means no
	float linkedFaderReloadValues[N_TRK + N_GRP];
	int8_t masterFaderScalesSends;// 1 = yes 
	

	// no need to save, with reset
	unsigned long soloBitMask;// when = 0ul, nothing to do, when non-zero, a track must check its solo to see if it should play
	int returnSoloBitMask;
	float sampleTime;
	bool requestLinkedFaderReload;
	float oldFaders[N_TRK + N_GRP];

	// no need to save, no reset
	Param *paMute;// all 20 (10) solos are here (track and group)
	Param *paSolo;// all 20 (10) solos are here (track and group)
	Param *paFade;// all 20 (10) faders are here (track and group)
	Param *paGroup;// all 16 (8) group numbers are here (track)
	float *values20;
	float maxTGFader;
	float fadeRates[N_TRK + N_GRP];// reset and json done in tracks and groups. fade rates for tracks and groups
	int groupUsage[N_GRP + 1];// bit 0 of first element shows if first track mapped to first group, etc... bitfields are mututally exclusive between all first 4 ints, last int is bitwise OR of first 4 ints.

	
	void clearLinked(int index) {linkBitMask &= ~(1 << index);}
	void setLinked(int index) {linkBitMask |= (1 << index);}
	void setLinked(int index, bool state) {if (state) setLinked(index); else clearLinked(index);}
	void toggleLinked(int index) {linkBitMask ^= (1 << index);}
	
	// track and group solos
	void updateSoloBitMask() {
		soloBitMask = 0ul;
		for (unsigned long trkOrGrp = 0; trkOrGrp < (N_TRK + N_GRP); trkOrGrp++) {
			updateSoloBit(trkOrGrp);
		}
	}
	void updateSoloBit(unsigned long trkOrGrp) {
		if (trkOrGrp < N_TRK) {
			if (paSolo[trkOrGrp].getValue() >= 0.5f) {
				soloBitMask |= (1 << trkOrGrp);
			}
			else {
				soloBitMask &= ~(1 << trkOrGrp);
			}	
		}
		else {// trkOrGrp >= N_TRK
			if (paSolo[trkOrGrp].getValue() >= 0.5f) {
				soloBitMask |= (1 << trkOrGrp);
			}
			else {
				soloBitMask &= ~(1 << trkOrGrp);
			}
		}
	}		
			
	
	// aux return solos
	void updateReturnSoloBits() {
		int newReturnSoloBitMask = 0;
		for (int auxi = 0; auxi < 4; auxi++) {
			if (values20[4 + auxi] >= 0.5f) {
				newReturnSoloBitMask |= (1 << auxi);
			}
		}
		returnSoloBitMask = newReturnSoloBitMask;
	}
		
	// linked faders
	void processLinked(int trgOrGrpNum, float newFader) {
		if (newFader != oldFaders[trgOrGrpNum]) {
			if (linkBitMask != 0l && isLinked(&linkBitMask, trgOrGrpNum)) {
				float delta = newFader - oldFaders[trgOrGrpNum];
				for (int trkOrGrp = 0; trkOrGrp < (N_TRK + N_GRP); trkOrGrp++) {
					if (isLinked(&linkBitMask, trkOrGrp) && trkOrGrp != trgOrGrpNum) {
						float newValue = paFade[trkOrGrp].getValue() + delta;
						newValue = clamp(newValue, 0.0f, maxTGFader);
						paFade[trkOrGrp].setValue(newValue);
						oldFaders[trkOrGrp] = newValue;// so that the other fader doesn't trigger a link propagation 
					}
				}
			}
			oldFaders[trgOrGrpNum] = newFader;
		}
	}

	// linked fade
	void fadeOtherLinkedTracks(int trkOrGrpNum, float newTarget) {
		if ((linkBitMask == 0l) || !isLinked(&linkBitMask, trkOrGrpNum)) {
			return;
		}
		for (int trkOrGrp = 0; trkOrGrp < (N_TRK + N_GRP); trkOrGrp++) {
			if (trkOrGrp != trkOrGrpNum && isLinked(&linkBitMask, trkOrGrp) && fadeRates[trkOrGrp] >= GlobalConst::minFadeRate) {
				if (newTarget >= 0.5f && paMute[trkOrGrp].getValue() >= 0.5f) {
					paMute[trkOrGrp].setValue(0.0f);
				}
				if (newTarget < 0.5f && paMute[trkOrGrp].getValue() < 0.5f) {
					paMute[trkOrGrp].setValue(1.0f);
				}
			}
		}		
	}
	
	void updateGroupUsage() {
		// clear groupUsage for all track in all groups, and bitwise OR int also
		for (int gu = 0; gu < (N_GRP + 1); gu++) {
			groupUsage[gu] = 0;
		}
		
		for (int trk = 0; trk < N_TRK; trk++) {
			// set groupUsage for this track in the new group
			int group = (int)(paGroup[trk].getValue() + 0.5f);
			if (group > 0) {
				groupUsage[group - 1] |= (1 << trk);
			}
		}
		
		// Bitwise OR of first ints in last int
		for (int grp = 0; grp < N_GRP; grp++) {
			groupUsage[N_GRP] |= groupUsage[grp];
		}
	}	
	
	void process() {// GlobalInfo
		if (requestLinkedFaderReload) {
			for (int trkOrGrp = 0; trkOrGrp < (N_TRK + N_GRP); trkOrGrp++) {
				oldFaders[trkOrGrp] = linkedFaderReloadValues[trkOrGrp];
				paFade[trkOrGrp].setValue(linkedFaderReloadValues[trkOrGrp]);
			}
			requestLinkedFaderReload = false;
		}
	}
	
	void construct(Param *_params, float* _values20) {
		paMute = &_params[TRACK_MUTE_PARAMS];
		paSolo = &_params[TRACK_SOLO_PARAMS];
		paFade = &_params[TRACK_FADER_PARAMS];
		paGroup = &_params[GROUP_SELECT_PARAMS];
		values20 = _values20;
		maxTGFader = std::pow(GlobalConst::trkAndGrpFaderMaxLinearGain, 1.0f / GlobalConst::trkAndGrpFaderScalingExponent);
	}	
	
	void onReset() {
		directOutPanStereoMomentCvLinearVol.cc4[0] = 3; // directOutsMode: post-solo by default
		directOutPanStereoMomentCvLinearVol.cc4[1] = 1; // panLawStereo
		directOutPanStereoMomentCvLinearVol.cc4[2] = 1; // momentaryCvButtons: momentary by default
		directOutPanStereoMomentCvLinearVol.cc4[3] = 0; // linearVolCvInputs: 0 means powN, 1 means linear		
		panLawMono = 1;
		directOutsSkipGroupedTracks = 0;
		auxSendsMode = 3;// post-solo should be default
		groupsControlTrackSendLevels = 0;
		auxReturnsMutedWhenMainSolo = 0;
		auxReturnsSolosMuteDry = 0;
		chainMode = 0;// pre should be default
		colorAndCloak.cc4[cloakedMode] = 0;
		colorAndCloak.cc4[vuColorGlobal] = 0;
		colorAndCloak.cc4[dispColorGlobal] = 0;
		colorAndCloak.cc4[detailsShow] = 0x7;
		symmetricalFade = false;
		fadeCvOutsWithVolCv = false;
		linkBitMask = 0;
		filterPos = 1;// default is post-insert
		groupedAuxReturnFeedbackProtection = 1;// protection is on by default
		ecoMode = 0xFFFF;// all 1's means yes, 0 means no
		for (int trkOrGrp = 0; trkOrGrp < (N_TRK + N_GRP); trkOrGrp++) {
			linkedFaderReloadValues[trkOrGrp] = 1.0f;
		}
		masterFaderScalesSends = 0;// false by default
		resetNonJson();
	}


	void resetNonJson() {
		updateSoloBitMask();
		updateReturnSoloBits();
		sampleTime = APP->engine->getSampleTime();
		requestLinkedFaderReload = true;// whether comming from onReset() or dataFromJson(), we need a synchronous fader reload of linked faders, and at this point we assume that the linkedFaderReloadValues[] have been setup.
		// oldFaders[] not done here since done synchronously by "requestLinkedFaderReload = true" above
		updateGroupUsage();
	}


	void dataToJson(json_t *rootJ) {
		// panLawMono 
		json_object_set_new(rootJ, "panLawMono", json_integer(panLawMono));

		// panLawStereo
		json_object_set_new(rootJ, "panLawStereo", json_integer(directOutPanStereoMomentCvLinearVol.cc4[1]));

		// directOutsMode
		json_object_set_new(rootJ, "directOutsMode", json_integer(directOutPanStereoMomentCvLinearVol.cc4[0]));
		
		// directOutsSkipGroupedTracks
		json_object_set_new(rootJ, "directOutsSkipGroupedTracks", json_integer(directOutsSkipGroupedTracks));
		
		// auxSendsMode
		json_object_set_new(rootJ, "auxSendsMode", json_integer(auxSendsMode));
		
		// groupsControlTrackSendLevels
		json_object_set_new(rootJ, "groupsControlTrackSendLevels", json_integer(groupsControlTrackSendLevels));
		
		// auxReturnsMutedWhenMainSolo
		json_object_set_new(rootJ, "auxReturnsMutedWhenMainSolo", json_integer(auxReturnsMutedWhenMainSolo));
		
		// auxReturnsSolosMuteDry
		json_object_set_new(rootJ, "auxReturnsSolosMuteDry", json_integer(auxReturnsSolosMuteDry));
		
		// chainMode
		json_object_set_new(rootJ, "chainMode", json_integer(chainMode));
		
		// colorAndCloak
		json_object_set_new(rootJ, "colorAndCloak", json_integer(colorAndCloak.cc1));
		
		// symmetricalFade
		json_object_set_new(rootJ, "symmetricalFade", json_boolean(symmetricalFade));
		
		// fadeCvOutsWithVolCv
		json_object_set_new(rootJ, "fadeCvOutsWithVolCv", json_boolean(fadeCvOutsWithVolCv));
		
		// linkBitMask
		json_object_set_new(rootJ, "linkBitMask", json_integer(linkBitMask));

		// filterPos
		json_object_set_new(rootJ, "filterPos", json_integer(filterPos));

		// groupedAuxReturnFeedbackProtection
		json_object_set_new(rootJ, "groupedAuxReturnFeedbackProtection", json_integer(groupedAuxReturnFeedbackProtection));

		// ecoMode
		json_object_set_new(rootJ, "ecoMode", json_integer(ecoMode));
		
		// faders (extra copy for linkedFaderReloadValues that will be populated in dataFromJson())
		json_t *fadersJ = json_array();
		for (int trkOrGrp = 0; trkOrGrp < (N_TRK + N_GRP); trkOrGrp++) {
			json_array_insert_new(fadersJ, trkOrGrp, json_real(paFade[TRACK_FADER_PARAMS + trkOrGrp].getValue()));
		}
		json_object_set_new(rootJ, "faders", fadersJ);		

		// momentaryCvButtons
		json_object_set_new(rootJ, "momentaryCvButtons", json_integer(directOutPanStereoMomentCvLinearVol.cc4[2]));

		// masterFaderScalesSends
		json_object_set_new(rootJ, "masterFaderScalesSends", json_integer(masterFaderScalesSends));

		// linearVolCvInputs
		json_object_set_new(rootJ, "linearVolCvInputs", json_integer(directOutPanStereoMomentCvLinearVol.cc4[3]));
	}


	void dataFromJson(json_t *rootJ, int nTrkSrc, int nGrpSrc) {
		// nTrkSrc and nGrpSrc are for those members which depend on number of tracks/groups
		
		// panLawMono
		json_t *panLawMonoJ = json_object_get(rootJ, "panLawMono");
		if (panLawMonoJ)
			panLawMono = json_integer_value(panLawMonoJ);
		
		// panLawStereo
		json_t *panLawStereoJ = json_object_get(rootJ, "panLawStereo");
		if (panLawStereoJ)
			directOutPanStereoMomentCvLinearVol.cc4[1] = json_integer_value(panLawStereoJ);
		
		// directOutsMode
		json_t *directOutsModeJ = json_object_get(rootJ, "directOutsMode");
		if (directOutsModeJ)
			directOutPanStereoMomentCvLinearVol.cc4[0] = json_integer_value(directOutsModeJ);
		
		// directOutsSkipGroupedTracks
		json_t *directOutsSkipGroupedTracksJ = json_object_get(rootJ, "directOutsSkipGroupedTracks");
		if (directOutsSkipGroupedTracksJ)
			directOutsSkipGroupedTracks = json_integer_value(directOutsSkipGroupedTracksJ);
		
		// auxSendsMode
		json_t *auxSendsModeJ = json_object_get(rootJ, "auxSendsMode");
		if (auxSendsModeJ)
			auxSendsMode = json_integer_value(auxSendsModeJ);
		
		// groupsControlTrackSendLevels
		json_t *groupsControlTrackSendLevelsJ = json_object_get(rootJ, "groupsControlTrackSendLevels");
		if (groupsControlTrackSendLevelsJ)
			groupsControlTrackSendLevels = json_integer_value(groupsControlTrackSendLevelsJ);
		
		// auxReturnsMutedWhenMainSolo
		json_t *auxReturnsMutedWhenMainSoloJ = json_object_get(rootJ, "auxReturnsMutedWhenMainSolo");
		if (auxReturnsMutedWhenMainSoloJ)
			auxReturnsMutedWhenMainSolo = json_integer_value(auxReturnsMutedWhenMainSoloJ);
		
		// auxReturnsSolosMuteDry
		json_t *auxReturnsSolosMuteDryJ = json_object_get(rootJ, "auxReturnsSolosMuteDry");
		if (auxReturnsSolosMuteDryJ)
			auxReturnsSolosMuteDry = json_integer_value(auxReturnsSolosMuteDryJ);
		
		// chainMode
		json_t *chainModeJ = json_object_get(rootJ, "chainMode");
		if (chainModeJ)
			chainMode = json_integer_value(chainModeJ);
		
		// colorAndCloak
		json_t *colorAndCloakJ = json_object_get(rootJ, "colorAndCloak");
		if (colorAndCloakJ)
			colorAndCloak.cc1 = json_integer_value(colorAndCloakJ);
		
		// symmetricalFade
		json_t *symmetricalFadeJ = json_object_get(rootJ, "symmetricalFade");
		if (symmetricalFadeJ)
			symmetricalFade = json_is_true(symmetricalFadeJ);

		// fadeCvOutsWithVolCv
		json_t *fadeCvOutsWithVolCvJ = json_object_get(rootJ, "fadeCvOutsWithVolCv");
		if (fadeCvOutsWithVolCvJ)
			fadeCvOutsWithVolCv = json_is_true(fadeCvOutsWithVolCvJ);

		// linkBitMask
		json_t *linkBitMaskJ = json_object_get(rootJ, "linkBitMask");
		if (linkBitMaskJ) {
			unsigned long newLinkBitMask = json_integer_value(linkBitMaskJ);
			if (N_TRK == nTrkSrc) {
				linkBitMask = newLinkBitMask;
			}
			else {
				// code below assumes when 8 track mixer has 2 groups and 16 track mixer has 4 groups
				if (N_TRK == 8) {// nTrkSrc is automatically 16, so moving from 16 down to 8
					linkBitMask = ( (newLinkBitMask & 0xFF) | ((newLinkBitMask & 0x30000) >> 8) );
				}
				else {// nTrkSrc is automatically 8, so moving from 8 up to 16
					linkBitMask &= 0xCFF00;// kill bits in sr that correspond to jr's bits
					linkBitMask |= ( (newLinkBitMask & 0xFF) | ((newLinkBitMask & 0x300) << 8) );
				}
			}
		}
		
		// filterPos
		json_t *filterPosJ = json_object_get(rootJ, "filterPos");
		if (filterPosJ)
			filterPos = json_integer_value(filterPosJ);
		
		// groupedAuxReturnFeedbackProtection
		json_t *groupedAuxReturnFeedbackProtectionJ = json_object_get(rootJ, "groupedAuxReturnFeedbackProtection");
		if (groupedAuxReturnFeedbackProtectionJ)
			groupedAuxReturnFeedbackProtection = json_integer_value(groupedAuxReturnFeedbackProtectionJ);
		
		// ecoMode
		json_t *ecoModeJ = json_object_get(rootJ, "ecoMode");
		if (ecoModeJ)
			ecoMode = json_integer_value(ecoModeJ);
		
		// faders (populate linkedFaderReloadValues)
		json_t *fadersJ = json_object_get(rootJ, "faders");
		if (fadersJ && N_TRK == nTrkSrc) {
			// for (int trkOrGrp = 0; trkOrGrp < (N_TRK + N_GRP); trkOrGrp++) {
				// json_t *fadersArrayJ = json_array_get(fadersJ, trkOrGrp);
				// if (fadersArrayJ)
					// linkedFaderReloadValues[trkOrGrp] = json_number_value(fadersArrayJ);
			// }

			// tracks
			for (int trk = 0; trk < std::min(N_TRK, nTrkSrc); trk++) {
				json_t *fadersArrayJ = json_array_get(fadersJ, trk);
				if (fadersArrayJ)
					linkedFaderReloadValues[trk] = json_number_value(fadersArrayJ);
			}
			// groups
			for (int grp = 0; grp < std::min(N_GRP, nGrpSrc); grp++) {
				json_t *fadersArrayJ = json_array_get(fadersJ, nTrkSrc + grp);
				if (fadersArrayJ)
					linkedFaderReloadValues[N_TRK + grp] = json_number_value(fadersArrayJ);
			}
		}
		else {// legacy or mixer interchange of different sizes (16 <-> 8 track)
			for (int trkOrGrp = 0; trkOrGrp < (N_TRK + N_GRP); trkOrGrp++) {
				linkedFaderReloadValues[trkOrGrp] = paFade[trkOrGrp].getValue();
			}
		}
		
		// momentaryCvButtons
		json_t *momentaryCvButtonsJ = json_object_get(rootJ, "momentaryCvButtons");
		if (momentaryCvButtonsJ)
			directOutPanStereoMomentCvLinearVol.cc4[2] = json_integer_value(momentaryCvButtonsJ);
		
		// masterFaderScalesSends
		json_t *masterFaderScalesSendsJ = json_object_get(rootJ, "masterFaderScalesSends");
		if (masterFaderScalesSendsJ)
			masterFaderScalesSends = json_integer_value(masterFaderScalesSendsJ);
		
		// linearVolCvInputs
		json_t *linearVolCvInputsJ = json_object_get(rootJ, "linearVolCvInputs");
		if (linearVolCvInputsJ)
			directOutPanStereoMomentCvLinearVol.cc4[3] = json_integer_value(linearVolCvInputsJ);
		
		// extern must call resetNonJson()
	}	
		
};// struct GlobalInfo


//*****************************************************************************

struct MixerMaster {
	// Constants
	
	// need to save, no reset
	// none
	
	// need to save, with reset
	bool dcBlock;
	int clipping; // 0 is soft, 1 is hard (must be single ls bit)
	float fadeRate; // mute when < minFadeRate, fade when >= minFadeRate. This is actually the fade time in seconds
	float fadeProfile; // exp when +1, lin when 0, log when -1
	int8_t vuColorThemeLocal;
	int8_t dispColorLocal;
	int8_t chainOnly;
	float dimGain;// slider uses this gain, but displays it in dB instead of linear
	char masterLabel[7];
	
	// no need to save, with reset
	private:
	simd::float_4 chainGainsAndMute;// 0=L, 1=R, 2=mute
	simd::float_4 gainMatrix;// L, R, RinL, LinR (used for fader-mono block)
	float volCv;
	public:
	TSlewLimiterSingle<simd::float_4> gainMatrixSlewers;
	TSlewLimiterSingle<simd::float_4> chainGainAndMuteSlewers;// chain gains are [0] and [1], mute is [2], unused is [3]
	private:
	FirstOrderStereoFilter dcBlockerStereo;// 6dB/oct
	public:
	VuMeterAllDual vu;// use mix[0..1]
	float fadeGain; // target of this gain is the value of the mute/fade button's param (i.e. 0.0f or 1.0f)
	float target;// used detect button press (needed to reset fadeGainXr and VUs)
	float fadeGainX;// absolute X value of fade, between 0.0f and 1.0f (for symmetrical fade)
	float fadeGainXr;// reset X value of fade, between 0.0f and 1.0f (for asymmetrical fade)
	float fadeGainScaled;
	float paramWithCV;
	float dimGainIntegerDB;// corresponds to dimGain, converted to dB, then rounded, then back to linear

	// no need to save, no reset
	GlobalInfo *gInfo;
	Param *params;
	Input *inChain;
	Input *inVol;

	float calcFadeGain() {return params[MAIN_MUTE_PARAM].getValue() >= 0.5f ? 0.0f : 1.0f;}
	bool isFadeMode() {return fadeRate >= GlobalConst::minFadeRate;}


	void construct(GlobalInfo *_gInfo, Param *_params, Input *_inputs) {
		gInfo = _gInfo;
		params = _params;
		inChain = &_inputs[CHAIN_INPUTS];
		inVol = &_inputs[GRPM_MUTESOLO_INPUT];
		gainMatrixSlewers.setRiseFall(simd::float_4(GlobalConst::antipopSlewSlow)); // slew rate is in input-units per second (ex: V/s)
		chainGainAndMuteSlewers.setRiseFall(simd::float_4(GlobalConst::antipopSlewFast)); // slew rate is in input-units per second (ex: V/s)
		dcBlockerStereo.setParameters(true, 0.1f);
	}


	void onReset() {
		dcBlock = false;
		clipping = 0;
		fadeRate = 0.0f;
		fadeProfile = 0.0f;
		vuColorThemeLocal = 0;
		dispColorLocal = 0;
		chainOnly = 0;
		dimGain = 0.25119f;// 0.1 = -20 dB, 0.25119 = -12 dB
		snprintf(masterLabel, 7, "MASTER");
		resetNonJson();
	}


	void resetNonJson() {
		chainGainsAndMute = 0.0f;
		gainMatrix = 0.0f;
		volCv = 0.0f;
		gainMatrixSlewers.reset();
		chainGainAndMuteSlewers.reset();
		setupDcBlocker();
		dcBlockerStereo.reset();
		vu.reset();
		fadeGain = calcFadeGain();
		target = fadeGain;
		fadeGainX = fadeGain;
		fadeGainXr = 0.0f;
		fadeGainScaled = fadeGain;// no pow needed here since 0.0f or 1.0f
		paramWithCV = -100.0f;
		dimGainIntegerDB = calcDimGainIntegerDB(dimGain);
	}


	void dataToJson(json_t *rootJ) {
		// dcBlock
		json_object_set_new(rootJ, "dcBlock", json_boolean(dcBlock));

		// clipping
		json_object_set_new(rootJ, "clipping", json_integer(clipping));
		
		// fadeRate
		json_object_set_new(rootJ, "fadeRate", json_real(fadeRate));
		
		// fadeProfile
		json_object_set_new(rootJ, "fadeProfile", json_real(fadeProfile));
		
		// vuColorThemeLocal
		json_object_set_new(rootJ, "vuColorThemeLocal", json_integer(vuColorThemeLocal));
		
		// dispColorLocal
		json_object_set_new(rootJ, "dispColorLocal", json_integer(dispColorLocal));
		
		// chainOnly
		json_object_set_new(rootJ, "chainOnly", json_integer(chainOnly));
		
		// dimGain
		json_object_set_new(rootJ, "dimGain", json_real(dimGain));
		
		// masterLabel
		json_object_set_new(rootJ, "masterLabel", json_string(masterLabel));
	}


	void dataFromJson(json_t *rootJ) {
		// dcBlock
		json_t *dcBlockJ = json_object_get(rootJ, "dcBlock");
		if (dcBlockJ)
			dcBlock = json_is_true(dcBlockJ);
		
		// clipping
		json_t *clippingJ = json_object_get(rootJ, "clipping");
		if (clippingJ)
			clipping = json_integer_value(clippingJ);
		
		// fadeRate
		json_t *fadeRateJ = json_object_get(rootJ, "fadeRate");
		if (fadeRateJ)
			fadeRate = json_number_value(fadeRateJ);
		
		// fadeProfile
		json_t *fadeProfileJ = json_object_get(rootJ, "fadeProfile");
		if (fadeProfileJ)
			fadeProfile = json_number_value(fadeProfileJ);
		
		// vuColorThemeLocal
		json_t *vuColorThemeLocalJ = json_object_get(rootJ, "vuColorThemeLocal");
		if (vuColorThemeLocalJ)
			vuColorThemeLocal = json_integer_value(vuColorThemeLocalJ);
		
		// dispColorLocal
		json_t *dispColorLocalJ = json_object_get(rootJ, "dispColorLocal");
		if (dispColorLocalJ)
			dispColorLocal = json_integer_value(dispColorLocalJ);
		
		// chainOnly
		json_t *chainOnlyJ = json_object_get(rootJ, "chainOnly");
		if (chainOnlyJ)
			chainOnly = json_integer_value(chainOnlyJ);
		
		// dimGain
		json_t *dimGainJ = json_object_get(rootJ, "dimGain");
		if (dimGainJ)
			dimGain = json_number_value(dimGainJ);

		// masterLabel
		json_t *textJ = json_object_get(rootJ, "masterLabel");
		if (textJ)
			snprintf(masterLabel, 7, "%s", json_string_value(textJ));
		
		// extern must call resetNonJson()
	}			
	
	void setupDcBlocker() {
		float fc = 10.0f;// Hz
		fc *= gInfo->sampleTime;// fc is in normalized freq for rest of method
		dcBlockerStereo.setParameters(true, fc);
	}
	
	
	void onSampleRateChange() {
		setupDcBlocker();
	}
	
	
	// Soft-clipping polynomial function
	// piecewise portion that handles inputs between 6 and 12 V
	// unipolar only, caller must take care of signs
	// assumes that 6 <= x <= 12
	// assumes f(x) := x  when x < 6
	// assumes f(x) := 10  when x > 12
	//
	// Chosen polynomial:
	// f(x) := a + b*x + c*x^2 + d*x^3
	//
	// Coefficient solving constraints:
	// f(6) = 6
	// f(12) = 10
	// f'(6) = 1 (unity slope at 6V)
	// f'(12) = 0 (horizontal at 12V)
	// where:
	// f'(x) := b + 2*c*x + 3*d*x^2
	// 
	// solve(system(f(6)=6,f(12)=10,f'(6)=1,f'(12)=0),{a,b,c,d})
	// 
	// solution:
	// a=2 and b=0 and c=(1/6) and d=(-1/108)
	float clipPoly(float inX) {
		return 2.0f + inX * inX * (1.0f/6.0f - inX * (1.0f/108.0f));
	}
	
	float clip(float inX) {// 0 = soft, 1 = hard
		if (inX <= 6.0f && inX >= -6.0f) {
			return inX;
		}
		if (clipping == 1) {// hard clip
			return clamp(inX, -10.0f, 10.0f);
		}
		// here clipping is 0, so do soft clip
		inX = clamp(inX, -12.0f, 12.0f);
		if (inX >= 0.0f)
			return clipPoly(inX);
		return -clipPoly(-inX);
	}


	void updateSlowValues() {		
		// calc ** chainGains **
		chainGainsAndMute[0] = inChain[0].isConnected() ? 1.0f : 0.0f;
		chainGainsAndMute[1] = inChain[1].isConnected() ? 1.0f : 0.0f;
	}
	
	
	void process(float *mix, bool eco) {// master
		// takes mix[0..1] and redeposits post in same place
		
		if (chainOnly != 0) {
			mix[0] = 0.0f;
			mix[1] = 0.0f;
		}
		
		if (eco) {
			// calc ** fadeGain, fadeGainX, fadeGainXr, target, fadeGainScaled **
			float newTarget = calcFadeGain();
			if (newTarget != target) {
				fadeGainXr = 0.0f;
				target = newTarget;
				vu.reset();
			}
			if (fadeGain != target) {
				if (isFadeMode()) {
					float deltaX = (gInfo->sampleTime / fadeRate) * (1 + (gInfo->ecoMode & 0x3));// last value is sub refresh
					fadeGain = updateFadeGain(fadeGain, target, &fadeGainX, &fadeGainXr, deltaX, fadeProfile, gInfo->symmetricalFade);
					fadeGainScaled = std::pow(fadeGain, GlobalConst::masterFaderScalingExponent);
				}
				else {// we are in mute mode
					fadeGain = target;
					fadeGainX = target;
					fadeGainScaled = target;// no pow needed here since 0.0f or 1.0f
				}	
			}
			// dim (this affects fadeGainScaled only, so treated like a partial mute, but no effect on fade pointers or other just effect on sound
			chainGainsAndMute[2] = fadeGainScaled;
			if (params[MAIN_DIM_PARAM].getValue() >= 0.5f) {
				chainGainsAndMute[2] *= dimGainIntegerDB;
			}
			
			// calc ** fader, paramWithCV **
			float fader = params[MAIN_FADER_PARAM].getValue();
			if (inVol->isConnected() && inVol->getChannels() >= (N_GRP * 2 + 4)) {
				volCv = clamp(inVol->getVoltage(N_GRP * 2 + 4 - 1) * 0.1f, 0.0f, 1.0f);
				paramWithCV = fader * volCv;
				if (gInfo->directOutPanStereoMomentCvLinearVol.cc4[3] == 0) {
					fader = paramWithCV;
				}			
			}
			else {
				volCv = 1.0f;
				paramWithCV = -100.0f;
			}

			// scaling
			fader = std::pow(fader, GlobalConst::masterFaderScalingExponent);
			
			// calc ** gainMatrix **
			// mono
			if (params[MAIN_MONO_PARAM].getValue() >= 0.5f) {
				gainMatrix = simd::float_4(0.5f * fader);
			}
			else {
				gainMatrix = simd::float_4(fader, fader, 0.0f, 0.0f);
			}
		}
		
		// Calc gains for chain input (with antipop when signal connect, impossible for disconnect)
		if (movemask(chainGainsAndMute == chainGainAndMuteSlewers.out) != 0xF) {// movemask returns 0xF when 4 floats are equal
			chainGainAndMuteSlewers.process(gInfo->sampleTime, chainGainsAndMute);
		}
		
		// Chain inputs when pre master
		if (gInfo->chainMode == 0) {
			if (chainGainAndMuteSlewers.out[0] != 0.0f) {
				mix[0] += inChain[0].getVoltage() * chainGainAndMuteSlewers.out[0];
			}
			if (chainGainAndMuteSlewers.out[1] != 0.0f) {
				mix[1] += inChain[1].getVoltage() * chainGainAndMuteSlewers.out[1];
			}
		}

		// Calc master gain with slewer and apply it
		simd::float_4 sigs(mix[0], mix[1], mix[1], mix[0]);// L, R, RinL, LinR
		if (movemask(gainMatrix == gainMatrixSlewers.out) != 0xF) {// movemask returns 0xF when 4 floats are equal
			gainMatrixSlewers.process(gInfo->sampleTime, gainMatrix);
		}
		sigs = sigs * gainMatrixSlewers.out;
		sigs[0] += sigs[2];// pre mute, do not change VU needs
		sigs[1] += sigs[3];// pre mute, do not change VU needs
		if (gInfo->directOutPanStereoMomentCvLinearVol.cc4[3] != 0) {
			sigs[0] *= volCv;
			sigs[1] *= volCv;
		}		
		
		// Set mix
		mix[0] = sigs[0] * chainGainAndMuteSlewers.out[2];
		mix[1] = sigs[1] * chainGainAndMuteSlewers.out[2];
		
		// VUs (no cloaked mode for master, always on)
		if (eco) {
			float sampleTimeEco = gInfo->sampleTime * (1 + (gInfo->ecoMode & 0x3));
			vu.process(sampleTimeEco, fadeGainScaled == 0.0f ? &sigs[0] : mix);
		}
				
		// Chain inputs when post master
		if (gInfo->chainMode == 1) {
			if (chainGainAndMuteSlewers.out[0] != 0.0f) {
				mix[0] += inChain[0].getVoltage() * chainGainAndMuteSlewers.out[0];
			}
			if (chainGainAndMuteSlewers.out[1] != 0.0f) {
				mix[1] += inChain[1].getVoltage() * chainGainAndMuteSlewers.out[1];
			}
		}
		
		// DC blocker (post VU)
		if (dcBlock) {
			dcBlockerStereo.process(mix, mix);
		}
		
		// Clipping (post VU, so that we can see true range)
		mix[0] = clip(mix[0]);
		mix[1] = clip(mix[1]);
	}		
};// struct MixerMaster



//*****************************************************************************


struct MixerGroup {
	// Constants
	// none
	
	// need to save, no reset
	// none
	
	// need to save, with reset
	char  *groupName;// write 4 chars always (space when needed), no null termination since all tracks names are concat and just one null at end of all
	float gainAdjust;// this is a gain here (not dB)
	float* fadeRate; // mute when < minFadeRate, fade when >= minFadeRate. This is actually the fade time in seconds
	float fadeProfile; // exp when +1, lin when 0, log when -1
	int8_t directOutsMode;// when per track
	int8_t auxSendsMode;// when per track
	int8_t panLawStereo;// when per track
	int8_t vuColorThemeLocal;
	int8_t filterPos;// 0 = pre insert, 1 = post insert, 2 = per track
	int8_t dispColorLocal;
	float panCvLevel;// 0 to 1.0f
	float stereoWidth;// 0 to 1.0f; 0 is mono, 1 is stereo, 2 is 200% stereo widening

	// no need to save, with reset
	TSlewLimiterSingle<simd::float_4> gainMatrixSlewers;
	SlewLimiterSingle stereoWidthSlewer;
	SlewLimiterSingle muteSoloGainSlewer;
	private:
	ButterworthThirdOrder hpFilter[2];// 18dB/oct
	ButterworthSecondOrder lpFilter[2];// 12db/oct
	float lastHpfCutoff;
	float lastLpfCutoff;
	simd::float_4 panMatrix;
	simd::float_4 gainMatrix;	
	float volCv;
	float oldPan;
	PackedBytes4 oldPanSignature;// [0] is pan stereo local, [1] is pan stereo global, [2] is pan mono global
	public:
	VuMeterAllDual vu;// use post[]
	float fadeGain; // target of this gain is the value of the mute/fade button's param (i.e. 0.0f or 1.0f)
	float target;
	float fadeGainX;
	float fadeGainXr;
	float fadeGainScaled;
	float paramWithCV;
	float pan;
	bool panCvConnected;
	
	// no need to save, no reset
	int groupNum;// 0 to 3 (1)
	std::string ids;
	GlobalInfo *gInfo;
	Input *inInsert;
	Input *inVol;
	Input *inPan;
	Param *paFade;
	Param *paMute;
	Param *paSolo;
	Param *paPan;
	Param *paHpfCutoff;
	Param *paLpfCutoff;
	float *taps;// [0],[1]: pre-insert L R; [32][33]: pre-fader L R, [64][65]: post-fader L R, [96][97]: post-mute-solo L R
	float *insertOuts;// [0][1]: insert outs for this group


	float calcFadeGain() {return paMute->getValue() >= 0.5f ? 0.0f : 1.0f;}
	bool isFadeMode() {return *fadeRate >= GlobalConst::minFadeRate;}


	void construct(int _groupNum, GlobalInfo *_gInfo, Input *_inputs, Param *_params, char* _groupName, float* _taps, float* _insertOuts) {
		groupNum = _groupNum;
		ids = "id_g" + std::to_string(groupNum) + "_";
		gInfo = _gInfo;
		inInsert = &_inputs[INSERT_GRP_AUX_INPUT];
		inVol = &_inputs[GROUP_VOL_INPUTS + groupNum];
		inPan = &_inputs[GROUP_PAN_INPUTS + groupNum];
		paFade = &_params[GROUP_FADER_PARAMS + groupNum];
		paMute = &_params[GROUP_MUTE_PARAMS + groupNum];
		paSolo = &_params[GROUP_SOLO_PARAMS + groupNum];
		paPan = &_params[GROUP_PAN_PARAMS + groupNum];
		paHpfCutoff = &_params[GROUP_HPCUT_PARAMS + groupNum];
		paLpfCutoff = &_params[GROUP_LPCUT_PARAMS + groupNum];
		groupName = _groupName;
		taps = _taps;
		insertOuts = _insertOuts;
		fadeRate = &(_gInfo->fadeRates[N_TRK + groupNum]);
		gainMatrixSlewers.setRiseFall(simd::float_4(GlobalConst::antipopSlewSlow)); // slew rate is in input-units per second (ex: V/s)
		stereoWidthSlewer.setRiseFall(GlobalConst::antipopSlewFast); // slew rate is in input-units per second (ex: V/s)
		muteSoloGainSlewer.setRiseFall(GlobalConst::antipopSlewFast); // slew rate is in input-units per second (ex: V/s)
		for (int i = 0; i < 2; i++) {
			hpFilter[i].setParameters(true, 0.1f);
			lpFilter[i].setParameters(false, 0.4f);
		}
	}


	void onReset() {
		snprintf(groupName, 4, "GRP"); groupName[3] = 0x30 + (char)groupNum + 1;		
		gainAdjust = 1.0f;
		*fadeRate = 0.0f;
		fadeProfile = 0.0f;
		directOutsMode = 3;// post-solo should be default
		auxSendsMode = 3;// post-solo should be default
		panLawStereo = 1;
		vuColorThemeLocal = 0;
		filterPos = 1;// default is post-insert
		dispColorLocal = 0;
		panCvLevel = 1.0f;
		stereoWidth = 1.0f;
		resetNonJson();
	}


	void resetNonJson() {
		panMatrix = 0.0f;
		gainMatrix = 0.0f;
		volCv = 0.0f;
		gainMatrixSlewers.reset();
		muteSoloGainSlewer.reset();
		setHPFCutoffFreq(paHpfCutoff->getValue());// off
		setLPFCutoffFreq(paLpfCutoff->getValue());// off
		// lastHpfCutoff; automatically set in setHPFCutoffFreq()
		// lastLpfCutoff; automatically set in setLPFCutoffFreq()
		for (int i = 0; i < 2; i++) {
			hpFilter[i].reset();
			lpFilter[i].reset();
		}
		oldPan = -10.0f;
		oldPanSignature.cc1 = 0xFFFFFFFF;
		vu.reset();
		fadeGain = calcFadeGain();
		target = fadeGain;
		fadeGainX = fadeGain;
		fadeGainXr = 0.0f;
		fadeGainScaled = fadeGain;// no pow needed here since 0.0f or 1.0f
		paramWithCV = -100.0f;
		pan = 0.5f;
		panCvConnected = false;
	}


	void dataToJson(json_t *rootJ) {
		// groupName
		// saved elsewhere
		
		// gainAdjust
		json_object_set_new(rootJ, (ids + "gainAdjust").c_str(), json_real(gainAdjust));
		
		// fadeRate
		json_object_set_new(rootJ, (ids + "fadeRate").c_str(), json_real(*fadeRate));
		
		// fadeProfile
		json_object_set_new(rootJ, (ids + "fadeProfile").c_str(), json_real(fadeProfile));
		
		// directOutsMode
		json_object_set_new(rootJ, (ids + "directOutsMode").c_str(), json_integer(directOutsMode));
		
		// auxSendsMode
		json_object_set_new(rootJ, (ids + "auxSendsMode").c_str(), json_integer(auxSendsMode));
		
		// panLawStereo
		json_object_set_new(rootJ, (ids + "panLawStereo").c_str(), json_integer(panLawStereo));

		// vuColorThemeLocal
		json_object_set_new(rootJ, (ids + "vuColorThemeLocal").c_str(), json_integer(vuColorThemeLocal));

		// filterPos
		json_object_set_new(rootJ, (ids + "filterPos").c_str(), json_integer(filterPos));

		// dispColorLocal
		json_object_set_new(rootJ, (ids + "dispColorLocal").c_str(), json_integer(dispColorLocal));
		
		// panCvLevel
		json_object_set_new(rootJ, (ids + "panCvLevel").c_str(), json_real(panCvLevel));

		// stereoWidth
		json_object_set_new(rootJ, (ids + "stereoWidth").c_str(), json_real(stereoWidth));
	}


	void dataFromJson(json_t *rootJ) {
		// groupName 
		// loaded elsewhere

		// gainAdjust
		json_t *gainAdjustJ = json_object_get(rootJ, (ids + "gainAdjust").c_str());
		if (gainAdjustJ)
			gainAdjust = json_number_value(gainAdjustJ);
		
		// fadeRate
		json_t *fadeRateJ = json_object_get(rootJ, (ids + "fadeRate").c_str());
		if (fadeRateJ)
			*fadeRate = json_number_value(fadeRateJ);
		
		// fadeProfile
		json_t *fadeProfileJ = json_object_get(rootJ, (ids + "fadeProfile").c_str());
		if (fadeProfileJ)
			fadeProfile = json_number_value(fadeProfileJ);

		// directOutsMode
		json_t *directOutsModeJ = json_object_get(rootJ, (ids + "directOutsMode").c_str());
		if (directOutsModeJ)
			directOutsMode = json_integer_value(directOutsModeJ);
		
		// auxSendsMode
		json_t *auxSendsModeJ = json_object_get(rootJ, (ids + "auxSendsMode").c_str());
		if (auxSendsModeJ)
			auxSendsMode = json_integer_value(auxSendsModeJ);
		
		// panLawStereo
		json_t *panLawStereoJ = json_object_get(rootJ, (ids + "panLawStereo").c_str());
		if (panLawStereoJ)
			panLawStereo = json_integer_value(panLawStereoJ);
		
		// vuColorThemeLocal
		json_t *vuColorThemeLocalJ = json_object_get(rootJ, (ids + "vuColorThemeLocal").c_str());
		if (vuColorThemeLocalJ)
			vuColorThemeLocal = json_integer_value(vuColorThemeLocalJ);
		
		// filterPos
		json_t *filterPosJ = json_object_get(rootJ, (ids + "filterPos").c_str());
		if (filterPosJ)
			filterPos = json_integer_value(filterPosJ);
		
		// dispColorLocal
		json_t *dispColorLocalJ = json_object_get(rootJ, (ids + "dispColorLocal").c_str());
		if (dispColorLocalJ)
			dispColorLocal = json_integer_value(dispColorLocalJ);
		
		// panCvLevel
		json_t *panCvLevelJ = json_object_get(rootJ, (ids + "panCvLevel").c_str());
		if (panCvLevelJ)
			panCvLevel = json_number_value(panCvLevelJ);
		
		// stereoWidth
		json_t *stereoWidthJ = json_object_get(rootJ, (ids + "stereoWidth").c_str());
		if (stereoWidthJ)
			stereoWidth = json_number_value(stereoWidthJ);
		
		// extern must call resetNonJson()
	}	

	void setHPFCutoffFreq(float fc) {
		paHpfCutoff->setValue(fc);
		lastHpfCutoff = fc;
		fc *= gInfo->sampleTime;// fc is in normalized freq for rest of method
		hpFilter[0].setParameters(true, fc);
		hpFilter[1].setParameters(true, fc);
	}
	float getHPFCutoffFreq() {return paHpfCutoff->getValue();}
	
	void setLPFCutoffFreq(float fc) {
		paLpfCutoff->setValue(fc);
		lastLpfCutoff = fc;
		fc *= gInfo->sampleTime;// fc is in normalized freq for rest of method
		lpFilter[0].setParameters(false, fc);
		lpFilter[1].setParameters(false, fc);
	}
	float getLPFCutoffFreq() {return paLpfCutoff->getValue();}

		
	void onSampleRateChange() {
		setHPFCutoffFreq(paHpfCutoff->getValue());
		setLPFCutoffFreq(paLpfCutoff->getValue());
	}

	
	void updateSlowValues() {
		// filters
		if (paHpfCutoff->getValue() != lastHpfCutoff) {
			setHPFCutoffFreq(paHpfCutoff->getValue());
		}
		if (paLpfCutoff->getValue() != lastLpfCutoff) {
			setLPFCutoffFreq(paLpfCutoff->getValue());
		}

		// ** process linked **
		gInfo->processLinked(N_TRK + groupNum, paFade->getValue());
		
		// ** detect pan mode change ** (and trigger recalc of panMatrix)
		PackedBytes4 newPanSig;
		newPanSig.cc4[0] = panLawStereo;
		newPanSig.cc4[1] = gInfo->directOutPanStereoMomentCvLinearVol.cc4[1];
		newPanSig.cc4[2] = gInfo->panLawMono;
		newPanSig.cc4[3] = 0xFF;
		if (newPanSig.cc1 != oldPanSignature.cc1) {
			oldPan = -10.0f;
			oldPanSignature.cc1 = newPanSig.cc1;
		}
	}
	

	void process(float *mix, bool eco) {// group
		// Tap[0],[1]: pre-insert (group inputs)
		// already set up by the mix master, so only stereo width to apply
		
		// Stereo width
		if (stereoWidth != stereoWidthSlewer.out) {
			stereoWidthSlewer.process(gInfo->sampleTime, stereoWidth);
		}
		if (stereoWidthSlewer.out != 1.0f) {
			applyStereoWidth(stereoWidthSlewer.out, &taps[0], &taps[1]);
		}

		
		// Tap[8],[9]: pre-fader (inserts and filters)
		// NEW VERSION
		if (gInfo->filterPos == 1 || (gInfo->filterPos == 2 && filterPos == 1)) {// if filters post insert
			// Insert outputs
			insertOuts[0] = taps[0];
			insertOuts[1] = taps[1];
			
			// Insert inputs
			if (inInsert->isConnected()) {
				taps[N_GRP * 2 + 0] = clamp20V(inInsert->getVoltage((groupNum << 1) + 0));
				taps[N_GRP * 2 + 1] = clamp20V(inInsert->getVoltage((groupNum << 1) + 1));
			}
			else {
				taps[N_GRP * 2 + 0] = taps[0];
				taps[N_GRP * 2 + 1] = taps[1];
			}

			// Filters
			// HPF
			if (getHPFCutoffFreq() >= GlobalConst::minHPFCutoffFreq) {
				taps[N_GRP * 2 + 0] = hpFilter[0].process(taps[N_GRP * 2 + 0]);
				taps[N_GRP * 2 + 1] = hpFilter[1].process(taps[N_GRP * 2 + 1]);
			}
			// LPF
			if (getLPFCutoffFreq() <= GlobalConst::maxLPFCutoffFreq) {
				taps[N_GRP * 2 + 0] = lpFilter[0].process(taps[N_GRP * 2 + 0]);
				taps[N_GRP * 2 + 1] = lpFilter[1].process(taps[N_GRP * 2 + 1]);
			}
		}
		else {// filters before inserts
			taps[N_GRP * 2 + 0] = taps[0];
			taps[N_GRP * 2 + 1] = taps[1];
			// Filters
			// HPF
			if (getHPFCutoffFreq() >= GlobalConst::minHPFCutoffFreq) {
				taps[N_GRP * 2 + 0] = hpFilter[0].process(taps[N_GRP * 2 + 0]);
				taps[N_GRP * 2 + 1] = hpFilter[1].process(taps[N_GRP * 2 + 1]);
			}
			// LPF
			if (getLPFCutoffFreq() <= GlobalConst::maxLPFCutoffFreq) {
				taps[N_GRP * 2 + 0] = lpFilter[0].process(taps[N_GRP * 2 + 0]);
				taps[N_GRP * 2 + 1] = lpFilter[1].process(taps[N_GRP * 2 + 1]);
			}
			
			// Insert outputs
			insertOuts[0] = taps[N_GRP * 2 + 0];
			insertOuts[1] = taps[N_GRP * 2 + 1];
			
			// Insert inputs
			if (inInsert->isConnected()) {
				taps[N_GRP * 2 + 0] = clamp20V(inInsert->getVoltage((groupNum << 1) + 0));
				taps[N_GRP * 2 + 1] = clamp20V(inInsert->getVoltage((groupNum << 1) + 1));
			}
		}// filterPos


		if (eco) {	
			// calc ** fadeGain, fadeGainX, fadeGainXr, target, fadeGainScaled **
			float newTarget = calcFadeGain();
			if (newTarget != target) {
				fadeGainXr = 0.0f;
				if (isFadeMode()) {
					gInfo->fadeOtherLinkedTracks(groupNum + N_TRK, newTarget);
				}
				target = newTarget;
				vu.reset();
			}
			if (fadeGain != target) {
				if (isFadeMode()) {
					float deltaX = (gInfo->sampleTime / *fadeRate) * (1 + (gInfo->ecoMode & 0x3));// last value is sub refresh
					fadeGain = updateFadeGain(fadeGain, target, &fadeGainX, &fadeGainXr, deltaX, fadeProfile, gInfo->symmetricalFade);
					fadeGainScaled = std::pow(fadeGain, GlobalConst::trkAndGrpFaderScalingExponent);
				}
				else {// we are in mute mode
					fadeGain = target;
					fadeGainX = target;
					fadeGainScaled = target;// no pow needed here since 0.0f or 1.0f
				}
			}

			// calc ** fader, paramWithCV **
			float fader = paFade->getValue();
			if (inVol->isConnected()) {
				volCv = clamp(inVol->getVoltage() * 0.1f, 0.0f, 1.0f);
				paramWithCV = fader * volCv;
				if (gInfo->directOutPanStereoMomentCvLinearVol.cc4[3] == 0) {
					fader = paramWithCV;
				}
			}
			else {
				volCv = 1.0f;
				paramWithCV = -100.0f;
			}

			// calc ** pan **
			pan = paPan->getValue();
			panCvConnected = inPan->isConnected();
			if (panCvConnected) {
				pan += inPan->getVoltage() * 0.1f * panCvLevel;// CV is a -5V to +5V input
				pan = clamp(pan, 0.0f, 1.0f);
			}

			// calc ** panMatrix **
			if (pan != oldPan) {
				panMatrix = 0.0f;// L, R, RinL, LinR (used for fader-pan block)
				if (pan == 0.5f) {
					panMatrix[1] = 1.0f;
					panMatrix[0] = 1.0f;
				}
				else {		
					// implicitly stereo for groups
					int stereoPanMode = (gInfo->directOutPanStereoMomentCvLinearVol.cc4[1] < 3 ? gInfo->directOutPanStereoMomentCvLinearVol.cc4[1] : panLawStereo);
					if (stereoPanMode == 0) {
						// Stereo balance linear, (+0 dB), same as mono No compensation
						panMatrix[1] = std::min(1.0f, pan * 2.0f);
						panMatrix[0] = std::min(1.0f, 2.0f - pan * 2.0f);
					}
					else if (stereoPanMode == 1) {
						// Stereo balance equal power (+3dB), same as mono Equal power
						sinCosSqrt2(&panMatrix[1], &panMatrix[0], pan * float(M_PI_2));
					}
					else {
						// True panning, equal power
						if (pan > 0.5f) {
							panMatrix[1] = 1.0f;
							panMatrix[2] = 0.0f;
							sinCos(&panMatrix[3], &panMatrix[0], (pan - 0.5f) * float(M_PI));
						}
						else {// must be < (not <= since = 0.5 is caught at above)
							sinCos(&panMatrix[1], &panMatrix[2], pan * float(M_PI));
							panMatrix[0] = 1.0f;
							panMatrix[3] = 0.0f;
						}
					}
				}
				oldPan = pan;
			}
			// calc ** gainMatrix **
			fader = std::pow(fader, GlobalConst::trkAndGrpFaderScalingExponent);// scaling
			gainMatrix = panMatrix * fader * gainAdjust;
		}
	
		// Calc group gains with slewer and apply it
		simd::float_4 sigs(taps[N_GRP * 2 + 0], taps[N_GRP * 2 + 1], taps[N_GRP * 2 + 1], taps[N_GRP * 2 + 0]);
		if (movemask(gainMatrix == gainMatrixSlewers.out) != 0xF) {// movemask returns 0xF when 4 floats are equal
			gainMatrixSlewers.process(gInfo->sampleTime, gainMatrix);
		}
		sigs = sigs * gainMatrixSlewers.out;
		taps[N_GRP * 4 + 0] = sigs[0] + sigs[2];
		taps[N_GRP * 4 + 1] = sigs[1] + sigs[3];
		if (gInfo->directOutPanStereoMomentCvLinearVol.cc4[3] != 0) {
			taps[N_GRP * 4 + 0] *= volCv;
			taps[N_GRP * 4 + 1] *= volCv;
		}

		// Calc muteSoloGainSlewed (solo not actually in here but in groups)
		if (fadeGainScaled != muteSoloGainSlewer.out) {
			muteSoloGainSlewer.process(gInfo->sampleTime, fadeGainScaled);
		}
		
		taps[N_GRP * 6 + 0] = taps[N_GRP * 4 + 0] * muteSoloGainSlewer.out;
		taps[N_GRP * 6 + 1] = taps[N_GRP * 4 + 1] * muteSoloGainSlewer.out;
			
		// Add to mix
		mix[0] += taps[N_GRP * 6 + 0];
		mix[1] += taps[N_GRP * 6 + 1];
		
		// VUs
		if (gInfo->colorAndCloak.cc4[cloakedMode] != 0) {
			vu.reset();
		}
		else if (eco) {
			float sampleTimeEco = gInfo->sampleTime * (1 + (gInfo->ecoMode & 0x3));
			vu.process(sampleTimeEco, &taps[N_GRP * (fadeGainScaled == 0.0f ? 4 : 6) + 0]);
		}
	}
};// struct MixerGroup



//*****************************************************************************


struct MixerTrack {
	// Constants
	// none
	
	// need to save, no reset
	// none
	
	// need to save, with reset
	char  *trackName;// write 4 chars always (space when needed), no null termination since all tracks names are concat and just one null at end of all	
	float gainAdjust;// this is a gain here (not dB)
	float* fadeRate; // mute when < minFadeRate, fade when >= minFadeRate. This is actually the fade time in seconds
	float fadeProfile; // exp when +1, lin when 0, log when -1
	int8_t directOutsMode;// when per track
	int8_t auxSendsMode;// when per track
	int8_t panLawStereo;// when per track
	int8_t vuColorThemeLocal;
	int8_t filterPos;// 0 = pre insert, 1 = post insert, 2 = per track
	int8_t dispColorLocal;
	int8_t polyStereo;// 0 = off (default), 1 = on
	float panCvLevel;// 0 to 1.0f
	float stereoWidth;// 0 to 1.0f; 0 is mono, 1 is stereo, 2 is 200% stereo widening
	int8_t invertInput;// 0 = off (default), 1 = on

	// no need to save, with reset
	bool stereo;// pan coefficients use this, so set up first
	private:
	float inGain;
	simd::float_4 panMatrix;
	simd::float_4 gainMatrix;	
	TSlewLimiterSingle<simd::float_4> gainMatrixSlewers;
	SlewLimiterSingle inGainSlewer;
	SlewLimiterSingle stereoWidthSlewer;
	SlewLimiterSingle muteSoloGainSlewer;
	ButterworthThirdOrder hpFilter[2];// 18dB/oct
	ButterworthSecondOrder lpFilter[2];// 12db/oct
	float lastHpfCutoff;
	float lastLpfCutoff;
	float oldPan;
	PackedBytes4 oldPanSignature;// [0] is pan stereo local, [1] is pan stereo global, [2] is pan mono global
	public:
	VuMeterAllDual vu;
	float fadeGain; // target of this gain is the value of the mute/fade button's param (i.e. 0.0f or 1.0f)
	float target;
	float fadeGainX;
	float fadeGainXr;
	float fadeGainScaled;
	float fadeGainScaledWithSolo;
	float paramWithCV;
	float pan;// this is set only in process() when eco, and also used only (elsewhere) in process() when eco
	bool panCvConnected;
	float volCv;
	float soloGain;

	// no need to save, no reset
	int trackNum;
	std::string ids;
	GlobalInfo *gInfo;
	Input *inSig;
	Input *inInsert;
	Input *inVol;
	Input *inVolTrack1;
	Input *inPan;
	Input *inPanTrack1;
	Param *paGroup;// 0.0 is no group (i.e. deposit post in mix[0..1]), 1.0 to 4.0 is group (i.e. deposit in groupTaps[2*g..2*g+1]. Always use setter since need to update gInfo!
	Param *paFade;
	Param *paMute;
	Param *paSolo;
	Param *paPan;
	Param *paHpfCutoff;
	Param *paLpfCutoff;
	float *taps;// [0],[1]: pre-insert L R; [32][33]: pre-fader L R, [64][65]: post-fader L R, [96][97]: post-mute-solo L R
	float* groupTaps;// [0..1] tap 0 of group 1, [1..2] tap 0 of group 2, etc.
	float *insertOuts;// [0][1]: insert outs for this track
	bool oldInUse = true;
	float fader = 0.0f;// this is set only in process() when eco, and also used only when eco in another section of this method


	float calcFadeGain() {return paMute->getValue() >= 0.5f ? 0.0f : 1.0f;}
	bool isFadeMode() {return *fadeRate >= GlobalConst::minFadeRate;}


	void construct(int _trackNum, GlobalInfo *_gInfo, Input *_inputs, Param *_params, char* _trackName, float* _taps, float* _groupTaps, float* _insertOuts) {
		trackNum = _trackNum;
		ids = "id_t" + std::to_string(trackNum) + "_";
		gInfo = _gInfo;
		inSig = &_inputs[TRACK_SIGNAL_INPUTS + 2 * trackNum + 0];
		inInsert = &_inputs[INSERT_TRACK_INPUTS];
		inVol = &_inputs[TRACK_VOL_INPUTS + trackNum];
		inVolTrack1 = &_inputs[TRACK_VOL_INPUTS + 0];
		inPan = &_inputs[TRACK_PAN_INPUTS + trackNum];
		inPanTrack1 = &_inputs[TRACK_PAN_INPUTS + 0];
		paGroup = &_params[GROUP_SELECT_PARAMS + trackNum];
		paFade = &_params[TRACK_FADER_PARAMS + trackNum];
		paMute = &_params[TRACK_MUTE_PARAMS + trackNum];
		paSolo = &_params[TRACK_SOLO_PARAMS + trackNum];
		paPan = &_params[TRACK_PAN_PARAMS + trackNum];
		paHpfCutoff = &_params[TRACK_HPCUT_PARAMS + trackNum];
		paLpfCutoff = &_params[TRACK_LPCUT_PARAMS + trackNum];
		trackName = _trackName;
		taps = _taps;
		groupTaps = _groupTaps;
		insertOuts = _insertOuts;
		fadeRate = &(_gInfo->fadeRates[trackNum]);
		gainMatrixSlewers.setRiseFall(simd::float_4(GlobalConst::antipopSlewSlow)); // slew rate is in input-units per second (ex: V/s)
		inGainSlewer.setRiseFall(GlobalConst::antipopSlewFast); // slew rate is in input-units per second (ex: V/s)
		stereoWidthSlewer.setRiseFall(GlobalConst::antipopSlewFast); // slew rate is in input-units per second (ex: V/s)
		muteSoloGainSlewer.setRiseFall(GlobalConst::antipopSlewFast); // slew rate is in input-units per second (ex: V/s)
		for (int i = 0; i < 2; i++) {
			hpFilter[i].setParameters(true, 0.1f);
			lpFilter[i].setParameters(false, 0.4f);
		}
	}


	void onReset() {
		snprintf(trackName, 4, "-%02u", (unsigned int)(trackNum + 1)); trackName[3] = '-';
		gainAdjust = 1.0f;
		*fadeRate = 0.0f;
		fadeProfile = 0.0f;
		directOutsMode = 3;// post-solo should be default
		auxSendsMode = 3;// post-solo should be default
		panLawStereo = 1;
		vuColorThemeLocal = 0;
		filterPos = 1;// default is post-insert
		dispColorLocal = 0;
		polyStereo = 0;
		panCvLevel = 1.0f;
		stereoWidth = 1.0f;
		invertInput = 0;
		resetNonJson();
	}


	void resetNonJson() {
		stereo = false;
		inGain = 0.0f;
		panMatrix = 0.0f;
		gainMatrix = 0.0f;
		gainMatrixSlewers.reset();
		inGainSlewer.reset();
		stereoWidthSlewer.reset();
		muteSoloGainSlewer.reset(); 
		setHPFCutoffFreq(paHpfCutoff->getValue());// off
		setLPFCutoffFreq(paLpfCutoff->getValue());// off
		// lastHpfCutoff; automatically set in setHPFCutoffFreq()
		// lastLpfCutoff; automatically set in setLPFCutoffFreq()
		for (int i = 0; i < 2; i++) {
			hpFilter[i].reset();
			lpFilter[i].reset();
		}
		oldPan = -10.0f;
		oldPanSignature.cc1 = 0xFFFFFFFF;
		vu.reset();
		fadeGain = calcFadeGain();
		target = fadeGain;
		fadeGainX = fadeGain;
		fadeGainXr = 0.0f;
		fadeGainScaled = fadeGain;// no pow needed here since 0.0f or 1.0f
		fadeGainScaledWithSolo = fadeGainScaled;
		paramWithCV = -100.0f;
		pan = 0.5f;
		panCvConnected = false;
		volCv = 1.0f;
		soloGain = 1.0f;
	}


	void dataToJson(json_t *rootJ) {
		// trackName 
		// saved elsewhere
		
		// gainAdjust
		json_object_set_new(rootJ, (ids + "gainAdjust").c_str(), json_real(gainAdjust));
		
		// fadeRate
		json_object_set_new(rootJ, (ids + "fadeRate").c_str(), json_real(*fadeRate));

		// fadeProfile
		json_object_set_new(rootJ, (ids + "fadeProfile").c_str(), json_real(fadeProfile));
		
		// directOutsMode
		json_object_set_new(rootJ, (ids + "directOutsMode").c_str(), json_integer(directOutsMode));
		
		// auxSendsMode
		json_object_set_new(rootJ, (ids + "auxSendsMode").c_str(), json_integer(auxSendsMode));
		
		// panLawStereo
		json_object_set_new(rootJ, (ids + "panLawStereo").c_str(), json_integer(panLawStereo));

		// vuColorThemeLocal
		json_object_set_new(rootJ, (ids + "vuColorThemeLocal").c_str(), json_integer(vuColorThemeLocal));

		// filterPos
		json_object_set_new(rootJ, (ids + "filterPos").c_str(), json_integer(filterPos));

		// dispColorLocal
		json_object_set_new(rootJ, (ids + "dispColorLocal").c_str(), json_integer(dispColorLocal));

		// polyStereo
		json_object_set_new(rootJ, (ids + "polyStereo").c_str(), json_integer(polyStereo));

		// panCvLevel
		json_object_set_new(rootJ, (ids + "panCvLevel").c_str(), json_real(panCvLevel));

		// stereoWidth
		json_object_set_new(rootJ, (ids + "stereoWidth").c_str(), json_real(stereoWidth));
		
		// invertInput
		json_object_set_new(rootJ, (ids + "invertInput").c_str(), json_integer(invertInput));
	}


	void dataFromJson(json_t *rootJ) {
		// trackName 
		// loaded elsewhere
		
		// gainAdjust
		json_t *gainAdjustJ = json_object_get(rootJ, (ids + "gainAdjust").c_str());
		if (gainAdjustJ)
			gainAdjust = json_number_value(gainAdjustJ);
		
		// fadeRate
		json_t *fadeRateJ = json_object_get(rootJ, (ids + "fadeRate").c_str());
		if (fadeRateJ)
			*fadeRate = json_number_value(fadeRateJ);
		
		// fadeProfile
		json_t *fadeProfileJ = json_object_get(rootJ, (ids + "fadeProfile").c_str());
		if (fadeProfileJ)
			fadeProfile = json_number_value(fadeProfileJ);

		// hpfCutoffFreq (legacy)
		json_t *hpfCutoffFreqJ = json_object_get(rootJ, (ids + "hpfCutoffFreq").c_str());
		if (hpfCutoffFreqJ)
			setHPFCutoffFreq(json_number_value(hpfCutoffFreqJ));
		
		// lpfCutoffFreq (legacy)
		json_t *lpfCutoffFreqJ = json_object_get(rootJ, (ids + "lpfCutoffFreq").c_str());
		if (lpfCutoffFreqJ)
			setLPFCutoffFreq(json_number_value(lpfCutoffFreqJ));
		
		// directOutsMode
		json_t *directOutsModeJ = json_object_get(rootJ, (ids + "directOutsMode").c_str());
		if (directOutsModeJ)
			directOutsMode = json_integer_value(directOutsModeJ);
		
		// auxSendsMode
		json_t *auxSendsModeJ = json_object_get(rootJ, (ids + "auxSendsMode").c_str());
		if (auxSendsModeJ)
			auxSendsMode = json_integer_value(auxSendsModeJ);
		
		// panLawStereo
		json_t *panLawStereoJ = json_object_get(rootJ, (ids + "panLawStereo").c_str());
		if (panLawStereoJ)
			panLawStereo = json_integer_value(panLawStereoJ);
		
		// vuColorThemeLocal
		json_t *vuColorThemeLocalJ = json_object_get(rootJ, (ids + "vuColorThemeLocal").c_str());
		if (vuColorThemeLocalJ)
			vuColorThemeLocal = json_integer_value(vuColorThemeLocalJ);
		
		// filterPos
		json_t *filterPosJ = json_object_get(rootJ, (ids + "filterPos").c_str());
		if (filterPosJ)
			filterPos = json_integer_value(filterPosJ);
		
		// dispColorLocal
		json_t *dispColorLocalJ = json_object_get(rootJ, (ids + "dispColorLocal").c_str());
		if (dispColorLocalJ)
			dispColorLocal = json_integer_value(dispColorLocalJ);
		
		// polyStereo
		json_t *polyStereoJ = json_object_get(rootJ, (ids + "polyStereo").c_str());
		if (polyStereoJ)
			polyStereo = json_integer_value(polyStereoJ);
		
		// panCvLevel
		json_t *panCvLevelJ = json_object_get(rootJ, (ids + "panCvLevel").c_str());
		if (panCvLevelJ)
			panCvLevel = json_number_value(panCvLevelJ);
		
		// stereoWidth
		json_t *stereoWidthJ = json_object_get(rootJ, (ids + "stereoWidth").c_str());
		if (stereoWidthJ)
			stereoWidth = json_number_value(stereoWidthJ);
		
		// invertInput
		json_t *invertInputJ = json_object_get(rootJ, (ids + "invertInput").c_str());
		if (invertInputJ)
			invertInput = json_integer_value(invertInputJ);
		
		// extern must call resetNonJson()
	}


	// level 1 read and write
	void write(TrackSettingsCpBuffer *dest) {
		dest->gainAdjust = gainAdjust;
		dest->fadeRate = *fadeRate;
		dest->fadeProfile = fadeProfile;
		dest->hpfCutoffFreq = paHpfCutoff->getValue();
		dest->lpfCutoffFreq = paLpfCutoff->getValue();	
		dest->directOutsMode = directOutsMode;
		dest->auxSendsMode = auxSendsMode;
		dest->panLawStereo = panLawStereo;
		dest->vuColorThemeLocal = vuColorThemeLocal;
		dest->filterPos = filterPos;
		dest->dispColorLocal = dispColorLocal;
		dest->polyStereo = polyStereo;
		dest->panCvLevel = panCvLevel;
		dest->stereoWidth = stereoWidth;
		dest->invertInput = invertInput;
		dest->linkedFader = isLinked(&(gInfo->linkBitMask), trackNum);
	}
	void read(TrackSettingsCpBuffer *src) {
		gainAdjust = src->gainAdjust;
		*fadeRate = src->fadeRate;
		fadeProfile = src->fadeProfile;
		setHPFCutoffFreq(src->hpfCutoffFreq);
		setLPFCutoffFreq(src->lpfCutoffFreq);	
		directOutsMode = src->directOutsMode;
		auxSendsMode = src->auxSendsMode;
		panLawStereo = src->panLawStereo;
		vuColorThemeLocal = src->vuColorThemeLocal;
		filterPos = src->filterPos;
		dispColorLocal = src->dispColorLocal;
		polyStereo = src->polyStereo;
		panCvLevel = src->panCvLevel;
		stereoWidth = src->stereoWidth;
		invertInput = src->invertInput;
		gInfo->setLinked(trackNum, src->linkedFader);
	}


	// level 2 read and write
	void write2(TrackSettingsCpBuffer *dest) {
		write(dest);
		dest->paGroup = paGroup->getValue();;
		dest->paFade = paFade->getValue();
		dest->paMute = paMute->getValue();
		dest->paSolo = paSolo->getValue();
		dest->paPan = paPan->getValue();
		for (int chr = 0; chr < 4; chr++) {
			dest->trackName[chr] = trackName[chr];
		}
		dest->fadeGain = fadeGain;
		dest->target = target;
		dest->fadeGainX = fadeGainX;
		dest->fadeGainXr = fadeGainXr;
		dest->fadeGainScaled = fadeGainScaled;
	}
	void read2(TrackSettingsCpBuffer *src) {
		read(src);
		paGroup->setValue(src->paGroup);
		paFade->setValue(src->paFade);
		paMute->setValue(src->paMute);
		paSolo->setValue(src->paSolo);
		paPan->setValue(src->paPan);
		for (int chr = 0; chr < 4; chr++) {
			trackName[chr] = src->trackName[chr];
		}
		fadeGain = src->fadeGain;
		target = src->target;
		fadeGainX = src->fadeGainX;
		fadeGainXr = src->fadeGainXr;
		fadeGainScaled = src->fadeGainScaled;
	}

	
	void setHPFCutoffFreq(float fc) {
		paHpfCutoff->setValue(fc);
		lastHpfCutoff = fc;
		fc *= gInfo->sampleTime;// fc is in normalized freq for rest of method
		hpFilter[0].setParameters(true, fc);
		hpFilter[1].setParameters(true, fc);
	}
	float getHPFCutoffFreq() {return paHpfCutoff->getValue();}
	
	void setLPFCutoffFreq(float fc) {
		paLpfCutoff->setValue(fc);
		lastLpfCutoff = fc;
		fc *= gInfo->sampleTime;// fc is in normalized freq for rest of method
		lpFilter[0].setParameters(false, fc);
		lpFilter[1].setParameters(false, fc);
	}
	float getLPFCutoffFreq() {return paLpfCutoff->getValue();}

	float calcSoloGain() {// returns 1.0f when the check for solo means this track should play, 0.0f otherwise
		if (gInfo->soloBitMask == 0ul) {// no track nor groups are soloed 
			return 1.0f;
		}
		// here at least one track or group is soloed
		int group = (int)(paGroup->getValue() + 0.5f);
		if ( ((gInfo->soloBitMask & (1 << trackNum)) != 0) ) {// if this track is soloed
			if (group == 0 || ((gInfo->soloBitMask & ((N_GRP * N_GRP - 1) << N_TRK)) == 0)) {// not grouped, or grouped but no groups are soloed, play
				return 1.0f;
			}
			// grouped and at least one group is soloed, so play only if its group is itself soloed
			return ( (gInfo->soloBitMask & (1ul << (N_TRK + group - 1))) != 0ul ? 1.0f : 0.0f);
		}
		// here this track is not soloed
		if ( (group != 0) && ( (gInfo->soloBitMask & (1ul << (N_TRK + group - 1))) != 0ul ) ) {// if going through soloed group  
			// check all solos of all tracks mapped to group, and return true if all those solos are off
			return ((gInfo->groupUsage[group - 1] & gInfo->soloBitMask) == 0) ? 1.0f : 0.0f;
		}
		return 0.0f;
	}


	void onSampleRateChange() {
		setHPFCutoffFreq(paHpfCutoff->getValue());
		setLPFCutoffFreq(paLpfCutoff->getValue());
	}

	
	void updateSlowValues() {
		// filters
		if (paHpfCutoff->getValue() != lastHpfCutoff) {
			setHPFCutoffFreq(paHpfCutoff->getValue());
		}
		if (paLpfCutoff->getValue() != lastLpfCutoff) {
			setLPFCutoffFreq(paLpfCutoff->getValue());
		}
		
		// calc ** stereo **
		bool newStereo = (inSig[0].isConnected() && inSig[1].isConnected()) || (polyStereo != 0 && inSig[0].isPolyphonic());
		if (stereo != newStereo) {
			stereo = newStereo;
			oldPan = -10.0f;
		}
		
		// ** detect pan mode change ** (and trigger recalc of panMatrix)
		PackedBytes4 newPanSig;
		newPanSig.cc4[0] = panLawStereo;
		newPanSig.cc4[1] = gInfo->directOutPanStereoMomentCvLinearVol.cc4[1];
		newPanSig.cc4[2] = gInfo->panLawMono;
		newPanSig.cc4[3] = 0xFF;
		if (newPanSig.cc1 != oldPanSignature.cc1) {
			oldPan = -10.0f;
			oldPanSignature.cc1 = newPanSig.cc1;
		}

		// calc ** inGain ** (with invertInput)
		inGain = inSig[0].isConnected() ? gainAdjust : 0.0f;
		if (invertInput != 0) {
			inGain *= -1.0f;
		}
		
		// soloGain
		soloGain = calcSoloGain();

		// ** process linked **
		gInfo->processLinked(trackNum, paFade->getValue());
	}
	

	void process(float *mix, bool eco) {// track		
		if (eco) {
			// calc ** fadeGain, fadeGainX, fadeGainXr, target, fadeGainScaled **
			float newTarget = calcFadeGain();
			if (newTarget != target) {
				fadeGainXr = 0.0f;
				if (isFadeMode()) {
					gInfo->fadeOtherLinkedTracks(trackNum, newTarget);
				}
				target = newTarget;
				vu.reset();
			}
			if (fadeGain != target) {
				if (isFadeMode()) {
					float deltaX = (gInfo->sampleTime / *fadeRate) * (1 + (gInfo->ecoMode & 0x3));// last value is sub refresh
					fadeGain = updateFadeGain(fadeGain, target, &fadeGainX, &fadeGainXr, deltaX, fadeProfile, gInfo->symmetricalFade);
					fadeGainScaled = std::pow(fadeGain, GlobalConst::trkAndGrpFaderScalingExponent);
				}
				else {// we are in mute mode
					fadeGain = target;
					fadeGainX = target;
					fadeGainScaled = target;// no pow needed here since 0.0f or 1.0f
				}
			}
			fadeGainScaledWithSolo = fadeGainScaled * soloGain;

			// calc ** fader, paramWithCV, volCv **
			fader = paFade->getValue();
			float volCvVoltage = 1e6;
			if (inVol->isConnected()) {
				volCvVoltage = inVol->getVoltage();
			}
			else if (inVolTrack1->getChannels() > trackNum) {
				// poly spread track 1 when sufficient channels in the poly cable
				volCvVoltage = inVolTrack1->getVoltage(trackNum);
			}
			if (volCvVoltage != 1e6) {
				volCv = clamp(volCvVoltage * 0.1f, 0.0f, 1.0f);
				paramWithCV = fader * volCv;
				if (gInfo->directOutPanStereoMomentCvLinearVol.cc4[3] == 0) {
					fader = paramWithCV;
				}	
			}
			else {
				volCv = 1.0f;
				paramWithCV = -100.0f;
			}

			// calc ** pan **
			pan = paPan->getValue();
			panCvConnected = inPan->isConnected();
			if (panCvConnected) {
				pan += inPan->getVoltage() * 0.1f * panCvLevel;// CV is a -5V to +5V input
				pan = clamp(pan, 0.0f, 1.0f);
			}
			else {
				// poly spread track 1 when sufficient channels in the poly cable
				panCvConnected = (inPanTrack1->getChannels() > trackNum);
				if (panCvConnected) {
					pan += inPanTrack1->getVoltage(trackNum) * 0.1f * panCvLevel;// CV is a -5V to +5V input
					pan = clamp(pan, 0.0f, 1.0f);
				}
			}
		}


		// optimize unused track
		if (!inSig[0].isConnected()) {
			if (oldInUse) {
				taps[0] = 0.0f; taps[1] = 0.0f;
				taps[N_TRK * 2 + 0] = 0.0f; taps[N_TRK * 2 + 1] = 0.0f;
				taps[N_TRK * 4 + 0] = 0.0f; taps[N_TRK * 4 + 1] = 0.0f;
				taps[N_TRK * 6 + 0] = 0.0f; taps[N_TRK * 6 + 1] = 0.0f;
				insertOuts[0] = 0.0f;
				insertOuts[1] = 0.0f;
				vu.reset();
				gainMatrixSlewers.reset();
				inGainSlewer.reset();
				stereoWidthSlewer.reset();
				muteSoloGainSlewer.reset();
				oldInUse = false;
			}
			return;
		}
		oldInUse = true;
			
		
		// Tap[0],[1]: pre-insert (inputs with gain adjust and stereo width)
		
		// in Gain
		if (inGain != inGainSlewer.out) {
			inGainSlewer.process(gInfo->sampleTime, inGain);
		}
		if (stereo) {// either because R is connected, or polyStereo is active and L is a poly cable
			if (inSig[1].isConnected()) {// if stereo because R connected
				taps[0] = inSig[0].getVoltageSum();
				taps[1] = inSig[1].getVoltageSum();
			}
			else {// here were are in polyStero mode, so take all odd numbered into L, even numbered into R (1-indexed)
				taps[0] = 0.0f;
				taps[1] = 0.0f;
				for (int c = 0; c < inSig[0].getChannels(); c++) {
					if ((c & 0x1) == 0) {// if L channels (odd channels when 1-indexed)
						taps[0] += inSig[0].getVoltage(c);
					}
					else {
						taps[1] += inSig[0].getVoltage(c);
					}
				}
			}
			taps[0] = clamp20V(taps[0] * inGainSlewer.out);
			taps[1] = clamp20V(taps[1] * inGainSlewer.out);
		}
		else {
			taps[0] = clamp20V(inSig[0].getVoltageSum() * inGainSlewer.out);
			taps[1] = taps[0];
		}
		
		// Stereo width
		if (stereoWidth != stereoWidthSlewer.out) {
			stereoWidthSlewer.process(gInfo->sampleTime, stereoWidth);
		}
		if (stereo && stereoWidthSlewer.out != 1.0f) {
			applyStereoWidth(stereoWidthSlewer.out, &taps[0], &taps[1]);
		}


		// Tap[32],[33]: pre-fader (inserts and filters)
		
		int insertPortIndex = trackNum >> 3;		
		if (gInfo->filterPos == 1 || (gInfo->filterPos == 2 && filterPos == 1)) {// if filters post insert
			// Insert outputs
			insertOuts[0] = taps[0];
			insertOuts[1] = stereo ? taps[1] : 0.0f;// don't send to R of insert outs when mono
			
			// Insert inputs
			if (inInsert[insertPortIndex].isConnected()) {
				taps[N_TRK * 2 + 0] = clamp20V(inInsert[insertPortIndex].getVoltage(((trackNum & 0x7) << 1) + 0));
				taps[N_TRK * 2 + 1] = stereo ? clamp20V(inInsert[insertPortIndex].getVoltage(((trackNum & 0x7) << 1) + 1)) : taps[N_TRK * 2 + 0];// don't receive from R of insert outs when mono, just normal L into R (need this for aux sends)
			}
			else {
				taps[N_TRK * 2 + 0] = taps[0];
				taps[N_TRK * 2 + 1] = taps[1];
			}

			// Filters
			// HPF
			if (getHPFCutoffFreq() >= GlobalConst::minHPFCutoffFreq) {
				taps[N_TRK * 2 + 0] = hpFilter[0].process(taps[N_TRK * 2 + 0]);
				taps[N_TRK * 2 + 1] = stereo ? hpFilter[1].process(taps[N_TRK * 2 + 1]) : taps[N_TRK * 2 + 0];
			}
			// LPF
			if (getLPFCutoffFreq() <= GlobalConst::maxLPFCutoffFreq) {
				taps[N_TRK * 2 + 0] = lpFilter[0].process(taps[N_TRK * 2 + 0]);
				taps[N_TRK * 2 + 1] = stereo ? lpFilter[1].process(taps[N_TRK * 2 + 1]) : taps[N_TRK * 2 + 0];
			}
		}
		else {// filters before inserts
			taps[N_TRK * 2 + 0] = taps[0];
			taps[N_TRK * 2 + 1] = taps[1];
			// Filters
			// HPF
			if (getHPFCutoffFreq() >= GlobalConst::minHPFCutoffFreq) {
				taps[N_TRK * 2 + 0] = hpFilter[0].process(taps[N_TRK * 2 + 0]);
				taps[N_TRK * 2 + 1] = stereo ? hpFilter[1].process(taps[N_TRK * 2 + 1]) : taps[N_TRK * 2 + 0];
			}
			// LPF
			if (getLPFCutoffFreq() <= GlobalConst::maxLPFCutoffFreq) {
				taps[N_TRK * 2 + 0] = lpFilter[0].process(taps[N_TRK * 2 + 0]);
				taps[N_TRK * 2 + 1] = stereo ? lpFilter[1].process(taps[N_TRK * 2 + 1]) : taps[N_TRK * 2 + 0];
			}
			
			// Insert outputs
			insertOuts[0] = taps[N_TRK * 2 + 0];
			insertOuts[1] = stereo ? taps[N_TRK * 2 + 1] : 0.0f;// don't send to R of insert outs when mono!
			
			// Insert inputs
			if (inInsert[insertPortIndex].isConnected()) {
				taps[N_TRK * 2 + 0] = clamp20V(inInsert[insertPortIndex].getVoltage(((trackNum & 0x7) << 1) + 0));
				taps[N_TRK * 2 + 1] = stereo ? clamp20V(inInsert[insertPortIndex].getVoltage(((trackNum & 0x7) << 1) + 1)) : taps[N_TRK * 2 + 0];// don't receive from R of insert outs when mono, just normal L into R (need this for aux sends)
			}
		}// filterPos
		
		
		// Tap[64],[65]: post-fader (pan and fader)
		
		if (eco) {
			// calc ** panMatrix **
			if (pan != oldPan) {
				panMatrix = 0.0f;// L, R, RinL, LinR (used for fader-pan block)
				if (pan == 0.5f) {
					if (!stereo) panMatrix[3] = 1.0f;
					else panMatrix[1] = 1.0f;
					panMatrix[0] = 1.0f;
				}
				else {		
					if (!stereo) {// mono
						if (gInfo->panLawMono == 3) {
							// Linear panning law (+6dB boost)
							panMatrix[3] = pan * 2.0f;
							panMatrix[0] = 2.0f - panMatrix[3];
						}
						else if (gInfo->panLawMono == 0) {
							// No compensation (+0dB boost)
							panMatrix[3] = std::min(1.0f, pan * 2.0f);
							panMatrix[0] = std::min(1.0f, 2.0f - pan * 2.0f);
						}
						else if (gInfo->panLawMono == 1) {
							// Equal power panning law (+3dB boost)
							sinCosSqrt2(&panMatrix[3], &panMatrix[0], pan * float(M_PI_2));
						}
						else {//if (gInfo->panLawMono == 2) {
							// Compromise (+4.5dB boost)
							sinCosSqrt2(&panMatrix[3], &panMatrix[0], pan * float(M_PI_2));
							panMatrix[3] = std::sqrt( std::abs( panMatrix[3] * (pan * 2.0f) ) );
							panMatrix[0] = std::sqrt( std::abs( panMatrix[0] * (2.0f - pan * 2.0f) ) );
						}
					}
					else {// stereo
						int stereoPanMode = (gInfo->directOutPanStereoMomentCvLinearVol.cc4[1] < 3 ? gInfo->directOutPanStereoMomentCvLinearVol.cc4[1] : panLawStereo);			
						if (stereoPanMode == 0) {
							// Stereo balance linear, (+0 dB), same as mono No compensation
							panMatrix[1] = std::min(1.0f, pan * 2.0f);
							panMatrix[0] = std::min(1.0f, 2.0f - pan * 2.0f);
						}
						else if (stereoPanMode == 1) {
							// Stereo balance equal power (+3dB), same as mono Equal power
							sinCosSqrt2(&panMatrix[1], &panMatrix[0], pan * float(M_PI_2));
						}
						else {
							// True panning, equal power
							if (pan > 0.5f) {
								panMatrix[1] = 1.0f;
								panMatrix[2] = 0.0f;
								sinCos(&panMatrix[3], &panMatrix[0], (pan - 0.5f) * float(M_PI));
							}
							else {// must be < (not <= since = 0.5 is caught at above)
								sinCos(&panMatrix[1], &panMatrix[2], pan * float(M_PI));
								panMatrix[0] = 1.0f;
								panMatrix[3] = 0.0f;
							}
						}
					}
				}
				oldPan = pan;
			}
			// calc ** gainMatrix **
			fader = std::pow(fader, GlobalConst::trkAndGrpFaderScalingExponent);// scaling
			gainMatrix = panMatrix * fader;
		}
		
		// Apply gainMatrix
		simd::float_4 sigs(taps[N_TRK * 2 + 0], taps[N_TRK * 2 + 1], taps[N_TRK * 2 + 1], taps[N_TRK * 2 + 0]);// L, R, RinL, LinR
		if (movemask(gainMatrix == gainMatrixSlewers.out) != 0xF) {// movemask returns 0xF when 4 floats are equal
			gainMatrixSlewers.process(gInfo->sampleTime, gainMatrix);
		}
		sigs *= gainMatrixSlewers.out;
		taps[N_TRK * 4 + 0] = sigs[0] + sigs[2];
		taps[N_TRK * 4 + 1] = sigs[1] + sigs[3];
		if (gInfo->directOutPanStereoMomentCvLinearVol.cc4[3] != 0) {
			taps[N_TRK * 4 + 0] *= volCv;
			taps[N_TRK * 4 + 1] *= volCv;
		}

		// Calc muteSoloGainSlewed
		if (fadeGainScaledWithSolo != muteSoloGainSlewer.out) {
			muteSoloGainSlewer.process(gInfo->sampleTime, fadeGainScaledWithSolo);
		}
		taps[N_TRK * 6 + 0] = taps[N_TRK * 4 + 0] * muteSoloGainSlewer.out;
		taps[N_TRK * 6 + 1] = taps[N_TRK * 4 + 1] * muteSoloGainSlewer.out;
			
			
		// Add to final mix or group
		if (paGroup->getValue() < 0.5f) {
			mix[0] += taps[N_TRK * 6 + 0];
			mix[1] += taps[N_TRK * 6 + 1];
		}
		else {
			int groupIndex = (int)(paGroup->getValue() - 0.5f);
			groupTaps[(groupIndex << 1) + 0] += taps[N_TRK * 6 + 0];
			groupTaps[(groupIndex << 1) + 1] += taps[N_TRK * 6 + 1];
		}
		
		// VUs
		if (gInfo->colorAndCloak.cc4[cloakedMode] != 0) {
			vu.reset();
		}
		else if (eco) {
			float sampleTimeEco = gInfo->sampleTime * (1 + (gInfo->ecoMode & 0x3));
			vu.process(sampleTimeEco, &taps[N_TRK * (fadeGainScaledWithSolo == 0.0f ? 4 : 6) + 0]);
		}
	}
};// struct MixerTrack



//*****************************************************************************



struct MixerAux {
	// Constants
	// none
	
	// need to save, no reset
	// none
	
	// need to save, with reset
	// none

	// no need to save, with reset
	private:
	simd::float_4 panMatrix;
	simd::float_4 gainMatrix;	
	TSlewLimiterSingle<simd::float_4> gainMatrixSlewers;
	SlewLimiterSingle muteSoloGainSlewer;
	float oldPan;
	PackedBytes4 oldPanSignature;// [0] is pan stereo local, [1] is pan stereo global, [2] is pan mono global
	public:
	VuMeterAllDual vu;
	float fadeGain; // target of this gain is the value of the mute/fade button's param (i.e. 0.0f or 1.0f)
	float target;
	float fadeGainX;
	float fadeGainXr;
	float fadeGainScaled;
	float fadeGainScaledWithSolo;
	float soloGain;


	// no need to save, no reset
	int auxNum;// 0 to 3
	std::string ids;
	GlobalInfo *gInfo;
	Input *inInsert;
	float *flMute;
	float *flSolo0;
	float *flGroup;
	float *fadeRate;
	float *fadeProfile;
	float *taps;
	int8_t* panLawStereoLocal;

	int getAuxGroup() {return (int)(*flGroup + 0.5f);}
	float calcFadeGain() {return *flMute >= 0.5f ? 0.0f : 1.0f;}
	bool isFadeMode() {return *fadeRate >= GlobalConst::minFadeRate;}


	void construct(int _auxNum, GlobalInfo *_gInfo, Input *_inputs, float* _values20, float* _taps, int8_t* _panLawStereoLocal) {
		auxNum = _auxNum;
		ids = "id_a" + std::to_string(auxNum) + "_";
		gInfo = _gInfo;
		inInsert = &_inputs[INSERT_GRP_AUX_INPUT];
		flMute = &_values20[auxNum];
		flGroup = &_values20[auxNum + 8];
		fadeRate = &_values20[auxNum + 12];
		fadeProfile = &_values20[auxNum + 16];
		taps = _taps;
		panLawStereoLocal = _panLawStereoLocal;
		gainMatrixSlewers.setRiseFall(simd::float_4(GlobalConst::antipopSlewSlow)); // slew rate is in input-units per second (ex: V/s)
		muteSoloGainSlewer.setRiseFall(GlobalConst::antipopSlewFast); // slew rate is in input-units per second (ex: V/s)
	}


	void onReset() {
		resetNonJson();
	}


	void resetNonJson() {
		panMatrix = 0.0f;
		gainMatrix = 0.0f;
		gainMatrixSlewers.reset();
		muteSoloGainSlewer.reset();
		oldPan = -10.0f;
		oldPanSignature.cc1 = 0xFFFFFFFF;
		vu.reset();
		fadeGain = calcFadeGain();
		target = fadeGain;
		fadeGainX = fadeGain;
		fadeGainXr = 0.0f;
		fadeGainScaled = fadeGain;// no pow needed here since 0.0f or 1.0f
		fadeGainScaledWithSolo = fadeGainScaled;
		soloGain = 1.0f;
	}	

	void dataToJson(json_t *rootJ) {}
	void dataFromJson(json_t *rootJ) {}
	
	float calcSoloGain() {
		if (gInfo->soloBitMask != 0 && gInfo->auxReturnsMutedWhenMainSolo) {
			// Handle "Mute aux returns when soloing track"
			// i.e. add aux returns to mix when no solo, or when solo and don't want mutes aux returns
			return 0.0f;
		}
		return (gInfo->returnSoloBitMask == 0 || (gInfo->returnSoloBitMask & (1 << auxNum)) != 0) ? 1.0f : 0.0f;
	}
	
	void updateSlowValues() {
		// ** detect pan mode change ** (and trigger recalc of panMatrix)
		PackedBytes4 newPanSig;
		newPanSig.cc4[0] = *panLawStereoLocal;
		newPanSig.cc4[1] = gInfo->directOutPanStereoMomentCvLinearVol.cc4[1];
		newPanSig.cc4[2] = gInfo->panLawMono;
		newPanSig.cc4[3] = 0xFF;
		if (newPanSig.cc1 != oldPanSignature.cc1) {
			oldPan = -10.0f;
			oldPanSignature.cc1 = newPanSig.cc1;
		}
		
		// soloGain
		soloGain = calcSoloGain();
	}
	
	void process(float *mix, float *auxRetFadePanFadecv, bool eco) {// mixer aux
		// auxRetFadePan[0] points fader value, 
		// auxRetFadePan[4] points pan value, 
		// auxRetFadePan[8] points fader cv value, 
		// all indexed for a given aux
		

		// Tap[0],[1]: pre-insert (aux inputs)
		// nothing to do, already set up by the auxspander
		
		// Tap[8],[9]: pre-fader (post insert)
		if (inInsert->isConnected()) {
			taps[8] = clamp20V(inInsert->getVoltage((auxNum << 1) + 8));
			taps[9] = clamp20V(inInsert->getVoltage((auxNum << 1) + 9));
		}
		else {
			taps[8] = taps[0];
			taps[9] = taps[1];
		}
		
		if (eco) {
			// calc ** fadeGain, fadeGainX, fadeGainXr, target, fadeGainScaled **
			float newTarget = calcFadeGain();
			if (newTarget != target) {
				fadeGainXr = 0.0f;
				target = newTarget;
				vu.reset();
			}
			if (fadeGain != target) {
				if (isFadeMode()) {
					float deltaX = (gInfo->sampleTime / *fadeRate) * (1 + (gInfo->ecoMode & 0x3));// last value is sub refresh
					fadeGain = updateFadeGain(fadeGain, target, &fadeGainX, &fadeGainXr, deltaX, *fadeProfile, gInfo->symmetricalFade);
					fadeGainScaled = std::pow(fadeGain, GlobalConst::globalAuxReturnScalingExponent);
				}
				else {// we are in mute mode
					fadeGain = target;
					fadeGainX = target;
					fadeGainScaled = target;// no pow needed here since 0.0f or 1.0f
				}
			}
			fadeGainScaledWithSolo = fadeGainScaled * soloGain;


			// calc ** panMatrix **
			float pan = auxRetFadePanFadecv[4];// cv input and clamping already done in auxspander
			if (pan != oldPan) {
				panMatrix = 0.0f;// L, R, RinL, LinR (used for fader-pan block)
				if (pan == 0.5f) {
					panMatrix[1] = 1.0f;
					panMatrix[0] = 1.0f;
				}
				else {	
					// implicitly stereo for aux
					int stereoPanMode = (gInfo->directOutPanStereoMomentCvLinearVol.cc4[1] < 3 ? gInfo->directOutPanStereoMomentCvLinearVol.cc4[1] : *panLawStereoLocal);
					if (stereoPanMode == 0) {
						// Stereo balance linear, (+0 dB), same as mono No compensation
						panMatrix[1] = std::min(1.0f, pan * 2.0f);
						panMatrix[0] = std::min(1.0f, 2.0f - pan * 2.0f);
					}
					else if (stereoPanMode == 1) {
						// Stereo balance equal power (+3dB), same as mono Equal power
						sinCosSqrt2(&panMatrix[1], &panMatrix[0], pan * float(M_PI_2));
					}
					else {
						// True panning, equal power
						if (pan > 0.5f) {
							panMatrix[1] = 1.0f;
							panMatrix[2] = 0.0f;
							sinCos(&panMatrix[3], &panMatrix[0], (pan - 0.5f) * float(M_PI));
						}
						else {// must be < (not <= since = 0.5 is caught at above)
							sinCos(&panMatrix[1], &panMatrix[2], pan * float(M_PI));
							panMatrix[0] = 1.0f;
							panMatrix[3] = 0.0f;
						}
					}
				}
				oldPan = pan;
			}
			// calc ** gainMatrix **
			gainMatrix = panMatrix * auxRetFadePanFadecv[0];
		}
		
		// Calc gainMatrixSlewed and apply it
		simd::float_4 sigs(taps[8], taps[9], taps[9], taps[8]);
		if (movemask(gainMatrix == gainMatrixSlewers.out) != 0xF) {// movemask returns 0xF when 4 floats are equal
			gainMatrixSlewers.process(gInfo->sampleTime, gainMatrix);
		}
		sigs = sigs * gainMatrixSlewers.out;	
		taps[16] = sigs[0] + sigs[2];
		taps[17] = sigs[1] + sigs[3];
		if (gInfo->directOutPanStereoMomentCvLinearVol.cc4[3] != 0) {
			taps[16] *= auxRetFadePanFadecv[8];
			taps[17] *= auxRetFadePanFadecv[8];
		}	
		
		// Calc muteSoloGainSlewed
		if (fadeGainScaledWithSolo != muteSoloGainSlewer.out) {
			muteSoloGainSlewer.process(gInfo->sampleTime, fadeGainScaledWithSolo);
		}
		
		taps[24] = taps[16] * muteSoloGainSlewer.out;
		taps[25] = taps[17] * muteSoloGainSlewer.out;
			
		mix[0] += taps[24];
		mix[1] += taps[25];
		
		// VUs
		if (gInfo->colorAndCloak.cc4[cloakedMode] != 0) {
			vu.reset();
		}
		else if (eco) {
			float sampleTimeEco = gInfo->sampleTime * (1 + (gInfo->ecoMode & 0x3));
			vu.process(sampleTimeEco, &taps[(fadeGainScaledWithSolo == 0.0f ? 16 : 24) + 0]);
		}

	}
};// struct MixerAux


