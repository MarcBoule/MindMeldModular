//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.txt for all licenses
//***********************************************************************************************

#ifndef MMM_MIXER_HPP
#define MMM_MIXER_HPP


#include "MindMeldModular.hpp"


enum ParamIds {
	ENUMS(TRACK_FADER_PARAMS, 16),
	ENUMS(GROUP_FADER_PARAMS, 4),
	ENUMS(TRACK_PAN_PARAMS, 16),
	ENUMS(GROUP_PAN_PARAMS, 4),
	ENUMS(TRACK_MUTE_PARAMS, 16),
	ENUMS(GROUP_MUTE_PARAMS, 4),
	ENUMS(TRACK_SOLO_PARAMS, 16),
	ENUMS(GROUP_SOLO_PARAMS, 4),
	MAIN_FADER_PARAM,
	ENUMS(GRP_INC_PARAMS, 16),
	ENUMS(GRP_DEC_PARAMS, 16),
	ENUMS(TRACK_LOWCUT_PARAMS, 16),// NUM_PARAMS_JR's last param is on previous line
	NUM_PARAMS
}; 
static const int NUM_PARAMS_JR = TRACK_LOWCUT_PARAMS;


enum InputIds {
	ENUMS(TRACK_SIGNAL_INPUTS, 16 * 2), // Track 0: 0 = L, 1 = R, Track 1: 2 = L, 3 = R, etc...
	ENUMS(TRACK_VOL_INPUTS, 16),
	ENUMS(GROUP_VOL_INPUTS, 4),
	ENUMS(TRACK_PAN_INPUTS, 16), 
	ENUMS(GROUP_PAN_INPUTS, 4), 
	NUM_INPUTS
};


enum OutputIds {
	ENUMS(MONITOR_OUTPUTS, 4), // Track 1-8, Track 9-16, Groups and Aux
	ENUMS(MAIN_OUTPUTS, 2),
	NUM_OUTPUTS
};


enum LightIds {
	NUM_LIGHTS
};


//*****************************************************************************


// managed by Mixer, not by tracks (tracks read only)
struct GlobalInfo {
	// need to save, no reset
	// none
	
	// need to save, with reset
	int panLawMono;// +0dB (no compensation),  +3 (equal power, default),  +4.5 (compromize),  +6dB (linear)
	int panLawStereo;// Stereo balance (+3dB boost since one channel lost, default),  True pan (linear redistribution but is not equal power)
	
	// no need to save, with reset
	bool soloAllOff;// when true, nothing to do, when false, a track must check its solo to see it should play

	// no need to save, no reset
	// none
	

	void onReset() {
		panLawMono = 1;
		panLawStereo = 0;
		// extern must call resetNonJson()
	}
	
	void resetNonJson(Param *soloParams) {
		updateSoloAllOff(soloParams);
	}
	
	void updateSoloAllOff(Param *soloParams) {
		bool newSoloAllOff = true;// separate variable so no glitch generated
		for (int i = 0; i < 16; i++) {
			if (soloParams[i].getValue() > 0.5) {
				newSoloAllOff = false;
				break;
			}
		}
		soloAllOff = newSoloAllOff;	
	}
	
	void onRandomize(bool editingSequence) {
		
	}
	

	void dataToJson(json_t *rootJ) {
		// panLawMono 
		json_object_set_new(rootJ, "panLawMono", json_integer(panLawMono));

		// panLawStereo
		json_object_set_new(rootJ, "panLawStereo", json_integer(panLawStereo));
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
		
		// extern must call resetNonJson()
	}
};// struct GlobalInfo


//*****************************************************************************


struct MixerTrack {
	// Constants
	static constexpr float trackFaderScalingExponent = 2.0f; // for example, 3.0f is x^3 scaling (seems to be what Console uses) (must be integer for best performance)
	static constexpr float trackFaderMaxLinearGain = 2.0f; // for example, 2.0f is +6 dB
	
	// need to save, no reset
	// none
	
	// need to save, with reset
	int group;// -1 is no group, 0 to 3 is group

	// no need to save, with reset
	bool stereo;// pan coefficients use this, so set up first
	float panLcoeff;
	float panRcoeff;
	float panLinRcoeff;// used only for True stereo panning
	float panRinLcoeff;// used only for True stereo panning

	// no need to save, no reset
	int trackNum;
	std::string ids;
	GlobalInfo *gInfo;
	Input *inL;
	Input *inR;
	Input *inVol;
	Input *inPan;
	Param *paFade;
	Param *paMute;
	Param *paSolo;
	Param *paPan;


