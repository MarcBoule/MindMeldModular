//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#pragma once

#include "../MindMeldModular.hpp"
#include "Util.hpp"
#include "ClockDetector.hpp"

class PresetAndShapeManager;


class PlayHead {	

	struct LengthSyncInfo {
		// syncTarget related:
		int ratioIndex;// ratio index that was used in the last length computation
		float swing;// swing that was used in the last length computation
		int32_t targetClockCount = -1;// -1 no target yet, -2 quantized-sync start, -3 playout current cycle unsynced then do quantized start, else should be a value in clockDetector's clockCount range
		int32_t targetCycleCount = -1;// should be an even number; only valid when targetClockCount is valid
	};

	public: 
	// Constants
	static constexpr float INF_REPS = 100.0f;
	static constexpr float DEFAULT_REPETITIONS = INF_REPS;
	static constexpr float DEFAULT_LENGTH_SYNC = 5.0f;// BMP mult/div index, 0 means first in table
	static constexpr float DEFAULT_LENGTH_UNSYNC = 0.0f;// 0 means 1 second
	static constexpr float LENGTH_UNSYNC_MULT = 1.0f;// if changes made to this, must update LenghUnsycCache::lastLengthUnsyncTime's initial value
	static constexpr float LENGTH_UNSYNC_BASE = 1800.0f;
	static constexpr float DEFAULT_SYNC = 0.0f;
	static constexpr float DEFAULT_LOCK = 0.0f;
	static constexpr float DEFAULT_SWING = 0.0f;
	static constexpr float MAX_SWING = 0.99f;
	static constexpr float DEFAULT_OFFSET = 0.0f;
	static constexpr float MAX_OFFSET = 8192.0f;
	static constexpr float DEFAULT_FREEZE = 0.0f;
	static constexpr float DEFAULT_PLAY = 1.0f;
	static constexpr float DEFAULT_LOOP = 0.0f;
	static constexpr float DEFAULT_TRIGLEVEL = 5.0f;
	static constexpr float DEFAULT_HYSTERESIS = 0.2f;// ratio of trig level
	static constexpr float HYSTERESIS_MAX = 1.0f;
	static constexpr float HYSTERESIS_MIN = 0.0f;
	static constexpr float DEFAULT_HOLDOFF = 0.01f;// 10ms to 1000ms
	static constexpr float HOLDOFF_MAX = 1.0f;
	static constexpr float HOLDOFF_MIN = 0.01f;
	static constexpr float SLOW_SLEW_RISE_FALL = 250.0f;// as agressive as possible
	static constexpr float SLOW_SLEW_DURATION = 1.0f / SLOW_SLEW_RISE_FALL;
	static constexpr float LOOP_SUS_SAFETY = 0.005f;
	enum PlayHeadStateIds {STEPPING, HELD, STOPPED};// HELD is needed for GATE mode

	private:
	// need to save, with reset
	Param* paRepetitions;
	Param* paLengthSync;
	Param* paLengthUnsync;// this is the exponent, the actual length time can be obtained using calcLengthUnsyncTime()
	Param* paSync;
	Param* paLock;
	Param* paSwing;
	Param* paFreeze;
	Param* paPhase;
	Param* paPlay;
	Param* paSustainLoop;
	Param* paOffset;
	Param* paAudition;
	Param* paTrigLevel;
	int8_t playMode;
	int8_t trigMode;
	float hysteresis;
	float holdOff;
	float loopStart;// 0.0f to loopEndAndSustain - LOOP_SUS_SAFETY;
	double loopEndAndSustain;// LOOP_SUS_SAFETY to 1.0f - LOOP_SUS_SAFETY; made double since equality test with xt for EOS
	PackedBytes4 playHeadSettings;
	PackedBytes4 playHeadSettings2;
	PackedBytes4 playHeadSettings3;


