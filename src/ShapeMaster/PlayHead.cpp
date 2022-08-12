//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "PlayHead.hpp"
#include "PresetAndShapeManager.hpp"


void PlayHead::construct(int _chanNum, uint32_t* _sosEosEoc, ClockDetector* _clockDetector, bool* _running, ParamQuantity* _paramQuantityRepititionSrc, Param* _chanParams, Input* _trigInput, float* _scEnvelope, PresetAndShapeManager* _presetAndShapeManager, dsp::PulseGenerator* _nodeTrigPulseGen, float* _nodeTrigDuration) {
	chanNum = _chanNum;
	sosEosEoc = _sosEosEoc;
	clockDetector = _clockDetector;
	running = _running;
	scEnvelope = _scEnvelope;
	presetAndShapeManager = _presetAndShapeManager;
	nodeTrigPulseGen = _nodeTrigPulseGen;
	nodeTrigDuration = _nodeTrigDuration;
	paramQuantityRepititionSrc = _paramQuantityRepititionSrc;
	paRepetitions = &_chanParams[REPETITIONS_PARAM];
	paLengthSync = &_chanParams[LENGTH_SYNC_PARAM];
	paLengthUnsync = &_chanParams[LENGTH_UNSYNC_PARAM];
	paSync = &_chanParams[SYNC_PARAM];
	paLock = &_chanParams[LOCK_PARAM];
	paSwing = &_chanParams[SWING_PARAM];
	paFreeze = &_chanParams[FREEZE_PARAM];
	paPlay = &_chanParams[PLAY_PARAM];
	paSustainLoop = &_chanParams[LOOP_PARAM];
	paOffset = &_chanParams[OFFSET_PARAM];
	auditionSlewer.setRiseFall(125.0f);
	paAudition = &_chanParams[AUDITION_PARAM];
	paTrigLevel = &_chanParams[TRIGLEV_PARAM];
	inTrig = _trigInput;
	// onReset(false); // not needed since ShapeMaster::onReset() will propagate to PlayHead::onReset();
}


void PlayHead::onReset(bool withParams) {
	if (withParams) {
		paRepetitions->setValue(DEFAULT_REPETITIONS);
		paLengthSync->setValue(DEFAULT_LENGTH_SYNC);
		paLengthUnsync->setValue(DEFAULT_LENGTH_UNSYNC);
		paSync->setValue(DEFAULT_SYNC);
		paLock->setValue(DEFAULT_LOCK);
		paSwing->setValue(DEFAULT_SWING);
		paFreeze->setValue(DEFAULT_FREEZE);
		paPlay->setValue(DEFAULT_PLAY);
		paSustainLoop->setValue(DEFAULT_LOOP);
		paOffset->setValue(DEFAULT_OFFSET);
		paAudition->setValue(0.0f);
		paTrigLevel->setValue(DEFAULT_TRIGLEVEL);
	}
	playMode = PM_FWD;
	trigMode = TM_AUTO;
	hysteresis = DEFAULT_HYSTERESIS;
	holdOff = DEFAULT_HOLDOFF;
	loopStart = 0.50f;
	loopEndAndSustain = 0.75;
	playHeadSettings.cc4[0] = 0x0;// EOC last cycle only (PRO only)
	playHeadSettings.cc4[1] = 0x1;// gate (aka CTRL) restart
	playHeadSettings.cc4[2] = 0x1;// T/G, SC retrigger
	playHeadSettings.cc4[3] = 0x1;// playhead never jumps (PRO only)
	playHeadSettings2.cc4[0] = 0x0;// EOS only after SOS (PRO only)
	playHeadSettings2.cc4[1] = 0x0;// low range trig level
	playHeadSettings2.cc4[2] = 0x1;// allow 1 bar locks (PRO only)
	playHeadSettings2.cc4[3] = 0x0;// Exclude EOC from OR (PRO only)
	playHeadSettings3.cc4[0] = 0x0;// Bipol CV mode
	playHeadSettings3.cc4[1] = 0x0;// Channel reset on sustain
	playHeadSettings3.cc4[2] = 0x0;// (unused)
	playHeadSettings3.cc4[3] = 0x0;// (unused)
	setRepetitions(setAndRetDefaultReps());// always have to do this since when the rep param itself was reset by Rack, the default value has not been updated yet, and only now the value is known
	resetNonJson();
	paLengthUnsync->setValue(DEFAULT_LENGTH_UNSYNC);// needed since engine thread can see sync turn off as the result of the ctrl-i and unsync will not be ok (because of mirroring sync effective length to unsync)
}


