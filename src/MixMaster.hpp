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
	int panLawMono;// +0dB (no compensation),  +3 (equal power),  +4.5 (compromize),  +6dB (linear, default)
	int8_t panLawStereo;// Stereo balance linear (default), Stereo balance equal power (+3dB boost since one channel lost),  True pan (linear redistribution but is not equal power), Per-track
	int8_t directOutsMode;// 0 is pre-insert, 1 is post-insert, 2 is post-fader, 3 is post-solo, 4 is per-track choice
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
	int8_t momentaryCvButtons;// 1 = yes (original rising edge only version), 0 = level sensitive (emulated with rising and falling detection)

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
			if (paSolo[trkOrGrp].getValue() > 0.5f) {
				soloBitMask |= (1 << trkOrGrp);
			}
			else {
				soloBitMask &= ~(1 << trkOrGrp);
			}	
		}
		else {// trkOrGrp >= N_TRK
			if (paSolo[trkOrGrp].getValue() > 0.5f) {
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
			if (values20[4 + auxi] > 0.5f) {
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
				if (newTarget > 0.5f && paMute[trkOrGrp].getValue() > 0.5f) {
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
		panLawMono = 1;
		panLawStereo = 1;
		directOutsMode = 3;// post-solo should be default
		auxSendsMode = 3;// post-solo should be default
		groupsControlTrackSendLevels = 0;
		auxReturnsMutedWhenMainSolo = 0;
		auxReturnsSolosMuteDry = 0;
		chainMode = 1;// post should be default
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
		momentaryCvButtons = 1;// momentary by default
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
		json_object_set_new(rootJ, "panLawStereo", json_integer(panLawStereo));

		// directOutsMode
		json_object_set_new(rootJ, "directOutsMode", json_integer(directOutsMode));
		
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
		json_object_set_new(rootJ, "momentaryCvButtons", json_integer(momentaryCvButtons));
	}


	void dataFromJson(json_t *rootJ) {
		// panLawMono
		json_t *panLawMonoJ = json_object_get(rootJ, "panLawMono");
		if (panLawMonoJ)
			panLawMono = json_integer_value(panLawMonoJ);
		
		// panLawStereo
		json_t *panLawStereoJ = json_object_get(rootJ, "panLawStereo");
		if (panLawStereoJ)
			panLawStereo = json_integer_value(panLawStereoJ);
		
		// directOutsMode
		json_t *directOutsModeJ = json_object_get(rootJ, "directOutsMode");
		if (directOutsModeJ)
			directOutsMode = json_integer_value(directOutsModeJ);
		
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
		if (linkBitMaskJ)
			linkBitMask = json_integer_value(linkBitMaskJ);
		
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
		if (fadersJ) {
			for (int trkOrGrp = 0; trkOrGrp < (N_TRK + N_GRP); trkOrGrp++) {
				json_t *fadersArrayJ = json_array_get(fadersJ, trkOrGrp);
				if (fadersArrayJ)
					linkedFaderReloadValues[trkOrGrp] = json_number_value(fadersArrayJ);
			}
		}
		else {// legacy
			for (int trkOrGrp = 0; trkOrGrp < (N_TRK + N_GRP); trkOrGrp++) {
				linkedFaderReloadValues[trkOrGrp] = paFade[trkOrGrp].getValue();
			}
		}
		
		// momentaryCvButtons
		json_t *momentaryCvButtonsJ = json_object_get(rootJ, "momentaryCvButtons");
		if (momentaryCvButtonsJ)
			momentaryCvButtons = json_integer_value(momentaryCvButtonsJ);
		
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
	float fadeProfile; // exp when +100, lin when 0, log when -100
	int8_t vuColorThemeLocal;
	int8_t dispColorLocal;
	float dimGain;// slider uses this gain, but displays it in dB instead of linear
	char masterLabel[7];
	
	// no need to save, with reset
	private:
	simd::float_4 chainGainsAndMute;// 0=L, 1=R, 2=mute
	float faderGain;
	simd::float_4 gainMatrix;// L, R, RinL, LinR (used for fader-mono block)
	dsp::TSlewLimiter<simd::float_4> gainMatrixSlewers;
	dsp::TSlewLimiter<simd::float_4> chainGainAndMuteSlewers;// chain gains are [0] and [1], mute is [2], unused is [3]
	OnePoleFilter dcBlocker[2];// 6dB/oct
	float oldFader;
	public:
	VuMeterAllDual vu;// use mix[0..1]
	float fadeGain; // target of this gain is the value of the mute/fade button's param (i.e. 0.0f or 1.0f)
	float fadeGainX;
	float fadeGainScaled;
	float paramWithCV;
	float dimGainIntegerDB;// corresponds to dimGain, converted to dB, then rounded, then back to linear
	float target;

	// no need to save, no reset
	GlobalInfo *gInfo;
	Param *params;
	Input *inChain;
	Input *inVol;

	float calcFadeGain() {return params[MAIN_MUTE_PARAM].getValue() > 0.5f ? 0.0f : 1.0f;}
	bool isFadeMode() {return fadeRate >= GlobalConst::minFadeRate;}


	void construct(GlobalInfo *_gInfo, Param *_params, Input *_inputs) {
		gInfo = _gInfo;
		params = _params;
		inChain = &_inputs[CHAIN_INPUTS];
		inVol = &_inputs[GRPM_MUTESOLO_INPUT];
		gainMatrixSlewers.setRiseFall(simd::float_4(GlobalConst::antipopSlewSlow), simd::float_4(GlobalConst::antipopSlewSlow)); // slew rate is in input-units per second (ex: V/s)
		chainGainAndMuteSlewers.setRiseFall(simd::float_4(GlobalConst::antipopSlewFast), simd::float_4(GlobalConst::antipopSlewFast)); // slew rate is in input-units per second (ex: V/s)
	}


	void onReset() {
		dcBlock = false;
		clipping = 0;
		fadeRate = 0.0f;
		fadeProfile = 0.0f;
		vuColorThemeLocal = 0;
		dispColorLocal = 0;
		dimGain = 0.25119f;// 0.1 = -20 dB, 0.25119 = -12 dB
		snprintf(masterLabel, 7, "MASTER");
		resetNonJson();
	}


	void resetNonJson() {
		chainGainsAndMute = 0.0f;
		faderGain = 0.0f;
		gainMatrix = 0.0f;
		gainMatrixSlewers.reset();
		chainGainAndMuteSlewers.reset();
		setupDcBlocker();
		oldFader = -10.0f;
		vu.reset();
		fadeGain = calcFadeGain();
		fadeGainX = gInfo->symmetricalFade ? fadeGain : 0.0f;
		fadeGainScaled = fadeGain;
		paramWithCV = -100.0f;
		dimGainIntegerDB = calcDimGainIntegerDB(dimGain);
		target = -1.0f;
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
		float dcCutoffFreq = 10.0f;// Hz
		dcCutoffFreq *= gInfo->sampleTime;// fc is in normalized freq for rest of method
		dcBlocker[0].setCutoff(dcCutoffFreq);
		dcBlocker[1].setCutoff(dcCutoffFreq);
	}
	
	
	void onSampleRateChange() {
		setupDcBlocker();
	}
	
	
	// Soft-clipping polynomial function
	// piecewise portion that handles inputs between 6 and 12 V
	// unipolar only, caller must take care of signs
	// assumes that 6 <= x <= 12
	// assumes f(x) := x  when x < 6
	// assumes f(x) := 12  when x > 12
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
		
		if (eco) {
			// calc ** fadeGain, fadeGainX, fadeGainScaled **
			if (isFadeMode()) {
				float newTarget = calcFadeGain();
				if (!gInfo->symmetricalFade && newTarget != target) {
					fadeGainX = 0.0f;
				}
				target = newTarget;
				if (fadeGain != target) {
					float deltaX = (gInfo->sampleTime / fadeRate) * (1 + (gInfo->ecoMode & 0x3));// last value is sub refresh
					fadeGain = updateFadeGain(fadeGain, target, &fadeGainX, deltaX, fadeProfile, gInfo->symmetricalFade);
					fadeGainScaled = std::pow(fadeGain, GlobalConst::masterFaderScalingExponent);
				}
			}
			else {// we are in mute mode
				fadeGain = calcFadeGain(); // do manually below to optimized fader mult
				fadeGainX = gInfo->symmetricalFade ? fadeGain : 0.0f;
				fadeGainScaled = fadeGain;// no pow needed here since 0.0f or 1.0f
			}	
			// dim (this affects fadeGainScaled only, so treated like a partial mute, but no effect on fade pointers or other just effect on sound
			chainGainsAndMute[2] = fadeGainScaled;
			if (params[MAIN_DIM_PARAM].getValue() > 0.5f) {
				chainGainsAndMute[2] *= dimGainIntegerDB;
			}
			
			// calc ** fader, paramWithCV **
			float fader = params[MAIN_FADER_PARAM].getValue();
			if (inVol->isConnected() && inVol->getChannels() >= (N_GRP * 2 + 4)) {
				fader *= clamp(inVol->getVoltage(N_GRP * 2 + 4 - 1) * 0.1f, 0.0f, 1.0f);//(multiplying, pre-scaling)
				paramWithCV = fader;
			}
			else {
				paramWithCV = -100.0f;
			}

			// scaling
			if (fader != oldFader) {
				oldFader = fader;
				faderGain = std::pow(fader, GlobalConst::masterFaderScalingExponent);
			}
			
			// calc ** gainMatrix **
			// mono
			if (params[MAIN_MONO_PARAM].getValue() > 0.5f) {
				gainMatrix = simd::float_4(0.5f * faderGain);
			}
			else {
				gainMatrix = simd::float_4(faderGain, faderGain, 0.0f, 0.0f);
			}
		}
		
		// Calc master gain with slewer
		if (movemask(gainMatrix == gainMatrixSlewers.out) != 0xF) {// movemask returns 0xF when 4 floats are equal
			gainMatrixSlewers.process(gInfo->sampleTime, gainMatrix);
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

		// Apply gainMatrixSlewed
		simd::float_4 sigs(mix[0], mix[1], mix[1], mix[0]);
		sigs = sigs * gainMatrixSlewers.out * chainGainAndMuteSlewers.out[2];
		
		// Set mix
		mix[0] = sigs[0] + sigs[2];
		mix[1] = sigs[1] + sigs[3];
		
		// VUs (no cloaked mode for master, always on)
		if (eco) {
			vu.process(gInfo->sampleTime * (1 + (gInfo->ecoMode & 0x3)), mix);
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
			mix[0] = dcBlocker[0].processHP(mix[0]);
			mix[1] = dcBlocker[1].processHP(mix[1]);
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
	float* fadeRate; // mute when < minFadeRate, fade when >= minFadeRate. This is actually the fade time in seconds
	float fadeProfile; // exp when +100, lin when 0, log when -100
	int8_t directOutsMode;// when per track
	int8_t auxSendsMode;// when per track
	int8_t panLawStereo;// when per track
	int8_t vuColorThemeLocal;
	int8_t dispColorLocal;
	float panCvLevel;// 0 to 1.0f

	// no need to save, with reset
	dsp::TSlewLimiter<simd::float_4> gainMatrixSlewers;
	dsp::SlewLimiter muteSoloGainSlewer;
	private:
	float faderGain;
	simd::float_4 panMatrix;
	simd::float_4 gainMatrix;	
	float oldPan;
	float oldFader;
	PackedBytes4 oldPanSignature;// [0] is pan stereo local, [1] is pan stereo global, [2] is pan mono global
	public:
	VuMeterAllDual vu;// use post[]
	float fadeGain; // target of this gain is the value of the mute/fade button's param (i.e. 0.0f or 1.0f)
	float fadeGainX;
	float fadeGainScaled;
	float paramWithCV;
	float pan;
	bool panCvConnected;
	float target = -1.0f;
	
	// no need to save, no reset
	int groupNum;// 0 to 3 (1)
	std::string ids;
	GlobalInfo *gInfo;
	Input *inInsert;
	Input *inVol;
	Input *inPan;
	Param *paFade;
	Param *paMute;
	Param *paPan;
	char  *groupName;// write 4 chars always (space when needed), no null termination since all tracks names are concat and just one null at end of all
	float *taps;// [0],[1]: pre-insert L R; [32][33]: pre-fader L R, [64][65]: post-fader L R, [96][97]: post-mute-solo L R

	float calcFadeGain() {return paMute->getValue() > 0.5f ? 0.0f : 1.0f;}
	bool isFadeMode() {return *fadeRate >= GlobalConst::minFadeRate;}



	void construct(int _groupNum, GlobalInfo *_gInfo, Input *_inputs, Param *_params, char* _groupName, float* _taps) {
		groupNum = _groupNum;
		ids = "id_g" + std::to_string(groupNum) + "_";
		gInfo = _gInfo;
		inInsert = &_inputs[INSERT_GRP_AUX_INPUT];
		inVol = &_inputs[GROUP_VOL_INPUTS + groupNum];
		inPan = &_inputs[GROUP_PAN_INPUTS + groupNum];
		paFade = &_params[GROUP_FADER_PARAMS + groupNum];
		paMute = &_params[GROUP_MUTE_PARAMS + groupNum];
		paPan = &_params[GROUP_PAN_PARAMS + groupNum];
		groupName = _groupName;
		taps = _taps;
		fadeRate = &(_gInfo->fadeRates[N_TRK + groupNum]);
		gainMatrixSlewers.setRiseFall(simd::float_4(GlobalConst::antipopSlewSlow), simd::float_4(GlobalConst::antipopSlewSlow)); // slew rate is in input-units per second (ex: V/s)
		muteSoloGainSlewer.setRiseFall(GlobalConst::antipopSlewFast, GlobalConst::antipopSlewFast); // slew rate is in input-units per second (ex: V/s)
	}


	void onReset() {
		*fadeRate = 0.0f;
		fadeProfile = 0.0f;
		directOutsMode = 3;// post-solo should be default
		auxSendsMode = 3;// post-solo should be default
		panLawStereo = 1;
		vuColorThemeLocal = 0;
		dispColorLocal = 0;
		panCvLevel = 1.0f;
		resetNonJson();
	}


	void resetNonJson() {
		panMatrix = 0.0f;
		faderGain = 0.0f;
		gainMatrix = 0.0f;
		gainMatrixSlewers.reset();
		muteSoloGainSlewer.reset();
		oldPan = -10.0f;
		oldFader = -10.0f;
		oldPanSignature.cc1 = 0xFFFFFFFF;
		vu.reset();
		fadeGain = calcFadeGain();
		fadeGainX = gInfo->symmetricalFade ? fadeGain : 0.0f;
		fadeGainScaled = fadeGain;// no pow needed here since 0.0f or 1.0f
		paramWithCV = -100.0f;
		pan = 0.5f;
		panCvConnected = false;
		target = -1.0f;
	}


	void dataToJson(json_t *rootJ) {
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

		// dispColorLocal
		json_object_set_new(rootJ, (ids + "dispColorLocal").c_str(), json_integer(dispColorLocal));
		
		// panCvLevel
		json_object_set_new(rootJ, (ids + "panCvLevel").c_str(), json_real(panCvLevel));
	}


	void dataFromJson(json_t *rootJ) {
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
		
		// dispColorLocal
		json_t *dispColorLocalJ = json_object_get(rootJ, (ids + "dispColorLocal").c_str());
		if (dispColorLocalJ)
			dispColorLocal = json_integer_value(dispColorLocalJ);
		
		// panCvLevel
		json_t *panCvLevelJ = json_object_get(rootJ, (ids + "panCvLevel").c_str());
		if (panCvLevelJ)
			panCvLevel = json_number_value(panCvLevelJ);
		
		// extern must call resetNonJson()
	}	
		
	void updateSlowValues() {
		// ** process linked **
		gInfo->processLinked(N_TRK + groupNum, paFade->getValue());
		
		// ** detect pan mode change ** (and trigger recalc of panMatrix)
		PackedBytes4 newPanSig;
		newPanSig.cc4[0] = panLawStereo;
		newPanSig.cc4[1] = gInfo->panLawStereo;
		newPanSig.cc4[2] = gInfo->panLawMono;
		newPanSig.cc4[3] = 0xFF;
		if (newPanSig.cc1 != oldPanSignature.cc1) {
			oldPan = -10.0f;
			oldPanSignature.cc1 = newPanSig.cc1;
		}
	}
	

	void process(float *mix, bool eco) {// group
		// Tap[0],[1]: pre-insert (group inputs)
		// nothing to do, already set up by the mix master
		
		// Tap[8],[9]: pre-fader (post insert)
		if (inInsert->isConnected()) {
			taps[N_GRP * 2 + 0] = clamp20V(inInsert->getVoltage((groupNum << 1) + 0));
			taps[N_GRP * 2 + 1] = clamp20V(inInsert->getVoltage((groupNum << 1) + 1));
		}
		else {
			taps[N_GRP * 2 + 0] = taps[0];
			taps[N_GRP * 2 + 1] = taps[1];
		}

		if (eco) {	
			// calc ** fadeGain, fadeGainX, fadeGainScaled **
			if (isFadeMode()) {
				float newTarget = calcFadeGain();
				if (newTarget != target) {
					if (!gInfo->symmetricalFade) {
						fadeGainX = 0.0f;
					}
					gInfo->fadeOtherLinkedTracks(groupNum + N_TRK, newTarget);
					target = newTarget;
				}
				if (fadeGain != target) {
					float deltaX = (gInfo->sampleTime / *fadeRate) * (1 + (gInfo->ecoMode & 0x3));// last value is sub refresh
					fadeGain = updateFadeGain(fadeGain, target, &fadeGainX, deltaX, fadeProfile, gInfo->symmetricalFade);
					fadeGainScaled = std::pow(fadeGain, GlobalConst::trkAndGrpFaderScalingExponent);
				}
			}
			else {// we are in mute mode
				fadeGain = calcFadeGain();
				fadeGainX = gInfo->symmetricalFade ? fadeGain : 0.0f;
				fadeGainScaled = fadeGain;// no pow needed here since 0.0f or 1.0f
			}

			// calc ** fader, paramWithCV **
			float fader = paFade->getValue();
			if (inVol->isConnected()) {
				fader *= clamp(inVol->getVoltage() * 0.1f, 0.0f, 1.0f);//(multiplying, pre-scaling)
				paramWithCV = fader;
			}
			else {
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
					int stereoPanMode = (gInfo->panLawStereo < 3 ? gInfo->panLawStereo : panLawStereo);
					if (stereoPanMode == 0) {
						// Stereo balance linear, (+0 dB), same as mono No compensation
						panMatrix[1] = std::min(1.0f, pan * 2.0f);
						panMatrix[0] = std::min(1.0f, 2.0f - pan * 2.0f);
					}
					else if (stereoPanMode == 1) {
						// Stereo balance equal power (+3dB), same as mono Equal power
						// panMatrix[1] = std::sin(pan * M_PI_2) * M_SQRT2;
						// panMatrix[0] = std::cos(pan * M_PI_2) * M_SQRT2;
						sinCosSqrt2(&panMatrix[1], &panMatrix[0], pan * M_PI_2);
					}
					else {
						// True panning, equal power
						// panMatrix[1] = pan >= 0.5f ? (1.0f) : (std::sin(pan * M_PI));
						// panMatrix[2] = pan >= 0.5f ? (0.0f) : (std::cos(pan * M_PI));
						// panMatrix[0] = pan <= 0.5f ? (1.0f) : (std::cos((pan - 0.5f) * M_PI));
						// panMatrix[3] = pan <= 0.5f ? (0.0f) : (std::sin((pan - 0.5f) * M_PI));
						if (pan > 0.5f) {
							panMatrix[1] = 1.0f;
							panMatrix[2] = 0.0f;
							sinCos(&panMatrix[3], &panMatrix[0], (pan - 0.5f) * M_PI);
						}
						else {// must be < (not <= since = 0.5 is caught at above)
							sinCos(&panMatrix[1], &panMatrix[2], pan * M_PI);
							panMatrix[0] = 1.0f;
							panMatrix[3] = 0.0f;
						}
					}
				}
			}
			// calc ** faderGain **
			if (fader != oldFader) {
				faderGain = std::pow(fader, GlobalConst::trkAndGrpFaderScalingExponent);// scaling
			}
			// calc ** gainMatrix **
			if (fader != oldFader || pan != oldPan) {
				oldFader = fader;
				oldPan = pan;	
				gainMatrix = panMatrix * faderGain;
			}
		}
	
		// Calc group gains with slewer
		if (movemask(gainMatrix == gainMatrixSlewers.out) != 0xF) {// movemask returns 0xF when 4 floats are equal
			gainMatrixSlewers.process(gInfo->sampleTime, gainMatrix);
		}
		
		// Apply gainMatrixSlewed
		simd::float_4 sigs(taps[N_GRP * 2 + 0], taps[N_GRP * 2 + 1], taps[N_GRP * 2 + 1], taps[N_GRP * 2 + 0]);
		sigs = sigs * gainMatrixSlewers.out;
		
		taps[N_GRP * 4 + 0] = sigs[0] + sigs[2];
		taps[N_GRP * 4 + 1] = sigs[1] + sigs[3];
		
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
			vu.process(gInfo->sampleTime * (1 + (gInfo->ecoMode & 0x3)), &taps[N_GRP * 6 + 0]);
		}
	}
};// struct MixerGroup



//*****************************************************************************


struct MixerTrack {
	// Constants
	static constexpr float hpfBiquadQ = 1.0f;// 1.0 Q since preceeeded by a one pole filter to get 18dB/oct
	
	// need to save, no reset
	// none
	
	// need to save, with reset
	float gainAdjust;// this is a gain here (not dB)
	float* fadeRate; // mute when < minFadeRate, fade when >= minFadeRate. This is actually the fade time in seconds
	float fadeProfile; // exp when +100, lin when 0, log when -100
	private:
	float hpfCutoffFreq;// always use getter and setter since tied to Biquad
	float lpfCutoffFreq;// always use getter and setter since tied to Biquad
	public:
	int8_t directOutsMode;// when per track
	int8_t auxSendsMode;// when per track
	int8_t panLawStereo;// when per track
	int8_t vuColorThemeLocal;
	int8_t filterPos;// 0 = pre insert, 1 = post insert, 2 = per track
	int8_t dispColorLocal;
	float panCvLevel;// 0 to 1.0f

	// no need to save, with reset
	private:
	bool stereo;// pan coefficients use this, so set up first
	float inGain;
	simd::float_4 panMatrix;
	float faderGain;
	simd::float_4 gainMatrix;	
	dsp::TSlewLimiter<simd::float_4> gainMatrixSlewers;
	dsp::SlewLimiter inGainSlewer;
	dsp::SlewLimiter muteSoloGainSlewer;
	OnePoleFilter hpPreFilter[2];// 6dB/oct
	dsp::BiquadFilter hpFilter[2];// 12dB/oct
	dsp::BiquadFilter lpFilter[2];// 12db/oct
	float oldPan;
	float oldFader;
	PackedBytes4 oldPanSignature;// [0] is pan stereo local, [1] is pan stereo global, [2] is pan mono global
	public:
	VuMeterAllDual vu;// use post[]
	float fadeGain; // target of this gain is the value of the mute/fade button's param (i.e. 0.0f or 1.0f)
	float fadeGainX;
	float fadeGainScaled;
	float fadeGainScaledWithSolo;
	float paramWithCV;
	float pan;// this is set only in process() when eco, and also used only (elsewhere) in process() when eco
	bool panCvConnected;
	float volCv;
	float target;
	float soloGain;

	// no need to save, no reset
	int trackNum;
	std::string ids;
	GlobalInfo *gInfo;
	Input *inSig;
	Input *inInsert;
	Input *inVol;
	Input *inPan;
	Param *paGroup;// 0.0 is no group (i.e. deposit post in mix[0..1]), 1.0 to 4.0 is group (i.e. deposit in groupTaps[2*g..2*g+1]. Always use setter since need to update gInfo!
	Param *paFade;
	Param *paMute;
	Param *paSolo;
	Param *paPan;
	char  *trackName;// write 4 chars always (space when needed), no null termination since all tracks names are concat and just one null at end of all
	float *taps;// [0],[1]: pre-insert L R; [32][33]: pre-fader L R, [64][65]: post-fader L R, [96][97]: post-mute-solo L R
	float* groupTaps;// [0..1] tap 0 of group 1, [1..2] tap 0 of group 2, etc.
	float *insertOuts;// [0][1]: insert outs for this track
	bool oldInUse = true;
	float fader = 0.0f;// this is set only in process() when eco, and also used only when eco in another section of this method


	float calcFadeGain() {return paMute->getValue() > 0.5f ? 0.0f : 1.0f;}
	bool isFadeMode() {return *fadeRate >= GlobalConst::minFadeRate;}


	void construct(int _trackNum, GlobalInfo *_gInfo, Input *_inputs, Param *_params, char* _trackName, float* _taps, float* _groupTaps, float* _insertOuts) {
		trackNum = _trackNum;
		ids = "id_t" + std::to_string(trackNum) + "_";
		gInfo = _gInfo;
		inSig = &_inputs[TRACK_SIGNAL_INPUTS + 2 * trackNum + 0];
		inInsert = &_inputs[INSERT_TRACK_INPUTS];
		inVol = &_inputs[TRACK_VOL_INPUTS + trackNum];
		inPan = &_inputs[TRACK_PAN_INPUTS + trackNum];
		paGroup = &_params[GROUP_SELECT_PARAMS + trackNum];
		paFade = &_params[TRACK_FADER_PARAMS + trackNum];
		paMute = &_params[TRACK_MUTE_PARAMS + trackNum];
		paSolo = &_params[TRACK_SOLO_PARAMS + trackNum];
		paPan = &_params[TRACK_PAN_PARAMS + trackNum];
		trackName = _trackName;
		taps = _taps;
		groupTaps = _groupTaps;
		insertOuts = _insertOuts;
		fadeRate = &(_gInfo->fadeRates[trackNum]);
		gainMatrixSlewers.setRiseFall(simd::float_4(GlobalConst::antipopSlewSlow), simd::float_4(GlobalConst::antipopSlewSlow)); // slew rate is in input-units per second (ex: V/s)
		inGainSlewer.setRiseFall(GlobalConst::antipopSlewFast, GlobalConst::antipopSlewFast); // slew rate is in input-units per second (ex: V/s)
		muteSoloGainSlewer.setRiseFall(GlobalConst::antipopSlewFast, GlobalConst::antipopSlewFast); // slew rate is in input-units per second (ex: V/s)
		for (int i = 0; i < 2; i++) {
			hpFilter[i].setParameters(dsp::BiquadFilter::HIGHPASS, 0.1, hpfBiquadQ, 0.0);
			lpFilter[i].setParameters(dsp::BiquadFilter::LOWPASS, 0.4, 0.707, 0.0);
		}
	}


	void onReset() {
		gainAdjust = 1.0f;
		*fadeRate = 0.0f;
		fadeProfile = 0.0f;
		setHPFCutoffFreq(13.0f);// off
		setLPFCutoffFreq(20010.0f);// off
		directOutsMode = 3;// post-solo should be default
		auxSendsMode = 3;// post-solo should be default
		panLawStereo = 1;
		vuColorThemeLocal = 0;
		filterPos = 1;// default is post-insert
		dispColorLocal = 0;
		panCvLevel = 1.0f;
		resetNonJson();
	}


	void resetNonJson() {
		stereo = false;
		inGain = 0.0f;
		panMatrix = 0.0f;
		faderGain = 0.0f;
		gainMatrix = 0.0f;
		gainMatrixSlewers.reset();
		inGainSlewer.reset();
		muteSoloGainSlewer.reset();
		for (int i = 0; i < 2; i++) {
			hpPreFilter[i].reset();
			hpFilter[i].reset();
			lpFilter[i].reset();
		}
		oldPan = -10.0f;
		oldFader = -10.0f;
		oldPanSignature.cc1 = 0xFFFFFFFF;
		vu.reset();
		fadeGain = calcFadeGain();
		fadeGainX = gInfo->symmetricalFade ? fadeGain : 0.0f;
		fadeGainScaled = fadeGain;// no pow needed here since 0.0f or 1.0f
		fadeGainScaledWithSolo = fadeGainScaled;
		paramWithCV = -100.0f;
		pan = 0.5f;
		panCvConnected = false;
		volCv = 1.0f;
		target = -1.0f;
		soloGain = 1.0f;
	}


	void dataToJson(json_t *rootJ) {
		// gainAdjust
		json_object_set_new(rootJ, (ids + "gainAdjust").c_str(), json_real(gainAdjust));
		
		// fadeRate
		json_object_set_new(rootJ, (ids + "fadeRate").c_str(), json_real(*fadeRate));

		// fadeProfile
		json_object_set_new(rootJ, (ids + "fadeProfile").c_str(), json_real(fadeProfile));
		
		// hpfCutoffFreq
		json_object_set_new(rootJ, (ids + "hpfCutoffFreq").c_str(), json_real(getHPFCutoffFreq()));
		
		// lpfCutoffFreq
		json_object_set_new(rootJ, (ids + "lpfCutoffFreq").c_str(), json_real(getLPFCutoffFreq()));
		
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
	}


	void dataFromJson(json_t *rootJ) {
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

		// hpfCutoffFreq
		json_t *hpfCutoffFreqJ = json_object_get(rootJ, (ids + "hpfCutoffFreq").c_str());
		if (hpfCutoffFreqJ)
			setHPFCutoffFreq(json_number_value(hpfCutoffFreqJ));
		
		// lpfCutoffFreq
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
		
		// panCvLevel
		json_t *panCvLevelJ = json_object_get(rootJ, (ids + "panCvLevel").c_str());
		if (panCvLevelJ)
			panCvLevel = json_number_value(panCvLevelJ);
		
		// extern must call resetNonJson()
	}


	// level 1 read and write
	void write(TrackSettingsCpBuffer *dest) {
		dest->gainAdjust = gainAdjust;
		dest->fadeRate = *fadeRate;
		dest->fadeProfile = fadeProfile;
		dest->hpfCutoffFreq = hpfCutoffFreq;
		dest->lpfCutoffFreq = lpfCutoffFreq;	
		dest->directOutsMode = directOutsMode;
		dest->auxSendsMode = auxSendsMode;
		dest->panLawStereo = panLawStereo;
		dest->vuColorThemeLocal = vuColorThemeLocal;
		dest->filterPos = filterPos;
		dest->dispColorLocal = dispColorLocal;
		dest->panCvLevel = panCvLevel;
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
		panCvLevel = src->panCvLevel;
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
		dest->fadeGainX = fadeGainX;
		// fadeGainScaled not really needed
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
		fadeGainX = src->fadeGainX;
		// fadeGainScaled not really needed
	}

	
	void setHPFCutoffFreq(float fc) {// always use this instead of directly accessing hpfCutoffFreq
		hpfCutoffFreq = fc;
		fc *= gInfo->sampleTime;// fc is in normalized freq for rest of method
		for (int i = 0; i < 2; i++) {
			hpPreFilter[i].setCutoff(fc);
			hpFilter[i].setParameters(dsp::BiquadFilter::HIGHPASS, fc, hpfBiquadQ, 0.0);
		}
	}
	float getHPFCutoffFreq() {return hpfCutoffFreq;}
	
	void setLPFCutoffFreq(float fc) {// always use this instead of directly accessing lpfCutoffFreq
		lpfCutoffFreq = fc;
		fc *= gInfo->sampleTime;// fc is in normalized freq for rest of method
		lpFilter[0].setParameters(dsp::BiquadFilter::LOWPASS, fc, 0.707, 0.0);
		lpFilter[1].setParameters(dsp::BiquadFilter::LOWPASS, fc, 0.707, 0.0);
	}
	float getLPFCutoffFreq() {return lpfCutoffFreq;}

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
		setHPFCutoffFreq(hpfCutoffFreq);
		setLPFCutoffFreq(lpfCutoffFreq);
	}

	
	void updateSlowValues() {
		// calc ** stereo **
		bool newStereo = inSig[1].isConnected();
		if (stereo != newStereo) {
			stereo = newStereo;
			oldPan = -10.0f;
		}
		
		// ** detect pan mode change ** (and trigger recalc of panMatrix)
		PackedBytes4 newPanSig;
		newPanSig.cc4[0] = panLawStereo;
		newPanSig.cc4[1] = gInfo->panLawStereo;
		newPanSig.cc4[2] = gInfo->panLawMono;
		newPanSig.cc4[3] = 0xFF;
		if (newPanSig.cc1 != oldPanSignature.cc1) {
			oldPan = -10.0f;
			oldPanSignature.cc1 = newPanSig.cc1;
		}

		// calc ** inGain **
		inGain = inSig[0].isConnected() ? gainAdjust : 0.0f;
		
		// soloGain
		soloGain = calcSoloGain();

		// ** process linked **
		gInfo->processLinked(trackNum, paFade->getValue());
	}
	

	void process(float *mix, bool eco) {// track		
		if (eco) {
			// calc ** fadeGain, fadeGainX, fadeGainScaled **
			if (isFadeMode()) {
				float newTarget = calcFadeGain();
				if (newTarget != target) {
					if (!gInfo->symmetricalFade) {
						fadeGainX = 0.0f;
					}
					gInfo->fadeOtherLinkedTracks(trackNum, newTarget);
					target = newTarget;
				}
				if (fadeGain != target) {
					float deltaX = (gInfo->sampleTime / *fadeRate) * (1 + (gInfo->ecoMode & 0x3));// last value is sub refresh
					fadeGain = updateFadeGain(fadeGain, target, &fadeGainX, deltaX, fadeProfile, gInfo->symmetricalFade);
					fadeGainScaled = std::pow(fadeGain, GlobalConst::trkAndGrpFaderScalingExponent);
				}
			}
			else {// we are in mute mode
				fadeGain = calcFadeGain();
				fadeGainX = gInfo->symmetricalFade ? fadeGain : 0.0f;
				fadeGainScaled = fadeGain;// no pow needed here since 0.0f or 1.0f
			}
			fadeGainScaledWithSolo = fadeGainScaled * soloGain;

			// calc ** fader, paramWithCV, volCv **
			fader = paFade->getValue();
			if (inVol->isConnected()) {
				volCv = clamp(inVol->getVoltage() * 0.1f, 0.0f, 1.0f);//(multiplying, pre-scaling)
				fader *= volCv;
				paramWithCV = fader;
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
		}


		// optimize unused track
		bool inUse = inSig[0].isConnected();
		if (!inUse) {
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
				muteSoloGainSlewer.reset();
				oldInUse = false;
			}
			return;
		}
		oldInUse = true;
			
		// Calc inGainSlewed
		if (inGain != inGainSlewer.out) {
			inGainSlewer.process(gInfo->sampleTime, inGain);
		}
		
		// Tap[0],[1]: pre-insert (Inputs with gain adjust)
		taps[0] = (clamp20V(inSig[0].getVoltageSum() * inGainSlewer.out));
		taps[1] = stereo ? (clamp20V(inSig[1].getVoltageSum() * inGainSlewer.out)) : taps[0];
		
		int insertPortIndex = trackNum >> 3;
		
		if (gInfo->filterPos == 1 || (gInfo->filterPos == 2 && filterPos == 1)) {// if filters post insert
			// Insert outputs
			insertOuts[0] = taps[0];
			insertOuts[1] = stereo ? taps[1] : 0.0f;// don't send to R of insert outs when mono
			
			// Post insert (taps[32..33] are provisional, since not yet filtered)
			if (inInsert[insertPortIndex].isConnected()) {
				taps[N_TRK * 2 + 0] = clamp20V(inInsert[insertPortIndex].getVoltage(((trackNum & 0x7) << 1) + 0));
				taps[N_TRK * 2 + 1] = stereo ? clamp20V(inInsert[insertPortIndex].getVoltage(((trackNum & 0x7) << 1) + 1)) : taps[N_TRK * 2 + 0];// don't receive from R of insert outs when mono, just normal L into R (need this for aux sends)
			}
			else {
				taps[N_TRK * 2 + 0] = taps[0];
				taps[N_TRK * 2 + 1] = taps[1];
			}

			// Filters
			// Tap[32],[33]: pre-fader (post insert)
			// HPF
			if (getHPFCutoffFreq() >= GlobalConst::minHPFCutoffFreq) {
				taps[N_TRK * 2 + 0] = hpFilter[0].process(hpPreFilter[0].processHP(taps[N_TRK * 2 + 0]));
				taps[N_TRK * 2 + 1] = stereo ? hpFilter[1].process(hpPreFilter[1].processHP(taps[N_TRK * 2 + 1])) : taps[N_TRK * 2 + 0];
			}
			// LPF
			if (getLPFCutoffFreq() <= GlobalConst::maxLPFCutoffFreq) {
				taps[N_TRK * 2 + 0] = lpFilter[0].process(taps[N_TRK * 2 + 0]);
				taps[N_TRK * 2 + 1]  = stereo ? lpFilter[1].process(taps[N_TRK * 2 + 1]) : taps[N_TRK * 2 + 0];
			}
		}
		else {// filters before inserts
			// Filters
			float filtered[2] = {taps[0], taps[1]};
			// HPF
			if (getHPFCutoffFreq() >= GlobalConst::minHPFCutoffFreq) {
				filtered[0] = hpFilter[0].process(hpPreFilter[0].processHP(filtered[0]));
				filtered[1] = stereo ? hpFilter[1].process(hpPreFilter[1].processHP(filtered[1])) : filtered[0];
			}
			// LPF
			if (getLPFCutoffFreq() <= GlobalConst::maxLPFCutoffFreq) {
				filtered[0] = lpFilter[0].process(filtered[0]);
				filtered[1] = stereo ? lpFilter[1].process(filtered[1]) : filtered[0];
			}
			
			// Insert outputs
			insertOuts[0] = filtered[0];
			insertOuts[1] = stereo ? filtered[1] : 0.0f;// don't send to R of insert outs when mono!
			
			// Tap[32],[33]: pre-fader (post insert)
			if (inInsert[insertPortIndex].isConnected()) {
				taps[N_TRK * 2 + 0] = clamp20V(inInsert[insertPortIndex].getVoltage(((trackNum & 0x7) << 1) + 0));
				taps[N_TRK * 2 + 1] = stereo ? clamp20V(inInsert[insertPortIndex].getVoltage(((trackNum & 0x7) << 1) + 1)) : taps[N_TRK * 2 + 0];// don't receive from R of insert outs when mono, just normal L into R (need this for aux sends)
			}
			else {
				taps[N_TRK * 2 + 0] = filtered[0];
				taps[N_TRK * 2 + 1] = filtered[1];
			}
		}// filterPos
		
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
							// panMatrix[3] = std::sin(pan * M_PI_2) * M_SQRT2;
							// panMatrix[0] = std::cos(pan * M_PI_2) * M_SQRT2;
							sinCosSqrt2(&panMatrix[3], &panMatrix[0], pan * M_PI_2);
						}
						else {//if (gInfo->panLawMono == 2) {
							// Compromise (+4.5dB boost)
							// panMatrix[3] = std::sqrt( std::abs( std::sin(pan * M_PI_2) * M_SQRT2   *   (pan * 2.0f) ) );
							// panMatrix[0] = std::sqrt( std::abs( std::cos(pan * M_PI_2) * M_SQRT2   *   (2.0f - pan * 2.0f) ) );
							sinCosSqrt2(&panMatrix[3], &panMatrix[0], pan * M_PI_2);
							panMatrix[3] = std::sqrt( std::abs( panMatrix[3] * (pan * 2.0f) ) );
							panMatrix[0] = std::sqrt( std::abs( panMatrix[0] * (2.0f - pan * 2.0f) ) );
						}
					}
					else {// stereo
						int stereoPanMode = (gInfo->panLawStereo < 3 ? gInfo->panLawStereo : panLawStereo);			
						if (stereoPanMode == 0) {
							// Stereo balance linear, (+0 dB), same as mono No compensation
							panMatrix[1] = std::min(1.0f, pan * 2.0f);
							panMatrix[0] = std::min(1.0f, 2.0f - pan * 2.0f);
						}
						else if (stereoPanMode == 1) {
							// Stereo balance equal power (+3dB), same as mono Equal power
							// panMatrix[1] = std::sin(pan * M_PI_2) * M_SQRT2;
							// panMatrix[0] = std::cos(pan * M_PI_2) * M_SQRT2;
							sinCosSqrt2(&panMatrix[1], &panMatrix[0], pan * M_PI_2);
						}
						else {
							// True panning, equal power
							// panMatrix[1] = pan >= 0.5f ? (1.0f) : (std::sin(pan * M_PI));
							// panMatrix[2] = pan >= 0.5f ? (0.0f) : (std::cos(pan * M_PI));
							// panMatrix[0] = pan <= 0.5f ? (1.0f) : (std::cos((pan - 0.5f) * M_PI));
							// panMatrix[3] = pan <= 0.5f ? (0.0f) : (std::sin((pan - 0.5f) * M_PI));
							if (pan > 0.5f) {
								panMatrix[1] = 1.0f;
								panMatrix[2] = 0.0f;
								sinCos(&panMatrix[3], &panMatrix[0], (pan - 0.5f) * M_PI);
							}
							else {// must be < (not <= since = 0.5 is caught at above)
								sinCos(&panMatrix[1], &panMatrix[2], pan * M_PI);
								panMatrix[0] = 1.0f;
								panMatrix[3] = 0.0f;
							}
						}
					}
				}
			}
			// calc ** faderGain **
			if (fader != oldFader) {
				faderGain = std::pow(fader, GlobalConst::trkAndGrpFaderScalingExponent);// scaling
			}
			// calc ** gainMatrix **
			if (fader != oldFader || pan != oldPan) {
				oldFader = fader;
				oldPan = pan;	
				gainMatrix = panMatrix * faderGain;
			}
		}
		
		// Calc gainMatrixSlewed
		if (movemask(gainMatrix == gainMatrixSlewers.out) != 0xF) {// movemask returns 0xF when 4 floats are equal
			gainMatrixSlewers.process(gInfo->sampleTime, gainMatrix);
		}
		
		// Tap[64],[65]: post-fader

		// Apply gainMatrixSlewed
		simd::float_4 sigs(taps[N_TRK * 2 + 0], taps[N_TRK * 2 + 1], taps[N_TRK * 2 + 1], taps[N_TRK * 2 + 0]);
		sigs *= gainMatrixSlewers.out;
		
		taps[N_TRK * 4 + 0] = sigs[0] + sigs[2];
		taps[N_TRK * 4 + 1] = sigs[1] + sigs[3];


		// Tap[96],[97]: post-mute-solo
		
		// Calc muteSoloGainSlewed
		if (fadeGainScaledWithSolo != muteSoloGainSlewer.out) {
			muteSoloGainSlewer.process(gInfo->sampleTime, fadeGainScaledWithSolo);
		}

		taps[N_TRK * 6 + 0] = taps[N_TRK * 4 + 0] * muteSoloGainSlewer.out;
		taps[N_TRK * 6 + 1] = taps[N_TRK * 4 + 1] * muteSoloGainSlewer.out;
			
		// Add to mix since gainMatrixSlewed can be non zero
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
			vu.process(gInfo->sampleTime * (1 + (gInfo->ecoMode & 0x3)), &taps[N_TRK * 6 + 0]);
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
	float faderGain;
	simd::float_4 gainMatrix;	
	dsp::TSlewLimiter<simd::float_4> gainMatrixSlewers;
	dsp::SlewLimiter muteSoloGainSlewer;
	float oldPan;
	float oldFader;
	PackedBytes4 oldPanSignature;// [0] is pan stereo local, [1] is pan stereo global, [2] is pan mono global
	public:
	float fadeGain; // target of this gain is the value of the mute/fade button's param (i.e. 0.0f or 1.0f)
	float fadeGainX;
	float fadeGainScaled;
	float fadeGainScaledWithSolo;
	float target;
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
	float calcFadeGain() {return *flMute > 0.5f ? 0.0f : 1.0f;}
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
		gainMatrixSlewers.setRiseFall(simd::float_4(GlobalConst::antipopSlewSlow), simd::float_4(GlobalConst::antipopSlewSlow)); // slew rate is in input-units per second (ex: V/s)
		muteSoloGainSlewer.setRiseFall(GlobalConst::antipopSlewFast, GlobalConst::antipopSlewFast); // slew rate is in input-units per second (ex: V/s)
	}


	void onReset() {
		resetNonJson();
	}


	void resetNonJson() {
		panMatrix = 0.0f;
		faderGain = 0.0f;
		gainMatrix = 0.0f;
		gainMatrixSlewers.reset();
		muteSoloGainSlewer.reset();
		oldPan = -10.0f;
		oldFader = -10.0f;
		oldPanSignature.cc1 = 0xFFFFFFFF;
		fadeGain = calcFadeGain();
		fadeGainX = gInfo->symmetricalFade ? fadeGain : 0.0f;
		fadeGainScaled = fadeGain;// no pow needed here since 0.0f or 1.0f
		fadeGainScaledWithSolo = fadeGainScaled;
		target = -1.0f;
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
		newPanSig.cc4[1] = gInfo->panLawStereo;
		newPanSig.cc4[2] = gInfo->panLawMono;
		newPanSig.cc4[3] = 0xFF;
		if (newPanSig.cc1 != oldPanSignature.cc1) {
			oldPan = -10.0f;
			oldPanSignature.cc1 = newPanSig.cc1;
		}
		
		// soloGain
		soloGain = calcSoloGain();
	}
	
	void process(float *mix, float *auxRetFadePan, bool eco) {// mixer aux
		// auxRetFadePan[0] points fader value, auxRetFadePan[4] points pan value, all indexed for a given aux
		

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
			// calc ** fadeGain, fadeGainX, fadeGainScaled **
			if (isFadeMode()) {
				float newTarget = calcFadeGain();
				if (newTarget != target) {
					if (!gInfo->symmetricalFade) {
						fadeGainX = 0.0f;
					}
					target = newTarget;
				}
				if (fadeGain != target) {
					float deltaX = (gInfo->sampleTime / *fadeRate) * (1 + (gInfo->ecoMode & 0x3));// last value is sub refresh
					fadeGain = updateFadeGain(fadeGain, target, &fadeGainX, deltaX, *fadeProfile, gInfo->symmetricalFade);
					fadeGainScaled = std::pow(fadeGain, GlobalConst::globalAuxReturnScalingExponent);
				}
			}
			else {// we are in mute mode
				fadeGain = calcFadeGain();
				fadeGainX = gInfo->symmetricalFade ? fadeGain : 0.0f;
				fadeGainScaled = fadeGain;// no pow needed here since 0.0f or 1.0f
			}
			fadeGainScaledWithSolo = fadeGainScaled * soloGain;


			// calc ** panMatrix **
			float pan = auxRetFadePan[4];// cv input and clamping already done in auxspander
			if (pan != oldPan) {
				panMatrix = 0.0f;// L, R, RinL, LinR (used for fader-pan block)
				if (pan == 0.5f) {
					panMatrix[1] = 1.0f;
					panMatrix[0] = 1.0f;
				}
				else {	
					// implicitly stereo for aux
					int stereoPanMode = (gInfo->panLawStereo < 3 ? gInfo->panLawStereo : *panLawStereoLocal);
					if (stereoPanMode == 0) {
						// Stereo balance linear, (+0 dB), same as mono No compensation
						panMatrix[1] = std::min(1.0f, pan * 2.0f);
						panMatrix[0] = std::min(1.0f, 2.0f - pan * 2.0f);
					}
					else if (stereoPanMode == 1) {
						// Stereo balance equal power (+3dB), same as mono Equal power
						// panMatrix[1] = std::sin(pan * M_PI_2) * M_SQRT2;
						// panMatrix[0] = std::cos(pan * M_PI_2) * M_SQRT2;
						sinCosSqrt2(&panMatrix[1], &panMatrix[0], pan * M_PI_2);
					}
					else {
						// True panning, equal power
						// panMatrix[1] = pan >= 0.5f ? (1.0f) : (std::sin(pan * M_PI));
						// panMatrix[2] = pan >= 0.5f ? (0.0f) : (std::cos(pan * M_PI));
						// panMatrix[0] = pan <= 0.5f ? (1.0f) : (std::cos((pan - 0.5f) * M_PI));
						// panMatrix[3] = pan <= 0.5f ? (0.0f) : (std::sin((pan - 0.5f) * M_PI));
						if (pan > 0.5f) {
							panMatrix[1] = 1.0f;
							panMatrix[2] = 0.0f;
							sinCos(&panMatrix[3], &panMatrix[0], (pan - 0.5f) * M_PI);
						}
						else {// must be < (not <= since = 0.5 is caught at above)
							sinCos(&panMatrix[1], &panMatrix[2], pan * M_PI);
							panMatrix[0] = 1.0f;
							panMatrix[3] = 0.0f;
						}
					}
				}
			}
			// calc ** faderGain **
			if (auxRetFadePan[0] != oldFader) {
				// fader and fader cv input (multiplying and pre-scaling) both done in auxspander
				faderGain = std::pow(auxRetFadePan[0], GlobalConst::globalAuxReturnScalingExponent);// scaling
			}
			// calc ** gainMatrix **
			if (auxRetFadePan[0] != oldFader || pan != oldPan) {
				oldFader = auxRetFadePan[0];
				oldPan = pan;	
				gainMatrix = panMatrix * faderGain;
			}			
		}
		
		// Calc gainMatrixSlewed
		if (movemask(gainMatrix == gainMatrixSlewers.out) != 0xF) {// movemask returns 0xF when 4 floats are equal
			gainMatrixSlewers.process(gInfo->sampleTime, gainMatrix);
		}
		
		// Apply gainMatrixSlewed
		simd::float_4 sigs(taps[8], taps[9], taps[9], taps[8]);
		sigs = sigs * gainMatrixSlewers.out;
		
		taps[16] = sigs[0] + sigs[2];
		taps[17] = sigs[1] + sigs[3];
		
		// Calc muteSoloGainSlewed
		if (fadeGainScaledWithSolo != muteSoloGainSlewer.out) {
			muteSoloGainSlewer.process(gInfo->sampleTime, fadeGainScaledWithSolo);
		}
		
		taps[24] = taps[16] * muteSoloGainSlewer.out;
		taps[25] = taps[17] * muteSoloGainSlewer.out;
			
		mix[0] += taps[24];
		mix[1] += taps[25];
	}
};// struct MixerAux


