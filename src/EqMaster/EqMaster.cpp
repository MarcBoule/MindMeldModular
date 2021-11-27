//***********************************************************************************************
//EQ module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "EqWidgets.hpp"
#include <thread>


struct EqMaster : Module {
	
	// see EqMasterCommon.hpp for param ids
	
	enum InputIds {
		ENUMS(SIG_INPUTS, 3),
		NUM_INPUTS
	};
	
	enum OutputIds {
		ENUMS(SIG_OUTPUTS, 3),
		NUM_OUTPUTS
	};
	
	enum LightIds {
		NUM_LIGHTS
	};

	
	// Expander
	MfeExpInterface expMessages[2];// messages from eq-expander, see enum in EqCommon.hpp

	// Constants
	int numChannels16 = 16;// avoids warning that happens when hardcode 16 (static const or directly use 16 in code below)
	int8_t cloakedMode = 0x0;

	// Need to save, no reset
	// none
	
	// Need to save, with reset
	int64_t mappedId;// 0 means manual
	char trackLabels[24 * 4 + 1];// needs to be saved in case we are detached
	int8_t trackLabelColors[24];
	int8_t trackVuColors[24];
	TrackEq trackEqs[24];
	PackedBytes4 miscSettings;// cc4[0] is ShowBandCurvesEQ, cc4[1] is fft type (0 = off, 1 = pre, 2 = post, 3 = freeze), cc4[2] is momentaryCvButtons (1 = yes (original rising edge only version), 0 = level sensitive (emulated with rising and falling detection)), cc4[3] is detailsShow
	PackedBytes4 miscSettings2;// cc4[0] is band label colours, cc4[1] is decay rate (0 = slow, 1 = med, 2 = fast), cc[2] is hide eq curves when bypassed, cc[3] is unused
	PackedBytes4 showFreqAsNotes;
	
	
	// No need to save, with reset
	int updateTrackLabelRequest;// 0 when nothing to do, 1 for read names in widget, 2 for same as 1 but force param refreshing
	VuMeterAllDual trackVu;
	int fftWriteHead;
	int page;
	uint32_t cvConnected;
	int drawBufSize;

	// No need to save, no reset
	RefreshCounter refresh;
	PFFFT_Setup* ffts;// https://bitbucket.org/jpommier/pffft/src/default/test_pffft.c
	float* fftIn[3];
	float* fftOut;
	TriggerRiseFall trackEnableCvTriggers[24+1];
	TriggerRiseFall trackBandCvTriggers[24][4];
	bool expPresentLeft = false;
	bool expPresentRight = false;
	std::mutex m;
	float *drawBuf;//[FFT_N] store log magnitude only in first half, log freq in second half (normally this is compacted freq bins, so not all array used)
	float *drawBufLin;//[FFT_N_2] store lin magnitude, used for calculating decay (normally this is compacted freq bins, so not all array used)
	float *windowFunc;//[FFT_N_2] precomputed window function for FFT; function is symetrical, so only first half of window is actually stored here
	bool requestStop = false;
	bool requestWork = false;
	int requestPage = 0;
	std::condition_variable cv;// https://thispointer.com//c11-multithreading-part-7-condition-variables-explained/
	std::thread worker;// http://www.cplusplus.com/reference/thread/thread/thread/
	
	int getSelectedTrack() {
		return (int)(params[TRACK_PARAM].getValue() + 0.5f);
	}

	void initTrackLabelsAndColors() {
		for (unsigned int trk = 0; trk < 16; trk++) {
			snprintf(&trackLabels[trk << 2], 5, "-%02u-", trk + 1);
		}
		for (unsigned int trk = 16; trk < 20; trk++) {
			snprintf(&trackLabels[trk << 2], 5, "GRP%1u", trk - 16 + 1);
		}
		for (unsigned int trk = 20; trk < 24; trk++) {
			snprintf(&trackLabels[trk << 2], 5, "AUX%1u", trk - 20 + 1);
		}
		for (unsigned int trk = 0; trk < 24; trk++) {
			trackLabelColors[trk] = 0;
			trackVuColors[trk] = 0;
		}
	}
	