void PlayHead::resetNonJson() {
	lastTrigMode = trigMode;
	localSyncButton = paSync->getValue() >= 0.5f;
	localLockButton = paLock->getValue() >= 0.5f;
	localFreezeButton = paFreeze->getValue() >= 0.5f;
	localPlayButton = paPlay->getValue() >= 0.5f;
	auditionSlewer.reset();
	trigTrigger.reset();
	scTrigger = true;
	holdOffTimer = 0.0f;
	setAndRetDefaultReps();
	offsetSwingLoopsWithCv = simd::float_4(paOffset->getValue(), paSwing->getValue(), loopStart, loopEndAndSustain);
	offsetSwingLoopsCvConnected = false;
	lengthSyncWithCv = paLengthSync->getValue();
	lengthUnsyncWithCv = paLengthUnsync->getValue();
	lengthCvConnected = false;
	initRun(true);
}


void PlayHead::initRun(bool withSlowSlew) {
	// will autostart itself when in TM_AUTO mode (and conditions are met to start, like play, etc, see start())
	if (withSlowSlew) {
		slowSlewPulseGen.trigger(SLOW_SLEW_DURATION);
		pendingTrig = -1;
	}
	stop();
	cycleCount = 0;
	lengthIndex = 0;
	xt = 0.0;
	// pendingTrig = -1;// don't want to always do this, since it can kill a good pending trigger (do it above when slowSlew instead)
	if (trigMode == TM_AUTO) {
		start();
	}
}

	
void PlayHead::dataToJsonPlayHead(json_t *channelJ, bool withParams, bool withProUnsyncMatch, bool withFullSettings) {
	if (withParams) {
		json_object_set_new(channelJ, "reps", json_real(paRepetitions->getValue()));
		#ifdef SM_PRO
		json_object_set_new(channelJ, "lengthSync", json_integer(getLengthSync(false)));
		if (isSync() && withProUnsyncMatch) {
			setLengthUnsyncParamToMatchSyncActualLength();
		}
		#else
		json_object_set_new(channelJ, "lengthSync", json_integer((int)DEFAULT_LENGTH_SYNC));
		#endif
		json_object_set_new(channelJ, "lengthUnsync", json_real(paLengthUnsync->getValue()));
		json_object_set_new(channelJ, "sync", json_real(paSync->getValue()));
		json_object_set_new(channelJ, "lock", json_real(paLock->getValue()));
		json_object_set_new(channelJ, "swing", json_real(paSwing->getValue()));
		json_object_set_new(channelJ, "freeze", json_real(paFreeze->getValue()));
		json_object_set_new(channelJ, "play", json_real(paPlay->getValue()));
		json_object_set_new(channelJ, "loop", json_real(paSustainLoop->getValue()));
		json_object_set_new(channelJ, "offset", json_real(paOffset->getValue()));
		json_object_set_new(channelJ, "audition", json_real(paAudition->getValue()));
		json_object_set_new(channelJ, "trigLevel", json_real(paTrigLevel->getValue()));
	}
	json_object_set_new(channelJ, "playMode", json_integer(playMode));
	json_object_set_new(channelJ, "triggerMode", json_integer(trigMode));
	json_object_set_new(channelJ, "hysteresis", json_real(hysteresis));
	json_object_set_new(channelJ, "holdOff", json_real(holdOff));
	json_object_set_new(channelJ, "loopStart", json_real(loopStart));
	json_object_set_new(channelJ, "loopEndAndSustain", json_real(loopEndAndSustain));
	if (withFullSettings) {
		json_object_set_new(channelJ, "playHeadSettings", json_integer(playHeadSettings.cc1));
	}	
	json_object_set_new(channelJ, "playHeadSettings2", json_integer(playHeadSettings2.cc1));
	json_object_set_new(channelJ, "playHeadSettings3", json_integer(playHeadSettings3.cc1));
}


