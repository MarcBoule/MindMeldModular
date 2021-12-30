//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#pragma once

#include "../MindMeldModular.hpp"
#include "ShapeMasterComponents.hpp"
#include "../dsp/LinkwitzRileyCrossover.hpp"
#include "../dsp/ButterworthFilters.hpp"
#include "PlayHead.hpp"
#include "Shape.hpp"


class PresetAndShapeManager;


class Channel {
	public: 
	
	// Constants
	static constexpr float DEFAULT_PHASE = 0.0f;
	static constexpr float DEFAULT_RESPONSE = 0.0f;
	static constexpr float MAX_RESPONSE = 0.999f;
	static constexpr float DEFAULT_WARP = 0.0f;
	static constexpr float MAX_WARP = 0.999f;
	static constexpr float DEFAULT_AMOUNT = 1.0f;
	static constexpr float DEFAULT_SLEW = 0.0f;// min slew knob, which should corresond to max slope (least effect)
	static constexpr float DEFAULT_SMOOTH = 0.0f;
	static constexpr float MAXFREQ_SMOOTH = 400.0f;
	static constexpr float MINFREQ_SMOOTH = 0.5f;
	static constexpr float DEFAULT_CROSSOVER = -1.0f;// -1.0f = crossover off
	static constexpr float CROSSOVER_MULT = 600.0f;
	static constexpr float CROSSOVER_BASE = 20.0e3f / CROSSOVER_MULT;
	static constexpr float CROSSOVER_OFF = -0.969953f;// param value that corresponds to 20 Hz
	static constexpr float DEFAULT_HIGH = 1.0f;
	static constexpr float DEFAULT_LOW = 1.0f;
	static constexpr float DEFAULT_SENSITIVITY = 0.5f;// dB or volts, not sure yet
	static constexpr float SENSITIVITY_MAX = 1.0f;
	static constexpr float SENSITIVITY_MIN = 0.0f;
	static constexpr float DEFAULT_NODETRIG_DURATION = 0.01f;
	
	static const int SCF_SCALING_EXP = 2;// must adjust bounds below when changing this; used in menu.cpp L776, L824; channel.hpp L199, L203, L527, L539; inverse used in channel.cpp L175, L184
	
	// static constexpr float SC_HPF_FREQ_MIN = 13.0f;
	static constexpr float SC_HPF_SQFREQ_MIN = 3.605551275464f;
	
	// static constexpr float SC_HPF_FREQ_DEF = 13.0f;
	static constexpr float SC_HPF_SQFREQ_DEF = 3.605551275464f;
	
	// static constexpr float SC_HPF_FREQ_MAX = 10000.0f;
	static constexpr float SC_HPF_SQFREQ_MAX = 100.0f;
	
	// static constexpr float SC_HPF_FREQ_OFF = 20.0f;
	static constexpr float SC_HPF_SQFREQ_OFF = 4.4721359549996f;
	
	// static constexpr float SC_LPF_FREQ_MIN = 100.0f;
	static constexpr float SC_LPF_SQFREQ_MIN = 10.0f;
	
	// static constexpr float SC_LPF_FREQ_DEF = 21000.0f;
	static constexpr float SC_LPF_SQFREQ_DEF = 144.91376746189f;
	
	// static constexpr float SC_LPF_FREQ_MAX = 21000.0f;
	static constexpr float SC_LPF_SQFREQ_MAX = 144.91376746189f;
	
	// static constexpr float SC_LPF_FREQ_OFF = 20000.0f;
	static constexpr float SC_LPF_SQFREQ_OFF = 141.42135623731f;
	

	private:	
	// need to save, no reset
	// none
	
