//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "Channel.hpp"
#include "PresetAndShapeManager.hpp"


// bool isPreset[4] = {true, true, false, false};
// bool isPrev[4] = {true, false, true, false};

  

void Channel::construct(int _chanNum, bool* _running, uint32_t* _sosEosEoc, ClockDetector* _clockDetector, Input* _inputs, Output* _outputs, Param* _params, std::vector<ParamQuantity*>* _paramQuantitiesSrc, PresetAndShapeManager* _presetAndShapeManager) {
	chanNum = _chanNum;
	running = _running;
	hpFilter.setParameters(true, 0.1f);
	lpFilter.setParameters(false, 0.4f);
	if (_inputs) {
		inInput = &_inputs[IN_INPUTS + chanNum];
		trigInput = &_inputs[TRIG_INPUTS + chanNum];
		scInput = &_inputs[SIDECHAIN_INPUT];
	}
	if (_outputs) {
		outOutput = &_outputs[OUT_OUTPUTS + chanNum];
		cvOutput = &_outputs[CV_OUTPUTS + chanNum];
	}
	paPhase = &_params[PHASE_PARAM + chanNum * NUM_CHAN_PARAMS];
	paResponse = &_params[RESPONSE_PARAM + chanNum * NUM_CHAN_PARAMS];
	paWarp = &_params[WARP_PARAM + chanNum * NUM_CHAN_PARAMS];
	paAmount = &_params[AMOUNT_PARAM + chanNum * NUM_CHAN_PARAMS];
	paSlew = &_params[SLEW_PARAM + chanNum * NUM_CHAN_PARAMS];
	paSmooth = &_params[SMOOTH_PARAM + chanNum * NUM_CHAN_PARAMS];
	paCrossover = &_params[CROSSOVER_PARAM + chanNum * NUM_CHAN_PARAMS];
	paHigh = &_params[HIGH_PARAM + chanNum * NUM_CHAN_PARAMS];
	paLow = &_params[LOW_PARAM + chanNum * NUM_CHAN_PARAMS];
	paPrevNextPreSha = &_params[PREV_NEXT_PRE_SHA + chanNum * NUM_CHAN_PARAMS];
	smoothFilter.setParameters(false, 0.4f);
	ParamQuantity* pqReps = NULL;
	if (_paramQuantitiesSrc) {
		pqReps = (*_paramQuantitiesSrc)[REPETITIONS_PARAM + chanNum * NUM_CHAN_PARAMS];
	}
	presetAndShapeManager = _presetAndShapeManager;// can be null
	
	playHead.construct(_chanNum, _sosEosEoc, _clockDetector, _running, pqReps, &_params[chanNum * NUM_CHAN_PARAMS], &_inputs[TRIG_INPUTS + chanNum], &scEnvelope, _presetAndShapeManager);
	// onReset(false); // not needed since ShapeMaster::onReset() will propagate to Channel::onReset();
}

void Channel::onReset(bool withParams) {		
	if (withParams) {
		paPhase->setValue(DEFAULT_PHASE);
		paResponse->setValue(DEFAULT_RESPONSE);
		paWarp->setValue(DEFAULT_WARP);
		paAmount->setValue(DEFAULT_AMOUNT);
		paSlew->setValue(DEFAULT_SLEW);			
		paSmooth->setValue(DEFAULT_SMOOTH);
		paCrossover->setValue(DEFAULT_CROSSOVER);
		paHigh->setValue(DEFAULT_HIGH);
		paLow->setValue(DEFAULT_LOW);
	}
	setHPFCutoffSqFreq(SC_HPF_SQFREQ_DEF);
	setLPFCutoffSqFreq(SC_LPF_SQFREQ_DEF);
	sensitivity = DEFAULT_SENSITIVITY;
	gainAdjustVca = 1.0f;
	gainAdjustSc = 1.0f;
	gridX = 16;
	rangeIndex = 0;
	channelSettings.cc4[0] = 0x0;// local invert shadow
	channelSettings.cc4[1] = chanNum;// channel color
	channelSettings.cc4[2] = POLY_NONE;// poly sum mode (see util.hpp for enum)
	channelSettings.cc4[3] = 0x0;// sidechain uses VCA in
	channelSettings2.cc4[0] = 16;// scopeVcaPolySel: 0 to 15 val is poly chan select, 16 is (polychan0+polychan1)/2; for all options, if not enough chans in cable, else defaults to chan 0
	setShowUnsyncLengthAs(0x0);// show unsynced length in: 0 = seconds, 1 = Hz, 2 = note; must use setShowUnsyncLengthAs() to change, do not write to channelSettings2.cc4[1] directly
	channelSettings2.cc4[2] = 0x1;// tooltip Y mode (1 is default volts, 0 is freq, 2 is notes)
	channelSettings2.cc4[3] = 0x0;// decoupledFirstLast
	presetPath = "";
	shapePath = "";
	chanName = string::f("Channel %i", chanNum + 1);
	randomSettings.reset();
	shape.onReset();
	playHead.onReset(withParams);
	resetNonJson();
}


