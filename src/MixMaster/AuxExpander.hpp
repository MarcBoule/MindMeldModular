//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


struct AuxspanderAux {
	// Constants
	// none
	
	// need to save, no reset
	// none
	
	// need to save, with reset
	private:
	float hpfCutoffFreq = 0.0f;// always use getter and setter since tied to Biquad
	float lpfCutoffFreq = 0.0f;// always use getter and setter since tied to Biquad
	public:
	float stereoWidth = 0.0f;// 0 to 1.0f; 0 is mono, 1 is stereo

	// no need to save, with reset
	bool stereo = false;
	private:
	ButterworthThirdOrder hpFilter[2];// 18dB/oct
	ButterworthSecondOrder lpFilter[2];// 12db/oct
	float sampleTime = 0.0f;
	SlewLimiterSingle stereoWidthSlewer;
	public:

	// no need to save, no reset
	int auxNum = 0;
	char *auxLabel = nullptr;
	std::string ids;
	Input *inSig = nullptr;
	int8_t *vuColorThemeLocal = nullptr;
	int8_t *directOutsModeLocal = nullptr;
	int8_t *panLawStereoLocal = nullptr;
	int8_t *dispColorAuxLocal = nullptr;
	float *panCvLevel = nullptr;
	float *auxFadeRatesAndProfile = nullptr;
	Param *trackAuxSendParam = nullptr;// all spaced out by 4, N_TRK of them
	Param *groupAuxSendParam = nullptr;// all spaced out by 4, N_GRP of them
	Param *globalAuxParam = nullptr;// all spaced out by 4: mute, solo, group, send, pan, return
	

	void construct(int _auxNum, Input *_inputs, Param *_params, char* _auxLabel, int8_t *_vuColorThemeLocal, int8_t *_directOutsModeLocal, int8_t *_panLawStereoLocal, int8_t *_dispColorAuxLocal, float *_panCvLevel, float *_auxFadeRatesAndProfile) {
		auxNum = _auxNum;
		auxLabel = _auxLabel;
		ids = "id_x" + std::to_string(auxNum) + "_";
		inSig = &_inputs[0 + 2 * auxNum + 0];
		for (int i = 0; i < 2; i++) {
			hpFilter[i].setParameters(true, 0.1f);
			lpFilter[i].setParameters(false, 0.4f);
		}
		stereoWidthSlewer.setRiseFall(GlobalConst::antipopSlewFast); // slew rate is in input-units per second (ex: V/s)
		vuColorThemeLocal = _vuColorThemeLocal;
		directOutsModeLocal = _directOutsModeLocal;
		panLawStereoLocal = _panLawStereoLocal;
		dispColorAuxLocal = _dispColorAuxLocal;
		panCvLevel = _panCvLevel;
		auxFadeRatesAndProfile = _auxFadeRatesAndProfile;
		trackAuxSendParam = &_params[TRACK_AUXSEND_PARAMS + auxNum];// all spaced out by 4, N_TRK of them
		groupAuxSendParam = &_params[GROUP_AUXSEND_PARAMS + auxNum];// all spaced out by 4, N_GRP of them
		globalAuxParam = &_params[GLOBAL_AUXMUTE_PARAMS + auxNum];// all spaced out by 4: mute, solo, group, send, pan, return
	}


	void onReset() {
		// not managed here:
		snprintf(auxLabel, 4, "AUX"); auxLabel[3] = 0x41 + auxNum;
		*vuColorThemeLocal = 0;
		*directOutsModeLocal = 3;// post-solo should be default
		*panLawStereoLocal = 1;
		*dispColorAuxLocal = 0;	
		*panCvLevel = 1.0f;
		*auxFadeRatesAndProfile = 0.0f;
		auxFadeRatesAndProfile[4] = 0.0f;
		
		// managed here:
		setHPFCutoffFreq(13.0f);// off
		setLPFCutoffFreq(20010.0f);// off
		stereoWidth = 1.0f;
		resetNonJson();
	}

	void resetNonJson() {
		stereo = false;
		for (int i = 0; i < 2; i++) {
			hpFilter[i].reset();
			lpFilter[i].reset();
		}
		sampleTime = APP->engine->getSampleTime();
		stereoWidthSlewer.reset();
	}
	
	void dataToJson(json_t *rootJ) {
		// hpfCutoffFreq
		json_object_set_new(rootJ, (ids + "hpfCutoffFreq").c_str(), json_real(getHPFCutoffFreq()));
		
		// lpfCutoffFreq
		json_object_set_new(rootJ, (ids + "lpfCutoffFreq").c_str(), json_real(getLPFCutoffFreq()));

		// stereoWidth
		json_object_set_new(rootJ, (ids + "stereoWidth").c_str(), json_real(stereoWidth));
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

		// stereoWidth
		json_t *stereoWidthJ = json_object_get(rootJ, (ids + "stereoWidth").c_str());
		if (stereoWidthJ)
			stereoWidth = json_number_value(stereoWidthJ);

		// extern must call resetNonJson()
	}	

	void setHPFCutoffFreq(float fc) {// always use this instead of directly accessing hpfCutoffFreq
		hpfCutoffFreq = fc;
		fc *= APP->engine->getSampleTime();// fc is in normalized freq for rest of method
		hpFilter[0].setParameters(true, fc);
		hpFilter[1].setParameters(true, fc);
	}
	float getHPFCutoffFreq() {return hpfCutoffFreq;}
	
	void setLPFCutoffFreq(float fc) {// always use this instead of directly accessing lpfCutoffFreq
		lpfCutoffFreq = fc;
		fc *= APP->engine->getSampleTime();// fc is in normalized freq for rest of method
		lpFilter[0].setParameters(false, fc);
		lpFilter[1].setParameters(false, fc);
	}
	float getLPFCutoffFreq() {return lpfCutoffFreq;}


	void onSampleRateChange() {
		setHPFCutoffFreq(hpfCutoffFreq);
		setLPFCutoffFreq(lpfCutoffFreq);
		sampleTime = APP->engine->getSampleTime();
	}


	void process(float *mix) {// auxspander aux	
		// optimize unused aux
		if (!inSig[0].isConnected()) {
			mix[0] = mix[1] = 0.0f;
			stereoWidthSlewer.reset();
			return;
		}
		
		// get inputs
		stereo = inSig[1].isConnected();
		mix[0] = clampNothing(inSig[0].getVoltage());
		mix[1] = stereo ? clampNothing(inSig[1].getVoltage()) : mix[0];
		
		// Stereo width
		if (stereoWidth != stereoWidthSlewer.out) {
			stereoWidthSlewer.process(sampleTime, stereoWidth);
		}
		if (stereo && stereoWidthSlewer.out != 1.0f) {
			applyStereoWidth(stereoWidthSlewer.out, &mix[0], &mix[1]);
		}		
		
		
		// Filters
		// HPF
		if (getHPFCutoffFreq() >= GlobalConst::minHPFCutoffFreq) {
			mix[0] = hpFilter[0].process(mix[0]);
			mix[1] = inSig[1].isConnected() ? hpFilter[1].process(mix[1]) : mix[0];
		}
		// LPF
		if (getLPFCutoffFreq() <= GlobalConst::maxLPFCutoffFreq) {
			mix[0] = lpFilter[0].process(mix[0]);
			mix[1] = inSig[1].isConnected() ? lpFilter[1].process(mix[1]) : mix[0];
		}
	}
};// struct AuxspanderAux