	// need to save, with reset
	Param* paPhase;
	Param* paResponse;
	Param* paWarp;
	Param* paAmount;
	Param* paSlew;
	Param* paSmooth;
	Param* paCrossover;
	Param* paHigh;
	Param* paLow;
	Param* paPrevNextPreSha;
	SlewLimiterSingle slewLimiter;
	float hpfCutoffSqFreq;// always use getter and setter since tied to Biquad
	float lpfCutoffSqFreq;// always use getter and setter since tied to Biquad
	float sensitivity;
	float gainAdjustVca;// this is a gain here (not dB)
	float gainAdjustSc;// this is a gain here (not dB)
	float nodeTrigDuration;
	uint8_t gridX;
	int8_t rangeIndex;
	public:
	PackedBytes4 channelSettings;
	PackedBytes4 channelSettings2;
	private:
	PackedBytes4 channelSettings3;
	std::string presetPath;
	std::string shapePath;
	std::string chanName;
	RandomSettings randomSettings;
	Shape shape;
	PlayHead playHead;
	

	// no need to save, with reset
	double sampleTime;
	LinkwitzRileyStereo8xCrossover xover;
	float lastCrossoverParamWithCv;
	ButterworthFourthOrder hpFilter;
	ButterworthFourthOrder lpFilter;
	FirstOrderFilter smoothFilter;
	float lastSmoothParam;
	double lastProcessXt;
	// float lastSlewParamWithCv;
	bool channelActive;
	int vcaPreSize;
	int vcaPostSize;
	float scSignal;// implicitly mono
	float scEnvelope;// implicitly mono
	dsp::SlewLimiter scEnvSlewer;
	public:
	simd::float_4 warpPhaseResponseAmountWithCv;// warp = [0]
	bool warpPhaseResponseAmountCvConnected;
	simd::float_4 xoverSlewWithCv;// xfreq = [0], xhigh = [1], xlow = [2], slew = [3] (slew unrelated to xvoer)
	bool xoverSlewCvConnected;
	private:
	dsp::PulseGenerator nodeTrigPulseGen;
		
	// no need to save, no reset
	int chanNum = 0;
	bool* running = NULL;
	Input* inInput = NULL;// use only in process() because of channelDirtyCache and possible nullness
	Input* scInput = NULL;// use only in process() because of channelDirtyCache and possible nullness
	Output* outOutput = NULL;// use only in process() because of channelDirtyCache and possible nullness
	Output* cvOutput = NULL;// use only in process() because of channelDirtyCache and possible nullness
	float lengthUnsyncOld = -10.0f;// don't need init nor reset for this, used in caching detection with param that is [-1.0f : 1.0f], used with next line
	std::string lengthUnsyncTextOld;// used with previous line
	float vcaPre[16] = {};
	float vcaPost[16] = {};
	PresetAndShapeManager* presetAndShapeManager;
	ClockDetector* clockDetector;
	bool prevNextButtonsClicked[4] = {false};// matches the PREV_NEXT_PRE_SHA param
	dsp::SchmittTrigger arrowButtonTriggers[4];	
	
	public:

	void construct(int _chanNum, bool* _running, uint32_t* _sosEosEoc, ClockDetector* _clockDetector, Input* _inputs, Output* _outputs, Param* _params, std::vector<ParamQuantity*>* _paramQuantitiesSrc, PresetAndShapeManager* _presetAndShapeManager);
	
	void onReset(bool withParams);
	
	void resetShape() {
		presetPath = "";
		shapePath = "";
		shape.onReset();
	}
	
	void resetNonJson();

	void initRun(bool withSlowSlew) {
		playHead.initRun(withSlowSlew);
	}
	
	json_t* dataToJsonChannel(bool withParams, bool withProUnsyncMatch, bool withFullSettings);
	
	bool dataFromJsonChannel(json_t *channelJ, bool withParams, bool isDirtyCacheLoad, bool withFullSettings);
	
	json_t* dataToJsonShape() {
		return shape.dataToJsonShape();
	}
	void dataFromJsonShape(json_t *shapeJ) {
		shape.dataFromJsonShape(shapeJ);
	}
	
	void copyShapeTo(Shape* destShape) {
		shape.copyShapeTo(destShape);
	}

	void pasteShapeFrom(Shape* srcShape) {
		shape.pasteShapeFrom(srcShape);
	}
	
