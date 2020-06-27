//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "MixerCommon.hpp"


//*****************************************************************************


// Math

// none


// Utility

float updateFadeGain(float fadeGain, float target, float *fadeGainX, float timeStepX, float shape, bool symmetricalFade) {
	// shape is 1.0f when exp, 0.0f when lin, -1.0f when log
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

float updateFadeGain2(float fadeGain, float target, float *fadeGainX, float *fadeGainXr, float timeStepX, float shape, bool symmetricalFade) {
	// shape is 1.0f when exp, 0.0f when lin, -1.0f when log
	static const float A = 4.0f;
	static const float E_A_M1 = (std::exp(A) - 1.0f);// e^A - 1
	
	float newFadeGain;

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
	*fadeGainXr += timeStepX;
	
	if (symmetricalFade) {
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
			float fadeGainDeltaExp = (std::exp(A * (*fadeGainXr)) - std::exp(A * (*fadeGainXr - timeStepX))) / E_A_M1;
			fadeGainDelta = crossfade(fadeGainDelta, fadeGainDeltaExp, shape);
		}
		else if (shape < 0.0f) {
			float fadeGainDeltaLog = (std::log((*fadeGainXr) * E_A_M1 + 1.0f) - std::log((*fadeGainXr + timeStepX) * E_A_M1 + 1.0f)) / A;
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
	filterPos = 1;// default is post-insert
	dispColorLocal = 0;
	panCvLevel = 1.0f;
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