bool PlayHead::dataFromJsonPlayHead(json_t *channelJ, bool withParams, bool isDirtyCacheLoad, bool withFullSettings) {
	// returns true when in free version we loaded a preset that has sync 
	bool unsupportedSync = false;
	if (withParams) {
		json_t *repsJ = json_object_get(channelJ, "reps");
		if (repsJ) paRepetitions->setValue(json_number_value(repsJ));
		
		json_t *lengthSyncJ = json_object_get(channelJ, "lengthSync");
		if (lengthSyncJ) paLengthSync->setValue((float)json_integer_value(lengthSyncJ));
		
		json_t *lengthUnsyncJ = json_object_get(channelJ, "lengthUnsync");
		if (lengthUnsyncJ) paLengthUnsync->setValue(json_number_value(lengthUnsyncJ));// lengthUnsyncTimeLast will be auto updated in PlayHead::process() if param changes value
		
		bool loadedSync = false;
		json_t *syncJ = json_object_get(channelJ, "sync");
		if (syncJ) loadedSync = json_number_value(syncJ);
		#ifdef SM_PRO
		paSync->setValue(loadedSync);
		json_t *lockJ = json_object_get(channelJ, "lock");
		if (lockJ) paLock->setValue(json_number_value(lockJ));
		#else
		unsupportedSync = loadedSync;
		paSync->setValue(0.0f);
		paLock->setValue(0.0f);
		#endif

		json_t *swingJ = json_object_get(channelJ, "swing");
		if (swingJ) paSwing->setValue(json_number_value(swingJ));
		
		json_t *freezeJ = json_object_get(channelJ, "freeze");
		if (freezeJ) paFreeze->setValue(json_number_value(freezeJ));			
		
		json_t *playJ = json_object_get(channelJ, "play");
		if (playJ) paPlay->setValue(json_number_value(playJ));
		
		json_t *loopJ = json_object_get(channelJ, "loop");
		if (loopJ) paSustainLoop->setValue(json_number_value(loopJ));					

		json_t *offsetJ = json_object_get(channelJ, "offset");
		if (offsetJ) paOffset->setValue(json_number_value(offsetJ));
		
		json_t *auditionJ = json_object_get(channelJ, "audition");
		if (auditionJ) paAudition->setValue(json_number_value(auditionJ));
		
		json_t *trigLevelJ = json_object_get(channelJ, "trigLevel");
		if (trigLevelJ) paTrigLevel->setValue(json_number_value(trigLevelJ));
	}

	json_t *playModeJ = json_object_get(channelJ, "playMode");
	if (playModeJ) playMode = json_integer_value(playModeJ);
	
	json_t *trigModeJ = json_object_get(channelJ, "triggerMode");
	if (trigModeJ) trigMode = json_integer_value(trigModeJ);
	
	json_t *hysteresisJ = json_object_get(channelJ, "hysteresis");
	if (hysteresisJ) hysteresis = json_number_value(hysteresisJ);
	
	json_t *holdOffJ = json_object_get(channelJ, "holdOff");
	if (holdOffJ) holdOff = json_number_value(holdOffJ);

	json_t *loopStartJ = json_object_get(channelJ, "loopStart");
	if (loopStartJ) loopStart = json_number_value(loopStartJ);

	json_t *loopEndAndSustainJ = json_object_get(channelJ, "loopEndAndSustain");
	if (loopEndAndSustainJ) loopEndAndSustain = json_number_value(loopEndAndSustainJ);

	if (withFullSettings) {
		json_t *playHeadSettingsJ = json_object_get(channelJ, "playHeadSettings");
		if (playHeadSettingsJ) playHeadSettings.cc1 = json_integer_value(playHeadSettingsJ);
	}

	json_t *playHeadSettings2J = json_object_get(channelJ, "playHeadSettings2");
	if (playHeadSettings2J) {
		PackedBytes4 newSettings2;
		newSettings2.cc1 = json_integer_value(playHeadSettings2J);
		if (withFullSettings) {
			playHeadSettings2.cc4[0] = newSettings2.cc4[0];
			playHeadSettings2.cc4[2] = newSettings2.cc4[2];
			playHeadSettings2.cc4[3] = newSettings2.cc4[3];
		}
		playHeadSettings2.cc4[1] = newSettings2.cc4[1];
	} 
	
	json_t *playHeadSettings3J = json_object_get(channelJ, "playHeadSettings3");
	if (playHeadSettings3J) playHeadSettings3.cc1 = json_integer_value(playHeadSettings3J);

	if (!isDirtyCacheLoad) {
		resetNonJson();
	}
	
	return unsupportedSync;
}