	void reverseShape() {
		shape.reverseShape();
	}
	void setPointWithSafety(int p, Vec newPt, int xQuant, int yQuant) {
		shape.setPointWithSafety(p, newPt, xQuant, yQuant, isDecoupledFirstAndLast());
	}
	
	void invertShape() {
		shape.invertShape();
	}
	
	void randomizeShape(bool withHistory) {
		// Push ShapeCompleteChange history action (rest is done further below)
		ShapeCompleteChange* h = NULL;
		if (withHistory) {
			h = new ShapeCompleteChange;
			h->shapeSrc = &shape;
			h->oldShape = new Shape();
			shape.copyShapeTo(h->oldShape);
		}
		
		shape.randomizeShape(&randomSettings, getGridX(), getRangeIndex());
		
		if (withHistory) {
			h->newShape = new Shape();
			shape.copyShapeTo(h->newShape);
			h->name = "randomise shape";
			APP->history->push(h);
		}	
	}


	void onSampleRateChange() {
		sampleTime = 1.0 / (double)APP->engine->getSampleRate();
		setCrossoverCutoffFreq();
		setHPFCutoffSqFreq(hpfCutoffSqFreq);
		setLPFCutoffSqFreq(lpfCutoffSqFreq);
		setSmoothCutoffFreq();
	}
	

	// Setters
	// --------

	void setCrossoverCutoffFreq() {
		lastCrossoverParamWithCv = xoverSlewWithCv[0];//paCrossover->getValue();
		float freq = CROSSOVER_MULT * std::pow(CROSSOVER_BASE, lastCrossoverParamWithCv);
		xover.setFilterCutoffs(freq * sampleTime, true);// true <-> is24dB
	}
	void setHPFCutoffSqFreq(float sqfc) {// always use this instead of directly accessing hpfCutoffFreq
		hpfCutoffSqFreq = sqfc;
		hpFilter.setParameters(true, std::pow(sqfc, SCF_SCALING_EXP) * APP->engine->getSampleTime());// don't use sampleTime since not ready when called by onReset()
	}
	void setLPFCutoffSqFreq(float sqfc) {// always use this instead of directly accessing lpfCutoffFreq
		lpfCutoffSqFreq = sqfc;
		lpFilter.setParameters(false, std::pow(sqfc, SCF_SCALING_EXP) * APP->engine->getSampleTime());// don't use sampleTime since not ready when called by onReset()
	}
	void setSmoothCutoffFreq() {
		lastSmoothParam = paSmooth->getValue();
		float fc = (MINFREQ_SMOOTH - MAXFREQ_SMOOTH) * std::pow(lastSmoothParam, 1.0f / 4.0f) + MAXFREQ_SMOOTH;
		smoothFilter.setParameters(false, fc * sampleTime);
	}
	void setHysteresis(float _hysteresis) {
		playHead.setHysteresis(_hysteresis);
	}
	void setHoldOff(float _holdOff) {
		playHead.setHoldOff(_holdOff);
	}
	void setSensitivity(float _sensitivity) {
		sensitivity = _sensitivity;
		float fall = rescale(sensitivity, SENSITIVITY_MIN, SENSITIVITY_MAX, 5.0f, 50.0f);
		scEnvSlewer.setRiseFall(1000.0f, fall);
	}
	void setGainAdjustVca(float _gainAdjust) {;
		gainAdjustVca = _gainAdjust;// this is a gain here (not dB)
	}
	void setGainAdjustSc(float _gainAdjust) {;
		gainAdjustSc = _gainAdjust;// this is a gain here (not dB)
	}
	void setNodeTrigDuration(float _duration) {;
		nodeTrigDuration = _duration;
	}
	void setPlayMode(int8_t _playMode) {
		playHead.setPlayMode(_playMode);
	}
	void setBipolCvMode(int8_t _bipolMode) {
		playHead.setBipolCvMode(_bipolMode);
	}
	void setTrigMode(int8_t _trigMode) {
		playHead.setTrigMode(_trigMode);
	}
	void setLoopStart(float _val) {
		playHead.setLoopStart(_val);
	}
	void setLoopEndAndSustain(float _val) {
		playHead.setLoopEndAndSustain(_val);
	}
	void setGridX(uint8_t _gridX, bool withHistory) {
		// Push GridXChange history action
		if (_gridX != gridX) {
			if (withHistory) {
				GridXChange* h = new GridXChange;
				h->channelSrc = this;
				h->oldGridX = gridX;
				h->newGridX = _gridX;
				APP->history->push(h);
			}
			
			gridX = _gridX;
		}
	}
	void setRangeIndex(int8_t _rangeIndex, bool withHistory) {
		// Push RangeIndexChange history action
		if (_rangeIndex != rangeIndex) {
			if (withHistory) {
				RangeIndexChange* h = new RangeIndexChange;
				h->channelSrc = this;
				h->oldRangeIndex = rangeIndex;
				h->newRangeIndex = _rangeIndex;
				APP->history->push(h);
			}

			rangeIndex = _rangeIndex;
		}
	}
	void toggleEocOnLastOnly() {
		playHead.toggleEocOnLastOnly();
	}
	void toggleGateRestart() {
		playHead.toggleGateRestart();
	}
	void toggleAllowRetrig() {
		playHead.toggleAllowRetrig();
	}
	void toggleBipolCvMode() {
		playHead.toggleBipolCvMode();
	}
	void toggleChannelResetOnSustain() {
		playHead.toggleChannelResetOnSustain();
	}
	#ifdef SM_PRO
	void togglePlayheadNeverJumps() {
		playHead.togglePlayheadNeverJumps();
	}
	void toggleEosOnlyAfterSos() {
		playHead.toggleEosOnlyAfterSos();
	}
	void toggleExcludeEocFromOr() {
		playHead.toggleExcludeEocFromOr();
	}
	void toggleAllow1BarLocks() {
		playHead.toggleAllow1BarLocks();
	}
	#endif
	void toggleSidechainLowTrig() {
		playHead.toggleSidechainLowTrig();
	}
	void setPresetPath(std::string newPresetPath) {
		presetPath = newPresetPath;
		shapePath = "";
	}
	void setShapePath(std::string newShapePath) {
		shapePath = newShapePath;
		presetPath = "";
	}
	void setChanName(std::string newChanName) {
		chanName = newChanName;
	}
	void updateChannelActive() {
		channelActive = cvOutput->isConnected() || outOutput->isConnected();
	}