	float *allocateAndCalcWindowFunc() {
		float *buf = (float*)pffft_aligned_malloc(FFT_N_2 * 4);
		for (unsigned int i = 0; i < (FFT_N_2 / 4); i++) {
			simd::float_4 p = {(float)(i * 4 + 0), (float)(i * 4 + 1), (float)(i * 4 + 2), (float)(i * 4 + 3)};
			p /= (float)(FFT_N - 1);
			p = dsp::blackmanHarris<simd::float_4>(p);
			p.store(&(buf[i * 4]));		
		}	
		return buf;
	}
	
		
	EqMaster() : worker(&EqMaster::worker_thread, this) {
		config(NUM_EQ_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		rightExpander.producerMessage = &expMessages[0];
		rightExpander.consumerMessage = &expMessages[1];
		leftExpander.producerMessage = &expMessages[0];
		leftExpander.consumerMessage = &expMessages[1];

		// Track knob
		configParam(TRACK_PARAM, 0.0f, 23.0f, 0.0f, "Track", "", 0.0f, 1.0f, 1.0f);//min, max, default, label = "", unit = "", displayBase = 0.f, displayMultiplier = 1.f, displayOffset = 0.f
		paramQuantities[TRACK_PARAM]->snapEnabled = true;
		
		// Track settings
		configParam(TRACK_ACTIVE_PARAM, 0.0f, 1.0f, DEFAULT_trackActive ? 1.0f : 0.0f, "Track EQ active");
		configParam(TRACK_GAIN_PARAM, -20.0f, 20.0f, DEFAULT_gain, "Track gain", " dB");
		for (int i = 0; i < 4; i++) {
			configParam(FREQ_ACTIVE_PARAMS + i, 0.0f, 1.0f, DEFAULT_bandActive, bandNames[i] + " active");
			configParam(FREQ_PARAMS + i, MIN_logFreq[i], MAX_logFreq[i], DEFAULT_logFreq[i], bandNames[i] + " freq", " Hz", 10.0f);
			configParam(GAIN_PARAMS + i, -20.0f, 20.0f, DEFAULT_gain, bandNames[i] + " gain", " dB");
			configParam(Q_PARAMS + i, 0.3f, 20.0f, DEFAULT_q[i], bandNames[i] + " Q");
		}
		configParam(LOW_PEAK_PARAM, 0.0f, 1.0f, DEFAULT_lowPeak ? 1.0f : 0.0f, "LF peak/shelf");
		configParam(HIGH_PEAK_PARAM, 0.0f, 1.0f, DEFAULT_highPeak ? 1.0f : 0.0f, "HF peak/shelf");
		configParam(GLOBAL_BYPASS_PARAM, 0.0f, 1.0f, 0.0f, "Global bypass");
		
		getParamQuantity(GLOBAL_BYPASS_PARAM)->randomizeEnabled = false;
		getParamQuantity(TRACK_PARAM)->randomizeEnabled = false;
		getParamQuantity(TRACK_ACTIVE_PARAM)->randomizeEnabled = false;
		
		configInput(SIG_INPUTS + 0, "Tracks 1-8");
		configInput(SIG_INPUTS + 1, "Tracks 9-16");
		configInput(SIG_INPUTS + 2, "Groups and Auxs");
		configInput(SIG_OUTPUTS + 0, "Tracks 1-8");
		configInput(SIG_OUTPUTS + 1, "Tracks 9-16");
		configInput(SIG_OUTPUTS + 2, "Groups and Auxs");
		
		for (int i = 0; i < 3; i++) {
			configBypass(SIG_INPUTS + i, SIG_OUTPUTS + i);
		}
		
		onReset();
		
		ffts = pffft_new_setup(FFT_N, PFFFT_REAL);
		fftIn[0] = (float*)pffft_aligned_malloc(FFT_N * 4);
		fftIn[1] = (float*)pffft_aligned_malloc(FFT_N * 4);
		fftIn[2] = (float*)pffft_aligned_malloc(FFT_N * 4);
		fftOut = (float*)pffft_aligned_malloc(FFT_N * 4);
		drawBuf = (float*)pffft_aligned_malloc(FFT_N * 4);
		drawBufLin = (float*)pffft_aligned_malloc(FFT_N_2 * 4);
		for (int i = 0; i < FFT_N_2; i++) {
			drawBuf[i] = -1.0f;
			drawBufLin[i] = 0.0f;
		}
		windowFunc = allocateAndCalcWindowFunc();
	}
  
	~EqMaster() {
		std::unique_lock<std::mutex> lk(m);
		requestStop = true;
		lk.unlock();
		cv.notify_one();
		worker.join();
		
		pffft_destroy_setup(ffts);
		pffft_aligned_free(fftIn[0]);
		pffft_aligned_free(fftIn[1]);
		pffft_aligned_free(fftIn[2]);
		pffft_aligned_free(fftOut);
		pffft_aligned_free(drawBuf);
		pffft_aligned_free(drawBufLin);
		pffft_aligned_free(windowFunc);
	}
  
	void onReset() override {
		mappedId = 0;
		initTrackLabelsAndColors();
		for (int t = 0; t < 24; t++) {
			trackEqs[t].init(t, APP->engine->getSampleRate(), &cvConnected);
		}
		miscSettings.cc4[0] = 0x1;// show band curves by default
		miscSettings.cc4[1] = SPEC_MASK_ON | SPEC_MASK_POST;
		miscSettings.cc4[2] = 0x1; // momentary by default
		miscSettings.cc4[3] = 0x7; // detailsShow
		miscSettings2.cc4[0] = 0;// band label colours
		miscSettings2.cc4[1] = 2;// decay rate fast
		miscSettings2.cc4[2] = 0;// hide eq curves when bypassed
		miscSettings2.cc4[3] = 0;// unused
		showFreqAsNotes.cc1 = 0;
		resetNonJson();
	}
	void resetNonJson() {
		updateTrackLabelRequest = 1;
		trackVu.reset();
		fftWriteHead = 0;
		page = 0;
		cvConnected = 0;
		drawBufSize = -1;// no data to draw yet
	}


	void onRandomize() override {
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// mappedId
		json_object_set_new(rootJ, "mappedId", json_integer(mappedId));
				
		// trackLabels
		json_object_set_new(rootJ, "trackLabels", json_string(trackLabels));
		
		// trackLabelColors
		json_t *trackLabelColorsJ = json_array();
		for (int t = 0; t < 24; t++) {
			json_array_insert_new(trackLabelColorsJ, t, json_integer(trackLabelColors[t]));
		}
		json_object_set_new(rootJ, "trackLabelColors", trackLabelColorsJ);		
				
		// trackVuColors
		json_t *trackVuColorsJ = json_array();
		for (int t = 0; t < 24; t++) {
			json_array_insert_new(trackVuColorsJ, t, json_integer(trackVuColors[t]));
		}
		json_object_set_new(rootJ, "trackVuColors", trackVuColorsJ);		
				
		// miscSettings
		json_object_set_new(rootJ, "miscSettings", json_integer(miscSettings.cc1));
				
		// miscSettings2
		json_object_set_new(rootJ, "miscSettings2", json_integer(miscSettings2.cc1));
				
		// showFreqAsNotes
		json_object_set_new(rootJ, "showFreqAsNotes", json_integer(showFreqAsNotes.cc1));
				
		// trackEqs
		// -------------
		
		// active
		json_t *activeJ = json_array();
		for (int t = 0; t < 24; t++) {
			json_array_insert_new(activeJ, t, json_boolean(trackEqs[t].getTrackActive()));
		}
		json_object_set_new(rootJ, "active", activeJ);		
		
		// bandActive
		json_t *bandActiveJ = json_array();
		for (int t = 0; t < 24; t++) {
			for (int f = 0; f < 4; f++) {
				json_array_insert_new(bandActiveJ, (t << 2) | f, json_real(trackEqs[t].getBandActive(f)));
			}
		}
		json_object_set_new(rootJ, "bandActive", bandActiveJ);		
		
		// freq
		json_t *freqJ = json_array();
		for (int t = 0; t < 24; t++) {
			for (int f = 0; f < 4; f++) {
				json_array_insert_new(freqJ, (t << 2) | f, json_real(trackEqs[t].getFreq(f)));
			}
		}
		json_object_set_new(rootJ, "freq", freqJ);		
		
		// gain
		json_t *gainJ = json_array();
		for (int t = 0; t < 24; t++) {
			for (int f = 0; f < 4; f++) {
				json_array_insert_new(gainJ, (t << 2) | f, json_real(trackEqs[t].getGain(f)));
			}
		}
		json_object_set_new(rootJ, "gain", gainJ);		
		
		// q
		json_t *qJ = json_array();
		for (int t = 0; t < 24; t++) {
			for (int f = 0; f < 4; f++) {
				json_array_insert_new(qJ, (t << 2) | f, json_real(trackEqs[t].getQ(f)));
			}
		}
		json_object_set_new(rootJ, "q", qJ);		
		
		// freqCvAtten
		freqJ = json_array();
		for (int t = 0; t < 24; t++) {
			for (int f = 0; f < 4; f++) {
				json_array_insert_new(freqJ, (t << 2) | f, json_real(trackEqs[t].freqCvAtten[f]));
			}
		}
		json_object_set_new(rootJ, "freqCvAtten", freqJ);		
		
		// gainCvAtten
		gainJ = json_array();
		for (int t = 0; t < 24; t++) {
			for (int f = 0; f < 4; f++) {
				json_array_insert_new(gainJ, (t << 2) | f, json_real(trackEqs[t].gainCvAtten[f]));
			}
		}
		json_object_set_new(rootJ, "gainCvAtten", gainJ);		
		
		// qCvAtten
		qJ = json_array();
		for (int t = 0; t < 24; t++) {
			for (int f = 0; f < 4; f++) {
				json_array_insert_new(qJ, (t << 2) | f, json_real(trackEqs[t].qCvAtten[f]));
			}
		}
		json_object_set_new(rootJ, "qCvAtten", qJ);		
				
		// lowPeak
		json_t *lowPeakJ = json_array();
		for (int t = 0; t < 24; t++) {
			json_array_insert_new(lowPeakJ, t, json_boolean(trackEqs[t].getLowPeak()));
		}
		json_object_set_new(rootJ, "lowPeak", lowPeakJ);		
		
		// highPeak
		json_t *highPeakJ = json_array();
		for (int t = 0; t < 24; t++) {
			json_array_insert_new(highPeakJ, t, json_boolean(trackEqs[t].getHighPeak()));
		}
		json_object_set_new(rootJ, "highPeak", highPeakJ);		
				
		// trackGain
		json_t *trackGainJ = json_array();
		for (int t = 0; t < 24; t++) {
			json_array_insert_new(trackGainJ, t, json_real(trackEqs[t].getTrackGain()));
		}
		json_object_set_new(rootJ, "trackGain", trackGainJ);		
		
		// -------------
				
		return rootJ;
	}


	void dataFromJson(json_t *rootJ) override {
		// mappedId
		json_t *mappedIdJ = json_object_get(rootJ, "mappedId");
		if (mappedIdJ)
			mappedId = json_integer_value(mappedIdJ);

		// trackLabels
		json_t *textJ = json_object_get(rootJ, "trackLabels");
		if (textJ)
			snprintf(trackLabels, 24 * 4 + 1, "%s", json_string_value(textJ));

		// trackLabelColors
		json_t *trackLabelColorsJ = json_object_get(rootJ, "trackLabelColors");
		if (trackLabelColorsJ) {
			for (int t = 0; t < 24; t++) {
				json_t *trackLabelColorsArrayJ = json_array_get(trackLabelColorsJ, t);
				if (trackLabelColorsArrayJ)
					trackLabelColors[t] = json_integer_value(trackLabelColorsArrayJ);
			}
		}

		// trackVuColors
		json_t *trackVuColorsJ = json_object_get(rootJ, "trackVuColors");
		if (trackVuColorsJ) {
			for (int t = 0; t < 24; t++) {
				json_t *trackVuColorsArrayJ = json_array_get(trackVuColorsJ, t);
				if (trackVuColorsArrayJ)
					trackVuColors[t] = json_integer_value(trackVuColorsArrayJ);
			}
		}

		// miscSettings
		json_t *miscSettingsJ = json_object_get(rootJ, "miscSettings");
		if (miscSettingsJ)
			miscSettings.cc1 = json_integer_value(miscSettingsJ);

		// miscSettings2
		json_t *miscSettings2J = json_object_get(rootJ, "miscSettings2");
		if (miscSettings2J)
			miscSettings2.cc1 = json_integer_value(miscSettings2J);

		// showFreqAsNotes
		json_t *showFreqAsNotesJ = json_object_get(rootJ, "showFreqAsNotes");
		if (showFreqAsNotesJ)
			showFreqAsNotes.cc1 = json_integer_value(showFreqAsNotesJ);

		// trackEqs
		// -------------

		// active
		json_t *activeJ = json_object_get(rootJ, "active");
		if (activeJ) {
			for (int t = 0; t < 24; t++) {
				json_t *activeArrayJ = json_array_get(activeJ, t);
				if (activeArrayJ)
					trackEqs[t].setTrackActive(json_is_true(activeArrayJ));
			}
		}

		// bandActive
		json_t *bandActiveJ = json_object_get(rootJ, "bandActive");
		if (bandActiveJ) {
			for (int t = 0; t < 24; t++) {
				for (int f = 0; f < 4; f++) {
					json_t *bandActiveArrayJ = json_array_get(bandActiveJ, (t << 2) | f);
					if (bandActiveArrayJ)
						trackEqs[t].setBandActive(f, json_number_value(bandActiveArrayJ));
				}
			}
		}

		// freq
		json_t *freqJ = json_object_get(rootJ, "freq");
		if (freqJ) {
			for (int t = 0; t < 24; t++) {
				for (int f = 0; f < 4; f++) {
					json_t *freqArrayJ = json_array_get(freqJ, (t << 2) | f);
					if (freqArrayJ)
						trackEqs[t].setFreq(f, json_number_value(freqArrayJ));
				}
			}
		}

		// gain
		json_t *gainJ = json_object_get(rootJ, "gain");
		if (gainJ) {
			for (int t = 0; t < 24; t++) {
				for (int f = 0; f < 4; f++) {
					json_t *gainArrayJ = json_array_get(gainJ, (t << 2) | f);
					if (gainArrayJ)
						trackEqs[t].setGain(f, json_number_value(gainArrayJ));
				}
			}
		}

		// q
		json_t *qJ = json_object_get(rootJ, "q");
		if (qJ) {
			for (int t = 0; t < 24; t++) {
				for (int f = 0; f < 4; f++) {
					json_t *qArrayJ = json_array_get(qJ, (t << 2) | f);
					if (qArrayJ)
						trackEqs[t].setQ(f, json_number_value(qArrayJ));
				}
			}
		}

		// freqCvAtten
		freqJ = json_object_get(rootJ, "freqCvAtten");
		if (freqJ) {
			for (int t = 0; t < 24; t++) {
				for (int f = 0; f < 4; f++) {
					json_t *freqArrayJ = json_array_get(freqJ, (t << 2) | f);
					if (freqArrayJ)
						trackEqs[t].freqCvAtten[f] = json_number_value(freqArrayJ);
				}
			}
		}

		// gainCvAtten
		gainJ = json_object_get(rootJ, "gainCvAtten");
		if (gainJ) {
			for (int t = 0; t < 24; t++) {
				for (int f = 0; f < 4; f++) {
					json_t *gainArrayJ = json_array_get(gainJ, (t << 2) | f);
					if (gainArrayJ)
						trackEqs[t].gainCvAtten[f] = json_number_value(gainArrayJ);
				}
			}
		}

		// qCvAtten
		qJ = json_object_get(rootJ, "qCvAtten");
		if (qJ) {
			for (int t = 0; t < 24; t++) {
				for (int f = 0; f < 4; f++) {
					json_t *qArrayJ = json_array_get(qJ, (t << 2) | f);
					if (qArrayJ)
						trackEqs[t].qCvAtten[f] = json_number_value(qArrayJ);
				}
			}
		}

		// lowPeak
		json_t *lowPeakJ = json_object_get(rootJ, "lowPeak");
		if (lowPeakJ) {
			for (int t = 0; t < 24; t++) {
				json_t *lowPeakArrayJ = json_array_get(lowPeakJ, t);
				if (lowPeakArrayJ)
					trackEqs[t].setLowPeak(json_is_true(lowPeakArrayJ));
			}
		}

		// highPeak
		json_t *highPeakJ = json_object_get(rootJ, "highPeak");
		if (highPeakJ) {
			for (int t = 0; t < 24; t++) {
				json_t *highPeakArrayJ = json_array_get(highPeakJ, t);
				if (highPeakArrayJ)
					trackEqs[t].setHighPeak(json_is_true(highPeakArrayJ));
			}
		}

		// trackGain
		json_t *trackGainJ = json_object_get(rootJ, "trackGain");
		if (trackGainJ) {
			for (int t = 0; t < 24; t++) {
				json_t *trackGainArrayJ = json_array_get(trackGainJ, t);
				if (trackGainArrayJ)
					trackEqs[t].setTrackGain(json_number_value(trackGainArrayJ));
			}
		}

		// -------------

		resetNonJson();
	}


	void onSampleRateChange() override {
		for (int t = 0; t < 24; t++) {
			trackEqs[t].updateSampleRate(APP->engine->getSampleRate());
		}
	}
	
	
	
	void worker_thread() {
		static const float vertScaling = 1.1f;
		static const float vertOffset = 10.0f;
		while (true) {
			std::unique_lock<std::mutex> lk(m);
			while (!requestWork && !requestStop) {
				cv.wait(lk);
			}
			lk.unlock();
			if (requestStop) break;
			
			// compute fft
			pffft_transform_ordered(ffts, fftIn[requestPage], fftOut, NULL, PFFFT_FORWARD);

			// calculate magnitude and store in 1st half of array
			for (int x = 0; x < FFT_N ; x += 2) {	
				fftOut[x >> 1] = fftOut[x + 0] * fftOut[x + 0] + fftOut[x + 1] * fftOut[x + 1];// sqrt is not needed in magnitude calc since when take log of this, it can be absorbed in scaling multiplier
			}
			
			// calculate pixel scaled log of frequency and store in 2nd half of array
			for (int x = 0; x < (FFT_N_2) / 4 ; x++) {
				int xt4 = x << 2;
				simd::float_4 vecp(xt4 + 0, xt4 + 1, xt4 + 2, xt4 + 3);
				vecp = (vecp / ((float)(FFT_N - 1))) * trackEqs[0].getSampleRate();// linear freq a this line
				vecp = simd::round(simd::rescale(simd::log10(vecp), minLogFreq, maxLogFreq, 0.0f, eqCurveWidth));// pixel scaled log freq at this line
				vecp.store(&fftOut[xt4 + FFT_N_2]);
			}
			
			// compact frequency bins
			int i = 1;// index into compacted bins 
			drawBuf[FFT_N_2] = fftOut[FFT_N_2];
			for (int x = 1; x < FFT_N_2 ; x++) {// index into non-compacted bins
				if (drawBuf[i - 1 + FFT_N_2] == fftOut[x + FFT_N_2]) {
					fftOut[i - 1] = std::fmax(fftOut[i - 1], fftOut[x]);
				}
				else {
					fftOut[i] = fftOut[x];
					drawBuf[i + FFT_N_2] = fftOut[x + FFT_N_2];
					i++;
				}
			}
			int compactedSize = i;
			
			// decay
			static constexpr float noDecay = 1000.0f;
			float decayFactor = 0.0f;
			if ((miscSettings.cc4[1] & SPEC_MASK_FREEZE) == 0) {
				if (miscSettings2.cc4[1] == 0) {// slow decay
					decayFactor = 5.0f;
				} 
				else if (miscSettings2.cc4[1] == 1) {// med decay
					decayFactor = 12.0f;
				}
				else if (miscSettings2.cc4[1] == 2) {// fast decay
					decayFactor = 20.0f;
				}
				else {
					decayFactor = noDecay;
				}
			}
			if (decayFactor != noDecay) {
				for (int i = 0; i < compactedSize; i++) {
					if (fftOut[i] > drawBufLin[i]) {
						drawBufLin[i] = fftOut[i];
					}
					else {
						drawBufLin[i] += (fftOut[i] - drawBufLin[i]) * decayFactor * FFT_N_2 / trackEqs[0].getSampleRate();// decay
					}
				}
			}
			else {
				memcpy(&drawBufLin[0], &fftOut[0], compactedSize * 4);
			}
			
			// calculate log of magnitude and transfer to drawBuf
			for (int x = 0; x < ((compactedSize + 3) >> 2) ; x++) {
				simd::float_4 vecp = simd::float_4::load(&drawBufLin[x << 2]);
				vecp = simd::fmax(vertScaling * 20.0f * simd::log10(vecp) + vertOffset, -1.0f);// fmax for proper enclosed region for fill
				vecp.store(&drawBuf[x << 2]);					
			}
		
			drawBufSize = compactedSize;

			requestWork = false;
		}
	}	

	void process(const ProcessArgs &args) override {
		int selectedTrack = getSelectedTrack();
		
		expPresentRight = (rightExpander.module && rightExpander.module->model == modelEqExpander);
		expPresentLeft = (leftExpander.module && leftExpander.module->model == modelEqExpander);
		
		
		//********** Inputs **********
		
		// From Eq-Expander
		if (expPresentRight || expPresentLeft) {
			MfeExpInterface *messagesFromExpander = expPresentRight ? // use only when expander present
											(MfeExpInterface*)rightExpander.consumerMessage :
											(MfeExpInterface*)leftExpander.consumerMessage;
			
			// track band values
			int index6 = clamp(messagesFromExpander->trackCvsIndex6, 0, 5);
			int cvConnectedSubset = messagesFromExpander->trackCvsConnected;
			for (int i = 0; i < 4; i++) {
				if ((cvConnectedSubset & (1 << i)) != 0) {
					int bandTrkIndex = (index6 << 2) + i;
					processTrackBandCvs(bandTrkIndex, selectedTrack, &(messagesFromExpander->trackCvs[16 * i]));
				}
			}
			cvConnected &= ~(0xF << (index6 << 2));// clear all connected bits for current subset
			cvConnected |= (cvConnectedSubset << (index6 << 2));// set relevant connected bits for current subset
 
			// track enables, each track refreshed at fs / 25
			int enableTrkIndex = clamp(messagesFromExpander->trackEnableIndex, 0, 24);
			processTrackEnableCvs(enableTrkIndex, selectedTrack, messagesFromExpander->trackEnable);
		}
		else {
			cvConnected = 0x000000;
		}
		
		if (refresh.processInputs()) {
			outputs[SIG_OUTPUTS + 0].setChannels(numChannels16);
			outputs[SIG_OUTPUTS + 1].setChannels(numChannels16);
			outputs[SIG_OUTPUTS + 2].setChannels(numChannels16);
		}// userInputs refresh
		
		
		//********** Outputs **********

		bool vuProcessed = false;
		for (int i = 0; i < 3; i++) {
			if (inputs[SIG_INPUTS + i].isConnected()) {
				for (int t = 0; t < 8; t++) {
					float* in = inputs[SIG_INPUTS + i].getVoltages((t << 1) + 0);
					float out[2];
					bool globalEnable = params[GLOBAL_BYPASS_PARAM].getValue() < 0.5f;
					trackEqs[(i << 3) + t].process(out, in, globalEnable);
					outputs[SIG_OUTPUTS + i].setVoltage(out[0], (t << 1) + 0);
					outputs[SIG_OUTPUTS + i].setVoltage(out[1], (t << 1) + 1);
					if ( ((i << 3) + t) == selectedTrack ) {
						// VU
						trackVu.process(args.sampleTime, out);
						vuProcessed = true;
						
						// Spectrum
						if ( (miscSettings.cc4[1] & SPEC_MASK_ON) != 0 ) {
							float sample = ((miscSettings.cc4[1] & SPEC_MASK_POST) == 0 ? 
												(in[0] + in[1]) : 
												(out[0] + out[1]));// no need to div by two, scaling done later
							
							// write sample into fft input buffers and apply windowing
							fftIn[page][fftWriteHead] = sample * windowFunc[fftWriteHead >= FFT_N_2 ? ((FFT_N - 1) - fftWriteHead) : fftWriteHead];// * dsp::blackmanHarris((float)fftWriteHead / (float)(FFT_N - 1));
							if (fftWriteHead >= FFT_N_2) {
								int offsetHead = fftWriteHead - FFT_N_2;
								fftIn[(page + 1) % 3][offsetHead] = sample * windowFunc[offsetHead];// * dsp::blackmanHarris((float)offsetHead / (float)(FFT_N - 1));
							}	
							
							// increment write head and possibly page
							fftWriteHead++;
							if (fftWriteHead >= FFT_N) {
								fftWriteHead = FFT_N_2;
								//thread 
								if (requestWork) {
									// INFO("FFT too slow, page skipped");
								}
								else {
									requestPage = page;
									requestWork = true;
									cv.notify_one();
								}
								page++;
								if (page >= 3) {
									page = 0;
								}
							}
						}
						else {
							fftWriteHead = 0;
							page = 0;
						}// Spectrum
					}
				}
			}
		}
		if (!vuProcessed) {
			trackVu.reset();
		}
		if (!vuProcessed || (miscSettings.cc4[1] & SPEC_MASK_ON) == 0) {
			drawBufSize = -1;
		}
		
		//********** Lights **********
		
		if (refresh.processLights()) {
		}

	}// process()
	
	
	void processTrackBandCvs(int bandTrkIndex, int selectedTrack, float *cvs) {
		// cvs[0]: LF active
		// cvs[1]: LF freq
		// cvs[2]: LF gain
		// cvs[3]: LF Q
		// cvs[4]: LMF active
		// cvs[5]: LMF freq
		// ...
		// cvs[12]: HF active
		// cvs[13]: HF freq
		// cvs[14]: HF gain
		// cvs[15]: HF Q

		for (int b = 0; b < 4; b++) {// 0 = LF, 1 = LMF, 2 = HMF, 3 = HF
			// band active
			int state = trackBandCvTriggers[bandTrkIndex][b].process(cvs[(b << 2) + 0]);
			if (state != 0) {
				if (miscSettings.cc4[2] == 1) {// if momentaryCvButtons
					if (state == 1) {// if rising edge
						// toggle
						float newState = (trackEqs[bandTrkIndex].getBandActive(b) < 0.5f ? 1.0f : 0.0f);// toggle
						trackEqs[bandTrkIndex].setBandActive(b, newState);
						if (bandTrkIndex == selectedTrack) {
							params[FREQ_ACTIVE_PARAMS + b].setValue(newState);
						}
					}
				}
				else {
					// gate level
					float newState = cvs[(b << 2) + 0] >= 0.5f ? 1.0f : 0.0f;
					trackEqs[bandTrkIndex].setBandActive(b, newState);
					if (bandTrkIndex == selectedTrack) {
						params[FREQ_ACTIVE_PARAMS + b].setValue(newState);
					}
				}
			}	

			// freq
			trackEqs[bandTrkIndex].setFreqCv(b, cvs[(b << 2) + 1]);
			// gain
			trackEqs[bandTrkIndex].setGainCv(b, cvs[(b << 2) + 2]);
			// q
			trackEqs[bandTrkIndex].setQCv(b, cvs[(b << 2) + 3]);
		}
	}
	
	void processTrackEnableCvs(int enableTrkIndex, int selectedTrack, float enableValue) {// does global bypass also
		int state = trackEnableCvTriggers[enableTrkIndex].process(enableValue);
		if (state != 0) {
			if (miscSettings.cc4[2] == 1) {// if momentaryCvButtons
				if (state == 1) {// if rising edge
					// toggle
					if (enableTrkIndex >= 24) {// global bypass
						params[GLOBAL_BYPASS_PARAM].setValue(params[GLOBAL_BYPASS_PARAM].getValue() >= 0.5f ? 0.0f : 1.0f);
					}
					else {				
						bool newState = !trackEqs[enableTrkIndex].getTrackActive();
						trackEqs[enableTrkIndex].setTrackActive(newState);
						if (enableTrkIndex == selectedTrack) {
							params[TRACK_ACTIVE_PARAM].setValue(newState ? 1.0f : 0.0f);
						}
					}
				}
			}
			else {
				// gate level
				bool newState = enableValue >= 0.5f;
				if (enableTrkIndex >= 24) {// global bypass
					params[GLOBAL_BYPASS_PARAM].setValue(newState ? 1.0f : 0.0f);
				}
				else {
					trackEqs[enableTrkIndex].setTrackActive(newState);
					if (enableTrkIndex == selectedTrack) {
						params[TRACK_ACTIVE_PARAM].setValue(newState ? 1.0f : 0.0f);
					}
				}
			}
		}
	}
};


//-----------------------------------------------------------------------------


struct EqMasterWidget : ModuleWidget {
	time_t oldTime = 0;
	int64_t oldMappedId = 0;
	int oldSelectedTrack = -1;
	TrackLabel* trackLabel;
	int lastMovedKnobId = -1;
	time_t lastMovedKnobTime = 0;
	int8_t cloakedMode = 0;
	simd::float_4 bandParamsWithCvs[3] = {};// [0] = freq, [1] = gain, [2] = q
	bool bandParamsCvConnected = false;
	PanelBorder* panelBorder;

	
	// Module's context menu
	// --------------------