bool PlayHead::isDirty(PlayHead* refPlayHead) {
	if (paSync->getValue() != refPlayHead->paSync->getValue()) return true;// float comparison ok since button
	if (paLock->getValue() != refPlayHead->paLock->getValue()) return true;// float comparison ok since button
	if (paRepetitions->getValue() != refPlayHead->paRepetitions->getValue()) return true;// int in float comparison
	if (paLengthSync->getValue() != refPlayHead->paLengthSync->getValue()) return true;// int in float comparison
	if (std::round(paLengthUnsync->getValue() * 1e4f) != std::round(refPlayHead->paLengthUnsync->getValue() * 1e4f)) return true;// float comparison to fourth decimal
	if (playMode != refPlayHead->playMode) return true;// int comparison
	if (trigMode != refPlayHead->trigMode) return true;// int comparison
	if (std::round(paSwing->getValue() * 1e3f) != std::round(refPlayHead->paSwing->getValue() * 1e3f)) return true;// percent comparison
	if (paOffset->getValue() != refPlayHead->paOffset->getValue()) return true;// int in float comparison
	if (paAudition->getValue() != refPlayHead->paAudition->getValue()) return true;// float comparison ok since button
	// if (paFreeze->getValue() != refPlayHead->paFreeze->getValue()) return true;// excluded from dirty comparison
	// if (paPlay->getValue() != refPlayHead->paPlay->getValue()) return true;// excluded from dirty comparison
	if (std::round(getTrigLevel() * 100.0f) != std::round(refPlayHead->getTrigLevel() * 100.0f)) return true;// centivolts
	if (!(getHysteresisText() == refPlayHead->getHysteresisText())) return true;// text comparison (easier)
	if (!(getHoldOffText() == refPlayHead->getHoldOffText())) return true;// text comparison (easier)
	if (paSustainLoop->getValue() != refPlayHead->paSustainLoop->getValue()) return true;
	if (std::round((float)loopEndAndSustain * 1e3f) != std::round((float)(refPlayHead->loopEndAndSustain) * 1e3f)) return true;// float comparison to third decimal
	if (std::round(loopStart * 1e3f) != std::round(refPlayHead->loopStart * 1e3f)) return true;// float comparison to third decimal
	if (playHeadSettings3.cc1 != refPlayHead->playHeadSettings3.cc1) return true;// int comparison
	return false;
}