	// Getters
	// --------
	
	bool getChannelActive() {
		return channelActive;
	}
	int getState() {
		return playHead.getState();
	}
	float getHPFCutoffSqFreq() {
		return hpfCutoffSqFreq;
	}
	float getLPFCutoffSqFreq() {
		return lpfCutoffSqFreq;
	}
	float getHysteresis() {
		return playHead.getHysteresis();
	}
	float getHoldOff() {
		return playHead.getHoldOff();
	}
	float getSensitivity() {
		return sensitivity;
	}
	float getGainAdjustVca() {
		return gainAdjustVca;// this is a gain here (not dB)
	}
	float getGainAdjustSc() {
		return gainAdjustSc;// this is a gain here (not dB)
	}
	float getNodeTrigDuration() {
		return nodeTrigDuration;
	}
	float getGainAdjustVcaDb() {
		return 20.0f * std::log10(gainAdjustVca);
	}
	float getGainAdjustScDb() {
		return 20.0f * std::log10(gainAdjustSc);
	}
	bool isSustain() {
		return playHead.isSustain();
	}	
	bool isLoop() {
		return playHead.isLoop();
	}	
	bool currTrigModeAllowsLoop() {
		return playHead.currTrigModeAllowsLoop();
	}	
	bool isLoopWithModeGuard() {
		return playHead.isLoopWithModeGuard();
	}	
	bool isInvalidLoopVsTrigMode() {
		return playHead.isInvalidLoopVsTrigMode();
	}	
	int8_t getPlayMode() {
		return playHead.getPlayMode();
	}
	int8_t getBipolCvMode() {
		return playHead.getBipolCvMode();
	}
	bool isChannelResetOnSustain() {
		return playHead.isChannelResetOnSustain();
	}
	int8_t getTrigMode() {
		return playHead.getTrigMode();
	}
	float getLoopStart() {
		return playHead.getLoopStart();
	}
	float getLoopStartWithCv() {
		return playHead.getLoopStartWithCv();
	}
	float getLoopEndAndSustain() {
		return playHead.getLoopEndAndSustain();
	}
	float getLoopEndAndSustainWithCv() {
		return playHead.getLoopEndAndSustainWithCv();
	}
	int getRepetitions() {
		return playHead.getRepetitions();
	}
	uint8_t getGridX() {
		return gridX;
	}
	int8_t getRangeIndex() {
		return rangeIndex;
	}
	bool getGateRestart() {
		return playHead.getGateRestart();
	}
	bool getAllowRetrig() {
		return playHead.getAllowRetrig();
	}
	#ifdef SM_PRO
	bool getPlayheadNeverJumps() {
		return playHead.getPlayheadNeverJumps();
	}
	bool getEocOnLastOnly() {
		return playHead.getEocOnLastOnly();
	}
	bool isExcludeEocFromOr() {
		return playHead.isExcludeEocFromOr();
	}
	bool getEosOnlyAfterSos() {
		return playHead.getEosOnlyAfterSos();
	}
	bool isAllow1BarLocks() {
		return playHead.isAllow1BarLocks();
	}
	#endif
	bool isSidechainLowTrig() {
		return playHead.isSidechainLowTrig();
	}
	bool getLocalInvertShadow() {
		return channelSettings.cc4[0] != 0;
	}
	int8_t getShowUnsyncedLengthAs() {
		return channelSettings2.cc4[1];
	}
	int8_t getShowTooltipVoltsAs() {
		return channelSettings2.cc4[2];
	}
	bool isDecoupledFirstAndLast() {
		return channelSettings2.cc4[3] != 0;
	}
	bool isNodeTriggers() {
		return channelSettings3.cc4[0] != 0;
	}
	int8_t getPolyMode() {
		return channelSettings.cc4[2];
	}
	bool isSidechainUseVca() {
		return channelSettings.cc4[3] != 0;
	}
	std::string getPresetPath() {
		return presetPath;
	}
	std::string getShapePath() {
		return shapePath;
	}
	std::string getChanName() {
		return chanName;
	}
	RandomSettings* getRandomSettings() {
		return &randomSettings;
	}
	Shape* getShape() {
		return &shape;
	}
	PlayHead* getPlayHead() {
		return &playHead;
	}
	bool isSync() {
		return playHead.isSync();
	}
	bool getAudition() {
		return playHead.getAudition();
	}
	bool inadmissibleLengthSync(int _lengthSyncIndex) {
		return playHead.inadmissibleLengthSync(_lengthSyncIndex);
	}
	#ifdef SM_PRO
	int getLengthSync() {
		return playHead.getLengthSync(false);
	}
	int getLengthSyncWithCv() {
		return playHead.getLengthSyncWithCv(false);
	}
	float calcLengthSyncTime() {
		return playHead.calcLengthSyncTime();
	}
	#endif
	float calcLengthUnsyncTime() {
		return playHead.calcLengthUnsyncTime();
	}
	std::string getLengthText(bool* inactive);