	void appendContextMenu(Menu *menu) override {		
		EqMaster* module = (EqMaster*)(this->module);
		assert(module);

		menu->addChild(new MenuSeparator());
		
		FetchLabelsItem *fetchItem = createMenuItem<FetchLabelsItem>("Fetch track labels from Mixer", RIGHT_ARROW);
		fetchItem->mappedIdSrc = &(module->mappedId);
		menu->addChild(fetchItem);

		if (module->expPresentLeft || module->expPresentRight) {
			MomentaryCvItem *momentItem = createMenuItem<MomentaryCvItem>("Track/band active CVs", RIGHT_ARROW);
			momentItem->momentaryCvButtonsSrc = &(module->miscSettings.cc4[2]);
			menu->addChild(momentItem);
		}
		
		DecayRateItem *decayItem = createMenuItem<DecayRateItem>("Analyser decay", RIGHT_ARROW);
		decayItem->decayRateSrc = &(module->miscSettings2.cc4[1]);
		menu->addChild(decayItem);

		HideEqWhenBypassItem *hideeqItem = createMenuItem<HideEqWhenBypassItem>("Hide EQ curves when bypassed", CHECKMARK(module->miscSettings2.cc4[2] != 0));
		hideeqItem->hideEqWhenBypass = &(module->miscSettings2.cc4[2]);
		menu->addChild(hideeqItem);

		menu->addChild(new MenuSeparator());
		
		DispTwoColorItem *dispColItem = createMenuItem<DispTwoColorItem>("Display colour", RIGHT_ARROW);
		dispColItem->srcColor = &(module->miscSettings2.cc4[0]);
		menu->addChild(dispColItem);
		
		if (module->mappedId == 0) {
			VuFiveColorItem *vuColItem = createMenuItem<VuFiveColorItem>("VU colour", RIGHT_ARROW);
			vuColItem->srcColors = module->trackVuColors;
			vuColItem->vectorSize = 24;
			menu->addChild(vuColItem);
		}
		
		KnobArcShowItem *knobArcShowItem = createMenuItem<KnobArcShowItem>("Knob arcs", RIGHT_ARROW);
		knobArcShowItem->srcDetailsShow = &(module->miscSettings.cc4[3]);
		menu->addChild(knobArcShowItem);
	}
	
	
	EqMasterWidget(EqMaster *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/eqmaster.svg")));
		SvgPanel* svgPanel = (SvgPanel*)getPanel();
		panelBorder = findBorder(svgPanel->fb);
		
		
		// Left side controls and inputs
		// ------------------
		static const float leftX = 8.29f;
		// Track label
		addChild(trackLabel = createWidgetCentered<TrackLabel>(mm2px(Vec(leftX, 11.5f))));
		if (module) {
			trackLabel->trackLabelColorsSrc = module->trackLabelColors;
			trackLabel->bandLabelColorsSrc = &(module->miscSettings2.cc4[0]);
			trackLabel->mappedId = &(module->mappedId);
			trackLabel->trackLabelsSrc = module->trackLabels;
			trackLabel->trackParamSrc = &(module->params[TRACK_PARAM]);
			trackLabel->trackEqsSrc = module->trackEqs;
			trackLabel->updateTrackLabelRequestSrc = &(module->updateTrackLabelRequest);
		}
		// Track knob
		TrackKnob* trackKnob;
		addParam(trackKnob = createParamCentered<TrackKnob>(mm2px(Vec(leftX, 22.7f)), module, TRACK_PARAM));
		if (module) {
			trackKnob->updateTrackLabelRequestSrc = &(module->updateTrackLabelRequest);
			trackKnob->trackEqsSrc = module->trackEqs;
			trackKnob->polyInputs = &(module->inputs[EqMaster::SIG_INPUTS]);
		}
		// Active switch
		ActiveSwitch* activeSwitch;
		addParam(activeSwitch = createParamCentered<ActiveSwitch>(mm2px(Vec(leftX, 48.775f)), module, TRACK_ACTIVE_PARAM));
		if (module) {
			activeSwitch->trackParamSrc = &(module->params[TRACK_PARAM]);
			activeSwitch->trackEqsSrc = module->trackEqs;
		}
		// Global bypass switch
		addParam(createParamCentered<MmBypassButton>(mm2px(Vec(leftX, 67.7f)), module, GLOBAL_BYPASS_PARAM));
		// Signal inputs
		static const float jackY = 84.35f;
		static const float jackDY = 12.8f;
		addInput(createInputCentered<MmPortGold>(mm2px(Vec(leftX, jackY)), module, EqMaster::SIG_INPUTS + 0));		
		addInput(createInputCentered<MmPortGold>(mm2px(Vec(leftX, jackY + jackDY)), module, EqMaster::SIG_INPUTS + 1));		
		addInput(createInputCentered<MmPortGold>(mm2px(Vec(leftX, jackY + jackDY * 2)), module, EqMaster::SIG_INPUTS + 2));		
		
		
		// Center part
		// Screen - Spectrum settings buttons
		SpectrumSettingsButtons *specButtons;
		addChild(specButtons = createWidget<SpectrumSettingsButtons>(mm2px(Vec(18.0f, 9.5f))));
		ShowBandCurvesButtons *bandsButton;
		addChild(bandsButton = createWidget<ShowBandCurvesButtons>(mm2px(Vec(95.0f, 9.5f))));
		if (module) {
			bandsButton->settingSrc = &module->miscSettings.cc4[0];
			specButtons->settingSrc = &module->miscSettings.cc4[1];
		}
		// Screen - EQ curve and grid
		EqCurveAndGrid* eqCurveAndGrid;
		addChild(eqCurveAndGrid = createWidgetCentered<EqCurveAndGrid>(mm2px(Vec(71.12f, 45.47f - 0.5f))));
		if (module) {
			eqCurveAndGrid->trackParamSrc = &(module->params[TRACK_PARAM]);
			eqCurveAndGrid->trackEqsSrc = module->trackEqs;
			eqCurveAndGrid->miscSettingsSrc = &(module->miscSettings);
			eqCurveAndGrid->miscSettings2Src = &(module->miscSettings2);
			eqCurveAndGrid->globalBypassParamSrc = &(module->params[GLOBAL_BYPASS_PARAM]);
			eqCurveAndGrid->bandParamsWithCvs = bandParamsWithCvs;
			eqCurveAndGrid->bandParamsCvConnected = &bandParamsCvConnected;
			eqCurveAndGrid->drawBuf = module->drawBuf;
			eqCurveAndGrid->drawBufSize = &(module->drawBufSize);
			eqCurveAndGrid->lastMovedKnobIdSrc = &lastMovedKnobId;
			eqCurveAndGrid->lastMovedKnobTimeSrc = &lastMovedKnobTime;
		}
		
		// Screen - Big Numbers
		BigNumbersEq* bigNumbersEq;
		addChild(bigNumbersEq = createWidgetCentered<BigNumbersEq>(mm2px(Vec(71.12f, 68.0f))));
		if (module) {
			bigNumbersEq->trackParamSrc = &(module->params[TRACK_PARAM]);
			bigNumbersEq->trackEqsSrc = module->trackEqs;
			bigNumbersEq->lastMovedKnobIdSrc = &lastMovedKnobId;
			bigNumbersEq->lastMovedKnobTimeSrc = &lastMovedKnobTime;
		}
		
		
		// Controls
		static const float ctrlX = 23.94f;
		static const float ctrlDX = 27.74f;
		
		// Peak/shelf buttons for LF and HF
		PeakShelfBase* peakShelfSwitches[4];
		// LF shelf:
		addParam(peakShelfSwitches[0] = createParamCentered<ShelfLowSwitch>(mm2px(Vec(19.39f, 81.55f)), module, LOW_PEAK_PARAM));
		// LF peak:
		addParam(peakShelfSwitches[1] = createParamCentered<PeakSwitch>(mm2px(Vec(39.63f, 81.55f)), module, LOW_PEAK_PARAM));
		// HF peak:
		addParam(peakShelfSwitches[2] = createParamCentered<PeakSwitch>(mm2px(Vec(102.6f, 81.55f)), module, HIGH_PEAK_PARAM));
		// HF shelf:
		addParam(peakShelfSwitches[3] = createParamCentered<ShelfHighSwitch>(mm2px(Vec(122.85f, 81.55f)), module, HIGH_PEAK_PARAM));
		if (module) {
			for (int b = 0; b < 4; b++) {
				peakShelfSwitches[b]->trackParamSrc = &(module->params[TRACK_PARAM]);
				peakShelfSwitches[b]->trackEqsSrc = module->trackEqs;
				peakShelfSwitches[b]->isLF = (b < 2);
			}
		}
		
		// band enable buttons
		BandSwitch* bandSwitches[4];	
		addParam(bandSwitches[0] = createParamCentered<BandActiveSwitch<0>>(mm2px(Vec(ctrlX + ctrlDX * 0 + 3.4f, 81.55f)), module, FREQ_ACTIVE_PARAMS + 0));
		addParam(bandSwitches[1] = createParamCentered<BandActiveSwitch<1>>(mm2px(Vec(ctrlX + ctrlDX * 1 + 2.4f, 81.55f)), module, FREQ_ACTIVE_PARAMS + 1));
		addParam(bandSwitches[2] = createParamCentered<BandActiveSwitch<2>>(mm2px(Vec(ctrlX + ctrlDX * 2 + 2.2f, 81.55f)), module, FREQ_ACTIVE_PARAMS + 2));
		addParam(bandSwitches[3] = createParamCentered<BandActiveSwitch<3>>(mm2px(Vec(ctrlX + ctrlDX * 3 + 3.1f, 81.55f)), module, FREQ_ACTIVE_PARAMS + 3));
		if (module) {
			for (int b = 0; b < 4; b++) {
				bandSwitches[b]->trackParamSrc = &(module->params[TRACK_PARAM]);
				bandSwitches[b]->freqActiveParamsSrc = &(module->params[FREQ_ACTIVE_PARAMS]);
				bandSwitches[b]->trackEqsSrc = module->trackEqs;
			}
		}	
		
		// Freq, gain and q knobs
		BandKnob* bandKnobs[12];
		// freq
		addParam(bandKnobs[0] = createParamCentered<EqFreqKnob<0>>(mm2px(Vec(ctrlX + ctrlDX * 0, 91.2f)), module, FREQ_PARAMS + 0));
		addParam(bandKnobs[1] = createParamCentered<EqFreqKnob<1>>(mm2px(Vec(ctrlX + ctrlDX * 1, 91.2f)), module, FREQ_PARAMS + 1));
		addParam(bandKnobs[2] = createParamCentered<EqFreqKnob<2>>(mm2px(Vec(ctrlX + ctrlDX * 2, 91.2f)), module, FREQ_PARAMS + 2));
		addParam(bandKnobs[3] = createParamCentered<EqFreqKnob<3>>(mm2px(Vec(ctrlX + ctrlDX * 3, 91.2f)), module, FREQ_PARAMS + 3));
		// gain
		addParam(bandKnobs[0 + 4] = createParamCentered<EqGainKnob<0>>(mm2px(Vec(ctrlX + ctrlDX * 0 + 11.04f, 101.78f)), module, GAIN_PARAMS + 0));
		addParam(bandKnobs[1 + 4] = createParamCentered<EqGainKnob<1>>(mm2px(Vec(ctrlX + ctrlDX * 1 + 11.04f, 101.78f)), module, GAIN_PARAMS + 1));
		addParam(bandKnobs[2 + 4] = createParamCentered<EqGainKnob<2>>(mm2px(Vec(ctrlX + ctrlDX * 2 + 11.04f, 101.78f)), module, GAIN_PARAMS + 2));
		addParam(bandKnobs[3 + 4] = createParamCentered<EqGainKnob<3>>(mm2px(Vec(ctrlX + ctrlDX * 3 + 11.04f, 101.78f)), module, GAIN_PARAMS + 3));
		// q
		addParam(bandKnobs[0 + 8] = createParamCentered<EqQKnob<0>>(mm2px(Vec(ctrlX + ctrlDX * 0, 112.37f)), module, Q_PARAMS + 0));
		addParam(bandKnobs[1 + 8] = createParamCentered<EqQKnob<1>>(mm2px(Vec(ctrlX + ctrlDX * 1, 112.37f)), module, Q_PARAMS + 1));
		addParam(bandKnobs[2 + 8] = createParamCentered<EqQKnob<2>>(mm2px(Vec(ctrlX + ctrlDX * 2, 112.37f)), module, Q_PARAMS + 2));
		addParam(bandKnobs[3 + 8] = createParamCentered<EqQKnob<3>>(mm2px(Vec(ctrlX + ctrlDX * 3, 112.37f)), module, Q_PARAMS + 3));
		//
		if (module) {
			for (int c = 0; c < 12; c++) {
				bandKnobs[c]->paramWithCV = &(bandParamsWithCvs[c >> 2][c & 0x3]);
				bandKnobs[c]->paramCvConnected = &bandParamsCvConnected;
				bandKnobs[c]->cloakedModeSrc = &cloakedMode;
				bandKnobs[c]->detailsShowSrc = &(module->miscSettings.cc4[3]);
				bandKnobs[c]->trackParamSrc = &(module->params[TRACK_PARAM]);
				bandKnobs[c]->trackEqsSrc = module->trackEqs;
				bandKnobs[c]->lastMovedKnobIdSrc = & lastMovedKnobId;
				bandKnobs[c]->lastMovedKnobTimeSrc = &lastMovedKnobTime;
			}
		}
		
		// Freq, gain and q labels
		BandLabelBase* bandLabels[12];// 4 freqs, 4 gains, 4 qs
		for (int b = 0; b < 4; b++) {
			addChild(bandLabels[b + 0] = createWidgetCentered<BandLabelFreq>(mm2px(Vec(ctrlX + ctrlDX * b + 11.4f, 91.2f + 0.9f))));
			addChild(bandLabels[b + 4] = createWidgetCentered<BandLabelGain>(mm2px(Vec(ctrlX + ctrlDX * b + 11.4f - 12.3f, 101.78f + 0.9f))));
			addChild(bandLabels[b + 8] = createWidgetCentered<BandLabelQ>(mm2px(Vec(ctrlX + ctrlDX * b + 11.4f, 112.37f + 0.9f))));
		}
		if (module) {
			for (int i = 0; i < 12; i++) {
				bandLabels[i]->bandLabelColorsSrc = &(module->miscSettings2.cc4[0]);
				bandLabels[i]->trackParamSrc = &(module->params[TRACK_PARAM]);
				bandLabels[i]->trackEqsSrc = module->trackEqs;
				bandLabels[i]->band = (i % 4);
			}
			for (int i = 0; i < 4; i++) {
				bandLabels[i]->showFreqAsNotesSrc = &(module->showFreqAsNotes.cc4[i]);
			}
		}
		
				
		// Right side VU and outputs
		// ------------------
		static const float rightX = 133.95f;
		// VU meter
		if (module) {
			VuMeterEq *newVU = createWidgetCentered<VuMeterEq>(mm2px(Vec(rightX, 37.5f)));
			newVU->srcLevels = module->trackVu.vuValues;
			newVU->trackVuColorsSrc = module->trackVuColors;
			newVU->trackParamSrc = &(module->params[TRACK_PARAM]);
			addChild(newVU);
		}
		// Gain knob
		TrackGainKnob* trackGainKnob;
		addParam(trackGainKnob = createParamCentered<TrackGainKnob>(mm2px(Vec(rightX, 67.0f)), module, TRACK_GAIN_PARAM));
		if (module) {
			trackGainKnob->trackParamSrc = &(module->params[TRACK_PARAM]);
			trackGainKnob->trackEqsSrc = module->trackEqs;
			trackGainKnob->detailsShowSrc = &(module->miscSettings.cc4[3]);
			trackGainKnob->cloakedModeSrc = &(module->cloakedMode);			
		}		
		// Signal outputs
		addOutput(createOutputCentered<MmPortGold>(mm2px(Vec(rightX, jackY)), module, EqMaster::SIG_OUTPUTS + 0));		
		addOutput(createOutputCentered<MmPortGold>(mm2px(Vec(rightX, jackY + jackDY)), module, EqMaster::SIG_OUTPUTS + 1));		
		addOutput(createOutputCentered<MmPortGold>(mm2px(Vec(rightX, jackY + jackDY * 2)), module, EqMaster::SIG_OUTPUTS + 2));		
	}
	
