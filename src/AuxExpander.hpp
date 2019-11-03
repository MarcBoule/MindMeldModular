//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


struct AuxspanderAux {
	// Constants
	static constexpr float hpfBiquadQ = 1.0f;// 1.0 Q since preceeeded by a one pole filter to get 18dB/oct
	
	// need to save, no reset
	// none
	
	// need to save, with reset
	private:
	float hpfCutoffFreq;// always use getter and setter since tied to Biquad
	float lpfCutoffFreq;// always use getter and setter since tied to Biquad
	public:

	// no need to save, with reset
	private:
	OnePoleFilter hpPreFilter[2];// 6dB/oct
	dsp::BiquadFilter hpFilter[2];// 12dB/oct
	dsp::BiquadFilter lpFilter[2];// 12db/oct
	public:

	// no need to save, no reset
	int auxNum;
	std::string ids;
	Input *inSig;


	void construct(int _auxNum, Input *_inputs) {
		auxNum = _auxNum;
		ids = "id_x" + std::to_string(auxNum) + "_";
		inSig = &_inputs[0 + 2 * auxNum + 0];
		for (int i = 0; i < 2; i++) {
			hpFilter[i].setParameters(dsp::BiquadFilter::HIGHPASS, 0.1, hpfBiquadQ, 0.0);
			lpFilter[i].setParameters(dsp::BiquadFilter::LOWPASS, 0.4, 0.707, 0.0);
		}
	}


	void onReset() {
		setHPFCutoffFreq(13.0f);// off
		setLPFCutoffFreq(20010.0f);// off
		resetNonJson();
	}

	void resetNonJson() {}
	
	void dataToJson(json_t *rootJ) {
		// hpfCutoffFreq
		json_object_set_new(rootJ, (ids + "hpfCutoffFreq").c_str(), json_real(getHPFCutoffFreq()));
		
		// lpfCutoffFreq
		json_object_set_new(rootJ, (ids + "lpfCutoffFreq").c_str(), json_real(getLPFCutoffFreq()));
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

		// extern must call resetNonJson()
	}	

	void setHPFCutoffFreq(float fc) {// always use this instead of directly accessing hpfCutoffFreq
		hpfCutoffFreq = fc;
		fc *= APP->engine->getSampleTime();// fc is in normalized freq for rest of method
		for (int i = 0; i < 2; i++) {
			hpPreFilter[i].setCutoff(fc);
			hpFilter[i].setParameters(dsp::BiquadFilter::HIGHPASS, fc, hpfBiquadQ, 0.0);
		}
	}
	float getHPFCutoffFreq() {return hpfCutoffFreq;}
	
	void setLPFCutoffFreq(float fc) {// always use this instead of directly accessing lpfCutoffFreq
		lpfCutoffFreq = fc;
		fc *= APP->engine->getSampleTime();// fc is in normalized freq for rest of method
		lpFilter[0].setParameters(dsp::BiquadFilter::LOWPASS, fc, 0.707, 0.0);
		lpFilter[1].setParameters(dsp::BiquadFilter::LOWPASS, fc, 0.707, 0.0);
	}
	float getLPFCutoffFreq() {return lpfCutoffFreq;}


	void onSampleRateChange() {
		setHPFCutoffFreq(hpfCutoffFreq);
		setLPFCutoffFreq(lpfCutoffFreq);
	}


	void process(float *mix) {// auxspander aux	
		// optimize unused aux
		if (!inSig[0].isConnected()) {
			mix[0] = mix[1] = 0.0f;
			return;
		}
		mix[0] = inSig[0].getVoltage();
		mix[1] = inSig[1].isConnected() ? inSig[1].getVoltage() : mix[0];
		// Filters
		// HPF
		if (getHPFCutoffFreq() >= GlobalConst::minHPFCutoffFreq) {
			mix[0] = hpFilter[0].process(hpPreFilter[0].processHP(mix[0]));
			mix[1] = inSig[1].isConnected() ? hpFilter[1].process(hpPreFilter[1].processHP(mix[1])) : mix[0];
		}
		// LPF
		if (getLPFCutoffFreq() <= GlobalConst::maxLPFCutoffFreq) {
			mix[0] = lpFilter[0].process(mix[0]);
			mix[1] = inSig[1].isConnected() ? lpFilter[1].process(mix[1]) : mix[0];
		}
	}
};// struct AuxspanderAux