	std::string getRepetitionsText(bool* inactive) {
		*inactive = (playHead.getTrigMode() == TM_CV);
		int reps = getRepetitions();
		if (reps >= (int)PlayHead::INF_REPS) {
			return "INF";
		}
		return string::f("%2i", clamp(reps, 1, 99));
	}
	
	std::string getOffsetText(bool* inactive) {
		*inactive = !(playHead.getTrigMode() == TM_TRIG_GATE || playHead.getTrigMode() == TM_SC);
		return string::f("%i", (int)(playHead.getOffset()));
	}
	
	std::string getSwingText(bool* inactive) {
		*inactive = playHead.getTrigMode() == TM_CV;
		std::string ret = string::f("%.1f%%", playHead.getSwing() * 100.0f);
		return ret == "-0.0%" ? "0.0%" : ret;
	}
	
	std::string getPhaseText() {
		return string::f("%.1f°", math::normalizeZero(paPhase->getValue() * 360.0f));
	}
	
	std::string getResponseText() {
		std::string ret = string::f("%.1f%%", paResponse->getValue() * 100.0f);
		return ret == "-0.0%" ? "0.0%" : ret;
	}
	
	std::string getWarpText() {
		std::string ret = string::f("%.1f%%", paWarp->getValue() * 100.0f);
		return ret == "-0.0%" ? "0.0%" : ret;
	}
	