	void construct(int _trackNum, GlobalInfo *_gInfo, Input *_inputs, Input *_inR, Input *_inVol, Input *_inPan, Param *_paFade, Param *_paMute, Param *_paSolo, Param *_paPan) {
		trackNum = _trackNum;
		gInfo = _gInfo;
		ids = "id" + std::to_string(trackNum) + "_";
		inL = &_inputs[TRACK_SIGNAL_INPUTS + 2 * trackNum + 0];
		inR = _inR;
		inVol = _inVol;
		inPan = _inPan;
		paFade = _paFade;
		paMute = _paMute;
		paSolo = _paSolo;
		paPan = _paPan;
	}
	
	
	void onReset() {
		group = -1;
		// extern must call resetNonJson()
	}
	void resetNonJson() {
		updatePanCoeff();
	}
	
	void updatePanCoeff() {
		stereo = inR->isConnected();
		
		float pan = paPan->getValue();
		if (inPan->isConnected()) {
			pan += inPan->getVoltage() * 0.1f;// this is a -5V to +5V input
		}
		
		panLinRcoeff = 0.0f;
		panRinLcoeff = 0.0f;
		if (!stereo) {// mono
			if (gInfo->panLawMono == 1) {
				// Equal power panning law (+3dB boost)
				panRcoeff = std::sin(pan * M_PI_2) * M_SQRT2;
				panLcoeff = std::cos(pan * M_PI_2) * M_SQRT2;
			}
			else if (gInfo->panLawMono == 0) {
				// No compensation (+0dB boost)
				panRcoeff = std::min(1.0f, pan * 2.0f);
				panLcoeff = std::min(1.0f, 2.0f - pan * 2.0f);
			}
			else if (gInfo->panLawMono == 2) {
				// Compromise (+4.5dB boost)
				panRcoeff = std::sqrt( std::abs( std::sin(pan * M_PI_2) * M_SQRT2   *   (pan * 2.0f) ) );
				panLcoeff = std::sqrt( std::abs( std::cos(pan * M_PI_2) * M_SQRT2   *   (2.0f - pan * 2.0f) ) );
			}
			else {
				// Linear panning law (+6dB boost)
				panRcoeff = pan * 2.0f;
				panLcoeff = 2.0f - panRcoeff;
			}
		}
		else {// stereo
			if (gInfo->panLawStereo == 0) {
				// Stereo balance (+3dB), same as mono equal power
				panRcoeff = std::sin(pan * M_PI_2) * M_SQRT2;
				panLcoeff = std::cos(pan * M_PI_2) * M_SQRT2;
			}
			else {
				// True panning, equal power
				panRcoeff = pan >= 0.5f ? (1.0f) : (std::sin(pan * M_PI));
				panRinLcoeff = pan >= 0.5f ? (0.0f) : (std::cos(pan * M_PI));
				panLcoeff = pan <= 0.5f ? (1.0f) : (std::cos((pan - 0.5f) * M_PI));
				panLinRcoeff = pan <= 0.5f ? (0.0f) : (std::sin((pan - 0.5f) * M_PI));
			}
		}
	}
	
	
	void onRandomize(bool editingSequence) {
		
	}
	
	
	
	void dataToJson(json_t *rootJ) {
		// group
		json_object_set_new(rootJ, (ids + "group").c_str(), json_integer(group));
	}
	
	void dataFromJson(json_t *rootJ) {
		// group
		json_t *groupJ = json_object_get(rootJ, (ids + "group").c_str());
		if (groupJ)
			group = json_integer_value(groupJ);
		
		// extern must call resetNonJson()
	}
	
	
	
	void process(float *mixL, float *mixR) {
		if (!inL->isConnected() || paMute->getValue() > 0.5f) {
			return;
		}
		if (!gInfo->soloAllOff && paSolo->getValue() < 0.5f) {
			return;
		}

		// Calc track fader gain
		float gain = std::pow(paFade->getValue(), trackFaderScalingExponent);
		
		// Get inputs and apply pan
		float sigL = inL->getVoltage();
		float sigR;
		if (!stereo) {// mono
			// Apply fader gain
			sigL *= gain;

			// Apply CV gain
			if (inVol->isConnected()) {
				float cv = clamp(inVol->getVoltage() / 10.f, 0.f, 1.f);
				sigL *= cv;
			}

			sigR = sigL;
		}
		else {// stereo
			sigR = inR->getVoltage();
		
			// Apply fader gain
			sigL *= gain;
			sigR *= gain;

			// Apply CV gain
			if (inVol->isConnected()) {
				float cv = clamp(inVol->getVoltage() / 10.f, 0.f, 1.f);
				sigL *= cv;
				sigR *= cv;
			}
		}

		// Apply pan
		float pannedSigL = sigL * panLcoeff;
		float pannedSigR = sigR * panRcoeff;
		if (stereo && gInfo->panLawStereo != 0) {
			pannedSigL += sigR * panRinLcoeff;
			pannedSigR += sigL * panLinRcoeff;
		}

		// Add to mix
		*mixL += pannedSigL;
		*mixR += pannedSigR;
	}
};// struct MixerTrack


//*****************************************************************************



#endif