void PlayHead::processSlow(ChanCvs *chanCvs) {
	
	#ifdef SM_PRO
	if (chanCvs && chanCvs->hasOffsetSwingLoops()) {
		offsetSwingLoopsWithCv = simd::clamp(
			simd::float_4(paOffset->getValue(), paSwing->getValue(), loopStart, loopEndAndSustain) + chanCvs->offsetSwingLoops, 
			simd::float_4(0.0f, 			    -PlayHead::MAX_SWING, 0.0f, LOOP_SUS_SAFETY),
			simd::float_4(PlayHead::MAX_OFFSET,  PlayHead::MAX_SWING, 1.0f, 1.0f - LOOP_SUS_SAFETY));
		offsetSwingLoopsCvConnected = true;
		// special loop stuff
		//   clamp above on offsetSwingLoopsWithCv[3] does same clipping as in setLoopEndAndSustain(), while next line does the same clipping as in setLoopStart()
		offsetSwingLoopsWithCv[2] = std::fmax(std::fmin(offsetSwingLoopsWithCv[2], offsetSwingLoopsWithCv[3] - LOOP_SUS_SAFETY), 0.0f);
	}
	else
	#endif
	{
		offsetSwingLoopsWithCv = simd::float_4(paOffset->getValue(), paSwing->getValue(), loopStart, loopEndAndSustain);
		offsetSwingLoopsCvConnected = false;
	}
	
	
	// trig mode
	if (trigMode != lastTrigMode) {
		// trig mode toggled
		if (isInvalidLoopVsTrigMode()) {
			paSustainLoop->setValue(0.0f);
		}	
		if (trigMode == TM_CV) {
			localSyncButton = false;
			localLockButton = false;
			paSync->setValue(0.0f);
			paLock->setValue(0.0f);
			paSustainLoop->setValue(0.0f);
		}
		lastTrigMode = trigMode;
		setRepetitions(setAndRetDefaultReps());
		auditionSlewer.reset();
		trigTrigger.state = (inTrig->getVoltage() >= 0.5f);// init such that can't trigger a trig when mode change
		scTrigger = true;
		holdOffTimer = 0.0f;
		paAudition->setValue(0.0f);
		initRun();
	}

	// sync and lock button
	bool newSync = isSync();
	if (newSync != localSyncButton) {
		// sync toggled
		localSyncButton = newSync;
		if (localSyncButton) {
			// sync turned on
			// paLock->setValue(1.0f);
			localLockButton = true;// avoid lock toggle detection below, since there's already a start() in here
		}
		else {
			// sync turned off, set length unsynced to length that we had when synced
			#ifdef SM_PRO
			setLengthUnsyncParamToMatchSyncActualLength();
			if (trigMode == TM_AUTO && state != STEPPING) {
				initRun(true);
			}
			#endif
		}
		if (state == STEPPING) {
			// must update lengths and target on sync toggle when we were stepping (if not currently stepping, it will be done when start() gets called)
			stop();// stepping has to be stopped before start since might not step right away when quantized
			start();
		}
	}
	bool newLock = isLock();
	if (newLock != localLockButton) {
		// lock toggled
		localLockButton = newLock;
		if (localSyncButton && state == STEPPING) {
			// must update lengths and target on lock toggle when we were stepping (if not currently stepping, it will be done when start() gets called)
			stop();// stepping has to be stopped before start since might not step right away when quantized
			start();
		}
	}	
	
	// freeze button
	bool newFreeze = paFreeze->getValue() >= 0.5f;
	if (newFreeze != localFreezeButton) {
		// freeze toggled
		localFreezeButton = newFreeze;
		if (localFreezeButton) {
			// freeze turned on
			if (state == STEPPING) {
				hold();
			}
		}
		else {
			// freeze turned off
			if (state == HELD) {
				start();
			}
		}
	}
	
	// play button
	bool newPlay = paPlay->getValue() >= 0.5f;
	if (newPlay != localPlayButton) {
		// play toggled
		// presetAndShapeManager->executeIfStaged(chanNum); // weird effect with this as when we turn play off with a staged preset change, it will load a preset that has its play on (most likely), and it will appear as clicking play off had no effect (it goes back on immediately), do this cleanAllWorkloads() instead
		presetAndShapeManager->clearAllWorkloads();
		localPlayButton = newPlay;
		initFreeze();// turn off freeze button
		initRun(!localPlayButton);// slow-slew when play turned off
	}
}// processSlow()