	std::string getAmountText() {
		return string::f("%.1f%%", math::normalizeZero(paAmount->getValue() * 100.0f));
	}
	std::string getSlewText() {
		return string::f("%.1f%%", math::normalizeZero(paSlew->getValue() * 100.0f));
	}
	
	std::string getSmoothText() {
		return string::f("%.1f%%", math::normalizeZero(paSmooth->getValue() * 100.0f));
	}
	std::string getCrossoverText(bool* inactive) {
		*inactive = !inInput->isConnected();
		if (paCrossover->getValue() >= CROSSOVER_OFF) {
			float freq = CROSSOVER_MULT * std::pow(CROSSOVER_BASE, paCrossover->getValue());// without CV
			if (freq < 10000.0f) {
				return string::f("%iHz", (int)(freq + 0.5f));
			}
			freq /= 1000.0f;
			return string::f("%.2fk", freq);
		}
		return "OFF";
	}
	
	std::string getHighText(bool* inactive) {
		*inactive = !inInput->isConnected();
		return string::f("%.1f%%", math::normalizeZero(paHigh->getValue() * 100.0f));
	}
	
	std::string getLowText(bool* inactive) {
		*inactive = !inInput->isConnected();
		return string::f("%.1f%%", math::normalizeZero(paLow->getValue() * 100.0f));
	}
	
	float getTrigLevel() {// takes low range trig level option into account, returns volts
		return playHead.getTrigLevel();
	}
	std::string getTrigLevelText(bool* inactive) {
		*inactive = !(playHead.getTrigMode() == TM_SC);
		return string::f("%.2fV", math::normalizeZero(getTrigLevel()));
	}
	
	bool isHpfCutoffActive() {
		return getHPFCutoffSqFreq() >= SC_HPF_SQFREQ_OFF;
	}
	std::string getHPFCutoffFreqText() {
		if (isHpfCutoffActive()) {
			float valCut = std::pow(getHPFCutoffSqFreq(), SCF_SCALING_EXP);
			if (valCut >= 1e3f) {
				valCut = std::round(valCut / 10.0f);
				return string::f("%g", math::normalizeZero(valCut / 100.0f));
			}
			return string::f("%i", (int)(math::normalizeZero(valCut) + 0.5f));
		}
		return "OFF";
	}	

	bool isLpfCutoffActive() {
		return getLPFCutoffSqFreq() <= SC_LPF_SQFREQ_OFF;
	}
	std::string getLPFCutoffFreqText() {
		if (isLpfCutoffActive()) {
			float valCut = std::pow(getLPFCutoffSqFreq(), SCF_SCALING_EXP);
			if (valCut >= 1e3f) {
				valCut = std::round(valCut / 10.0f);
				return string::f("%g", math::normalizeZero(valCut / 100.0f));
			}
			return string::f("%i", (int)(math::normalizeZero(valCut) + 0.5f));
		}
		return "OFF";
	}	

	std::string getHysteresisText() {
		return playHead.getHysteresisText();
	}	
	std::string getHoldOffText() {
		return playHead.getHoldOffText();
	}
	std::string getSensitivityText(float valSens) {// caller should pass return value of getSensitivity()
		return string::f("%.1f", math::normalizeZero(valSens) * 100.0f);
	}	
	
	std::string getGainAdjustDbText(float gainInDb) {// caller should pass return value of getGainAdjustVcaDb() or getGainAdjustScDb() 
		gainInDb = std::round(gainInDb * 100.0f);
		std::string ret = string::f("%.1f", gainInDb / 100.0f);
		return ret == "-0.0" ? "0.0" : ret;
	}
	std::string getTrigModeText() {// short version
		return trigModeNames[playHead.getTrigMode()];
	}
	