	// no need to save, with reset
	int state;
	int32_t cycleCount;// start at 0
	int32_t lengthIndex;// start at 0, can only be 0 or 1
	double xt;// in normalized time [0: 1[ (only moves forward for simplicity, code will adjust for backward cycles)
	long pendingTrig;
	dsp::PulseGenerator slowSlewPulseGen;// to force a slower slew when state change or reset, etc. The pulse's duration should be 1/RF (in seconds) where RF is the riseFall number if the dependant slew generator
	int8_t lastTrigMode;
	bool localSyncButton;// use this within playhead when critical to get a consistent read on this, since there is toggle code that must run consistently with process()'s view on button state (so can't use button directly, because toggle code is slow)
	bool localLockButton;// same comment as localSyncButton
	bool localFreezeButton;
	bool localPlayButton;
	SlewLimiterSingle auditionSlewer;
	TriggerRiseFall trigTrigger;
	bool scTrigger;// make bool so that it can use trig
	float holdOffTimer;// <= 0.0f means ready to accept new sc trig, > 0.0f means no new trig can register
	public:
	simd::float_4 offsetSwingLoopsWithCv;// offset = [0], swing = [1], loopstart = [2], loopend = [3]
	bool offsetSwingLoopsCvConnected;
	float lengthSyncWithCv;
	float lengthUnsyncWithCv;
	bool lengthCvConnected;// serves for both sync and unsync
	private:
	
	
	// no need to save, no reset
	int chanNum = 0;
	uint32_t* sosEosEoc = NULL;
	bool loopingOrSustaining = false;// reset by start(), which is only way to get STEPPING; STEPPING guards loopingOrSustaining use
	float lastPaLengthUnsync = 0.0f;
	float lastPaLengthUnsyncWithCv = 0.0f;
	double lastLengthUnsyncTime = (double)LENGTH_UNSYNC_MULT;
	double lastLengthUnsyncTimeWithCv = (double)LENGTH_UNSYNC_MULT;
	ClockDetector* clockDetector = NULL;
	bool* running = NULL;
	float* scEnvelope = NULL;
	PresetAndShapeManager* presetAndShapeManager;
	ParamQuantity* paramQuantityRepititionSrc = NULL;
	Input* inTrig;
	double length = 1.0;// this is not a double length; only valid when state == STEPPING
	double lengths[2] = {1.0, 1.0} ;// sum must equal 2 * length; only valid when state == STEPPING
	LengthSyncInfo lsi;
	bool reverse;// valid only after calling this.process()
	dsp::PulseGenerator* nodeTrigPulseGen;
	float* nodeTrigDuration;
	
	
	#ifdef SM_PRO
	void setSos() {
		*sosEosEoc |= (0x1 << (16 + chanNum));
	}
	void setEos() {
		*sosEosEoc |= (0x1 << (8  + chanNum));
	}
	void setEoc() {
		*sosEosEoc |= (0x1 << (0  + chanNum));
		if (isExcludeEocFromOr()) {
			*sosEosEoc |= (0x1 << (24  + chanNum));
		}
	}
	#endif
	
	
	
	public: 
	
	void construct(int _chanNum, uint32_t* _sosEosEoc, ClockDetector* _clockDetector, bool* _running, ParamQuantity* _paramQuantityRepititionSrc, Param* _chanParams, Input* _trigInput, float* _scEnvelope, PresetAndShapeManager* _presetAndShapeManager, dsp::PulseGenerator* _nodeTrigPulseGen, float* nodeTrigDuration);
	
	void onReset(bool withParams);
	
	void resetNonJson();
	
	void initRun(bool withSlowSlew = false);
	
	float setAndRetDefaultReps() {
		float defaultReps = (trigMode == TM_AUTO || trigMode == TM_GATE_CTRL) ? INF_REPS : 1.0f;
		if (paramQuantityRepititionSrc) {
			paramQuantityRepititionSrc->defaultValue = defaultReps;
		}
		return defaultReps;
	}
		
	void dataToJsonPlayHead(json_t *channelJ, bool withParams, bool withProUnsyncMatch, bool withFullSettings);
	
	bool dataFromJsonPlayHead(json_t *channelJ, bool withParams, bool isDirtyCacheLoad, bool withFullSettings);
	
	
	// Setters
	// --------

	void setRepetitions(float reps) {
		paRepetitions->setValue(reps);
	}
	
	void setPlayMode(int8_t _playMode) {
		playMode = _playMode;
	}
	
