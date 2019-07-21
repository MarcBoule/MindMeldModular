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
	ENUMS(DIRECT_OUTPUTS, 4), // Track 1-8, Track 9-16, Groups and Aux
	ENUMS(MAIN_OUTPUTS, 2),
	NUM_OUTPUTS
};


enum LightIds {
	ENUMS(TRACK_HPF_LIGHTS, 16),
	ENUMS(GROUP_HPF_LIGHTS, 4),
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
	int directOutsMode;// 0 is pre-fader, 1 is post-fader
	
	// no need to save, with reset
	unsigned long soloBitMask;// when = 0ul, nothing to do, when non-zero, a track must check its solo to see it should play

	// no need to save, no reset
	// none
	

	void onReset() {
		panLawMono = 1;
		panLawStereo = 0;
		directOutsMode = 1;// post should be default
		// extern must call resetNonJson()
	}
	
	void resetNonJson(Param *soloParams) {
		updateSoloBitMask(soloParams);
	}
	
	void updateSoloBitMask(Param *soloParams) {
		unsigned long newSoloBitMask = 0ul;// separate variable so no glitch generated
		for (unsigned long i = 0; i < 16; i++) {
			if (soloParams[i].getValue() > 0.5) {
				newSoloBitMask |= (1 << i);
			}
		}
		soloBitMask = newSoloBitMask;	
	}
	void updateSoloBit(unsigned int trk, bool state) {
		if (state) 
			soloBitMask |= (1 << trk);
		else
			soloBitMask &= ~(1 << trk);
	}
	
	void onRandomize(bool editingSequence) {
		
	}
	

	void dataToJson(json_t *rootJ) {
		// panLawMono 
		json_object_set_new(rootJ, "panLawMono", json_integer(panLawMono));

		// panLawStereo
		json_object_set_new(rootJ, "panLawStereo", json_integer(panLawStereo));

		// directOutsMode
		json_object_set_new(rootJ, "directOutsMode", json_integer(directOutsMode));
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
		
		// extern must call resetNonJson()
	}
};// struct GlobalInfo


//*****************************************************************************


struct MixerTrack {
	// Constants
	static constexpr float trackFaderScalingExponent = 3.0f; // for example, 3.0f is x^3 scaling (seems to be what Console uses) (must be integer for best performance)
	static constexpr float trackFaderMaxLinearGain = 2.0f; // for example, 2.0f is +6 dB
	static constexpr float minHPFCutoffFreq = 30.0f;
	
	// need to save, no reset
	// none
	
	// need to save, with reset
	int group;// -1 is no group, 0 to 3 is group
	float gainAdjust;// this is a gain here (not dB)
	float hpfCutoffFreq;

	// no need to save, with reset
	bool stereo;// pan coefficients use this, so set up first
	float panCoeff[2];
	float panLinRcoeff;// used only for True stereo panning
	float panRinLcoeff;// used only for True stereo panning
	dsp::SlewLimiter gainSlewers[2];

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
	char* trackName;// write 4 chars always (space when needed), no null termination since all tracks names are concat and just one null at end of all
	float pre[2];// for pre-track (aka fader+pan) monitor outputs
	float post[2];// for VUs and post-track monitor outputs


