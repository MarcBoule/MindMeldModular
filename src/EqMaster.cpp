//***********************************************************************************************
//EQ module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "EqWidgets.hpp"


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

	typedef ExpansionInterface Intf;
	
	// Expander
	float expMessages[2][Intf::MFE_NUM_VALUES] = {};// messages from eq-expander, see enum in EqCommon.hpp

	// Constants
	int numChannels16 = 16;// avoids warning that happens when hardcode 16 (static const or directly use 16 in code below)

	// Need to save, no reset
	int panelTheme;
	
	// Need to save, with reset
	int mappedId;// 0 means manual
	char trackLabels[24 * 4 + 1];// needs to be saved in case we are detached
	int8_t trackLabelColors[24];
	int8_t trackVuColors[24];
	TrackEq trackEqs[24];
	PackedBytes4 miscSettings;// cc4[0] is ShowBandCurvesEQ, cc4[1] is fft type (0 = off, 1 = pre, 2 = post, 3 = freeze), cc4[2] is momentaryCvButtons (1 = yes (original rising edge only version), 0 = level sensitive (emulated with rising and falling detection))
	PackedBytes4 showFreqAsNotes;
	
	// No need to save, with reset
	int updateTrackLabelRequest;// 0 when nothing to do, 1 for read names in widget
	VuMeterAllDual trackVu;
	int fftWriteHead;
	uint32_t cvConnected;

	// No need to save, no reset
	RefreshCounter refresh;
	PFFFT_Setup* ffts;// https://bitbucket.org/jpommier/pffft/src/default/test_pffft.c
	float* fftIn;
	float* fftOut;
	bool spectrumActive;// only for when input is unconnected
	bool globalEnable;
	TriggerRiseFall trackEnableCvTriggers[24];
	TriggerRiseFall trackBandCvTriggers[24][4];
	bool expPresentLeft = false;
	bool expPresentRight = false;
	
	int getSelectedTrack() {
		return (int)(params[TRACK_PARAM].getValue() + 0.5f);
	}

	void initTrackLabelsAndColors() {
		for (int trk = 0; trk < 16; trk++) {
			snprintf(&trackLabels[trk << 2], 5, "-%02i-", trk + 1);
		}
		for (int trk = 16; trk < 20; trk++) {
			snprintf(&trackLabels[trk << 2], 5, "GRP%1i", trk - 16 + 1);
		}
		for (int trk = 20; trk < 24; trk++) {
			snprintf(&trackLabels[trk << 2], 5, "AUX%1i", trk - 20 + 1);
		}
		for (int trk = 0; trk < 24; trk++) {
			trackLabelColors[trk] = 0;
			trackVuColors[trk] = 0;
		}
	}
	
		
	EqMaster() {
		config(NUM_EQ_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		rightExpander.producerMessage = expMessages[0];
		rightExpander.consumerMessage = expMessages[1];
		leftExpander.producerMessage = expMessages[0];
		leftExpander.consumerMessage = expMessages[1];

		// Track knob
		configParam(TRACK_PARAM, 0.0f, 23.0f, 0.0f, "Track", "", 0.0f, 1.0f, 1.0f);//min, max, default, label = "", unit = "", displayBase = 0.f, displayMultiplier = 1.f, displayOffset = 0.f
		
		// Track settings
		configParam(TRACK_ACTIVE_PARAM, 0.0f, 1.0f, DEFAULT_trackActive ? 1.0f : 0.0f, "Track EQ active");
		configParam(TRACK_GAIN_PARAM, -20.0f, 20.0f, DEFAULT_gain, "Track gain", " dB");
		configParam(FREQ_PARAMS + 0, MIN_freq[0], MAX_freq[0], DEFAULT_freq[0], "LF freq", " Hz");
		configParam(FREQ_PARAMS + 1, MIN_freq[1], MAX_freq[1], DEFAULT_freq[1], "LMF freq", " Hz");
		configParam(FREQ_PARAMS + 2, MIN_freq[2], MAX_freq[2], DEFAULT_freq[2], "HMF freq", " Hz");
		configParam(FREQ_PARAMS + 3, MIN_freq[3], MAX_freq[3], DEFAULT_freq[3], "HF freq", " Hz");
		for (int i = 0; i < 4; i++) {
			configParam(FREQ_ACTIVE_PARAMS + i, 0.0f, 1.0f, DEFAULT_bandActive, bandNames[i] + " active");
			configParam(GAIN_PARAMS + i, -20.0f, 20.0f, DEFAULT_gain, bandNames[i] + " gain", " dB");
			configParam(Q_PARAMS + i, 0.3f, 20.0f, DEFAULT_q[i], bandNames[i] + " Q");
		}
		configParam(LOW_PEAK_PARAM, 0.0f, 1.0f, DEFAULT_lowPeak ? 1.0f : 0.0f, "LF peak/shelf");
		configParam(HIGH_PEAK_PARAM, 0.0f, 1.0f, DEFAULT_highPeak ? 1.0f : 0.0f, "HF peak/shelf");
		configParam(GLOBAL_BYPASS_PARAM, 0.0f, 1.0f, 0.0f, "Global bypass");
		
		onReset();
		
		panelTheme = 0;
		ffts = pffft_new_setup(FFT_N, PFFFT_REAL);
		fftIn = (float*)pffft_aligned_malloc(FFT_N * 4);
		fftOut = (float*)pffft_aligned_malloc(FFT_N * 4);
		spectrumActive = false;
		globalEnable = params[GLOBAL_BYPASS_PARAM].getValue() < 0.5f;
	}
  
	~EqMaster() {
		pffft_destroy_setup(ffts);
		pffft_aligned_free(fftIn);
		pffft_aligned_free(fftOut);
	}
  
	void onReset() override {
		mappedId = 0;
		initTrackLabelsAndColors();
		for (int t = 0; t < 24; t++) {
			trackEqs[t].init(t, APP->engine->getSampleRate(), &cvConnected);
		}
		miscSettings.cc4[0] = 0x1;// show band curves by default
		miscSettings.cc4[1] = SPEC_POST;
		miscSettings.cc4[2] = 0x1; // momentary by default
		showFreqAsNotes.cc1 = 0;
		resetNonJson();
	}
	void resetNonJson() {
		updateTrackLabelRequest = 1;
		trackVu.reset();
		fftWriteHead = 0;
		cvConnected = 0;
	}


	void onRandomize() override {
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));
				
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
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

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
	

	void process(const ProcessArgs &args) override {
		int selectedTrack = getSelectedTrack();
		
		expPresentRight = (rightExpander.module && rightExpander.module->model == modelEqExpander);
		expPresentLeft = (leftExpander.module && leftExpander.module->model == modelEqExpander);
		
		
		//********** Inputs **********
		
		// From Eq-Expander
		if (expPresentRight || expPresentLeft) {
			float *messagesFromExpander = expPresentRight ? // use only when expander present
											(float*)rightExpander.consumerMessage :
											(float*)leftExpander.consumerMessage;
			
			// track band values
			int index6 = clamp((int)(messagesFromExpander[Intf::MFE_TRACK_CVS_INDEX6]), 0, 5);
			int cvConnectedSubset = (int)(messagesFromExpander[Intf::MFE_TRACK_CVS_CONNECTED]);
			for (int i = 0; i < 4; i++) {
				if ((cvConnectedSubset & (1 << i)) != 0) {
					int bandTrkIndex = (index6 << 2) + i;
					processTrackBandCvs(bandTrkIndex, selectedTrack, &messagesFromExpander[Intf::MFE_TRACK_CVS + 16 * i]);
				}
			}
			cvConnected &= ~(0xF << (index6 << 2));// clear all connected bits for current subset
			cvConnected |= (cvConnectedSubset << (index6 << 2));// set relevant connected bits for current subset
 
			// track enables, each track refreshed at fs / 24
			int enableTrkIndex = clamp((int)(messagesFromExpander[Intf::MFE_TRACK_ENABLE_INDEX]), 0, 23);
			processTrackEnableCvs(enableTrkIndex, selectedTrack, messagesFromExpander[Intf::MFE_TRACK_ENABLE]);
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
		globalEnable = params[GLOBAL_BYPASS_PARAM].getValue() < 0.5f;
		for (int i = 0; i < 3; i++) {
			if (inputs[SIG_INPUTS + i].isConnected()) {
				for (int t = 0; t < 8; t++) {
					float* in = inputs[SIG_INPUTS + i].getVoltages((t << 1) + 0);
					float out[2];
					trackEqs[(i << 3) + t].process(out, in, globalEnable);
					outputs[SIG_OUTPUTS + i].setVoltage(out[0], (t << 1) + 0);
					outputs[SIG_OUTPUTS + i].setVoltage(out[1], (t << 1) + 1);
					if ( ((i << 3) + t) == selectedTrack ) {
						// VU
						trackVu.process(args.sampleTime, out);
						vuProcessed = true;
						
						// Spectrum
						if ( (fftWriteHead < FFT_N) && (miscSettings.cc4[1] == SPEC_PRE || miscSettings.cc4[1] == SPEC_POST) ) {
							// write pre or post into fft's input buffer
							if (miscSettings.cc4[1] == SPEC_PRE) {
								fftIn[fftWriteHead] = (in[0] + in[1]);// no need to div by two, scaling done later
							}
							else {// SPEC_POST
								fftIn[fftWriteHead] = (out[0] + out[1]);// no need to div by two, scaling done later
							}
							// apply window
							if ((fftWriteHead & 0x3) == 0x3) {
								simd::float_4 p = {(float)(fftWriteHead - 3), (float)(fftWriteHead - 2), (float)(fftWriteHead - 1), (float)(fftWriteHead - 0)};
								p /= (float)(FFT_N - 1);
								simd::float_4 windowed = simd::float_4::load(&(fftIn[fftWriteHead & ~0x3]));
								windowed *= dsp::blackmanHarris<simd::float_4>(p);
								windowed.store(&(fftIn[fftWriteHead & ~0x3]));
							}
							fftWriteHead++;
						}
						else if (miscSettings.cc4[1] == SPEC_NONE || miscSettings.cc4[1] == SPEC_FREEZE) {
							fftWriteHead = 0;
						}
					}
				}
			}
		}
		if (!vuProcessed) {
			trackVu.reset();
		}
		spectrumActive = vuProcessed && miscSettings.cc4[1] != SPEC_NONE;
		
		
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
						bool newState = !trackEqs[bandTrkIndex].getBandActive(b);
						trackEqs[bandTrkIndex].setBandActive(b, newState);
						if (bandTrkIndex == selectedTrack) {
							params[FREQ_ACTIVE_PARAMS + b].setValue(newState ? 1.0f : 0.0f);
						}
					}
				}
				else {
					// gate level
					bool newState = cvs[(b << 2) + 0] > 0.5f;
					trackEqs[bandTrkIndex].setBandActive(b, newState);
					if (bandTrkIndex == selectedTrack) {
						params[FREQ_ACTIVE_PARAMS + b].setValue(newState ? 1.0f : 0.0f);
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
	
	void processTrackEnableCvs(int enableTrkIndex, int selectedTrack, float enableValue) {
		int state = trackEnableCvTriggers[enableTrkIndex].process(enableValue);
		if (state != 0) {
			if (miscSettings.cc4[2] == 1) {// if momentaryCvButtons
				if (state == 1) {// if rising edge
					// toggle
					bool newState = !trackEqs[enableTrkIndex].getTrackActive();
					trackEqs[enableTrkIndex].setTrackActive(newState);
					if (enableTrkIndex == selectedTrack) {
						params[TRACK_ACTIVE_PARAM].setValue(newState ? 1.0f : 0.0f);
					}
				}
			}
			else {
				// gate level
				bool newState = enableValue > 0.5f;
				trackEqs[enableTrkIndex].setTrackActive(newState);
				if (enableTrkIndex == selectedTrack) {
					params[TRACK_ACTIVE_PARAM].setValue(newState ? 1.0f : 0.0f);
				}
			}
		}
	}
};


//-----------------------------------------------------------------------------


struct EqMasterWidget : ModuleWidget {
	time_t oldTime = 0;
	int oldMappedId = 0;
	int oldSelectedTrack = -1;
	TrackLabel* trackLabel;
	int lastMovedKnobId = -1;
	time_t lastMovedKnobTime = 0;
	int8_t cloakedMode = 0;
	int8_t detailsShow = 0x7;
	simd::float_4 bandParamsWithCvs[3] = {};// [0] = freq, [1] = gain, [2] = q
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

		if (module->mappedId == 0) {
			DispColorEqItem *dispColItem = createMenuItem<DispColorEqItem>("Display colour", RIGHT_ARROW);
			dispColItem->srcColors = module->trackLabelColors;
			menu->addChild(dispColItem);
			
			VuColorEqItem *vuColItem = createMenuItem<VuColorEqItem>("VU colour", RIGHT_ARROW);
			vuColItem->srcColors = module->trackVuColors;
			menu->addChild(vuColItem);
		}
	}
	
	
	EqMasterWidget(EqMaster *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/EQmaster.svg")));
		panelBorder = findBorder(panel);
		
		
		// Left side controls and inputs
		// ------------------
		static const float leftX = 8.29f;
		// Track label
		addChild(trackLabel = createWidgetCentered<TrackLabel>(mm2px(Vec(leftX, 11.5f))));
		if (module) {
			trackLabel->trackLabelColorsSrc = module->trackLabelColors;
			trackLabel->trackLabelsSrc = module->trackLabels;
			trackLabel->trackParamSrc = &(module->params[TRACK_PARAM]);
			trackLabel->trackEqsSrc = module->trackEqs;
		}
		// Track knob
		TrackKnob* trackKnob;
		addParam(trackKnob = createDynamicParamCentered<TrackKnob>(mm2px(Vec(leftX, 22.7f)), module, TRACK_PARAM, module ? &module->panelTheme : NULL));
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
		// Bypass switch
		addParam(createDynamicParamCentered<DynBypassButton>(mm2px(Vec(leftX, 68.0f)), module, GLOBAL_BYPASS_PARAM, module ? &module->panelTheme : NULL));
		// Signal inputs
		static const float jackY = 84.35f;
		static const float jackDY = 12.8f;
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(leftX, jackY)), true, module, EqMaster::SIG_INPUTS + 0, module ? &module->panelTheme : NULL));		
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(leftX, jackY + jackDY)), true, module, EqMaster::SIG_INPUTS + 1, module ? &module->panelTheme : NULL));		
		addInput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(leftX, jackY + jackDY * 2)), true, module, EqMaster::SIG_INPUTS + 2, module ? &module->panelTheme : NULL));		
		
		
		// Center part
		// Screen - Spectrum settings buttons
		SpectrumSettingsButtons *bandsButton;
		addChild(bandsButton = createWidget<SpectrumSettingsButtons>(mm2px(Vec(18.0f, 9.5f))));
		ShowBandCurvesButtons *specButtons;
		addChild(specButtons = createWidget<ShowBandCurvesButtons>(mm2px(Vec(95.0f, 9.5f))));
		if (module) {
			specButtons->settingSrc = &module->miscSettings.cc4[0];
			bandsButton->settingSrc = &module->miscSettings.cc4[1];
		}
		// Screen - EQ curve and grid
		EqCurveAndGrid* eqCurveAndGrid;
		addChild(eqCurveAndGrid = createWidgetCentered<EqCurveAndGrid>(mm2px(Vec(71.12f, 45.47f - 0.5f))));
		if (module) {
			eqCurveAndGrid->trackParamSrc = &(module->params[TRACK_PARAM]);
			eqCurveAndGrid->trackEqsSrc = module->trackEqs;
			eqCurveAndGrid->miscSettingsSrc = &(module->miscSettings);
			eqCurveAndGrid->fftWriteHeadSrc = &(module->fftWriteHead);
			eqCurveAndGrid->ffts = module->ffts;
			eqCurveAndGrid->fftIn = module->fftIn;
			eqCurveAndGrid->fftOut = module->fftOut;
			eqCurveAndGrid->spectrumActiveSrc = &(module->spectrumActive);
			eqCurveAndGrid->globalEnableSrc = &(module->globalEnable);
			eqCurveAndGrid->bandParamsWithCvs = bandParamsWithCvs;
		}
		
		// Screen - Big Numbers
		BigNumbers* bigNumbers;
		addChild(bigNumbers = createWidgetCentered<BigNumbers>(mm2px(Vec(71.12f, 68.0f))));
		if (module) {
			bigNumbers->trackParamSrc = &(module->params[TRACK_PARAM]);
			bigNumbers->trackEqsSrc = module->trackEqs;
			bigNumbers->lastMovedKnobIdSrc = & lastMovedKnobId;
			bigNumbers->lastMovedKnobTimeSrc = &lastMovedKnobTime;
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
				bandSwitches[b]->trackEqsSrc = module->trackEqs;
			}
		}	
		
		// Freq, gain and q knobs
		BandKnob* bandKnobs[12];
		// freq
		addParam(bandKnobs[0] = createDynamicParamCentered<EqFreqKnob<0>>(mm2px(Vec(ctrlX + ctrlDX * 0, 91.2f)), module, FREQ_PARAMS + 0, module ? &module->panelTheme : NULL));
		addParam(bandKnobs[1] = createDynamicParamCentered<EqFreqKnob<1>>(mm2px(Vec(ctrlX + ctrlDX * 1, 91.2f)), module, FREQ_PARAMS + 1, module ? &module->panelTheme : NULL));
		addParam(bandKnobs[2] = createDynamicParamCentered<EqFreqKnob<2>>(mm2px(Vec(ctrlX + ctrlDX * 2, 91.2f)), module, FREQ_PARAMS + 2, module ? &module->panelTheme : NULL));
		addParam(bandKnobs[3] = createDynamicParamCentered<EqFreqKnob<3>>(mm2px(Vec(ctrlX + ctrlDX * 3, 91.2f)), module, FREQ_PARAMS + 3, module ? &module->panelTheme : NULL));
		// gain
		addParam(bandKnobs[0 + 4] = createDynamicParamCentered<EqGainKnob<0>>(mm2px(Vec(ctrlX + ctrlDX * 0 + 11.04f, 101.78f)), module, GAIN_PARAMS + 0, module ? &module->panelTheme : NULL));
		addParam(bandKnobs[1 + 4] = createDynamicParamCentered<EqGainKnob<1>>(mm2px(Vec(ctrlX + ctrlDX * 1 + 11.04f, 101.78f)), module, GAIN_PARAMS + 1, module ? &module->panelTheme : NULL));
		addParam(bandKnobs[2 + 4] = createDynamicParamCentered<EqGainKnob<2>>(mm2px(Vec(ctrlX + ctrlDX * 2 + 11.04f, 101.78f)), module, GAIN_PARAMS + 2, module ? &module->panelTheme : NULL));
		addParam(bandKnobs[3 + 4] = createDynamicParamCentered<EqGainKnob<3>>(mm2px(Vec(ctrlX + ctrlDX * 3 + 11.04f, 101.78f)), module, GAIN_PARAMS + 3, module ? &module->panelTheme : NULL));
		// q
		addParam(bandKnobs[0 + 8] = createDynamicParamCentered<EqQKnob<0>>(mm2px(Vec(ctrlX + ctrlDX * 0, 112.37f)), module, Q_PARAMS + 0, module ? &module->panelTheme : NULL));
		addParam(bandKnobs[1 + 8] = createDynamicParamCentered<EqQKnob<1>>(mm2px(Vec(ctrlX + ctrlDX * 1, 112.37f)), module, Q_PARAMS + 1, module ? &module->panelTheme : NULL));
		addParam(bandKnobs[2 + 8] = createDynamicParamCentered<EqQKnob<2>>(mm2px(Vec(ctrlX + ctrlDX * 2, 112.37f)), module, Q_PARAMS + 2, module ? &module->panelTheme : NULL));
		addParam(bandKnobs[3 + 8] = createDynamicParamCentered<EqQKnob<3>>(mm2px(Vec(ctrlX + ctrlDX * 3, 112.37f)), module, Q_PARAMS + 3, module ? &module->panelTheme : NULL));
		//
		if (module) {
			for (int c = 0; c < 12; c++) {
				bandKnobs[c]->paramWithCV = &(bandParamsWithCvs[c >> 2][c & 0x3]);
				bandKnobs[c]->cloakedModeSrc = &cloakedMode;
				bandKnobs[c]->detailsShowSrc = &detailsShow;
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
				bandLabels[i]->trackLabelColorsSrc = module->trackLabelColors;
				bandLabels[i]->trackLabelsSrc = module->trackLabels;
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
			newVU->srcLevels = &(module->trackVu);
			newVU->trackVuColorsSrc = module->trackVuColors;
			newVU->trackParamSrc = &(module->params[TRACK_PARAM]);
			addChild(newVU);
		}
		// Gain knob
		TrackGainKnob* trackGainKnob;
		addParam(trackGainKnob = createDynamicParamCentered<TrackGainKnob>(mm2px(Vec(rightX, 67.0f)), module, TRACK_GAIN_PARAM, module ? &module->panelTheme : NULL));
		if (module) {
			trackGainKnob->trackParamSrc = &(module->params[TRACK_PARAM]);
			trackGainKnob->trackEqsSrc = module->trackEqs;
		}		
		// Signal outputs
		addOutput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(rightX, jackY)), false, module, EqMaster::SIG_OUTPUTS + 0, module ? &module->panelTheme : NULL));		
		addOutput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(rightX, jackY + jackDY)), false, module, EqMaster::SIG_OUTPUTS + 1, module ? &module->panelTheme : NULL));		
		addOutput(createDynamicPortCentered<DynPortGold>(mm2px(Vec(rightX, jackY + jackDY * 2)), false, module, EqMaster::SIG_OUTPUTS + 2, module ? &module->panelTheme : NULL));		
		
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
				trackLabel->text = std::string(&(module->trackLabels[trk * 4]), 4);
				module->updateTrackLabelRequest = 0;// all done pulling
			}

			if (oldSelectedTrack != trk) {// update controls when user selects another track
				module->params[TRACK_ACTIVE_PARAM].setValue(module->trackEqs[trk].getTrackActive() ? 1.0f : 0.0f);
				module->params[TRACK_GAIN_PARAM].setValue(module->trackEqs[trk].getTrackGain());
				for (int c = 0; c < 4; c++) {
					module->params[FREQ_ACTIVE_PARAMS + c].setValue(module->trackEqs[trk].getBandActive(c) ? 1.0f : 0.0f);
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
				((SvgPanel*)panel)->dirty = true;
			}
		}
		Widget::step();
	}
	
	void draw(const DrawArgs &args) override {
		if (module) {
			EqMaster* module = (EqMaster*)(this->module);
			int trk = module->getSelectedTrack();	
			// prepare values with cvs for draw methods (knob arcs, eq curve)
			bandParamsWithCvs[0] = module->trackEqs[trk].getFreqWithCvVec();
			bandParamsWithCvs[1] = module->trackEqs[trk].getGainWithCvVec();
			bandParamsWithCvs[2] = module->trackEqs[trk].getQWithCvVec();
		}
		ModuleWidget::draw(args);
	}
};


Model *modelEqMaster = createModel<EqMaster, EqMasterWidget>("EqMaster");