void Channel::resetNonJson() {
	sampleTime = 1.0 / (double)APP->engine->getSampleRate();
	xover.reset();
	setCrossoverCutoffFreq();
	// lastCrossoverParamWithCv; automatically set in setCrossoverCutoffFreq()
	hpFilter.reset();
	setHPFCutoffSqFreq(hpfCutoffSqFreq);	
	lpFilter.reset();
	setLPFCutoffSqFreq(lpfCutoffSqFreq);
	smoothFilter.reset();
	setSmoothCutoffFreq();
	// lastSmoothParam; automatically set in setSmoothCutoffFreq()
	lastProcessXt = 0.0;
	// setSlewRate();
	// lastSlewParamWithCv; automatically set in setSlewRate()
	updateChannelActive();// channelActive
	vcaPreSize = 0;
	vcaPostSize = 0;
	scSignal = 0.0f;
	scEnvelope = 0.0f;
	scEnvSlewer.reset();
	setSensitivity(sensitivity);
	// scEnvSlewer; automatically set in setSensitivity(), sensitivity must be valid
	warpPhaseResponseAmountWithCv = simd::float_4(paWarp->getValue(), paPhase->getValue(), paResponse->getValue(), paAmount->getValue());
	warpPhaseResponseAmountCvConnected = false;
	xoverSlewWithCv = simd::float_4(paCrossover->getValue(), paHigh->getValue(), paLow->getValue(), paSlew->getValue());
	xoverSlewCvConnected = false;
}


json_t* Channel::dataToJsonChannel(bool withParams, bool withProUnsyncMatch, bool withFullSettings) {
	json_t* channelJ = json_object();
	if (withParams) {
		json_object_set_new(channelJ, "phase", json_real(paPhase->getValue()));
		json_object_set_new(channelJ, "response", json_real(paResponse->getValue()));
		json_object_set_new(channelJ, "warp", json_real(paWarp->getValue()));
		json_object_set_new(channelJ, "level", json_real(paAmount->getValue()));
		json_object_set_new(channelJ, "slew", json_real(paSlew->getValue()));			
		json_object_set_new(channelJ, "smooth", json_real(paSmooth->getValue()));
		json_object_set_new(channelJ, "crossover", json_real(paCrossover->getValue()));
		json_object_set_new(channelJ, "high", json_real(paHigh->getValue()));
		json_object_set_new(channelJ, "low", json_real(paLow->getValue()));
	}
	json_object_set_new(channelJ, "hpfCutoffSqFreq", json_real(getHPFCutoffSqFreq()));
	json_object_set_new(channelJ, "lpfCutoffSqFreq", json_real(getLPFCutoffSqFreq()));
	json_object_set_new(channelJ, "sensitivity", json_real(sensitivity));
	json_object_set_new(channelJ, "gainAdjustSc", json_real(gainAdjustSc));
	json_object_set_new(channelJ, "gridX", json_integer(gridX));
	json_object_set_new(channelJ, "rangeIndex", json_integer(rangeIndex));
	json_object_set_new(channelJ, "channelSettings", json_integer(channelSettings.cc1));
	json_object_set_new(channelJ, "channelSettings2", json_integer(channelSettings2.cc1));
	json_object_set_new(channelJ, "presetPath", json_string(presetPath.c_str()));
	json_object_set_new(channelJ, "shapePath", json_string(shapePath.c_str()));
	if (withFullSettings) {
		json_object_set_new(channelJ, "gainAdjustVca", json_real(gainAdjustVca));
		json_object_set_new(channelJ, "chanName", json_string(chanName.c_str()));
	}
	randomSettings.dataToJson(channelJ);
	json_object_set_new(channelJ, "shape", shape.dataToJsonShape());
	playHead.dataToJsonPlayHead(channelJ, withParams, withProUnsyncMatch, withFullSettings);

	return channelJ;
}