	void setBipolCvMode(int8_t _bipolMode) {
		playHeadSettings3.cc4[0] = _bipolMode;
	}
	void setChannelResetOnSustain(int8_t _rstSus) {
		playHeadSettings3.cc4[1] = _rstSus;
	}
	
	void setTrigMode(int8_t _trigMode) {
		trigMode = _trigMode;
	}
	void setHysteresis(float _hysteresis) {
		hysteresis = _hysteresis;
	}
	void setHoldOff(float _holdOff) {
		holdOff = _holdOff;
	}

	float setLoopStart(float _val) {
		loopStart = std::fmax(std::fmin(_val, loopEndAndSustain - LOOP_SUS_SAFETY), 0.0f);
		return loopStart;
	}
	void setLoopEndAndSustain(float _val) {
		loopEndAndSustain = (double)clamp(_val, LOOP_SUS_SAFETY, 1.0f - LOOP_SUS_SAFETY);// must have safety at right edge or else could hit it and count as a rep (and restart a new rep without any loop/sus!)
		setLoopStart(loopStart);
	}

	void initFreeze() {
		paFreeze->setValue(DEFAULT_FREEZE);
		localFreezeButton = DEFAULT_FREEZE >= 0.5f;
	}

	// Getters
	// --------

	int getState() {
		return state;
	}	
	
	float getCoreLength() {
		return length;
	}
	
	int getRepetitions() {
		return (int)(paRepetitions->getValue() + 0.5f);
	}
	
	#ifdef SM_PRO
	int getLengthSync(bool withInadmissibleFilter) {
		// returns an index into a ratio table
		int _lengthSyncIndex = (int)paLengthSync->getValue();
		if (withInadmissibleFilter) {
			_lengthSyncIndex = filterLengthForPpqnSync(_lengthSyncIndex);
			paLengthSync->setValue((float)_lengthSyncIndex);// this is buggy in Rack v2
		}
		return _lengthSyncIndex;
	}
	int getLengthSyncWithCv(bool withInadmissibleFilter) {
		// returns an index into a ratio table
		int _lengthSyncIndexWithCv = (int)lengthSyncWithCv;
		if (withInadmissibleFilter) {
			// filter _lengthSyncIndexWithCv
			_lengthSyncIndexWithCv = filterLengthForPpqnSync(_lengthSyncIndexWithCv);
			// but also do the same filter-and-set as in getLengthSync() with the param
			int _lengthSyncIndex = (int)paLengthSync->getValue();
			_lengthSyncIndex = filterLengthForPpqnSync(_lengthSyncIndex);
			paLengthSync->setValue((float)_lengthSyncIndex);// this is buggy in Rack v2
		}
		return _lengthSyncIndexWithCv;
	}
	#endif
	
	bool inadmissibleLengthSync(int _lengthSyncIndex) {
		if (!isSync()) {
			return false;
		}
		int ppqn = clockDetector->getPpqn();
		return (_lengthSyncIndex == 0 && (ppqn <= 24)) || (_lengthSyncIndex == 1 && (ppqn <= 12));
	}
	
	float getLengthUnsync() {
		// this is the exponent, the actual length time can be obtained using calcLengthUnsyncTime()
		return paLengthUnsync->getValue();
	}
	
	float getSwing() {
		return paSwing->getValue();
	}
	
	float getOffset() {
		return paOffset->getValue();
	}
	
	float getTrigLevel() {// takes low range trig level option into account, returns volts
		float trigLevRanged = paTrigLevel->getValue();
		if (isSidechainLowTrig()) {
			trigLevRanged *= 0.5f;
		}
		return trigLevRanged;
	}
	
	float getHysteresis() {
		return hysteresis;
	}
	float getHoldOff() {
		return holdOff;
	}
	bool getAudition() {
		return paAudition->getValue() >= 0.5f;
	}
	float getAuditionGain() {
		return auditionSlewer.out;
	}

	bool isSync() {
		// use only for non critical UI stuff, for playhead use localSyncButton
		return paSync->getValue() >= 0.5f;
	}