double PlayHead::process(ChanCvs *chanCvs) {
	// returns a phase in [0 : 1[ 
	// only called when channel is active 

	#ifdef SM_PRO
	if (chanCvs) {
		if (isSync()) {
			lengthSyncWithCv = paLengthSync->getValue();
			if (chanCvs->hasLengthSync()) {
				lengthSyncWithCv = clamp(lengthSyncWithCv + chanCvs->lengthSync, 0.0f, (float)(NUM_RATIOS - 1));
			}
		}
		else {
			lengthUnsyncWithCv = paLengthUnsync->getValue();
			if (chanCvs->hasLengthUnsync()) {
				lengthUnsyncWithCv = clamp(lengthUnsyncWithCv + chanCvs->lengthUnsync, -1.0f, 1.0f);
			}
		}
		lengthCvConnected = true;
	}
	else
	#endif
	{
		lengthSyncWithCv = paLengthSync->getValue();
		lengthUnsyncWithCv = paLengthUnsync->getValue();
		lengthCvConnected = false;
	}

	// SC
	if (trigMode == TM_SC) {
		float paAuditionValue = paAudition->getValue();
		if (auditionSlewer.out != paAuditionValue) {
			auditionSlewer.process(clockDetector->getSampleTime(), paAuditionValue);
		}
		processSidechain();
	}
	// T/G
	else if (trigMode == TM_TRIG_GATE || trigMode == TM_GATE_CTRL) {
		processTrig();
	}
	// CV
	else if (trigMode == TM_CV) {
		if ( *running && localPlayButton && !localFreezeButton ) {
			float inV = inTrig->getVoltage();
			if (isBipolCvMode()) {
				inV += 5.0f;
			}
			xt = clamp(inV * 0.1f, 0.0f, 1.0f);
		}
		reverse = false;
		return xt;
	}
	
	// pending trig counter
	if (pendingTrig >= 0) {
		pendingTrig--;
		if (pendingTrig < 0 && (trigMode == TM_TRIG_GATE || trigMode == TM_SC)) {
			initRun();
			start();// initRun() will not auto start when not TM_AUTO
		}
	}	


	// Return phase and reverse
	double ret = xt;
	reverse = (playMode == PM_REV) || (playMode == PM_PNG && (lengthIndex != 0));
	if (reverse) {
		ret = 1.0 - ret;
	}
	
	#ifdef SM_PRO
	// set lengths
	if (localSyncButton && lsi.targetClockCount != -3) {
		// synced
		if (clockDetector->getClockEdgeDetected()) {
			int32_t clockCount = clockDetector->getClockCount();
			double pulseInterval = clockDetector->getPulseInterval();
			int32_t newRatioIndex = getLengthSyncWithCv(true);//getLengthSync(true);
			
			if (localLockButton) {
				// locked sync (aka quantized sync)
				if (lsi.targetClockCount == -2) {
					// quantized start
					// targetClockCount will stay -2 if doesn't align
					lsi.ratioIndex = newRatioIndex;
					doQuantizedStartIfAligns(clockCount);
				}					
				else if (state == STEPPING) {
					processQuantizedStepping(clockCount, pulseInterval, newRatioIndex);
				}
			}
			else {
				// unlocked sync (aka unquantized sync)
				if (state == STEPPING) {
					syncLengthUpdate(clockCount, pulseInterval, true, newRatioIndex);
				}	
			}// quantized or not
		}// clockEdgeDetected
	}
	else 
	#endif	
	{
		// unsynced
		if (state == STEPPING) {
			#ifdef SM_PRO
			if (lsi.targetClockCount == -3) {
				length = calcLengthSyncTime(getLengthSyncWithCv(true));//getLengthSync(true));
				updateSubLengths(offsetSwingLoopsWithCv[1]);//paSwing->getValue());
			}
			else
			#endif
			{
				length = calcLengthUnsyncTimeWithCv();//calcLengthUnsyncTime();
				updateSubLengths(offsetSwingLoopsWithCv[1]);//paSwing->getValue());
			}
		}
	}// if (localSyncButton)

	
	// step the shape
	if (state == STEPPING) {	
		// advance xt and check for sustain and loop
		double newXt = xt + clockDetector->getSampleTime() / lengths[lengthIndex];
		// check for SOS and also for sustain when TM_GATE_CTRL or TM_TRIG_GATE
		if (isSustain()) {
			// sustain, when not off, is used for SOS in all modes
			double sus = getLoopEndAndSustainWithCv<double>();
			if (reverse) {
				sus = 1.0 - sus;
			}
			if (xt == sus) {
				#ifdef SM_PRO
				setEos();
				#endif
			}
			else if (newXt >= sus && xt < sus) {
				// reached sustain marker
				if (trigTrigger.state) {
					#ifdef SM_PRO
					setSos();
					#endif
					loopingOrSustaining = true;						
					// check actual sustain if loop is off and "gate_ctrl or trig_gate" mode
					if ( (trigMode == TM_GATE_CTRL || trigMode == TM_TRIG_GATE) ) {
						// handle sustain (lower priority)
						newXt = sus;// needed to force startup EOS (see "if (xt == sus)" a few lines above)
						hold();
					}
				}
				else {
					// playthrough
					#ifdef SM_PRO
					if (loopingOrSustaining || !getEosOnlyAfterSos()) {
						setEos();
					}		
					#endif
					loopingOrSustaining = false;
					if (isChannelResetOnSustain()) {						
						initRun();
						newXt = xt;
					}
				}
			}	
		}
		else if (isLoopWithModeGuard()) {
			// loop
			float loopRight = getLoopEndAndSustainWithCv();
			float loopLeft = getLoopStartWithCv();
			if (reverse) {
				float loopRightTmp = 1.0f - loopLeft;
				loopLeft = 1.0f - loopRight;
				loopRight = loopRightTmp;
			}
			if (newXt >= loopRight && xt < loopRight) {
				// reached loopRight
				if (trigTrigger.state) {
					#ifdef SM_PRO
					if (loopingOrSustaining == false) {
						setSos();
					}
					#endif
					loopingOrSustaining = true;
					// goto start
					newXt = loopLeft;
					if (localSyncButton && localLockButton) {
						lsi.targetClockCount = -3;
					}
					else {
						lsi.targetClockCount = -1;
					}
				}
				else {
					// playthrough
					#ifdef SM_PRO
					if (loopingOrSustaining || !getEosOnlyAfterSos()) {
						setEos();
					}
					#endif
					loopingOrSustaining = false;
				}
			}
		}			


		xt = newXt;
		if (xt >= 1.0) {
			// EOC
			nodeTrigPulseGen->trigger(*nodeTrigDuration);
			cycleCount++;
			lengthIndex ^= 0x1;
			int maxRep = getRepetitions();
			if ( (maxRep < (int)INF_REPS) && (cycleCount >= (int32_t)(maxRep << (playMode == PM_PNG ? 1 : 0))) ) {
				// finished 
				#ifdef SM_PRO
				setEoc();// always fires for both types of EOC
				#endif
				if (trigMode == TM_AUTO) {
					stop(); 
					xt = 0.0f;
				}
				else {
					initRun();
				}
			}
			else {
				// not finished
				#ifdef SM_PRO
				if (!getEocOnLastOnly()) {
					setEoc();// only fire this if wanted each cycle
				}	
				#endif
				// check if unsynced playout (which if applicable has now just finished) and need to request sync start again (for locked unfreeze/unsustain etc.)
				if (lsi.targetClockCount == -3) {
					stop();
					lsi.targetClockCount = -2;
				}
				// xt wrap around
				xt = std::fmod(xt, 1.0);// [0 : 1[
				double overtime = xt * lengths[lengthIndex ^ 0x1];// xor because lengthIndex is now on other length
				xt = overtime / lengths[lengthIndex];
				xt = std::fmod(xt, 1.0);// safety in case huge overtime compared to next cycle's length (ex. +99% swing)
			}
			presetAndShapeManager->executeIfStaged(chanNum);
		}// if EOC
		
	}// if (state == STEPPING)		
	
	return ret;
}// process()

	
void PlayHead::processTrig() {
	// assumes: trigMode is either TM_TRIG_GATE or TM_GATE_CTRL
	int trigTriggerEvent = trigTrigger.process(inTrig->getVoltage());// this should not be slow 
	if (trigTriggerEvent != 0) {// && (trigMode == TM_TRIG_GATE || trigMode == TM_GATE_CTRL)) {
		if (trigTriggerEvent == 1) {
			// rising trig/gate edge
			if (trigMode == TM_TRIG_GATE) {
				startDelayed(getAllowRetrig());
			}
			else { //if (trigMode == TM_GATE_CTRL) {
				start();
			}
		}
		else {//if (trigTriggerEvent == -1) {
			// falling trig/gate edge
			if (trigMode == TM_TRIG_GATE) {
				if (state == HELD) {
					start();
				}
			}
			else { // if (trigMode == TM_GATE_CTRL) {
				if (getGateRestart()) {
					initRun(true);// slow-slew when gate restart allowed and gate is turned off 
				}
				else {
					hold();
				}
			}
		}
	}
}

void PlayHead::processSidechain() {
	// assumes: trigMode is TM_SC
	float trigLevel = getTrigLevel();
	if (scTrigger) {
		// high to low
		if (*scEnvelope <= trigLevel * hysteresis) {
			scTrigger = false;
		}
	}
	else {
		// low to high
		if (*scEnvelope > trigLevel) {
			scTrigger = true;
			if (holdOffTimer <= 0.0f) {
				startDelayed(getAllowRetrig());// holdOffTimer is set in the start() that gets triggered by startDelayed() when applicable
			}
		}
	}
	if (holdOffTimer > 0.0f) {
		holdOffTimer -= clockDetector->getSampleTime();
	}
}