	std::string getPlayModeText() {
		if (playHead.getTrigMode() != TM_CV) {
			return playModeNames[getPlayMode()];
		}
		return playHead.isBipolCvMode() ? std::string("BI") : std::string("UNI");
	}
	
	std::string getRangeText() {
		return getRangeText(rangeIndex);
	}
	std::string getRangeText(int _rangeIndex) {
		int rangeMax = rangeValues[_rangeIndex];
		if (rangeMax > 0) {
			return string::f("0 - %iV", rangeMax);
		}
		return string::f("+/- %iV", -rangeMax);
	}
	
	float getPlayHeadPosition() {
		// returns -1.0f when no play head to display
		if (!*running) {
			return -1.0f;
		}
		if (playHead.getPlayMode() == PM_REV) {
			if (lastProcessXt == 1.0f) {
				return -1.0f;
			}
		}
		else if (lastProcessXt == 0.0f) {
			return -1.0f;
		}
		return (float)lastProcessXt;
	}
	float getScopePosition() {
		// returns -1.0f when no scope to display
		if (!*running) {
			return -1.0f;
		}
		return (float)lastProcessXt;
	}

	void toggleSidechainUseVca() {
		channelSettings.cc4[3] ^= 0x1;
	}
	
	void setShowUnsyncLengthAs(int8_t valShow) {
		channelSettings2.cc4[1] = valShow;
		lengthUnsyncOld = 1e6f;// force recalc of length unsync text
	}
	void setShowTooltipVoltsAs(int8_t valShow) {
		channelSettings2.cc4[2] = valShow;
	}
	void toggleDecoupledFirstAndLast() {
		channelSettings2.cc4[3] ^= 0x1;
		if (!isDecoupledFirstAndLast()) {
			shape.coupleFirstAndLast();
		}
	}
	void toggleNodeTriggers() {
		channelSettings3.cc4[0] ^= 0x1;
		nodeTrigPulseGen.reset();
	}
	
	int getVcaPreSize() {
		return vcaPreSize;
	}
	int getVcaPostSize() {
		return vcaPostSize;
	}
	float getVcaPre(int c) {
		return vcaPre[c];
	}
	float getVcaPost(int c) {
		return vcaPost[c];
	}
	float getScSignal() {
		return scSignal;
	}
	float getScEnvelope() {
		return scEnvelope;
	}
	
	bool* getArrowButtonClicked(int i) {
		return &prevNextButtonsClicked[i];
	}
	
	// --------


	// Comparison for dirty
	
	bool isDirty(Channel* refChan);
	
	bool isDirtyShape(Channel* refChan) {
		return shape.isDirty(&(refChan->shape));
	}


	// Warp and response function:
	// y(x) := a*x*e^(b*x)
	// solve( y(1/2) = c and y(1) = 1, {a, b} )
	//   a = 4*c^2 and b = 2*ln(1/(2*c))
	// substitution and re-arranging yields
	// y(x) := x*(2c)^(2(1-x))
	// where:
	//   c above is within [MIN_CTRL  to  1-MIN_CTRL]
	//   x, y are within [0:1] 
	// optimization potentiel: create poly approx for a suite of c values, then use interpolation (morphing) when called with intermediary c value
	template<typename T>
	T _y(T _x, T c) {
		// assumes but not critical: 0 <= _x <= 1
		// returns: 0 <= _y <= 1
		// assumes given c is within [-1 + MIN_CTRL  to  1 - MIN_CTRL]
		if (_x > (T)1.0) {
			return (T)1.0;
		}
		if (c == (T)0.0) {
			return _x;
		}
		
		bool mirror = c > (T)0.0;
		if (mirror) {
			c *= (T)-1.0;
			_x = (T)1.0 - _x;
		}
		
		c += (T)1.0;
		T _y = _x * std::pow(c, (T)2.0 * ((T)1.0 - _x));
	
		if (mirror) {
			_y = (T)1.0 - _y;
		}
				
		return _y;
	}