	bool isLock() {
		// use only for non critical UI stuff, for playhead use localLockButton
		return paLock->getValue() >= 0.5f;
	}	

	bool isSustain() {
		return paSustainLoop->getValue() >= 0.5f && paSustainLoop->getValue() < 1.5f;
	}	
	bool isLoop() {
		return paSustainLoop->getValue() >= 1.5f;
	}	
	bool currTrigModeAllowsLoop() {
		return trigMode == TM_GATE_CTRL || trigMode == TM_TRIG_GATE;
	}	
	bool isLoopWithModeGuard() {
		return isLoop() && currTrigModeAllowsLoop();
	}	
	bool isInvalidLoopVsTrigMode() {
		return isLoop() && !currTrigModeAllowsLoop();
	}	

	int8_t getPlayMode() {
		return playMode;
	}
	
	int8_t getBipolCvMode() {
		return playHeadSettings3.cc4[0];
	}
	
	int8_t getTrigMode() {
		return trigMode;
	}

	float getLoopStart() {
		return loopStart;
	}
	float getLoopStartWithCv() {
		return offsetSwingLoopsWithCv[2];
	}
	template<typename T = float>
	T getLoopEndAndSustain() {
		return (T)loopEndAndSustain;
	}
	template<typename T = float>
	T getLoopEndAndSustainWithCv() {
		return (T)offsetSwingLoopsWithCv[3];
	}
	
	bool isSlowSlew() {
		return slowSlewPulseGen.remaining > 0.f;
	}
	
	std::string getHysteresisText() {
		return string::f("%.1f",  math::normalizeZero(getHysteresis() * 100.0f));
	}	
	std::string getHoldOffText() {
		float valHold = getHoldOff();
		if (valHold < 1.0f) {
			return string::f("%.1f", math::normalizeZero(valHold * 1000.0f));
		}
		return string::f("%.2f", math::normalizeZero(valHold));
	}
	bool getReverse() {
		return reverse;
	}