bool Channel::dataFromJsonChannel(json_t *channelJ, bool withParams, bool isDirtyCacheLoad, bool withFullSettings) {
	// returns true when in free version we loaded a preset that has sync 
	if (withParams) {
		json_t *phaseJ = json_object_get(channelJ, "phase");
		if (phaseJ) paPhase->setValue(json_number_value(phaseJ));
		
		json_t *responseJ = json_object_get(channelJ, "response");
		if (responseJ) paResponse->setValue(json_number_value(responseJ));
		
		json_t *warpJ = json_object_get(channelJ, "warp");
		if (warpJ) paWarp->setValue(json_number_value(warpJ));
		
		json_t *amountJ = json_object_get(channelJ, "level");
		if (amountJ) paAmount->setValue(json_number_value(amountJ));
		
		json_t *slewJ = json_object_get(channelJ, "slew");
		if (slewJ) paSlew->setValue(json_number_value(slewJ));
		
		json_t *smoothJ = json_object_get(channelJ, "smooth");
		if (smoothJ) paSmooth->setValue(json_number_value(smoothJ));
		
		json_t *crossoverJ = json_object_get(channelJ, "crossover");
		if (crossoverJ) paCrossover->setValue(json_number_value(crossoverJ));
		
		json_t *highJ = json_object_get(channelJ, "high");
		if (highJ) paHigh->setValue(json_number_value(highJ));
		
		json_t *lowJ = json_object_get(channelJ, "low");
		if (lowJ) paLow->setValue(json_number_value(lowJ));
	}
	
	json_t *hpfCutoffSqFreqJ = json_object_get(channelJ, "hpfCutoffSqFreq");
	if (hpfCutoffSqFreqJ) {
		hpfCutoffSqFreq = json_number_value(hpfCutoffSqFreqJ);
	}
	else {// legacy
		json_t *hpfCutoffFreqJ = json_object_get(channelJ, "hpfCutoffFreq");
		if (hpfCutoffFreqJ) hpfCutoffSqFreq = std::pow(json_number_value(hpfCutoffFreqJ), 1.0f / (float)SCF_SCALING_EXP);
	}

	json_t *lpfCutoffSqFreqJ = json_object_get(channelJ, "lpfCutoffSqFreq");
	if (lpfCutoffSqFreqJ) {
		lpfCutoffSqFreq = json_number_value(lpfCutoffSqFreqJ);
	}
	else {// legacy
		json_t *lpfCutoffFreqJ = json_object_get(channelJ, "lpfCutoffFreq");
		if (lpfCutoffFreqJ) lpfCutoffSqFreq = std::pow(json_number_value(lpfCutoffFreqJ), 1.0f / (float)SCF_SCALING_EXP);
	}
	
	json_t *sensitivityJ = json_object_get(channelJ, "sensitivity");
	if (sensitivityJ) sensitivity = json_number_value(sensitivityJ);
	
	json_t *gainAdjustScJ = json_object_get(channelJ, "gainAdjustSc");
	if (gainAdjustScJ) gainAdjustSc = json_number_value(gainAdjustScJ);
	
	json_t *gridXJ = json_object_get(channelJ, "gridX");
	if (gridXJ) gridX = json_integer_value(gridXJ);
	
	json_t *rangeIndexJ = json_object_get(channelJ, "rangeIndex");
	if (rangeIndexJ) rangeIndex = json_integer_value(rangeIndexJ);
	
	json_t *channelSettingsJ = json_object_get(channelJ, "channelSettings");
	if (channelSettingsJ) {
		PackedBytes4 newSettings;
		newSettings.cc1 = json_integer_value(channelSettingsJ);
		if (withFullSettings) {
			// not saved with preset
			channelSettings.cc4[0] = newSettings.cc4[0];
			channelSettings.cc4[1] = newSettings.cc4[1];
			channelSettings.cc4[2] = newSettings.cc4[2];
		}
		channelSettings.cc4[3] = newSettings.cc4[3];
	}
	
	json_t *channelSettings2J = json_object_get(channelJ, "channelSettings2");
	if (channelSettings2J) {
		PackedBytes4 newSettings2;
		newSettings2.cc1 = json_integer_value(channelSettings2J);
		if (withFullSettings) {
			// not saved with preset
			channelSettings2.cc4[0] = newSettings2.cc4[0];
			channelSettings2.cc4[2] = newSettings2.cc4[2];
			channelSettings2.cc4[3] = newSettings2.cc4[3];
		}
		setShowUnsyncLengthAs(newSettings2.cc4[1]);
	}

	json_t *presetPathJ = json_object_get(channelJ, "presetPath");
	if (presetPathJ) presetPath = json_string_value(presetPathJ);

	json_t *shapePathJ = json_object_get(channelJ, "shapePath");
	if (shapePathJ) shapePath = json_string_value(shapePathJ);
	
	if (withFullSettings) {
		json_t *gainAdjustVcaJ = json_object_get(channelJ, "gainAdjustVca");
		if (gainAdjustVcaJ) gainAdjustVca = json_number_value(gainAdjustVcaJ);
	
		json_t *chanNameJ = json_object_get(channelJ, "chanName");
		if (chanNameJ) chanName = json_string_value(chanNameJ);
	}
	
	randomSettings.reset();// legacy (for presets that didn't have random settings saved in them)
	randomSettings.dataFromJson(channelJ);
	
	json_t *shapeJ = json_object_get(channelJ, "shape");
	if (shapeJ) shape.dataFromJsonShape(shapeJ);
	
	bool unsupportedSync = playHead.dataFromJsonPlayHead(channelJ, withParams, isDirtyCacheLoad, withFullSettings);
	
	if (!isDirtyCacheLoad) {
		resetNonJson();
	}
	
	return unsupportedSync;
}	