	void construct(int _trackNum, GlobalInfo *_gInfo, Input *_inputs, Param *_params, char* _trackName) {
		trackNum = _trackNum;
		gInfo = _gInfo;
		ids = "id" + std::to_string(trackNum) + "_";
		inL = &_inputs[TRACK_SIGNAL_INPUTS + 2 * trackNum + 0];
		inR = &_inputs[TRACK_SIGNAL_INPUTS + 2 * trackNum + 1];
		inVol = &_inputs[TRACK_VOL_INPUTS + trackNum];
		inPan = &_inputs[TRACK_PAN_INPUTS + trackNum];
		paFade = &_params[TRACK_FADER_PARAMS + trackNum];
		paMute = &_params[TRACK_MUTE_PARAMS + trackNum];
		paSolo = &_params[TRACK_SOLO_PARAMS + trackNum];
		paPan = &_params[TRACK_PAN_PARAMS + trackNum];
		trackName = _trackName;
		gainSlewers[0].setRiseFall(100.0f, 100.0f); // slew rate is in input-units per second (ex: V/s)
		gainSlewers[0].setRiseFall(100.0f, 100.0f); // 100.0f was calibrated with SB testing
	}
	
	
	void onReset() {
		group = -1;
		gainAdjust = 1.0f;
		hpfCutoffFreq = 1.0f;// off
		// extern must call resetNonJson()
	}
	void resetNonJson() {
		updatePanCoeff();
		gainSlewers[0].reset();
		gainSlewers[1].reset();
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
				panCoeff[1] = std::sin(pan * M_PI_2) * M_SQRT2;
				panCoeff[0] = std::cos(pan * M_PI_2) * M_SQRT2;
			}
			else if (gInfo->panLawMono == 0) {
				// No compensation (+0dB boost)
				panCoeff[1] = std::min(1.0f, pan * 2.0f);
				panCoeff[0] = std::min(1.0f, 2.0f - pan * 2.0f);
			}
			else if (gInfo->panLawMono == 2) {
				// Compromise (+4.5dB boost)
				panCoeff[1] = std::sqrt( std::abs( std::sin(pan * M_PI_2) * M_SQRT2   *   (pan * 2.0f) ) );
				panCoeff[0] = std::sqrt( std::abs( std::cos(pan * M_PI_2) * M_SQRT2   *   (2.0f - pan * 2.0f) ) );
			}
			else {
				// Linear panning law (+6dB boost)
				panCoeff[1] = pan * 2.0f;
				panCoeff[0] = 2.0f - panCoeff[1];
			}
		}
		else {// stereo
			if (gInfo->panLawStereo == 0) {
				// Stereo balance (+3dB), same as mono equal power
				panCoeff[1] = std::sin(pan * M_PI_2) * M_SQRT2;
				panCoeff[0] = std::cos(pan * M_PI_2) * M_SQRT2;
			}
			else {
				// True panning, equal power
				panCoeff[1] = pan >= 0.5f ? (1.0f) : (std::sin(pan * M_PI));
				panRinLcoeff = pan >= 0.5f ? (0.0f) : (std::cos(pan * M_PI));
				panCoeff[0] = pan <= 0.5f ? (1.0f) : (std::cos((pan - 0.5f) * M_PI));
				panLinRcoeff = pan <= 0.5f ? (0.0f) : (std::sin((pan - 0.5f) * M_PI));
			}
		}
	}
	
	
	void onRandomize(bool editingSequence) {
		
	}
	
	
	
	void dataToJson(json_t *rootJ) {
		// group
		json_object_set_new(rootJ, (ids + "group").c_str(), json_integer(group));

		// gainAdjust
		json_object_set_new(rootJ, (ids + "gainAdjust").c_str(), json_real(gainAdjust));
		
		// hpfCutoffFreq
		json_object_set_new(rootJ, (ids + "hpfCutoffFreq").c_str(), json_real(hpfCutoffFreq));
	}
	
	void dataFromJson(json_t *rootJ) {
		// group
		json_t *groupJ = json_object_get(rootJ, (ids + "group").c_str());
		if (groupJ)
			group = json_integer_value(groupJ);
		
		// gainAdjust
		json_t *gainAdjustJ = json_object_get(rootJ, (ids + "gainAdjust").c_str());
		if (gainAdjustJ)
			gainAdjust = json_number_value(gainAdjustJ);
		
		// hpfCutoffFreq
		json_t *hpfCutoffFreqJ = json_object_get(rootJ, (ids + "hpfCutoffFreq").c_str());
		if (hpfCutoffFreqJ)
			hpfCutoffFreq = json_number_value(hpfCutoffFreqJ);
		
		// extern must call resetNonJson()
	}
	
	
	
	void process(float *mix, float sampleTime) {
		if (!inL->isConnected()) {
			pre[0] = 0.0f;
			pre[1] = 0.0f;
			post[0] = 0.0f;
			post[1] = 0.0f;
			return;
		}
		
		// Here we have an input, so get it
		float sigL = pre[0] = inL->getVoltage();
		float sigR = pre[1] = stereo ? inR->getVoltage() : sigL;
		

		// Calc track gain
		// Gain adjust
		float gain = gainAdjust;
		// solo and mute
		if ( (gInfo->soloBitMask != 0ul && paSolo->getValue() < 0.5f) || (paMute->getValue() > 0.5f) ) {
			gain = 0.0f;
		}
		if (gain != 0.0f) {
			// fader
			float faderGain = std::pow(paFade->getValue(), trackFaderScalingExponent);
			if (gain == 1.0f) {
				gain = faderGain;
			}
			else {
				gain *= faderGain;
			}
			// vol CV input
			if (inVol->isConnected()) {
				gain *= clamp(inVol->getVoltage() / 10.f, 0.f, 1.f);
			}
		}
		float slewedGain = gainSlewers[0].process(sampleTime, gain);
		// TODO split gain into L,R gains and slew separately
		
		// Apply gain
		if (!stereo) {// mono
			sigL *= slewedGain;
			sigR = sigL;
		}
		else {// stereo
			sigL *= slewedGain;
			sigR *= slewedGain;
		}

		// Apply pan
		post[0] = sigL * panCoeff[0];
		post[1] = sigR * panCoeff[1];
		if (stereo && gInfo->panLawStereo != 0) {
			post[0] += sigR * panRinLcoeff;
			post[1] += sigL * panLinRcoeff;
		}

		// Add to mix
		mix[0] += post[0];
		mix[1] += post[1];
	}
};// struct MixerTrack


//*****************************************************************************



#endif