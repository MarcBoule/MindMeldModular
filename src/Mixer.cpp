//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.txt for all licenses
//***********************************************************************************************


#include "Mixer.hpp"


//*****************************************************************************


// Math

// none


// Utility

float updateFadeGain(float fadeGain, float target, float *fadeGainX, float timeStepX, float shape, bool symmetricalFade) {
	static const float A = 4.0f;
	static const float E_A_M1 = (std::exp(A) - 1.0f);// e^A - 1
	
	float newFadeGain;
	
	if (symmetricalFade) {
		// in here, target is intepreted as a targetX, which is same levels as a target on fadeGain (Y) since both are 0.0f or 1.0f
		// in here, fadeGain is not used since we compute a new fadeGain directly, as opposed to a delta
		if (target < *fadeGainX) {
			*fadeGainX -= timeStepX;
			if (*fadeGainX < target) {
				*fadeGainX = target;
			}
		}
		else if (target > *fadeGainX) {
			*fadeGainX += timeStepX;
			if (*fadeGainX > target) {
				*fadeGainX = target;
			}
		}
		
		newFadeGain = *fadeGainX;// linear
		if (*fadeGainX != target) {
			if (shape > 0.0f) {	
				float expY = (std::exp(A * *fadeGainX) - 1.0f)/E_A_M1;
				newFadeGain = crossfade(newFadeGain, expY, shape);
			}
			else if (shape < 0.0f) {
				float logY = std::log(*fadeGainX * E_A_M1 + 1.0f) / A;
				newFadeGain = crossfade(newFadeGain, logY, -1.0f * shape);		
			}
		}
	}
	else {// asymmetrical fade
		float fadeGainDelta = timeStepX;// linear
		
		if (shape > 0.0f) {	
			float fadeGainDeltaExp = (std::exp(A * (*fadeGainX + timeStepX)) - std::exp(A * (*fadeGainX))) / E_A_M1;
			fadeGainDelta = crossfade(fadeGainDelta, fadeGainDeltaExp, shape);
		}
		else if (shape < 0.0f) {
			float fadeGainDeltaLog = (std::log((*fadeGainX + timeStepX) * E_A_M1 + 1.0f) - std::log((*fadeGainX) * E_A_M1 + 1.0f)) / A;
			fadeGainDelta = crossfade(fadeGainDelta, fadeGainDeltaLog, -1.0f * shape);		
		}
		
		newFadeGain = fadeGain;
		if (target > fadeGain) {
			newFadeGain += fadeGainDelta;
		}
		else if (target < fadeGain) {
			newFadeGain -= fadeGainDelta;
		}	

		if (target > fadeGain && target < newFadeGain) {
			newFadeGain = target;
		}
		else if (target < fadeGain && target > newFadeGain) {
			newFadeGain = target;
		}
		else {
			*fadeGainX += timeStepX;
		}
	}
	
	return newFadeGain;
}


void TrackSettingsCpBuffer::reset() {
	// first level
	gainAdjust = 1.0f;
	fadeRate = 0.0f;
	fadeProfile = 0.0f;
	hpfCutoffFreq = 13.0f;// !! user must call filters' setCutoffs manually when copy pasting these
	lpfCutoffFreq = 20010.0f;// !! user must call filters' setCutoffs manually when copy pasting these
	directOutsMode = 3;
	auxSendsMode = 3;
	panLawStereo = 1;
	vuColorThemeLocal = 0;
	linkedFader = false;
	
	// second level
	paGroup = 0.0f;
	paFade = 1.0f;
	paMute = 0.0f;
	paSolo = 0.0f;
	paPan = 0.5f;
	trackName[0] = '-'; trackName[0] = '0'; trackName[0] = '0'; trackName[0] = '-';
	fadeGain = 1.0f;
	fadeGainX = 0.0f;
}



//*****************************************************************************


//struct GlobalInfo
	
void GlobalInfo::construct(Param *_params, float* _values12) {
	paSolo = &_params[TRACK_SOLO_PARAMS];
	paFade = &_params[TRACK_FADER_PARAMS];
	values12 = _values12;
	maxTGFader = std::pow(trkAndGrpFaderMaxLinearGain, 1.0f / trkAndGrpFaderScalingExponent);
}