std::string Channel::getLengthText(bool* inactive) {
	*inactive = (playHead.getTrigMode() == TM_CV);
	#ifdef SM_PRO
	if (isSync()) {
		int _lengthSyncIndex = playHead.getLengthSync(false);
		*inactive |= inadmissibleLengthSync(_lengthSyncIndex);
		return multDivTable[_lengthSyncIndex].shortName;
	}
	#endif
	// else we are unsynced
	float lengthUnsync = playHead.getLengthUnsync();
	if (lengthUnsync == lengthUnsyncOld) {// no need to test channelSettings2.cc4[1] since lengthUnsyncOld gets set to 1e6 when setting is set (must use method to set it!)
		return lengthUnsyncTextOld;
	}
	
	lengthUnsyncOld = lengthUnsync;
	float lengthUnsyncTime = calcLengthUnsyncTime();
	
	if (channelSettings2.cc4[1] == 0) {
		// show unsynced length as time
		if (lengthUnsyncTime < 0.01f) {
			lengthUnsyncTextOld = string::f("%.2fms", lengthUnsyncTime * 1000.0f);
		}
		else if (lengthUnsyncTime < 0.1f) {
			lengthUnsyncTextOld = string::f("%.1fms", lengthUnsyncTime * 1000.0f);
		}
		else if (lengthUnsyncTime < 1.0f) {
			lengthUnsyncTextOld = string::f("%.0fms", lengthUnsyncTime * 1000.0f);
		}
		else if (lengthUnsyncTime < 10.0f) {
			lengthUnsyncTextOld = string::f("%.2fs", lengthUnsyncTime);
		}
		else if (lengthUnsyncTime < 60.0f) {
			lengthUnsyncTextOld = string::f("%.1fs", lengthUnsyncTime);
		}
		else {
			int intTime = (int)(lengthUnsyncTime + 0.5f);
			int minutes = intTime / 60;
			int seconds = intTime % 60;
			lengthUnsyncTextOld = string::f("%im%is", minutes, seconds);
		}
	}
	else if (channelSettings2.cc4[1] == 1) {
		// show unsynced length as freq
		float freq = 1.0f / lengthUnsyncTime;
		if (freq >= 1000.0f) {
			lengthUnsyncTextOld = string::f("%.3gkHz", freq / 1000.0f);
		}
		else if (freq >= 1.0f) {
			lengthUnsyncTextOld = string::f("%.4gHz", freq);
		}
		else if (freq >= 0.001f) {
			lengthUnsyncTextOld = string::f("%.3gmHz", freq * 1000.0f);
		}
		else {
			lengthUnsyncTextOld = string::f("%.2gmHz", freq * 1000.0f);
		}
	}
	else {
		float voct = unsuncedLengthParamToVoct(lengthUnsync);
		char note[8];
		printNote(voct, note, true);

		lengthUnsyncTextOld = note;
	}
	return lengthUnsyncTextOld;
}


