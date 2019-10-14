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


// struct MixerGroup

void MixerGroup::construct(int _groupNum, GlobalInfo *_gInfo, Input *_inputs, Param *_params, char* _groupName, float* _taps) {
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


void MixerGroup::onReset() {
	fadeRate = 0.0f;
	fadeProfile = 0.0f;
	directOutsMode = 3;// post-solo should be default
	auxSendsMode = 3;// post-solo should be default
	panLawStereo = 1;
	vuColorThemeLocal = 0;
	dispColorLocal = 0;
	resetNonJson();
}


void MixerGroup::resetNonJson() {
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


void MixerGroup::dataToJson(json_t *rootJ) {
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


void MixerGroup::dataFromJson(json_t *rootJ) {
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



//*****************************************************************************


// struct MixerTrack 

void MixerTrack::construct(int _trackNum, GlobalInfo *_gInfo, Input *_inputs, Param *_params, char* _trackName, float* _taps, float* _groupTaps, float* _insertOuts) {
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


void MixerTrack::onReset() {
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


void MixerTrack::resetNonJson() {
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


void MixerTrack::dataToJson(json_t *rootJ) {
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


void MixerTrack::dataFromJson(json_t *rootJ) {
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


// level 1 read and write
void MixerTrack::write(TrackSettingsCpBuffer *dest) {
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
void MixerTrack::read(TrackSettingsCpBuffer *src) {
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
void MixerTrack::write2(TrackSettingsCpBuffer *dest) {
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
void MixerTrack::read2(TrackSettingsCpBuffer *src) {
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



//*****************************************************************************


// struct MixerAux 

void MixerAux::construct(int _auxNum, GlobalInfo *_gInfo, Input *_inputs, float* _values12, float* _taps, int8_t* _panLawStereoLocal) {
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


void MixerAux::onReset() {
	resetNonJson();
}


void MixerAux::resetNonJson() {
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
	


//*****************************************************************************


// struct AuxspanderAux 

void AuxspanderAux::construct(int _auxNum, Input *_inputs) {
	auxNum = _auxNum;
	ids = "id_x" + std::to_string(auxNum) + "_";
	inSig = &_inputs[0 + 2 * auxNum + 0];
	for (int i = 0; i < 2; i++) {
		hpFilter[i].setParameters(dsp::BiquadFilter::HIGHPASS, 0.1, hpfBiquadQ, 0.0);
		lpFilter[i].setParameters(dsp::BiquadFilter::LOWPASS, 0.4, 0.707, 0.0);
	}
}


void AuxspanderAux::onReset() {
	setHPFCutoffFreq(13.0f);// off
	setLPFCutoffFreq(20010.0f);// off
	resetNonJson();
}


void AuxspanderAux::dataToJson(json_t *rootJ) {
	// hpfCutoffFreq
	json_object_set_new(rootJ, (ids + "hpfCutoffFreq").c_str(), json_real(getHPFCutoffFreq()));
	
	// lpfCutoffFreq
	json_object_set_new(rootJ, (ids + "lpfCutoffFreq").c_str(), json_real(getLPFCutoffFreq()));
}


void AuxspanderAux::dataFromJson(json_t *rootJ) {
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