void GlobalInfo::onReset() {
	panLawMono = 1;
	panLawStereo = 1;
	directOutsMode = 3;// post-solo should be default
	auxSendsMode = 3;// post-solo should be default
	muteTrkSendsWhenGrpMuted = 0;
	auxReturnsMutedWhenMainSolo = 0;
	auxReturnsSolosMuteDry = 0;
	chainMode = 1;// post should be default
	colorAndCloak.cc4[cloakedMode] = 0;
	colorAndCloak.cc4[vuColorGlobal] = 0;
	colorAndCloak.cc4[dispColor] = 0;
	colorAndCloak.cc4[detailsShow] = 0x7;
	for (int i = 0; i < 4; i++) {
		groupUsage[i] = 0;
	}
	symmetricalFade = false;
	fadeCvOutsWithVolCv = false;
	linkBitMask = 0;
	filterPos = 1;// default is post-insert
	groupedAuxReturnFeedbackProtection = 1;// protection is on by default
	ecoMode = 0xFFFF;// all 1's means yes, 0 means no
	resetNonJson();
}


void GlobalInfo::resetNonJson() {
	updateSoloBitMask();
	updateReturnSoloBits();
	sampleTime = APP->engine->getSampleTime();
}


void GlobalInfo::dataToJson(json_t *rootJ) {
	// panLawMono 
	json_object_set_new(rootJ, "panLawMono", json_integer(panLawMono));

	// panLawStereo
	json_object_set_new(rootJ, "panLawStereo", json_integer(panLawStereo));

	// directOutsMode
	json_object_set_new(rootJ, "directOutsMode", json_integer(directOutsMode));
	
	// auxSendsMode
	json_object_set_new(rootJ, "auxSendsMode", json_integer(auxSendsMode));
	
	// muteTrkSendsWhenGrpMuted
	json_object_set_new(rootJ, "muteTrkSendsWhenGrpMuted", json_integer(muteTrkSendsWhenGrpMuted));
	
	// auxReturnsMutedWhenMainSolo
	json_object_set_new(rootJ, "auxReturnsMutedWhenMainSolo", json_integer(auxReturnsMutedWhenMainSolo));
	
	// auxReturnsSolosMuteDry
	json_object_set_new(rootJ, "auxReturnsSolosMuteDry", json_integer(auxReturnsSolosMuteDry));
	
	// chainMode
	json_object_set_new(rootJ, "chainMode", json_integer(chainMode));
	
	// colorAndCloak
	json_object_set_new(rootJ, "colorAndCloak", json_integer(colorAndCloak.cc1));
	
	// groupUsage does not need to be saved here, it is computed indirectly in MixerTrack::dataFromJson();
	
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
}


void GlobalInfo::dataFromJson(json_t *rootJ) {
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
	
	// muteTrkSendsWhenGrpMuted
	json_t *muteTrkSendsWhenGrpMutedJ = json_object_get(rootJ, "muteTrkSendsWhenGrpMuted");
	if (muteTrkSendsWhenGrpMutedJ)
		muteTrkSendsWhenGrpMuted = json_integer_value(muteTrkSendsWhenGrpMutedJ);
	
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
	
	// groupUsage does not need to be loaded here, it is computed indirectly in MixerTrack::dataFromJson();
	
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
	
	// extern must call resetNonJson()
}


//*****************************************************************************


// struct MixerMaster 

void MixerMaster::construct(GlobalInfo *_gInfo, Param *_params, Input *_inputs) {
	gInfo = _gInfo;
	params = _params;
	inChain = &_inputs[CHAIN_INPUTS];
	inVol = &_inputs[GRPM_MUTESOLO_INPUT];
	gainMatrixSlewers.setRiseFall(simd::float_4(GlobalInfo::antipopSlew), simd::float_4(GlobalInfo::antipopSlew)); // slew rate is in input-units per second (ex: V/s)
	chainGainSlewers[0].setRiseFall(GlobalInfo::antipopSlew, GlobalInfo::antipopSlew); // slew rate is in input-units per second (ex: V/s)
	chainGainSlewers[1].setRiseFall(GlobalInfo::antipopSlew, GlobalInfo::antipopSlew); // slew rate is in input-units per second (ex: V/s)
}