bool Channel::isDirty(Channel* refChan) {
	bool inactive;// unused for dirty comparison
	if (std::round(paPhase->getValue() * 3600.0f) != std::round(refChan->paPhase->getValue() * 3600.0f)) return true;// 1 decimal deg comp		
	if (std::round(paResponse->getValue() * 1e3f) != std::round(refChan->paResponse->getValue() * 1e3f)) return true;// percent comparison
	if (std::round(paWarp->getValue() * 1e3f) != std::round(refChan->paWarp->getValue() * 1e3f)) return true;// percent comparison
	if (std::round(paAmount->getValue() * 1e3f) != std::round(refChan->paAmount->getValue() * 1e3f)) return true;// percent comparison
	if (std::round(paSlew->getValue() * 1e3f) != std::round(refChan->paSlew->getValue() * 1e3f)) return true;// percent comparison
	if (std::round(paSmooth->getValue() * 1e3f) != std::round(refChan->paSmooth->getValue() * 1e3f)) return true;// percent comparison
	if (!(getCrossoverText(&inactive) == refChan->getCrossoverText(&inactive))) return true;// text comparison (easier)
	if (std::round(paHigh->getValue() * 1e3f) != std::round(refChan->paHigh->getValue() * 1e3f)) return true;// percent comparison
	if (std::round(paLow->getValue() * 1e3f) != std::round(refChan->paLow->getValue() * 1e3f)) return true;// percent comparison
	if (!(getHPFCutoffFreqText() == refChan->getHPFCutoffFreqText())) return true;// text comparison (easier)
	if (!(getLPFCutoffFreqText() == refChan->getLPFCutoffFreqText())) return true;// text comparison (easier)
	if (!(getSensitivityText(getSensitivity()) == refChan->getSensitivityText(refChan->getSensitivity()))) return true;// text comparison (easier)
	if (!(getGainAdjustDbText(getGainAdjustVcaDb()) == refChan->getGainAdjustDbText(refChan->getGainAdjustVcaDb()))) return true;// text comparison (easier)
	if (!(getGainAdjustDbText(getGainAdjustScDb()) == refChan->getGainAdjustDbText(refChan->getGainAdjustScDb()))) return true;// text comparison (easier)
	if (gridX != refChan->gridX) return true;// int comparison
	if (rangeIndex != refChan->rangeIndex) return true;// int comparison
	// if (chanSettings.cc1 != refChan->chanSettings.cc1) return true;// 4x int comparison, but excluded from dirty comparison
	// if (chanSettings2.cc1 != refChan->chanSettings2.cc1) return true;// 4x int comparison, but excluded from dirty comparison
	// if (!(presetPath == refChan->presetPath)) return true;// text comp, not relevant for dirty comparison
	// if (!(shapePath == refChan->shapePath)) return true;// text comp, not relevant for dirty comparison
	// if (!(chanName == refChan->chanName)) return true;// text comp, not relevant for dirty comparison
	if (playHead.isDirty(&(refChan->playHead))) {
		return true;
	}
	if (randomSettings.isDirty(&(refChan->randomSettings))) {
		return true;
	}
	return isDirtyShape(refChan);
}


void Channel::processSlow(ChanCvs *chanCvs) {
	// slew knob
	// if (xoverSlewWithCv[3] != lastSlewParamWithCv) {
		// setSlewRate();
	// }
	playHead.processSlow(chanCvs);
}