	// horizontal modifiers
	// --------------------	
	
	template<typename T>
	T applyWarp(T xt) {
		return _y<T>(xt, -warpPhaseResponseAmountWithCv[0]);//paWarp->getValue());
	}

	template<typename T>
	T applyPhase(T xt) {
		xt += (T)warpPhaseResponseAmountWithCv[1];//paPhase->getValue();
		if (xt > (T)1.0) {
			xt -= std::floor(xt);
		}
		return xt;
	}


	// vertical modifiers
	// --------------------
	
	float applyResponse(float cvVal) {
		return _y<float>(cvVal, warpPhaseResponseAmountWithCv[2]);//paResponse->getValue());
	}


	float applyAmount(float cvVal) {
		float center = shape.getPointY(0);
		return center + (cvVal - center) * warpPhaseResponseAmountWithCv[3];//paAmount->getValue();
	}
	
	
	float applySlewAndSmooth(float cvVal) {
		// slew
		float riseFall;// = slewLimiter.riseFall;
		if (playHead.isSlowSlew()) {
			riseFall = std::fmin(PlayHead::SLOW_SLEW_RISE_FALL, slewLimiter.riseFall);
		}
		else if (xoverSlewWithCv[3] >= 0.001f) {
			// if is relative slew && slew is on
			riseFall = 1.0f / (xoverSlewWithCv[3] * playHead.getCoreLength());
		}
		else {
			riseFall = 10.0f / sampleTime;// effectively turns off the slew limiter, 10.0f is used in case it's applied to a voltage, but in this case here it's applied to a normalized value right below
		}
		cvVal = slewLimiter.process(sampleTime, cvVal , riseFall);
				
		// smooth
		if (lastSmoothParam > 0.001f) {// don't compare with 0.0f because of Rack parameter smoothing, it will take time to get to 0.0f
			return smoothFilter.process(cvVal);
		}
		return cvVal;
	}


	float applyRange(float cvVal) {
		// cvVal is [0:1]
		// returns volts according to channel's range setting
		int rangeValMax = rangeValues[rangeIndex];
		if (rangeValMax > 0) {
			cvVal *= (float)rangeValMax;
		}
		else {
			cvVal *= -2.0 * (float)rangeValMax;
			cvVal += (float)rangeValMax;
		}
		return cvVal;
	}

	float applyInverseRange(float volts) {
		// normalizes volts to [0:1] according to channel's range setting
		// return a normalized value [0:1]
		int rangeValMax = rangeValues[rangeIndex];
		if (rangeValMax > 0) {
			volts /= (float)rangeValMax;
		}
		else {
			volts -= (float)rangeValMax;
			volts /= -2.0 * (float)rangeValMax;
		}
		return clamp(volts, 0.0f, 1.0f);
	}

	// --------------------


	float evalShapeForProcess(double xt) {
		xt = applyWarp<double>(xt);
		xt = applyPhase<double>(xt);
		
		float cvVal = shape.evalShapeForProcess(xt);
		
		cvVal = applyResponse(cvVal);
		cvVal = applyAmount(cvVal);
		cvVal = applySlewAndSmooth(cvVal);
		
		return cvVal;
	}


	float evalShapeForShadow(float xt, int* epc) {
		// this should take into account only the following modifiers:
		// horizontal: phase and warp
		// vertical  : response and amount		
		xt = applyWarp<float>(xt);
		xt = applyPhase<float>(xt);
		
		float cvVal = shape.evalShapeForDisplay(xt, epc);
		
		cvVal = applyResponse(cvVal);
		cvVal = applyAmount(cvVal);
		
		return cvVal;
	}


	void updateRunning() {
		// freeze/resume when run toggled
		if (*running) {
			playHead.start();
		}
		else {
			playHead.hold();
		}
	}
	

	void processSlow(ChanCvs *chanCvs);
	

	void process(bool fsDiv8, ChanCvs *chanCvs);

};// class Channel
