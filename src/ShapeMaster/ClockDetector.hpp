//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//Based on code from the Fundamental plugin by Andrew Belt
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#pragma once

#include "rack.hpp"
#include "Util.hpp"


class ClockDetector {
	// constants
	static const int CLOCK_MEM_MAX = 8;
	// static const int32_t CLOCK_COUNT_MAX = 2949120;
	
	// need to save, with reset
	int ppqn;
	int ppqnAvg;
	double clockPeriodSynced;// always holds the last good clock period
	
	// no need to save, with reset
	float sampleRate;
	double sampleTime;
	int32_t clockCount;// timestamp for each clock edge (0 is not an actual registered clock edge but does represent implicit edge when run is turned on)
	int32_t clockSampleTotal;
	int32_t clockSampleMem[CLOCK_MEM_MAX];
	int clockSampleMemHead;
	bool clockEdgeDetected;
	
	
	public:
	
	
	ClockDetector() {
		onReset();
	}
	
	void onReset() {
		ppqn = 48;
		ppqnAvg = 4;
		clockPeriodSynced = 0.5; // 120 BPM default
		resetNonJson();
	}
	
	
	void resetNonJson() {
		sampleRate = APP->engine->getSampleRate();
		sampleTime = 1.0 / sampleRate;
		resetClockDetector();
	}
	void resetClockDetector() {
		clockCount = 0;
		clockSampleTotal = 0;
		for (int i = 0; i < ppqnAvg - 1; i++) {
			clockSampleMem[i] = ((long)(clockPeriodSynced * sampleRate)) / ppqn;
			clockSampleTotal += clockSampleMem[i];
		}
		clockSampleMemHead = ppqnAvg - 1;// will start counting anew in last bin
		clockSampleMem[clockSampleMemHead] = 0;
		clockEdgeDetected = false;
	}
	
	void dataToJson(json_t* rootJ) {
		// ppqn
		json_object_set_new(rootJ, "ppqn", json_integer(ppqn));

		// ppqnAvg
		json_object_set_new(rootJ, "ppqnAvg", json_integer(ppqnAvg));

		// clockPeriodSynced
		json_object_set_new(rootJ, "clockPeriodSynced", json_real(clockPeriodSynced));
	}
	
	void dataFromJson(json_t* rootJ) {
		// ppqn
		json_t *ppqnJ = json_object_get(rootJ, "ppqn");
		if (ppqnJ) {
			ppqn = std::max<int>(std::min<int>(json_integer_value(ppqnJ), ppqnValues[NUM_PPQN_CHOICES-1]), ppqnValues[0]);
		}

		// ppqnAvg
		json_t *ppqnAvgJ = json_object_get(rootJ, "ppqnAvg");
		if (ppqnAvgJ) ppqnAvg = json_integer_value(ppqnAvgJ);

		// clockPeriodSynced
		json_t *clockPeriodSyncedJ = json_object_get(rootJ, "clockPeriodSynced");
		if (clockPeriodSyncedJ) clockPeriodSynced = json_number_value(clockPeriodSyncedJ);
		
		resetNonJson();
	}
	
	
	void onSampleRateChange() {
		sampleRate = APP->engine->getSampleRate();
		sampleTime = 1.0 / sampleRate;
		resetClockDetector();
	}


	// Setters
	// --------

	void setPpqn(int _ppqn) {
		ppqn = _ppqn;
	}
	void setPpqnAvg(int _ppqnAvg) {
		ppqnAvg = _ppqnAvg;
	}
	
	// Getters
	// --------

	double getSampleTime() {
		return sampleTime;
	}

	int getPpqn() {
		return ppqn;
	}
	int getPpqnAvg() {
		return ppqnAvg;
	}
	
	int32_t getClockCount() {
		return clockCount;
	}

	double getClockPeriodSynced() {
		return clockPeriodSynced;
	}
	
	bool getClockEdgeDetected() {
		return clockEdgeDetected;
	}

	// --------

	
	double timeToNextPulse() {
		int32_t samplesInOneInterval = (int32_t)(clockPeriodSynced * sampleRate / ppqnAvg);
		int32_t samplesToNextPulse = std::max<int32_t>(samplesInOneInterval - clockSampleMem[clockSampleMemHead], 0);
		return sampleTime * (double)samplesToNextPulse;
	}
	
	
	int32_t samplesSinceLastPulse() {
		return clockSampleMem[clockSampleMemHead];
	}
	
	
	double timeSinceLastPulse() {
		return sampleTime * (double)samplesSinceLastPulse();
	}
	
	
	double getPulseInterval() {
		return clockPeriodSynced / (double)ppqn;
	}
	
	
	void process(bool edgeDetected) {
		// edgeDetected can only be true when running (includes initial when run activated)
		if (edgeDetected) {
			clockCount++;
			clockSampleTotal += clockSampleMem[clockSampleMemHead];
			clockPeriodSynced = ( (double)(clockSampleTotal * ppqn * sampleTime) ) / ( (double)ppqnAvg );
			// DEBUG("%i: bpm = %g %i", clockCount, 60.0f / clockPeriodSynced, clockSampleTotal);
			clockSampleMemHead = (clockSampleMemHead + 1) % ppqnAvg;
			clockSampleTotal -= clockSampleMem[clockSampleMemHead];
			clockSampleMem[clockSampleMemHead] = 0;
		}
		clockSampleMem[clockSampleMemHead]++;
		if (clockSampleMem[clockSampleMemHead] > (int32_t)(sampleRate) * 2l) {// timeout 30 BPM (2s) on clock pulses, so at 4 ppqn this is 7.5 BPM
			resetClockDetector();
		}
		clockEdgeDetected = edgeDetected;// do this last to make sure clockPeriodSynced is updated when clockEdgeDetected is set true
	}
};