void Channel::process(bool fsDiv8, ChanCvs *chanCvs) {
	updateChannelActive();
	
	if (fsDiv8) {// a form of slow, but not as slow as processSlow()
		// preset and shape prev and next buttons
		if (!presetPath.empty()) {
			float buttonVal = (chanCvs && chanCvs->hasPrevPreset()) ? 10.0f : paPrevNextPreSha[0].getValue();
			if (arrowButtonTriggers[0].process(buttonVal)) {
				presetAndShapeManager->executeOrStageWorkload(chanNum, WT_PREV_PRESET, prevNextButtonsClicked[0], playHead.getState() == PlayHead::STEPPING);
			}
			buttonVal = (chanCvs && chanCvs->hasNextPreset()) ? 10.0f : paPrevNextPreSha[1].getValue();
			if (arrowButtonTriggers[1].process(buttonVal)) {
				presetAndShapeManager->executeOrStageWorkload(chanNum, WT_NEXT_PRESET, prevNextButtonsClicked[1], playHead.getState() == PlayHead::STEPPING);
			}
		}
		else if (!shapePath.empty()) {
			float buttonVal = (chanCvs && chanCvs->hasPrevShape()) ? 10.0f : paPrevNextPreSha[2].getValue();
			if (arrowButtonTriggers[2].process(buttonVal)) {
				presetAndShapeManager->executeOrStageWorkload(chanNum, WT_PREV_SHAPE, prevNextButtonsClicked[2], playHead.getState() == PlayHead::STEPPING);
			}
			buttonVal = (chanCvs && chanCvs->hasNextShape()) ? 10.0f : paPrevNextPreSha[3].getValue();
			if (arrowButtonTriggers[3].process(buttonVal)) {
				presetAndShapeManager->executeOrStageWorkload(chanNum, WT_NEXT_SHAPE, prevNextButtonsClicked[3], playHead.getState() == PlayHead::STEPPING);
			}
		}

		// smooth knob
		if (paSmooth->getValue() != lastSmoothParam) {
			setSmoothCutoffFreq();
		}
		
		// crossover knob refresh
		if (xoverSlewWithCv[0] != lastCrossoverParamWithCv) {
			setCrossoverCutoffFreq();
		}
		
		playHead.processDiv8();// only has slowSlewPulseGen refresh
	}
	
	
	
	#ifdef SM_PRO
	if (chanCvs) {
		if (chanCvs->trigs != 0) {
			if (chanCvs->hasChannelReset()) {
				initRun(true);
			}
			if (chanCvs->hasPlay()) {
				playHead.togglePlay();
			}
			if (chanCvs->hasFreeze()) {
				playHead.toggleFreeze();
			}
			if (chanCvs->hasReverse()) {
				presetAndShapeManager->executeOrStageWorkload(chanNum, WT_REVERSE, false, false);
			}
			if (chanCvs->hasInvert()) {
				presetAndShapeManager->executeOrStageWorkload(chanNum, WT_INVERT, false, false);
			}
			if (chanCvs->hasRandom()) {
				presetAndShapeManager->executeOrStageWorkload(chanNum, WT_RANDOM, false, false);
			}
		}
	}
	#endif
	
	#ifdef SM_PRO
	if (chanCvs && channelActive && chanCvs->hasWarpPhasRespAmnt()) {
		warpPhaseResponseAmountWithCv = simd::clamp(
			simd::float_4(paWarp->getValue(), paPhase->getValue(), paResponse->getValue(), paAmount->getValue()) + chanCvs->warpPhasRespAmnt, 
			simd::float_4(-Channel::MAX_WARP, 0.0f, -Channel::MAX_RESPONSE, 0.0f),
			simd::float_4( Channel::MAX_WARP, 1.0f,  Channel::MAX_RESPONSE, 1.0f));
		warpPhaseResponseAmountCvConnected = true;
	}
	else 
	#endif
	{
		warpPhaseResponseAmountWithCv = simd::float_4(paWarp->getValue(), paPhase->getValue(), paResponse->getValue(), paAmount->getValue());	
		warpPhaseResponseAmountCvConnected = false;
	}


	#ifdef SM_PRO
	if (chanCvs && channelActive && chanCvs->hasXoverSlew()) {
		xoverSlewWithCv = simd::clamp(
			simd::float_4(paCrossover->getValue(), paHigh->getValue(), paLow->getValue(), paSlew->getValue()) + chanCvs->xoverSlew, 
			simd::float_4(-1.0f, 0.0f, 0.0f, 0.0f),
			simd::float_4( 1.0f, 1.0f, 1.0f, 1.0f));
		xoverSlewCvConnected = true;
	}
	else 
	#endif
	{
		xoverSlewWithCv = simd::float_4(paCrossover->getValue(), paHigh->getValue(), paLow->getValue(), paSlew->getValue());	
		xoverSlewCvConnected = false;
	}


	// shape processing
	if (channelActive) {				
		// process playhead and get shape CV
		lastProcessXt = playHead.process(chanCvs);
		float shapeCv = evalShapeForProcess(lastProcessXt);

		// CV OUTPUT
		// --------
		cvOutput->setVoltage(applyRange(shapeCv));
		
		
		// VCA
		// --------
		
		// vcaPre and vcaPreSize
		vcaPreSize = inInput->getChannels();
		if (vcaPreSize > 0) {
			int vcaPreMax = std::min(vcaPreSize, polyModeChanOut[getPolyMode()]);
			for (int c = 0; c < vcaPreSize; c++) {
				if (c < vcaPreMax) {
					vcaPre[c] = inInput->getVoltage(c) * gainAdjustVca;
				}
				else {
					vcaPre[c % vcaPreMax] += inInput->getVoltage(c) * gainAdjustVca;
				}
			}
			vcaPreSize = vcaPreMax;
		}
		
		// scSignal
		scSignal = 0.0f;
		if (getTrigMode() == TM_SC) {
			bool needsScProcessing = false;
			if (isSidechainUseVca() && vcaPreSize > 0) {
				scSignal = vcaPre[0];
				for (int c = 1; c < vcaPreSize; c++) {
					scSignal += vcaPre[c];
				}
				needsScProcessing = true;
			}
			else if (!isSidechainUseVca() && scInput->getChannels() > chanNum) {
				scSignal = scInput->getVoltage(chanNum);
				needsScProcessing = true;	
			}
			if (needsScProcessing) {
				scSignal *= gainAdjustSc;
				// HPF
				if (isHpfCutoffActive()) {
					scSignal = hpFilter.process(scSignal);
				}
				// LPF
				if (isLpfCutoffActive()) {
					scSignal = lpFilter.process(scSignal);
				}
				scEnvelope = scEnvSlewer.process(std::fabs(sampleTime), scSignal);
			}
		}
		
		
		// vcaPost and vcaPostSize
		vcaPostSize = outOutput->getChannels();// already poly mode correct, see ShapeMaster.cpp
		if (vcaPostSize > 0) {
			
			// (crossover needed even if audition because of scope and audition antipop)
			if ((paCrossover->getValue() >= CROSSOVER_OFF) && vcaPreSize > 0) {
				float gainLow = 1.0f + (shapeCv - 1.0f) * xoverSlewWithCv[2];//paLow->getValue();
				float gainHigh = 1.0f + (shapeCv - 1.0f) * xoverSlewWithCv[1];//paHigh->getValue();
				for (int c2 = 0; c2 < ((vcaPostSize + 1) >> 1); c2++) {
					int iL = c2 << 1;
					int iR = iL + 1;
					simd::float_4 lrRes = xover.process(vcaPre[iL], vcaPre[iR], c2);
					// lrRes[0] = left low, left high, right low, lrRes[3] = right high
					vcaPost[iL] = lrRes[0] * gainLow + lrRes[1] * gainHigh;
					vcaPost[iR] = lrRes[2] * gainLow + lrRes[3] * gainHigh;
				}
			}
			else {
				for (int c = 0; c < vcaPostSize; c++) {
					if (c < vcaPreSize) {
						vcaPost[c] = vcaPre[c] * shapeCv;
					}
					else {
						vcaPost[c] = 0.0f;
					}
				}
			}		

			// write VCA output, with possible audition crossfade
			if (playHead.getTrigMode() == TM_SC && playHead.getAudition() && playHead.getAuditionGain() != 0.0f) {
				float auditionGain = playHead.getAuditionGain();
				float crossfaded = vcaPost[0] * (1.0f - auditionGain) + scSignal * auditionGain;
				outOutput->setVoltage(crossfaded, 0);
				if (vcaPostSize > 1) {
					crossfaded = vcaPost[1] * (1.0f - auditionGain) + scSignal * auditionGain;
					outOutput->setVoltage(crossfaded, 1);
				}
				for (int c = 2; c < vcaPostSize; c++) {
					crossfaded = vcaPost[c] * (1.0f - auditionGain) + 0.0f * auditionGain;
					outOutput->setVoltage(crossfaded, c);
				}
			}
			else {
				for (int c = 0; c < vcaPostSize; c++) {
					outOutput->setVoltage(vcaPost[c], c);
				}
			}
		}
	}// if (channelActive)
	else {
		// needed for scope
		vcaPreSize = 0;
		vcaPostSize = 0;
		scSignal = 0.0f;
	}
}// process