	// --------
	
	
	bool getEocOnLastOnly() {
		return playHeadSettings.cc4[0] != 0;
	}
	bool getGateRestart() {
		return playHeadSettings.cc4[1] != 0;
	}
	bool getAllowRetrig() {
		return playHeadSettings.cc4[2] != 0;
	}
	bool isBipolCvMode() {
		return playHeadSettings3.cc4[0] != 0;
	}
	bool isChannelResetOnSustain() {
		return playHeadSettings3.cc4[1] != 0;
	}
	#ifdef SM_PRO
	bool getPlayheadNeverJumps() {
		return playHeadSettings.cc4[3] != 0;
	}
	bool getEosOnlyAfterSos() {
		return playHeadSettings2.cc4[0] != 0;
	}
	bool isAllow1BarLocks() {
		return playHeadSettings2.cc4[2] != 0;
	}
	bool isExcludeEocFromOr() {
		return playHeadSettings2.cc4[3] != 0;
	}
	#endif
	bool isSidechainLowTrig() {
		return playHeadSettings2.cc4[1] != 0;
	}
	void toggleEocOnLastOnly() {
		playHeadSettings.cc4[0] ^= 0x1;
	}
	void toggleGateRestart() {
		playHeadSettings.cc4[1] ^= 0x1;
	}
	void toggleAllowRetrig() {
		playHeadSettings.cc4[2] ^= 0x1;
	}
	void toggleBipolCvMode() {
		playHeadSettings3.cc4[0] ^= 0x1;
	}
	void toggleChannelResetOnSustain() {
		playHeadSettings3.cc4[1] ^= 0x1;
	}
	#ifdef SM_PRO
	void togglePlayheadNeverJumps() {
		playHeadSettings.cc4[3] ^= 0x1;
	}
	void toggleEosOnlyAfterSos() {
		playHeadSettings2.cc4[0] ^= 0x1;
	}
	void toggleExcludeEocFromOr() {
		playHeadSettings2.cc4[3] ^= 0x1;
	}
	void toggleAllow1BarLocks() {
		playHeadSettings2.cc4[2] ^= 0x1;
	}
	void togglePlay() {
		paPlay->setValue(paPlay->getValue() >= 0.5f ? 0.0f: 1.0f);
	}
	void toggleFreeze() {
		paFreeze->setValue(paFreeze->getValue() >= 0.5f ? 0.0f: 1.0f);
	}
	#endif
	void toggleSidechainLowTrig() {
		playHeadSettings2.cc4[1] ^= 0x1;
	}
	#ifdef SM_PRO
	float calcLengthSyncTime() {// use calcLengthSyncTime(getLengthSync(true)) to get double, this is just for point menu
		return calcLengthSyncTime(getLengthSync(true));
	}
	void setLengthUnsyncParamToMatchSyncActualLength() {
		double lengthUnsync = calcLengthSyncTime(getLengthSync(true));
		lengthUnsync = std::log(lengthUnsync / LENGTH_UNSYNC_MULT);
		lengthUnsync = lengthUnsync / std::log(LENGTH_UNSYNC_BASE);
		paLengthUnsync->setValue(lengthUnsync);
	}
	#endif

	
	bool isDirty(PlayHead* refPlayHead);
	
	
	void hold() {
		state = HELD;
	}
	
	
	void stop() {
		state = STOPPED;
	}
	
	
	void startDelayed(bool allowRetrig) {
		// pendingTrig can be set to 0 for immediate start (when offet is 0), which will implicitly happen on the same sample since channel code will call process() after having called startDelayed() in the same sample
		// when startDelayed() is called and there is already a pendingTrig, the new start is simply ignored
		if ( *running && localPlayButton && !localFreezeButton && 
				pendingTrig == -1 && (allowRetrig || (state == STOPPED)) ) {
			pendingTrig = (long)offsetSwingLoopsWithCv[0];//paOffset->getValue();
		}
	}
	
	
	void start() {
		// this method should be the only way to set state = STEPPING, because of sync code below
		// lengths (and lsi) should be set in here when synced, since must play properly until next clock
		// no lengths (nor lsi) need to be set when not synced, since process() will set length each sample when stepping
		if ( *running && localPlayButton && !localFreezeButton ) {			
			loopingOrSustaining = false;
			holdOffTimer = holdOff;// only SC uses this, but simpler to put here
			#ifdef SM_PRO
			if (localSyncButton) {
				// synced		
				startSynced();
			}
			else 
			#endif
			{
				// unsynced
				state = STEPPING;
			}
			if (state == STEPPING) {
				if ((xt == 0.0 || xt == 1.0) && (paPhase == 0.0f || paPhase == 1.0f)) {
					nodeTrigPulseGen->trigger(*nodeTrigDuration);
				}
			}
		}
	}
	

	double calcLengthUnsyncTime() {
		float paLengthUnsyncValue = paLengthUnsync->getValue();
		if (paLengthUnsyncValue == lastPaLengthUnsync) {
			return lastLengthUnsyncTime;
		}
		lastPaLengthUnsync = paLengthUnsyncValue;
		return lastLengthUnsyncTime = ((double)LENGTH_UNSYNC_MULT) * std::pow<double>(LENGTH_UNSYNC_BASE, paLengthUnsyncValue);
	}
	double calcLengthUnsyncTimeWithCv() {
		if (lengthUnsyncWithCv == lastPaLengthUnsyncWithCv) {
			return lastLengthUnsyncTimeWithCv;
		}
		lastPaLengthUnsyncWithCv = lengthUnsyncWithCv;
		return lastLengthUnsyncTimeWithCv = ((double)LENGTH_UNSYNC_MULT) * std::pow<double>(LENGTH_UNSYNC_BASE, lengthUnsyncWithCv);
	}
	void updateSubLengths(float _swing) {
		// call after lengths has been changed
		lengths[0] = length * (double)(1.0f + _swing);
		lengths[1] = length + length - lengths[0];
	}
	

	// Process related
	// --------
	
	void processSlow(ChanCvs *chanCvs);
	
	void processDiv8() {
		slowSlewPulseGen.process(clockDetector->getSampleTime() * 8.0f);
	}

	double process(ChanCvs *chanCvs);
			
	void processTrig();
	
	void processSidechain();
	
	#ifdef SM_PRO
	#include "../Sync.hpp"
	#endif

};