	void step() override {
		EqMaster* module = (EqMaster*)(this->module);
		if (module) {
			int trk = module->getSelectedTrack();			
			
			// update labels from message bus at 1Hz
			time_t currentTime = time(0);
			if (currentTime != oldTime) {
				oldTime = currentTime;

				if (module->mappedId == 0) {
					if (oldMappedId != 0) {
						module->initTrackLabelsAndColors();
					}
				}
				else {
					MixerMessage message;
					message.id = module->mappedId;
					mixerMessageBus.receive(&message);
					if (message.id == 0) {// if deregistered
						// do nothing! since it may be possible that the eq is loaded and step executes before the mixer modules are finished loading
						// module->initTrackLabelsAndColors();
						// module->mappedId = 0;
					}
					else {
						if (message.isJr) {
							module->initTrackLabelsAndColors();// need this since message.trkGrpAuxLabels is not completely filled
							// Track labels
							memcpy(module->trackLabels, message.trkGrpAuxLabels, 8 * 4);
							memcpy(&(module->trackLabels[16 * 4]), &(message.trkGrpAuxLabels[16 * 4]), 2 * 4);
							memcpy(&(module->trackLabels[(16 + 4) * 4]), &(message.trkGrpAuxLabels[(16 + 4) * 4]), 4 * 4);
							// VU colors
							if (message.vuColors[0] >= numVuThemes) {// per track
								memcpy(module->trackVuColors, &message.vuColors[1], 8);
								memcpy(&module->trackVuColors[16], &message.vuColors[1 + 16], 2);
								memcpy(&module->trackVuColors[16 + 4], &message.vuColors[1 + 16 + 4], 4);
							}
							else {
								for (int t = 0; t < 8; t++) {
									module->trackVuColors[t] = message.vuColors[0];
								}
								for (int t = 0; t < 2; t++) {
									module->trackVuColors[t + 16] = message.vuColors[0];
								}
								for (int t = 0; t < 4; t++) {
									module->trackVuColors[t + 16 + 4] = message.vuColors[0];
								}
							}
							// display colors
							if (message.dispColors[0] >= numDispThemes) {// per track
								memcpy(module->trackLabelColors, &message.dispColors[1], 8);
								memcpy(&module->trackLabelColors[16], &message.dispColors[1 + 16], 2);
								memcpy(&module->trackLabelColors[16 + 4], &message.dispColors[1 + 16 + 4], 4);
							}
							else {
								for (int t = 0; t < 8; t++) {
									module->trackLabelColors[t] = message.dispColors[0];
								}
								for (int t = 0; t < 2; t++) {
									module->trackLabelColors[t + 16] = message.dispColors[0];
								}
								for (int t = 0; t < 4; t++) {
									module->trackLabelColors[t + 16 + 4] = message.dispColors[0];
								}
							}
						}
						else {
							// Track labels
							memcpy(module->trackLabels, message.trkGrpAuxLabels, 24 * 4);
							// VU colors
							if (message.vuColors[0] >= numVuThemes) {// per track
								memcpy(module->trackVuColors, &message.vuColors[1], 24);
							}
							else {
								for (int t = 0; t < 24; t++) {
									module->trackVuColors[t] = message.vuColors[0];
								}
							}
							// display colors
							if (message.dispColors[0] >= numDispThemes) {// per track
								memcpy(module->trackLabelColors, &message.dispColors[1], 24);
							}
							else {
								for (int t = 0; t < 24; t++) {
									module->trackLabelColors[t] = message.dispColors[0];
								}
							}
						}
					}
				}
				module->updateTrackLabelRequest = 1;
				
				oldMappedId = module->mappedId;
			}

			// Track label (pull from module or this step method)
			if (module->updateTrackLabelRequest != 0) {// pull request from module
				if (module->updateTrackLabelRequest > 1) {
					oldSelectedTrack = -1;
				}
				trackLabel->text = std::string(&(module->trackLabels[trk * 4]), 4);
				module->paramQuantities[TRACK_PARAM]->unit = trackLabel->text;
				module->paramQuantities[TRACK_PARAM]->unit.append(")");
				module->paramQuantities[TRACK_PARAM]->unit.insert(0, " (");
				module->updateTrackLabelRequest = 0;// all done pulling
			}

			if (oldSelectedTrack != trk) {// update controls when user selects another track
				module->params[TRACK_ACTIVE_PARAM].setValue(module->trackEqs[trk].getTrackActive() ? 1.0f : 0.0f);
				module->params[TRACK_GAIN_PARAM].setValue(module->trackEqs[trk].getTrackGain());
				for (int c = 0; c < 4; c++) {
					module->params[FREQ_ACTIVE_PARAMS + c].setValue(module->trackEqs[trk].getBandActive(c) >= 0.5f ? 1.0f : 0.0f);
					module->params[FREQ_PARAMS + c].setValue(module->trackEqs[trk].getFreq(c));
					module->params[GAIN_PARAMS + c].setValue(module->trackEqs[trk].getGain(c));
					module->params[Q_PARAMS + c].setValue(module->trackEqs[trk].getQ(c));
				}	
				module->params[LOW_PEAK_PARAM].setValue(module->trackEqs[trk].getLowPeak() ? 1.0f : 0.0f);
				module->params[HIGH_PEAK_PARAM].setValue(module->trackEqs[trk].getHighPeak() ? 1.0f : 0.0f);
				oldSelectedTrack = trk;
			}
			
			// Borders	
			int newSizeAdd = 0;
			if (module->expPresentRight) {
				newSizeAdd = 6;// should be +3 but already using +6 below, and needs to be different so no zoom bug
			}
			else if (module->expPresentLeft) {
				newSizeAdd = 3;
			}
			if (panelBorder->box.size.x != (box.size.x + newSizeAdd)) {
				panelBorder->box.pos.x = (newSizeAdd == 3 ? -3 : 0);
				panelBorder->box.size.x = (box.size.x + newSizeAdd);
				SvgPanel* svgPanel = (SvgPanel*)getPanel();
				svgPanel->fb->dirty = true;
			}
		}
		Widget::step();
	}
};


Model *modelEqMaster = createModel<EqMaster, EqMasterWidget>("EqMaster");