void MixerMaster::onReset() {
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


void MixerMaster::resetNonJson() {
	chainGains[0] = 0.0f;
	chainGains[1] = 0.0f;
	faderGain = 0.0f;
	gainMatrix = simd::float_4::zero();
	gainMatrixSlewers.reset();
	chainGainSlewers[0].reset();
	chainGainSlewers[1].reset();
	setupDcBlocker();
	oldFader = -10.0f;
	vu.reset();
	fadeGain = calcFadeGain();
	fadeGainX = gInfo->symmetricalFade ? fadeGain : 0.0f;
	paramWithCV = -1.0f;
	updateDimGainIntegerDB();
	target = -1.0f;
}


void MixerMaster::dataToJson(json_t *rootJ) {
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


void MixerMaster::dataFromJson(json_t *rootJ) {
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



//*****************************************************************************

/*

struct MixerGroup {
	// Constants
	static constexpr float minFadeRate = 0.1f;
	
	// need to save, no reset
	// none
	
	// need to save, with reset
	float fadeRate; // mute when < minFadeRate, fade when >= minFadeRate. This is actually the fade time in seconds
	float fadeProfile; // exp when +100, lin when 0, log when -100
	int8_t directOutsMode;// when per track
	int8_t auxSendsMode;// when per track
	int8_t panLawStereo;// when per track
	int8_t vuColorThemeLocal;
	int8_t dispColorLocal;

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
	VuMeterAllDual vu;// use post[]
	float fadeGain; // target of this gain is the value of the mute/fade button's param (i.e. 0.0f or 1.0f)
	float fadeGainX;
	float fadeGainScaled;
	float paramWithCV;
	float panWithCV;
	float target = -1.0f;
	
	// no need to save, no reset
	int groupNum;// 0 to 3
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
	bool isLinked() {return gInfo->isLinked(16 + groupNum);}
	void toggleLinked() {gInfo->toggleLinked(16 + groupNum);}


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
		gainMatrixSlewers.setRiseFall(simd::float_4(GlobalInfo::antipopSlew), simd::float_4(GlobalInfo::antipopSlew)); // slew rate is in input-units per second (ex: V/s)
		muteSoloGainSlewer.setRiseFall(GlobalInfo::antipopSlew, GlobalInfo::antipopSlew); // slew rate is in input-units per second (ex: V/s)
	}
	
	
	void onReset() {
		fadeRate = 0.0f;
		fadeProfile = 0.0f;
		directOutsMode = 3;// post-solo should be default
		auxSendsMode = 3;// post-solo should be default
		panLawStereo = 1;
		vuColorThemeLocal = 0;
		dispColorLocal = 0;
		resetNonJson();
	}
	void resetNonJson() {
		panMatrix = simd::float_4::zero();
		faderGain = 0.0f;
		gainMatrix = simd::float_4::zero();
		gainMatrixSlewers.reset();
		muteSoloGainSlewer.reset();
		oldPan = -10.0f;
		oldFader = -10.0f;
		oldPanSignature.cc1 = 0xFFFFFFFF;
		vu.reset();
		fadeGain = calcFadeGain();
		fadeGainX = gInfo->symmetricalFade ? fadeGain : 0.0f;
		fadeGainScaled = fadeGain;// no pow needed here since 0.0f or 1.0f
		paramWithCV = -1.0f;
		panWithCV = -1.0f;
		target = -1.0f;
	}
	
	
	void updateSlowValues() {
		// ** process linked **
		gInfo->processLinked(16 + groupNum, paFade->getValue());
		
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
			taps[8] = inInsert->getVoltage((groupNum << 1) + 0);
			taps[9] = inInsert->getVoltage((groupNum << 1) + 1);
		}
		else {
			taps[8] = taps[0];
			taps[9] = taps[1];
		}

		if (eco) {	
			// calc ** fadeGain, fadeGainX, fadeGainScaled **
			if (fadeRate >= minFadeRate) {// if we are in fade mode
				float newTarget = calcFadeGain();
				if (!gInfo->symmetricalFade && newTarget != target) {
					fadeGainX = 0.0f;
				}
				target = newTarget;
				if (fadeGain != target) {
					float deltaX = (gInfo->sampleTime / fadeRate) * (1 + (gInfo->ecoMode & 0x3));// last value is sub refresh
					fadeGain = updateFadeGain(fadeGain, target, &fadeGainX, deltaX, fadeProfile, gInfo->symmetricalFade);
					fadeGainScaled = std::pow(fadeGain, GlobalInfo::trkAndGrpFaderScalingExponent);
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
				paramWithCV = -1.0f;
			}

			// calc ** pan, panWithCV **
			float pan = paPan->getValue();
			if (inPan->isConnected()) {
				pan += inPan->getVoltage() * 0.1f;// CV is a -5V to +5V input
				pan = clamp(pan, 0.0f, 1.0f);
				panWithCV = pan;
			}
			else {
				panWithCV = -1.0f;
			}

			// calc ** panMatrix **
			if (pan != oldPan) {
				panMatrix = simd::float_4::zero();// L, R, RinL, LinR (used for fader-pan block)
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
				faderGain = std::pow(fader, GlobalInfo::trkAndGrpFaderScalingExponent);// scaling
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
		simd::float_4 sigs(taps[8], taps[9], taps[9], taps[8]);
		sigs = sigs * gainMatrixSlewers.out;
		
		taps[16] = sigs[0] + sigs[2];
		taps[17] = sigs[1] + sigs[3];
		
		// Calc muteSoloGainSlewed (solo not actually in here but in groups)
		if (fadeGainScaled != muteSoloGainSlewer.out) {
			muteSoloGainSlewer.process(gInfo->sampleTime, fadeGainScaled);
		}
		
		taps[24] = taps[16] * muteSoloGainSlewer.out;
		taps[25] = taps[17] * muteSoloGainSlewer.out;
			
		// Add to mix
		mix[0] += taps[24];
		mix[1] += taps[25];
		
		// VUs
		if (gInfo->colorAndCloak.cc4[cloakedMode] != 0) {
			vu.reset();
		}
		else if (eco) {
			vu.process(gInfo->sampleTime * (1 + (gInfo->ecoMode & 0x3)), &taps[24]);
		}
	}
	
	
	void dataToJson(json_t *rootJ) {
		// fadeRate
		json_object_set_new(rootJ, (ids + "fadeRate").c_str(), json_real(fadeRate));
		
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
	}
	
	void dataFromJson(json_t *rootJ) {
		// fadeRate
		json_t *fadeRateJ = json_object_get(rootJ, (ids + "fadeRate").c_str());
		if (fadeRateJ)
			fadeRate = json_number_value(fadeRateJ);
		
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
		
		// extern must call resetNonJson()
	}
};// struct MixerGroup



//*****************************************************************************


struct MixerTrack {
	// Constants
	static constexpr float minHPFCutoffFreq = 20.0f;
	static constexpr float maxLPFCutoffFreq = 20000.0f;
	static constexpr float minFadeRate = 0.1f;
	static constexpr float hpfBiquadQ = 1.0f;// 1.0 Q since preceeeded by a one pole filter to get 18dB/oct
	
	// need to save, no reset
	// none
	
	// need to save, with reset
	float gainAdjust;// this is a gain here (not dB)
	float fadeRate; // mute when < minFadeRate, fade when >= minFadeRate. This is actually the fade time in seconds
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
	float paramWithCV;
	float panWithCV;
	float volCv;
	float target;

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
	float pan = 0.5f;// this is set only in process() when eco, and also used only when eco in another section of this method


	float calcFadeGain() {return paMute->getValue() > 0.5f ? 0.0f : 1.0f;}
	bool isLinked() {return gInfo->isLinked(trackNum);}
	void toggleLinked() {gInfo->toggleLinked(trackNum);}


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
		gainMatrixSlewers.setRiseFall(simd::float_4(GlobalInfo::antipopSlew), simd::float_4(GlobalInfo::antipopSlew)); // slew rate is in input-units per second (ex: V/s)
		inGainSlewer.setRiseFall(GlobalInfo::antipopSlew, GlobalInfo::antipopSlew); // slew rate is in input-units per second (ex: V/s)
		muteSoloGainSlewer.setRiseFall(GlobalInfo::antipopSlew, GlobalInfo::antipopSlew); // slew rate is in input-units per second (ex: V/s)
		for (int i = 0; i < 2; i++) {
			hpFilter[i].setParameters(dsp::BiquadFilter::HIGHPASS, 0.1, hpfBiquadQ, 0.0);
			lpFilter[i].setParameters(dsp::BiquadFilter::LOWPASS, 0.4, 0.707, 0.0);
		}
	}
	
	
	void onReset() {
		gainAdjust = 1.0f;
		fadeRate = 0.0f;
		fadeProfile = 0.0f;
		setHPFCutoffFreq(13.0f);// off
		setLPFCutoffFreq(20010.0f);// off
		directOutsMode = 3;// post-solo should be default
		auxSendsMode = 3;// post-solo should be default
		panLawStereo = 1;
		vuColorThemeLocal = 0;
		filterPos = 1;// default is post-insert
		dispColorLocal = 0;
		resetNonJson();
	}
	void resetNonJson() {
		stereo = false;
		inGain = 0.0f;
		panMatrix = simd::float_4::zero();
		faderGain = 0.0f;
		gainMatrix = simd::float_4::zero();
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
		paramWithCV = -1.0f;
		panWithCV = -1.0f;
		volCv = 1.0f;
		target = -1.0f;
	}
	
	
	// level 1 read and write
	void write(TrackSettingsCpBuffer *dest) {
		dest->gainAdjust = gainAdjust;
		dest->fadeRate = fadeRate;
		dest->fadeProfile = fadeProfile;
		dest->hpfCutoffFreq = hpfCutoffFreq;
		dest->lpfCutoffFreq = lpfCutoffFreq;	
		dest->directOutsMode = directOutsMode;
		dest->auxSendsMode = auxSendsMode;
		dest->panLawStereo = panLawStereo;
		dest->vuColorThemeLocal = vuColorThemeLocal;
		dest->linkedFader = gInfo->isLinked(trackNum);
	}
	void read(TrackSettingsCpBuffer *src) {
		gainAdjust = src->gainAdjust;
		fadeRate = src->fadeRate;
		fadeProfile = src->fadeProfile;
		setHPFCutoffFreq(src->hpfCutoffFreq);
		setLPFCutoffFreq(src->lpfCutoffFreq);	
		directOutsMode = src->directOutsMode;
		auxSendsMode = src->auxSendsMode;
		panLawStereo = src->panLawStereo;
		vuColorThemeLocal = src->vuColorThemeLocal;
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
	
	
	void updateGroupUsage() {
		// clear groupUsage for this track in all groups
		for (int i = 0; i < 4; i++) {
			gInfo->groupUsage[i] &= ~(1 << trackNum);
		}
		
		// set groupUsage for this track in the new group
		int group = (int)(paGroup->getValue() + 0.5f);
		if (group > 0) {
			gInfo->groupUsage[group - 1] |= (1 << trackNum);
		}
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
			if (group == 0 || ((gInfo->soloBitMask & 0xF0000) == 0)) {// not grouped, or grouped but no groups are soloed, play
				return 1.0f;
			}
			// grouped and at least one group is soloed, so play only if its group is itself soloed
			return ( (gInfo->soloBitMask & (1ul << (16 + group - 1))) != 0ul ? 1.0f : 0.0f);
		}
		// here this track is not soloed
		if ( (group != 0) && ( (gInfo->soloBitMask & (1ul << (16 + group - 1))) != 0ul ) ) {// if going through soloed group  
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
		// ** update group usage **
		updateGroupUsage();
		
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

		// ** process linked **
		gInfo->processLinked(trackNum, paFade->getValue());
	}
	
	
	void process(float *mix, bool eco) {// track		
		if (eco) {
			// calc ** fadeGain, fadeGainX, fadeGainScaled **
			if (fadeRate >= minFadeRate) {// if we are in fade mode
				float newTarget = calcFadeGain();
				if (!gInfo->symmetricalFade && newTarget != target) {
					fadeGainX = 0.0f;
				}
				target = newTarget;
				if (fadeGain != target) {
					float deltaX = (gInfo->sampleTime / fadeRate) * (1 + (gInfo->ecoMode & 0x3));// last value is sub refresh
					fadeGain = updateFadeGain(fadeGain, target, &fadeGainX, deltaX, fadeProfile, gInfo->symmetricalFade);
					fadeGainScaled = std::pow(fadeGain, GlobalInfo::trkAndGrpFaderScalingExponent);
				}
			}
			else {// we are in mute mode
				fadeGain = calcFadeGain();
				fadeGainX = gInfo->symmetricalFade ? fadeGain : 0.0f;
				fadeGainScaled = fadeGain;// no pow needed here since 0.0f or 1.0f
			}
			fadeGainScaled *= calcSoloGain();

			// calc ** fader, paramWithCV, volCv **
			fader = paFade->getValue();
			if (inVol->isConnected()) {
				volCv = clamp(inVol->getVoltage() * 0.1f, 0.0f, 1.0f);//(multiplying, pre-scaling)
				fader *= volCv;
				paramWithCV = fader;
			}
			else {
				volCv = 1.0f;
				paramWithCV = -1.0f;
			}

			// calc ** pan, panWithCV **
			pan = paPan->getValue();
			if (inPan->isConnected()) {
				pan += inPan->getVoltage() * 0.1f;// CV is a -5V to +5V input
				pan = clamp(pan, 0.0f, 1.0f);
				panWithCV = pan;
			}
			else {
				panWithCV = -1.0f;
			}
		}


		// optimize unused track
		bool inUse = inSig[0].isConnected();
		if (!inUse) {
			if (oldInUse) {
				taps[0] = 0.0f; taps[1] = 0.0f;
				taps[32] = 0.0f; taps[33] = 0.0f;
				taps[64] = 0.0f; taps[65] = 0.0f;
				taps[96] = 0.0f; taps[97] = 0.0f;
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
		taps[0] = (inSig[0].getVoltage() * inGainSlewer.out);
		taps[1] = stereo ? (inSig[1].getVoltage() * inGainSlewer.out) : taps[0];
		
		int insertPortIndex = trackNum >> 3;
		
		if (gInfo->filterPos == 1 || (gInfo->filterPos == 2 && filterPos == 1)) {
			// Insert outputs
			insertOuts[0] = taps[0];
			insertOuts[1] = stereo ? taps[1] : 0.0f;// don't send to R of insert outs when mono
			
			// Post insert (taps[32..33] are provisional, since not yet filtered)
			if (inInsert[insertPortIndex].isConnected()) {
				taps[32] = inInsert[insertPortIndex].getVoltage(((trackNum & 0x7) << 1) + 0);
				taps[33] = inInsert[insertPortIndex].getVoltage(((trackNum & 0x7) << 1) + 1);
			}
			else {
				taps[32] = taps[0];
				taps[33] = taps[1];
			}

			// Filters
			// Tap[32],[33]: pre-fader (post insert)
			// HPF
			if (getHPFCutoffFreq() >= minHPFCutoffFreq) {
				taps[32] = hpFilter[0].process(hpPreFilter[0].processHP(taps[32]));
				taps[33] = stereo ? hpFilter[1].process(hpPreFilter[1].processHP(taps[33])) : taps[32];
			}
			// LPF
			if (getLPFCutoffFreq() <= maxLPFCutoffFreq) {
				taps[32] = lpFilter[0].process(taps[32]);
				taps[33]  = stereo ? lpFilter[1].process(taps[33]) : taps[32];
			}
		}
		else {// filters before inserts
			// Filters
			float filtered[2] = {taps[0], taps[1]};
			// HPF
			if (getHPFCutoffFreq() >= minHPFCutoffFreq) {
				filtered[0] = hpFilter[0].process(hpPreFilter[0].processHP(filtered[0]));
				filtered[1] = stereo ? hpFilter[1].process(hpPreFilter[1].processHP(filtered[1])) : filtered[0];
			}
			// LPF
			if (getLPFCutoffFreq() <= maxLPFCutoffFreq) {
				filtered[0] = lpFilter[0].process(filtered[0]);
				filtered[1] = stereo ? lpFilter[1].process(filtered[1]) : filtered[0];
			}
			
			// Insert outputs
			insertOuts[0] = filtered[0];
			insertOuts[1] = stereo ? filtered[1] : 0.0f;// don't send to R of insert outs when mono!
			
			// Tap[32],[33]: pre-fader (post insert)
			if (inInsert[insertPortIndex].isConnected()) {
				taps[32] = inInsert[insertPortIndex].getVoltage(((trackNum & 0x7) << 1) + 0);
				taps[33] = inInsert[insertPortIndex].getVoltage(((trackNum & 0x7) << 1) + 1);
			}
			else {
				taps[32] = filtered[0];
				taps[33] = filtered[1];
			}
		}// filterPos
		
		if (eco) {
			// calc ** panMatrix **
			if (pan != oldPan) {
				panMatrix = simd::float_4::zero();// L, R, RinL, LinR (used for fader-pan block)
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
				faderGain = std::pow(fader, GlobalInfo::trkAndGrpFaderScalingExponent);// scaling
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
		simd::float_4 sigs(taps[32], taps[33], taps[33], taps[32]);
		sigs *= gainMatrixSlewers.out;
		
		taps[64] = sigs[0] + sigs[2];
		taps[65] = sigs[1] + sigs[3];


		// Tap[96],[97]: post-mute-solo
		
		// Calc muteSoloGainSlewed
		if (fadeGainScaled != muteSoloGainSlewer.out) {
			muteSoloGainSlewer.process(gInfo->sampleTime, fadeGainScaled);
		}

		taps[96] = taps[64] * muteSoloGainSlewer.out;
		taps[97] = taps[65] * muteSoloGainSlewer.out;
			
		// Add to mix since gainMatrixSlewed can be non zero
		if (paGroup->getValue() < 0.5f) {
			mix[0] += taps[96];
			mix[1] += taps[97];
		}
		else {
			int groupIndex = (int)(paGroup->getValue() - 0.5f);
			groupTaps[(groupIndex << 1) + 0] += taps[96];
			groupTaps[(groupIndex << 1) + 1] += taps[97];
		}
		
		// VUs
		if (gInfo->colorAndCloak.cc4[cloakedMode] != 0) {
			vu.reset();
		}
		else if (eco) {
			vu.process(gInfo->sampleTime * (1 + (gInfo->ecoMode & 0x3)), &taps[96]);
		}
	}
		
		
	void dataToJson(json_t *rootJ) {
		// gainAdjust
		json_object_set_new(rootJ, (ids + "gainAdjust").c_str(), json_real(gainAdjust));
		
		// fadeRate
		json_object_set_new(rootJ, (ids + "fadeRate").c_str(), json_real(fadeRate));

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
	}
	
	
	void dataFromJson(json_t *rootJ) {
		// gainAdjust
		json_t *gainAdjustJ = json_object_get(rootJ, (ids + "gainAdjust").c_str());
		if (gainAdjustJ)
			gainAdjust = json_number_value(gainAdjustJ);
		
		// fadeRate
		json_t *fadeRateJ = json_object_get(rootJ, (ids + "fadeRate").c_str());
		if (fadeRateJ)
			fadeRate = json_number_value(fadeRateJ);
		
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
		
		// extern must call resetNonJson()
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
	float muteSoloGain; // value of the mute/fade * solo_enable
	float oldPan;
	float oldFader;
	PackedBytes4 oldPanSignature;// [0] is pan stereo local, [1] is pan stereo global, [2] is pan mono global
	public:

	// no need to save, no reset
	int auxNum;// 0 to 3
	std::string ids;
	GlobalInfo *gInfo;
	Input *inInsert;
	float *flMute;
	float *flSolo0;
	float *flGroup;
	float *taps;
	int8_t* panLawStereoLocal;

	int getAuxGroup() {return (int)(*flGroup + 0.5f);}


	void construct(int _auxNum, GlobalInfo *_gInfo, Input *_inputs, float* _values12, float* _taps, int8_t* _panLawStereoLocal) {
		auxNum = _auxNum;
		ids = "id_a" + std::to_string(auxNum) + "_";
		gInfo = _gInfo;
		inInsert = &_inputs[INSERT_GRP_AUX_INPUT];
		flMute = &_values12[auxNum];
		flGroup = &_values12[auxNum + 8];
		taps = _taps;
		panLawStereoLocal = _panLawStereoLocal;
		gainMatrixSlewers.setRiseFall(simd::float_4(GlobalInfo::antipopSlew), simd::float_4(GlobalInfo::antipopSlew)); // slew rate is in input-units per second (ex: V/s)
		muteSoloGainSlewer.setRiseFall(GlobalInfo::antipopSlew, GlobalInfo::antipopSlew); // slew rate is in input-units per second (ex: V/s)
	}
	
	
	void onReset() {
		resetNonJson();
	}
	void resetNonJson() {
		panMatrix = simd::float_4::zero();
		faderGain = 0.0f;
		gainMatrix = simd::float_4::zero();
		gainMatrixSlewers.reset();
		muteSoloGainSlewer.reset();
		muteSoloGain = 0.0f;
		oldPan = -10.0f;
		oldFader = -10.0f;
		oldPanSignature.cc1 = 0xFFFFFFFF;
	}
	
	
	void updateSlowValues() {
		// calc ** muteSoloGain **
		float soloGain = (gInfo->returnSoloBitMask == 0 || (gInfo->returnSoloBitMask & (1 << auxNum)) != 0) ? 1.0f : 0.0f;
		muteSoloGain = (*flMute > 0.5f ? 0.0f : 1.0f) * soloGain;
		// Handle "Mute aux returns when soloing track"
		// i.e. add aux returns to mix when no solo, or when solo and don't want mutes aux returns
		if (gInfo->soloBitMask != 0 && gInfo->auxReturnsMutedWhenMainSolo) {
			muteSoloGain = 0.0f;
		}
		
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
		
	}
	
	void process(float *mix, float *auxRetFadePan, bool eco) {// mixer aux
		// auxRetFadePan[0] points fader value, auxRetFadePan[4] points pan value, all indexed for a given aux
		
		// Tap[0],[1]: pre-insert (aux inputs)
		// nothing to do, already set up by the auxspander
		
		// Tap[8],[9]: pre-fader (post insert)
		if (inInsert->isConnected()) {
			taps[8] = inInsert->getVoltage((auxNum << 1) + 8);
			taps[9] = inInsert->getVoltage((auxNum << 1) + 9);
		}
		else {
			taps[8] = taps[0];
			taps[9] = taps[1];
		}
		
		if (eco) {
			// calc ** panMatrix **
			float pan = auxRetFadePan[4];// cv input and clamping already done in auxspander
			if (pan != oldPan) {
				panMatrix = simd::float_4::zero();// L, R, RinL, LinR (used for fader-pan block)
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
				faderGain = std::pow(auxRetFadePan[0], GlobalInfo::globalAuxReturnScalingExponent);// scaling
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
		if (muteSoloGain != muteSoloGainSlewer.out) {
			muteSoloGainSlewer.process(gInfo->sampleTime, muteSoloGain);
		}
		
		taps[24] = taps[16] * muteSoloGainSlewer.out;
		taps[25] = taps[17] * muteSoloGainSlewer.out;
			
		mix[0] += taps[24];
		mix[1] += taps[25];
	}
	
	
	void dataToJson(json_t *rootJ) {
		// none
	}
	
	void dataFromJson(json_t *rootJ) {
		// none
		
		// extern must call resetNonJson()
	}
};// struct MixerAux



//*****************************************************************************


struct AuxspanderAux {
	// Constants
	static constexpr float minHPFCutoffFreq = 20.0f;
	static constexpr float maxLPFCutoffFreq = 20000.0f;
	static constexpr float hpfBiquadQ = 1.0f;// 1.0 Q since preceeeded by a one pole filter to get 18dB/oct
	
	// need to save, no reset
	// none
	
	// need to save, with reset
	private:
	float hpfCutoffFreq;// always use getter and setter since tied to Biquad
	float lpfCutoffFreq;// always use getter and setter since tied to Biquad
	public:

	// no need to save, with reset
	private:
	OnePoleFilter hpPreFilter[2];// 6dB/oct
	dsp::BiquadFilter hpFilter[2];// 12dB/oct
	dsp::BiquadFilter lpFilter[2];// 12db/oct
	public:

	// no need to save, no reset
	int auxNum;
	std::string ids;
	Input *inSig;


	void construct(int _auxNum, Input *_inputs) {
		auxNum = _auxNum;
		ids = "id_x" + std::to_string(auxNum) + "_";
		inSig = &_inputs[0 + 2 * auxNum + 0];
		for (int i = 0; i < 2; i++) {
			hpFilter[i].setParameters(dsp::BiquadFilter::HIGHPASS, 0.1, hpfBiquadQ, 0.0);
			lpFilter[i].setParameters(dsp::BiquadFilter::LOWPASS, 0.4, 0.707, 0.0);
		}
	}
	
	
	void onReset() {
		setHPFCutoffFreq(13.0f);// off
		setLPFCutoffFreq(20010.0f);// off
		resetNonJson();
	}
	void resetNonJson() {
	}
	

	void setHPFCutoffFreq(float fc) {// always use this instead of directly accessing hpfCutoffFreq
		hpfCutoffFreq = fc;
		fc *= APP->engine->getSampleTime();// fc is in normalized freq for rest of method
		for (int i = 0; i < 2; i++) {
			hpPreFilter[i].setCutoff(fc);
			hpFilter[i].setParameters(dsp::BiquadFilter::HIGHPASS, fc, hpfBiquadQ, 0.0);
		}
	}
	float getHPFCutoffFreq() {return hpfCutoffFreq;}
	
	void setLPFCutoffFreq(float fc) {// always use this instead of directly accessing lpfCutoffFreq
		lpfCutoffFreq = fc;
		fc *= APP->engine->getSampleTime();// fc is in normalized freq for rest of method
		lpFilter[0].setParameters(dsp::BiquadFilter::LOWPASS, fc, 0.707, 0.0);
		lpFilter[1].setParameters(dsp::BiquadFilter::LOWPASS, fc, 0.707, 0.0);
	}
	float getLPFCutoffFreq() {return lpfCutoffFreq;}


	void onSampleRateChange() {
		setHPFCutoffFreq(hpfCutoffFreq);
		setLPFCutoffFreq(lpfCutoffFreq);
	}


	void process(float *mix) {// auxspander aux	
		// optimize unused aux
		if (!inSig[0].isConnected()) {
			mix[0] = mix[1] = 0.0f;
			return;
		}
		mix[0] = inSig[0].getVoltage();
		mix[1] = inSig[1].isConnected() ? inSig[1].getVoltage() : mix[0];
		// Filters
		// HPF
		if (getHPFCutoffFreq() >= minHPFCutoffFreq) {
			mix[0] = hpFilter[0].process(hpPreFilter[0].processHP(mix[0]));
			mix[1] = inSig[1].isConnected() ? hpFilter[1].process(hpPreFilter[1].processHP(mix[1])) : mix[0];
		}
		// LPF
		if (getLPFCutoffFreq() <= maxLPFCutoffFreq) {
			mix[0] = lpFilter[0].process(mix[0]);
			mix[1] = inSig[1].isConnected() ? lpFilter[1].process(mix[1]) : mix[0];
		}
	}
		
	void dataToJson(json_t *rootJ) {
		// hpfCutoffFreq
		json_object_set_new(rootJ, (ids + "hpfCutoffFreq").c_str(), json_real(getHPFCutoffFreq()));
		
		// lpfCutoffFreq
		json_object_set_new(rootJ, (ids + "lpfCutoffFreq").c_str(), json_real(getLPFCutoffFreq()));
	}
	
	void dataFromJson(json_t *rootJ) {
		// hpfCutoffFreq
		json_t *hpfCutoffFreqJ = json_object_get(rootJ, (ids + "hpfCutoffFreq").c_str());
		if (hpfCutoffFreqJ)
			setHPFCutoffFreq(json_number_value(hpfCutoffFreqJ));
		
		// lpfCutoffFreq
		json_t *lpfCutoffFreqJ = json_object_get(rootJ, (ids + "lpfCutoffFreq").c_str());
		if (lpfCutoffFreqJ)
			setLPFCutoffFreq(json_number_value(lpfCutoffFreqJ));

		// extern must call resetNonJson()
	}
};// struct AuxspanderAux

*/