//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#ifndef MMM_EQWIDGETS_HPP
#define MMM_EQWIDGETS_HPP

#include "EqMenus.hpp"
#include "dsp/fft.hpp"
#include <condition_variable>


// Labels
// --------------------

struct TrackLabel : LedDisplayChoice {
	int8_t *trackLabelColorsSrc = NULL;
	int8_t *bandLabelColorsSrc;
	int *mappedId;
	char *trackLabelsSrc;
	Param *trackParamSrc;
	TrackEq *trackEqsSrc;
	int* updateTrackLabelRequestSrc = NULL;

	TrackLabel() {
		box.size = mm2px(Vec(10.6f, 5.0f));
		textOffset = Vec(4.2f, 11.3f);
		text = "-00-";
	};
	
	void draw(const DrawArgs &args) override {
		if (trackLabelColorsSrc) {
			if (*mappedId == 0) {
				color = DISP_COLORS[*bandLabelColorsSrc];
			}
			else {
				int currTrk = (int)(trackParamSrc->getValue() + 0.5f);
				color = DISP_COLORS[trackLabelColorsSrc[currTrk]];
			}
		}	
		LedDisplayChoice::draw(args);
	}
	
	struct TrackSelectItem : MenuItem {
		Param* trackParamSrc;
		int trackNumber;
		void onAction(const event::Action &e) override {
			trackParamSrc->setValue((float)trackNumber);
		}
	};
	
	void onButton(const event::Button &e) override {
		if (e.button == GLFW_MOUSE_BUTTON_RIGHT && e.action == GLFW_PRESS) {
			ui::Menu *menu = createMenu();
			
			int currTrk = (int)(trackParamSrc->getValue() + 0.5f);
			
			InitializeEqTrackItem<TrackEq> *initTrackItem = createMenuItem<InitializeEqTrackItem<TrackEq>>("Initialize track settings", "");
			initTrackItem->srcTrack = &trackEqsSrc[currTrk];
			initTrackItem->updateTrackLabelRequestSrc = updateTrackLabelRequestSrc;
			menu->addChild(initTrackItem);			

			CopyTrackSettingsItem *copyItem = createMenuItem<CopyTrackSettingsItem>("Copy track settings to:", RIGHT_ARROW);
			copyItem->trackLabelsSrc = trackLabelsSrc;
			copyItem->trackEqsSrc = trackEqsSrc;
			copyItem->trackNumSrc = currTrk;
			menu->addChild(copyItem);


			MenuLabel *trkSelLabel = new MenuLabel();
			trkSelLabel->text = "Select Track: ";
			menu->addChild(trkSelLabel);
			
			for (int trk = 0; trk < 24; trk++) {
				bool onSource = (trk == currTrk);
				TrackSelectItem *tsItem = createMenuItem<TrackSelectItem>(std::string(&(trackLabelsSrc[trk * 4]), 4), CHECKMARK(onSource));
				tsItem->trackParamSrc = trackParamSrc;
				tsItem->trackNumber = trk;
				tsItem->disabled = onSource;
				menu->addChild(tsItem);
			}
			
			e.consume(this);
			return;
		}
		LedDisplayChoice::onButton(e);		
	}	
};


struct BandLabelBase : widget::OpaqueWidget {
	// This struct is adapted from Rack's LedDisplayChoice in app/LedDisplay.{c,h}pp

	// user must set up
	int8_t* bandLabelColorsSrc = NULL;
	Param* trackParamSrc = NULL;
	TrackEq *trackEqsSrc;
	int band;
	int8_t *showFreqAsNotesSrc;// used by freq labels only (derived class)
	
	// local
	std::string text;
	std::shared_ptr<Font> font;
	math::Vec textOffset;
	NVGcolor color;
	
	
	BandLabelBase() {
		box.size = mm2px(Vec(10.6f, 5.0f));
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/RobotoCondensed-Regular.ttf"));
		color = DISP_COLORS[0];
		textOffset = Vec(4.2f, 11.3f);
		text = "---";
	};
	
	virtual void prepareText() {}
	
	void draw(const DrawArgs &args) override {
		prepareText();
		
		if (bandLabelColorsSrc) {
			color = DISP_COLORS[*bandLabelColorsSrc];
		}	

		nvgScissor(args.vg, RECT_ARGS(args.clipBox));
		if (font->handle >= 0) {
			nvgFillColor(args.vg, color);
			nvgFontFaceId(args.vg, font->handle);
			nvgTextLetterSpacing(args.vg, 0.0);

			nvgFontSize(args.vg, 10.5f);
			nvgText(args.vg, textOffset.x, textOffset.y, text.c_str(), NULL);
		}
		nvgResetScissor(args.vg);		
	}
	
	void onButton(const event::Button& e) override {
		OpaqueWidget::onButton(e);

		if (e.action == GLFW_PRESS && (e.button == GLFW_MOUSE_BUTTON_LEFT || e.button == GLFW_MOUSE_BUTTON_RIGHT)) {
			event::Action eAction;
			onAction(eAction);
			e.consume(this);
		}	
	}
};

struct BandLabelFreq : BandLabelBase {
	void prepareText() override {
		if (trackParamSrc) {
			int trk = (int)(trackParamSrc->getValue() + 0.5f);
			float freq = std::pow(10, trackEqsSrc[trk].getFreq(band));
			if (*showFreqAsNotesSrc == 0) {			
				if (freq < 10000.0f) {
					text = string::f("%i", (int)(freq + 0.5f));
				}
				else {
					freq /= 1000.0f;
					text = string::f("%.2fk", freq);
				}
			}
			else {
				char noteBuf[6];
				float cvVal = std::log2(freq / dsp::FREQ_C4);
				printNote(cvVal, noteBuf, true);
				text = noteBuf;
			}
		}
	}
		
	void onButton(const event::Button& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT) {
			ui::Menu *menu = createMenu();

			// cv level slider
			int trk = (int)(trackParamSrc->getValue() + 0.5f);
			CvLevelSlider *cvLevSlider = new CvLevelSlider(&(trackEqsSrc[trk].freqCvAtten[band]));
			cvLevSlider->box.size.x = 200.0f;
			menu->addChild(cvLevSlider);

			// show notes checkmark
			ShowNotesItem *showNotesItem = createMenuItem<ShowNotesItem>("Show freq as note", CHECKMARK(*showFreqAsNotesSrc != 0));
			showNotesItem->showFreqAsNotesSrc = showFreqAsNotesSrc;
			menu->addChild(showNotesItem);

			event::Action eAction;
			onAction(eAction);
			e.consume(this);
		}	
		else {
			BandLabelBase::onButton(e);
		}
	}
};

struct BandLabelGain : BandLabelBase {
	void prepareText() override {
		if (trackParamSrc) {
			int trk = (int)(trackParamSrc->getValue() + 0.5f);
			float gain = trackEqsSrc[trk].getGain(band);
			if (std::fabs(gain) < 10.0f) {
				text = string::f("%.2f", math::normalizeZero(gain));
			}
			else {
				text = string::f("%.1f", math::normalizeZero(gain));
			}
		}
	}
	
	void onButton(const event::Button& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT) {
			ui::Menu *menu = createMenu();

			// cv level slider
			int trk = (int)(trackParamSrc->getValue() + 0.5f);
			CvLevelSlider *cvLevSlider = new CvLevelSlider(&(trackEqsSrc[trk].gainCvAtten[band]));
			cvLevSlider->box.size.x = 200.0f;
			menu->addChild(cvLevSlider);

			event::Action eAction;
			onAction(eAction);
			e.consume(this);
		}	
		else {
			BandLabelBase::onButton(e);
		}
	}
};

struct BandLabelQ : BandLabelBase {
	void prepareText() override {
		if (trackParamSrc) {
			int trk = (int)(trackParamSrc->getValue() + 0.5f);
			float q = trackEqsSrc[trk].getQ(band);
			text = string::f("%.2f", math::normalizeZero(q));
		}
	}
	
	void onButton(const event::Button& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT) {
			ui::Menu *menu = createMenu();

			// cv level slider
			int trk = (int)(trackParamSrc->getValue() + 0.5f);
			CvLevelSlider *cvLevSlider = new CvLevelSlider(&(trackEqsSrc[trk].qCvAtten[band]));
			cvLevSlider->box.size.x = 200.0f;
			menu->addChild(cvLevSlider);

			event::Action eAction;
			onAction(eAction);
			e.consume(this);
		}	
		else {
			BandLabelBase::onButton(e);
		}
	}};



// Displays
// --------------------


struct SpectrumSettingsButtons : OpaqueWidget {
	const float textWidths[5] = {45.0f, 21.0f, 21.0f, 26.0f, 32.0f};
	const std::string textStrings[5] = {"ANALYSER:", "OFF", "PRE", "POST", "FREEZE"};
	
	// user must set up
	int8_t *settingSrc = NULL;
	
	// local
	std::shared_ptr<Font> font;
	NVGcolor colorOff;
	NVGcolor colorOn;
	
	
	SpectrumSettingsButtons() {
		box.size = mm2px(Vec(textWidths[0] + textWidths[1] + textWidths[2] + textWidths[3] + textWidths[4], 5.0f));
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/RobotoCondensed-Regular.ttf"));
		colorOff = SCHEME_GRAY;
		colorOn = SCHEME_YELLOW;
	}
	
	void draw(const DrawArgs &args) override {
		if (font->handle >= 0) {
			// nvgStrokeColor(args.vg, SCHEME_YELLOW);
			// nvgStrokeWidth(args.vg, 0.7f);
			
			nvgFontFaceId(args.vg, font->handle);
			nvgTextLetterSpacing(args.vg, 0.0);
			nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
			nvgFontSize(args.vg, 10.0f);
			

			bool specOn = (settingSrc && *settingSrc & SPEC_MASK_ON) != 0;
			bool specPost = (settingSrc && *settingSrc & SPEC_MASK_POST) != 0;
			bool specFreeze = (settingSrc && *settingSrc & SPEC_MASK_FREEZE) != 0;
			
			// ANALYSER
			float posx = 0.0f;
			nvgFillColor(args.vg, SCHEME_LIGHT_GRAY);
			nvgText(args.vg, posx + 3.0f, box.size.y / 2.0f, textStrings[0].c_str(), NULL);
			posx += textWidths[0];
			
			// OFF
			nvgFillColor(args.vg, (!specOn) ? colorOn : colorOff);
			nvgText(args.vg, posx + 3.0f, box.size.y / 2.0f, textStrings[1].c_str(), NULL);
			posx += textWidths[1];
			
			// PRE
			nvgFillColor(args.vg, (specOn && !specFreeze && !specPost) ? colorOn : colorOff);
			nvgText(args.vg, posx + 3.0f, box.size.y / 2.0f, textStrings[2].c_str(), NULL);
			posx += textWidths[2];
			
			// POST
			nvgFillColor(args.vg, (specOn && !specFreeze && specPost) ? colorOn : colorOff);
			nvgText(args.vg, posx + 3.0f, box.size.y / 2.0f, textStrings[3].c_str(), NULL);
			posx += textWidths[3];
			
			// FREEZE
			nvgFillColor(args.vg, (specOn && specFreeze) ? colorOn : colorOff);
			nvgText(args.vg, posx + 3.0f, box.size.y / 2.0f, textStrings[4].c_str(), NULL);
		}
	}
	
	void onButton(const event::Button& e) override {
		if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS) {
			float leftX = textWidths[0];
			// click OFF
			if (e.pos.x > leftX && e.pos.x < leftX + textWidths[1]) {
				*settingSrc &= ~(SPEC_MASK_ON | SPEC_MASK_FREEZE);// clear freeze and on/off bits, keep pre/post bit unchanged
			}
			leftX += textWidths[1];
			// click PRE
			if (e.pos.x > leftX && e.pos.x < leftX + textWidths[2]) {
				*settingSrc = (SPEC_MASK_ON);// set on/off bit, clear others
			}
			leftX += textWidths[2];
			// click POST
			if (e.pos.x > leftX && e.pos.x < leftX + textWidths[3]) {
				*settingSrc = (SPEC_MASK_ON | SPEC_MASK_POST);// set on/off and pre/post bits, clear freeze
			}
			leftX += textWidths[3];
			// click FREEZE
			if (e.pos.x > leftX && e.pos.x < leftX + textWidths[4]) {
				*settingSrc |= SPEC_MASK_ON;// set on/off bit and keep pre/post bit unchanged
				*settingSrc ^= SPEC_MASK_FREEZE;// toggle freeze bit
			}
		}
		OpaqueWidget::onButton(e);
	}
};



struct ShowBandCurvesButtons : OpaqueWidget {
	const float textWidths[3] = {33.0f, 24.0f, 29.0f};
	const std::string textStrings[3] = {"BANDS:", "HIDE", "SHOW"};
	
	// user must set up
	int8_t *settingSrc = NULL;
	
	// local
	std::shared_ptr<Font> font;
	NVGcolor colorOff;
	NVGcolor colorOn;
	
	
	ShowBandCurvesButtons() {
		box.size = mm2px(Vec(textWidths[0] + textWidths[1] + textWidths[2], 5.0f));
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/RobotoCondensed-Regular.ttf"));
		colorOff = SCHEME_GRAY;
		colorOn = SCHEME_YELLOW;
	}
	
	void draw(const DrawArgs &args) override {
		if (font->handle >= 0) {
			// nvgStrokeColor(args.vg, SCHEME_YELLOW);
			// nvgStrokeWidth(args.vg, 0.7f);
			
			nvgFontFaceId(args.vg, font->handle);
			nvgTextLetterSpacing(args.vg, 0.0);
			nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
			nvgFontSize(args.vg, 10.0f);
			
			float posx = 0.0f;
			for (int l = 0; l < 3; l++) {
				if (l == 0) {
					nvgFillColor(args.vg, SCHEME_LIGHT_GRAY);
				}
				else if (settingSrc != NULL && (*settingSrc == l - 1)) {
					nvgFillColor(args.vg, colorOn);
				}
				else {
					nvgFillColor(args.vg, colorOff);
				}
				nvgText(args.vg, posx + 3.0f, box.size.y / 2.0f, textStrings[l].c_str(), NULL);
				// nvgBeginPath(args.vg);
				// nvgRect(args.vg, posx, 0.0f, textWidths[l], box.size.y);
				// nvgStroke(args.vg);	
				posx += textWidths[l];
			}
		}
	}
	
	void onButton(const event::Button& e) override {
		if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS) {
			if (e.pos.x > textWidths[0]) {
				*settingSrc ^= 0x1;
			}
		}
		OpaqueWidget::onButton(e);
	}
};


struct BigNumbers : TransparentWidget {
	// user must set up
	Param* trackParamSrc = NULL;
	TrackEq* trackEqsSrc;
	int* lastMovedKnobIdSrc;
	time_t* lastMovedKnobTimeSrc;
	
	
	// local
	std::shared_ptr<Font> font;
	NVGcolor color;
	std::string text;
	math::Vec textOffset;
	
	
	BigNumbers() {
		box.size = mm2px(Vec(40.0f, 15.0f));
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/RobotoCondensed-Regular.ttf"));
		color = SCHEME_LIGHT_GRAY;
		textOffset = Vec(box.size.div(2.0f));
	}
		
	void draw(const DrawArgs &args) override {
		if (trackParamSrc != NULL) {
			time_t currTime = time(0);
			if (currTime - *lastMovedKnobTimeSrc < 4) {
				text = "";
				int srcId = *lastMovedKnobIdSrc;
				int currTrk = (int)(trackParamSrc->getValue() + 0.5f);
				if (srcId >= FREQ_PARAMS && srcId < FREQ_PARAMS + 4) {
					float freq = std::pow(10, trackEqsSrc[currTrk].getFreq(srcId - FREQ_PARAMS));
					if (freq < 10000.0f) {
						text = string::f("%i Hz", (int)(freq + 0.5f));
					}
					else {
						text = string::f("%.2f kHz", freq / 1000.0f);
					}
				}				
				else if (srcId >= GAIN_PARAMS && srcId < GAIN_PARAMS + 4) {
					float gain = trackEqsSrc[currTrk].getGain(srcId - GAIN_PARAMS);
					if (std::fabs(gain) < 10.0f) {
						text = string::f("%.2f dB", math::normalizeZero(gain));
					}
					else {
						text = string::f("%.1f dB", math::normalizeZero(gain));
					}
				}
				else if (srcId >= Q_PARAMS && srcId < Q_PARAMS + 4) {
					int band = srcId - Q_PARAMS;
					float q = trackEqsSrc[currTrk].getQ(band);
					text = string::f("%.2f", math::normalizeZero(q));
				}

			
				if (font->handle >= 0 && text.compare("") != 0) {
					nvgFillColor(args.vg, color);
					nvgFontFaceId(args.vg, font->handle);
					nvgTextLetterSpacing(args.vg, 0.0);
					nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
					nvgFontSize(args.vg, 24.0f);
					nvgText(args.vg, textOffset.x, textOffset.y, text.c_str(), NULL);
				}
			}
		}
	}
};


struct EqCurveAndGrid : TransparentWidget {
	static constexpr float minDb = -20.0f;
	static constexpr float maxDb = 20.0f;
	static const int numDrawSteps = 200;
	float stepLogFreqs[numDrawSteps + 4 + 1];// 4 for cursors, 1 since will loop with "<= numDrawSteps"
	simd::float_4 stepDbs[numDrawSteps + 4 + 1];// 4 for cursors, 1 since will loop with "<= numDrawSteps"
	
	// user must set up
	Param *trackParamSrc = NULL;
	TrackEq *trackEqsSrc;
	PackedBytes4 *miscSettingsSrc;	
	PackedBytes4 *miscSettings2Src;	
	Param *globalBypassParamSrc;
	simd::float_4 *bandParamsWithCvs;// [0] = freq, [1] = gain, [2] = q
	bool *bandParamsCvConnected;
	float *drawBuf;// store log magnitude only in first half, log freq in second half
	int *drawBufSize;
	int* lastMovedKnobIdSrc;
	time_t* lastMovedKnobTimeSrc;
	
	// internal
	QuattroBiQuadCoeff drawEq;
	std::shared_ptr<Font> font;
	float sampleRate;// use only in scope of it being set in draw()
	int currTrk;// use only in scope of it being set in draw()
		
	
	EqCurveAndGrid() {
		box.size = Vec(eqCurveWidth, mm2px(60.605f));	
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/RobotoCondensed-Regular.ttf"));
	}
	
	
	void draw(const DrawArgs &args) override {
		nvgSave(args.vg);
		
		// grid
		drawGrid(args);
		
		if (trackParamSrc != NULL) {
			currTrk = (int)(trackParamSrc->getValue() + 0.5f);
			sampleRate = trackEqsSrc[0].getSampleRate();
		
			nvgScissor(args.vg, RECT_ARGS(args.clipBox));
			
			// spectrum
			if (*drawBufSize > 0) {
				drawSpectrum(args);
			}

			bool hideEqCurves = miscSettings2Src->cc4[2] != 0 && (!trackEqsSrc[currTrk].getTrackActive() || globalBypassParamSrc->getValue() >= 0.5f);

			drawGridtext(args, hideEqCurves);
			
			// EQ curves
			if (!hideEqCurves) {
				calcCurveData();
				drawAllEqCurves(args);
			}
			
			nvgResetScissor(args.vg);					
		}
		
		nvgRestore(args.vg);
	}
	
	
	// grid lines
	void vertLineAtFreq(const DrawArgs &args, float freq) {
		float logFreq = std::log10(freq);
		float lineX = math::rescale(logFreq, minLogFreq, maxLogFreq, 0.0f, box.size.x);
		nvgMoveTo(args.vg, lineX, 0.0f);
		nvgLineTo(args.vg, lineX, box.size.y);
	}
	void horzLineAtDb(const DrawArgs &args, float dB) {
		float lineY = math::rescale(dB, minDb, maxDb, box.size.y, 0.0f);
		nvgMoveTo(args.vg, 0.0f, lineY);
		nvgLineTo(args.vg, box.size.x, lineY);
	}
	void drawGrid(const DrawArgs &args) {
		NVGcolor lineCol = nvgRGB(0x37, 0x37, 0x37);
		NVGcolor screenCol = nvgRGB(38, 38, 38);
		nvgStrokeColor(args.vg, lineCol);
		nvgStrokeWidth(args.vg, 0.7f);
		
		// vertical lines
		NVGpaint grad = nvgLinearGradient(args.vg, 0.0f, box.size.y * 34.0f / 40.0f, 0.0f, box.size.y, lineCol, screenCol);
		nvgBeginPath(args.vg);
		vertLineAtFreq(args, 30.0f);
		vertLineAtFreq(args, 40.0f);
		vertLineAtFreq(args, 50.0f);
		for (int i = 1; i <= 5; i++) {
			vertLineAtFreq(args, 100.0f * (float)i);
			vertLineAtFreq(args, 1000.0f * (float)i);
		}
		vertLineAtFreq(args, 10000.0f);
		vertLineAtFreq(args, 20000.0f);
		nvgStrokePaint(args.vg, grad);
		nvgStroke(args.vg);
		
		// horizontal lines
		nvgBeginPath(args.vg);
		horzLineAtDb(args, 20.0f);
		horzLineAtDb(args, 12.0f);
		horzLineAtDb(args, 6.0f);
		horzLineAtDb(args, 0.0f);
		horzLineAtDb(args, -6.0f);
		horzLineAtDb(args, -12.0f);
		//nvgRect(args.vg, 0.0f, 0.0f, box.size.x, box.size.y);
		nvgStroke(args.vg);	
	}
	
	
	// text labels in grid lines
	void textAtFreqAndDb(const DrawArgs &args, float freq, float dB, std::string text) {
		float logFreq = std::log10(freq);
		float textX = math::rescale(logFreq, minLogFreq, maxLogFreq, 0.0f, box.size.x);
		float textY = math::rescale(dB, minDb, maxDb, box.size.y, 0.0f);
		nvgText(args.vg, textX, textY - 3.0f, text.c_str(), NULL);
	}
	void drawGridtext(const DrawArgs &args, bool hideDb) {
		// text labels
		if (font->handle >= 0) {
			nvgFillColor(args.vg, nvgRGB(0x97, 0x97, 0x97));
			nvgFontFaceId(args.vg, font->handle);
			nvgTextLetterSpacing(args.vg, 0.0);
			nvgFontSize(args.vg, 9.0f);
			nvgTextAlign(args.vg, NVG_ALIGN_CENTER);
			// frequency
			textAtFreqAndDb(args, 50.0f, -20.0f, "50");
			textAtFreqAndDb(args, 100.0f, -20.0f, "100");
			textAtFreqAndDb(args, 500.0f, -20.0f, "500");
			textAtFreqAndDb(args, 1000.0f, -20.0f, "1k");
			textAtFreqAndDb(args, 5000.0f, -20.0f, "5k");
			textAtFreqAndDb(args, 10000.0f, -20.0f, "10k");
			// dB
			if (!hideDb) {
				nvgTextAlign(args.vg, NVG_ALIGN_LEFT);
				textAtFreqAndDb(args, 22.0f, -12.0f, "-12");
				textAtFreqAndDb(args, 22.0f, -6.0f, "-6");
				textAtFreqAndDb(args, 22.0f, 0.0f, "0 dB");
				textAtFreqAndDb(args, 22.0f, 6.0f, "+6");
				textAtFreqAndDb(args, 22.0f, 12.0f, "+12");
			}
		}
	}
	
	
	// spectrum
	void drawSpectrum(const DrawArgs &args) {
		nvgLineCap(args.vg, NVG_ROUND);
		nvgMiterLimit(args.vg, 1.0f);
		NVGcolor fillcolTop = SCHEME_LIGHT_GRAY;
		NVGcolor fillcolBot = SCHEME_LIGHT_GRAY;
		fillcolTop.a = 0.25f;
		fillcolBot.a = 0.1f;
		nvgFillColor(args.vg, fillcolTop);
		nvgStrokeColor(args.vg, nvgRGB(99, 99, 99));
		nvgStrokeWidth(args.vg, 0.5f);

		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, -1.0f, box.size.y + 3.0f);// + 3.0f for proper enclosed region for fill, -1.0f is a hack to not show the side stroke
		float specX = 0.0f;
		float specY = 0.0f;
		for (int x = 1; x < *drawBufSize; x++) {	
			float ampl = drawBuf[x];
			specX = drawBuf[x + FFT_N_2];
			specY = ampl;
			if (x == 1) {
				nvgLineTo(args.vg, -1.0f, box.size.y - specY );// cheat with a specX of 0 since the first freq is just above 20Hz when FFT_N = 2048, bring to -1.0f though as a hack to not show the side stroke
			}
			else {
				nvgLineTo(args.vg, specX, box.size.y - specY );
			}
		}
		nvgLineTo(args.vg, specX + 1.0f, box.size.y - specY );// +1.0f is a hack to not show the side stroke
		nvgLineTo(args.vg, specX + 1.0f, box.size.y + 3.0f );// +3.0f for proper enclosed region for fill, +1.0f is a hack to not show the side stroke
		nvgClosePath(args.vg);
		
		NVGpaint grad = nvgLinearGradient(args.vg, 0.0f, box.size.y / 2.3f, 0.0f, box.size.y, fillcolTop, fillcolBot);
		nvgFillPaint(args.vg, grad);
		nvgFill(args.vg);
		nvgStroke(args.vg);
	}

	
	// eq curves
	void calcCurveData() {
		// contract: populate stepLogFreqs[], stepDbs[]

		// prepare values with cvs for draw methods (knob arcs, eq curve)
		bool _cvConnected = trackEqsSrc[currTrk].getCvConnected();
		bandParamsWithCvs[0] = trackEqsSrc[currTrk].getFreqWithCvVec(_cvConnected);
		bandParamsWithCvs[1] = trackEqsSrc[currTrk].getGainWithCvVec(_cvConnected);
		bandParamsWithCvs[2] = trackEqsSrc[currTrk].getQWithCvVec(_cvConnected);
		*bandParamsCvConnected = _cvConnected;

		// set eqCoefficients of separate drawEq according to active track and get cursor points of each band		
		simd::float_4 logFreqCursors = sortFloat4(bandParamsWithCvs[0]);
		simd::float_4 normalizedFreq = simd::fmin(0.5f, simd::pow(10.0f, bandParamsWithCvs[0]) / sampleRate);
		for (int b = 0; b < 4; b++) {
			float linearGain = (trackEqsSrc[currTrk].getBandActive(b) >= 0.5f) ? std::pow(10.0f, bandParamsWithCvs[1][b] / 20.0f) : 1.0f;
			drawEq.setParameters(b, trackEqsSrc[currTrk].getBandType(b), normalizedFreq[b], linearGain, bandParamsWithCvs[2][b]);
		}
		
		// fill freq response curve data
		float delLogX = (maxLogFreq - minLogFreq) / ((float)numDrawSteps);
		int c = 0;// index into logFreqCursors (which are sorted)
		for (int x = 0, i = 0; x <= numDrawSteps; x++, i++) {
			float logFreqX = minLogFreq + delLogX * (float)x;
			if ( (c < 4) && (logFreqCursors[c] < logFreqX) ) {
				stepLogFreqs[i] = logFreqCursors[c];
				c++;
				x--;
			}
			else {
				stepLogFreqs[i] = logFreqX;
			}
			stepDbs[i] = drawEq.getFrequencyResponse(std::pow(10.0f, stepLogFreqs[i]) / sampleRate);
		}
	}
	void drawAllEqCurves(const DrawArgs &args) {
		// draw frequency response curve
		nvgLineCap(args.vg, NVG_ROUND);
		nvgMiterLimit(args.vg, 1.0f);
		NVGcolor bandColors[4] = {nvgRGB(146, 32, 22), nvgRGB(0, 155, 137), nvgRGB(50, 99, 148),nvgRGB(111, 81, 113)};
		if (miscSettingsSrc->cc4[0] != 0) {
			for (int b = 0; b < 4; b++) {
				if (trackEqsSrc[currTrk].getBandActive(b) >= 0.5f) {
					drawEqCurveBand(b, args, bandColors[b]);
				}
			}
		}
		drawEqCurveTotal(args);
	}
	void lineToAtLogFreqAndDb(const DrawArgs &args, float logFreq, float dB) {
		float pX = math::rescale(logFreq, minLogFreq, maxLogFreq, 0.0f, box.size.x);
		float pY = math::rescale(dB, minDb, maxDb, box.size.y, 0.0f);
		nvgLineTo(args.vg, pX, pY);
	}
	void moveToAtLogFreqAndDb(const DrawArgs &args, float logFreq, float dB) {
		float pX = math::rescale(logFreq, minLogFreq, maxLogFreq, 0.0f, box.size.x);
		float pY = math::rescale(dB, minDb, maxDb, box.size.y, 0.0f);
		nvgMoveTo(args.vg, pX, pY);
	}
	void drawEqCurveTotal(const DrawArgs &args) {
		if (trackEqsSrc[currTrk].getTrackActive() && globalBypassParamSrc->getValue() < 0.5f) {
			nvgStrokeColor(args.vg, SCHEME_LIGHT_GRAY);
		}
		else {
			nvgStrokeColor(args.vg, SCHEME_GRAY);
		}
		nvgStrokeWidth(args.vg, 1.25f);	
		nvgBeginPath(args.vg);
		for (int x = 0; x <= (numDrawSteps + 4); x++) {	
			if (x == 0) {
				moveToAtLogFreqAndDb(args, stepLogFreqs[x], stepDbs[x][0] + stepDbs[x][1] + stepDbs[x][2] + stepDbs[x][3]);
			}
			else {
				lineToAtLogFreqAndDb(args, stepLogFreqs[x], stepDbs[x][0] + stepDbs[x][1] + stepDbs[x][2] + stepDbs[x][3]);
			}
		}
		nvgStroke(args.vg);
	}
	void drawEqCurveBand(int band, const DrawArgs &args, NVGcolor col) {
		NVGcolor fillColCursor = col;
		NVGcolor fillCol0 = col;
		fillColCursor.a = 0.5f;
		fillCol0.a = 0.1f;
		nvgFillColor(args.vg, fillColCursor);
		nvgStrokeColor(args.vg, col);
		nvgStrokeWidth(args.vg, 1.0f);
		
		nvgBeginPath(args.vg);
		moveToAtLogFreqAndDb(args, minLogFreq - 0.05f, 0.0f);// 0.05f is a hack to not show the side stroke
		for (int x = 0; x <= (numDrawSteps + 4); x++) {	
			lineToAtLogFreqAndDb(args, stepLogFreqs[x], stepDbs[x][band]);
		}
		lineToAtLogFreqAndDb(args, maxLogFreq + 0.05f, 0.0f);// 0.05f is a hack to not show the side stroke
		nvgClosePath(args.vg);
		
		NVGpaint grad;
		float cursorPY = (box.size.y / 2.0f * (1.0f - bandParamsWithCvs[1][band] / 20.0f));
		if (bandParamsWithCvs[1][band] > 0.0f) {
			grad = nvgLinearGradient(args.vg, 0.0f, cursorPY, 0.0f, box.size.y / 2.0f, fillColCursor, fillCol0);
		}
		else {
			grad = nvgLinearGradient(args.vg, 0.0f, box.size.y / 2.0f, 0.0f, cursorPY, fillCol0, fillColCursor);
		}
		nvgFillPaint(args.vg, grad);
		nvgFill(args.vg);
		nvgStroke(args.vg);
		
		// cursor circle
		nvgBeginPath(args.vg);
		float cursorPX = math::rescale(bandParamsWithCvs[0][band], minLogFreq, maxLogFreq, 0.0f, box.size.x);
		nvgCircle(args.vg, cursorPX, cursorPY, 3.0f);
		nvgClosePath(args.vg);
		
		nvgFillColor(args.vg, col);
		nvgFill(args.vg);
		int knob12i = *lastMovedKnobIdSrc - FREQ_PARAMS;
		if ( (band == (knob12i & 0x3)) && (knob12i >= 0) && (knob12i < 12) && (time(0) - *lastMovedKnobTimeSrc < 4) ) {
			nvgStrokeColor(args.vg, SCHEME_LIGHT_GRAY);
			nvgStrokeWidth(args.vg, 0.75f);
			nvgStroke(args.vg);
		}
	}
};



// Knobs
// --------------------

static const NVGcolor COL_GRAY = nvgRGB(111, 111, 111);
static const NVGcolor COL_GREEN = nvgRGB(127, 200, 68);
static const NVGcolor COL_RED = nvgRGB(247, 23, 41);


struct TrackKnob : MmBigKnobWhite {
	static constexpr float radius = 18.0f;
	static constexpr float dotSize = 1.1f;

	// user must setup:
	int* updateTrackLabelRequestSrc = NULL;
	TrackEq* trackEqsSrc;
	Input* polyInputs;
	
	// internal
	int refresh;// 0 to 23
	int numTracks;// typically 24
	Vec cVec;
	float totAng;
	float px[24];
	float py[24];
	bool nonDefaultState[24];
	
	
	TrackKnob() {
		snap = true;
		
		refresh = 0;
		numTracks = -1;// force fillDotPosAndDefState() in first pass in draw() where paramQuantity is non null
		cVec = box.size.div(2.0f);
		totAng = maxAngle - minAngle;
	}
	
	void randomize() override {}
	
	void fillDotPosAndDefState() {
		// requires numTracks to be up to date
		float deltAng = totAng / ((float)numTracks - 1.0f);
		float ang = minAngle;
		for (int trk = 0; trk < numTracks; trk++) {
			px[trk] = cVec.x + radius * std::sin(ang);
			py[trk] = cVec.y - radius * std::cos(ang);
			ang += deltAng;
			nonDefaultState[trk] = trackEqsSrc[trk].isNonDefaultState();
		}
	}
	
	
	void onChange(const event::Change& e) override {
		if (updateTrackLabelRequestSrc) {
			*updateTrackLabelRequestSrc = 1;
		}
		MmBigKnobWhite::onChange(e);
	}
	
	
	void draw(const DrawArgs &args) override {
		MmBigKnobWhite::draw(args);
		if (paramQuantity) {
			int newNumTracks = (int)(paramQuantity->getMaxValue() + 1.5f);
			if (newNumTracks != numTracks) {
				numTracks = newNumTracks;
				fillDotPosAndDefState();
			}
			int selectedTrack = (int)(paramQuantity->getValue() + 0.5f);
			for (int trk = 0; trk < numTracks; trk++) {
				if (trk == refresh) {
					nonDefaultState[trk] = trackEqsSrc[trk].isNonDefaultState();
				}
				nvgBeginPath(args.vg);
				nvgCircle(args.vg, px[trk], py[trk], dotSize);
				if (trk == selectedTrack) {
					nvgFillColor(args.vg, SCHEME_WHITE);
				}
				else if (!polyInputs[trk >> 3].isConnected() || !nonDefaultState[trk]) {// if unconnected or in default state
					nvgFillColor(args.vg, COL_GRAY);
				}
				else if (trackEqsSrc[trk].getTrackActive()) {// here we are connected and not in default state
					nvgFillColor(args.vg, COL_GREEN);
				}
				else {
					nvgFillColor(args.vg, COL_RED);
				}
				nvgFill(args.vg);		
			}
		}
		refresh++;
		if (refresh > 23) refresh = 0;
	}	
};

struct TrackGainKnob : MmSmallKnobGrey8mm {
	Param* trackParamSrc;
	TrackEq* trackEqsSrc;
	
	void onChange(const event::Change& e) override {
		MmSmallKnobGrey8mm::onChange(e);
		if (paramQuantity) {
			int currTrk = (int)(trackParamSrc->getValue() + 0.5f);
			trackEqsSrc[currTrk].setTrackGain(paramQuantity->getValue());
		}
	}
};

struct BandKnob : MmKnobWithArc {
	// user must setup 
	Param* trackParamSrc;
	TrackEq* trackEqsSrc = NULL;
	int* lastMovedKnobIdSrc;
	time_t* lastMovedKnobTimeSrc;
	
	// auto setup 
	int band;
	
	
	void loadGraphics(int _band) {
		band = _band;
		if (band == 0) {
			setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/eq/lf-knob.svg")));
			arcColor = nvgRGB(222, 61, 46);
		}
		else if (band == 1) {
			setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/eq/lmf-knob.svg")));
			arcColor = nvgRGB(0, 155, 137);
		}
		else if (band == 2) {
			setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/eq/hmf-knob.svg")));
			arcColor = nvgRGB(58, 115, 171);
		}
		else {
			setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/eq/hf-knob.svg")));
			arcColor = nvgRGB(134, 99, 137);
		}
	}
	
	void onDragMove(const event::DragMove& e) override {
		MmKnobWithArc::onDragMove(e);
		if (paramQuantity) {
			*lastMovedKnobIdSrc = paramQuantity->paramId;
			*lastMovedKnobTimeSrc = time(0);
		}
	}
};
	
template<int BAND>// 0 = LF, 1 = LMF, 2 = HMF, 3 = HF
struct EqFreqKnob : BandKnob {
	EqFreqKnob() {
		loadGraphics(BAND);
	}
		
	void onChange(const event::Change& e) override {
		BandKnob::onChange(e);
		if (paramQuantity) {
			int currTrk = (int)(trackParamSrc->getValue() + 0.5f);
			trackEqsSrc[currTrk].setFreq(BAND, paramQuantity->getValue());
		}
	}
};

template<int BAND>// 0 = LF, 1 = LMF, 2 = HMF, 3 = HF
struct EqGainKnob : BandKnob {
	EqGainKnob() {
		loadGraphics(BAND);
		topCentered = true;
	}

	void onChange(const event::Change& e) override {
		BandKnob::onChange(e);
		if (paramQuantity) {
			int currTrk = (int)(trackParamSrc->getValue() + 0.5f);
			trackEqsSrc[currTrk].setGain(BAND, paramQuantity->getValue());
		}
	}
};

template<int BAND>// 0 = LF, 1 = LMF, 2 = HMF, 3 = HF
struct EqQKnob : BandKnob {
	EqQKnob() {
		loadGraphics(BAND);
	}
	
	void onChange(const event::Change& e) override {
		BandKnob::onChange(e);
		if (paramQuantity) {
			int currTrk = (int)(trackParamSrc->getValue() + 0.5f);
			trackEqsSrc[currTrk].setQ(BAND, paramQuantity->getValue());
		}
	}
};



// Switches and buttons
// --------------------

struct ActiveSwitch : MmSwitch {
	Param* trackParamSrc;
	TrackEq* trackEqsSrc;

	void onChange(const event::Change& e) override {
		MmSwitch::onChange(e);
		if (paramQuantity) {
			int currTrk = (int)(trackParamSrc->getValue() + 0.5f);
			trackEqsSrc[currTrk].setTrackActive(paramQuantity->getValue() >= 0.5f);
		}
	}
	void randomize() override {}
};

struct BandSwitch : app::SvgSwitch {
	Param* trackParamSrc;
	Param* freqActiveParamsSrc;
	TrackEq* trackEqsSrc;
	float preSoloStates[4] = {1.0f, 1.0f, 1.0f, 1.0f};
	int soloedBand = -1;

	void loadGraphics(int band) {
		if (band == 0) {
			addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/eq/band1-off.svg")));
			addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/eq/band1-on.svg")));
		}
		else if (band == 1) {
			addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/eq/band2-off.svg")));
			addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/eq/band2-on.svg")));
		}
		else if (band == 2) {
			addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/eq/band3-off.svg")));
			addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/eq/band3-on.svg")));
		}
		else {
			addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/eq/band4-off.svg")));
			addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/eq/band4-on.svg")));
		}
	}
};
template<int BAND>
struct BandActiveSwitch : BandSwitch {
	BandActiveSwitch() {
		loadGraphics(BAND);
	}
	void onChange(const event::Change& e) override {
		BandSwitch::onChange(e);
		if (paramQuantity) {
			int currTrk = (int)(trackParamSrc->getValue() + 0.5f);
			trackEqsSrc[currTrk].setBandActive(BAND, paramQuantity->getValue());
		}
	}
	void onButton(const event::Button &e) override {
		if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS) {
			if ((APP->window->getMods() & RACK_MOD_MASK) == GLFW_MOD_ALT) {
				// solo
				if ((soloedBand == -1) || (soloedBand != -1 && soloedBand != BAND)) {// entering solo
					soloedBand = BAND;
					for (int b = 0; b < 4; b++) {// save all states, turn on selected band and turn off all other bands
						preSoloStates[b] = freqActiveParamsSrc[b].getValue();
						freqActiveParamsSrc[b].setValue(0.0f);// turning off current is before click such that click will turn on
					}
				}
				else {
					if (soloedBand == BAND) {// leaving solo
						soloedBand = -1;
						for (int b = 0; b < 4; b++) {
							if (b == BAND) {
								freqActiveParamsSrc[b].setValue(1.0f - preSoloStates[b]);
							}
							else {
								freqActiveParamsSrc[b].setValue(preSoloStates[b]);
							}
						}
					}
				}
				e.consume(this);
				return;
			}
		}
		BandSwitch::onButton(e);		
	}
};

struct PeakShelfBase : app::SvgSwitch {
	Param* trackParamSrc;
	TrackEq* trackEqsSrc;
	bool isLF;
};
struct PeakSwitch : PeakShelfBase {
	PeakSwitch() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/eq/bell-off.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/eq/bell-on.svg")));
	}
	void onChange(const event::Change& e) override {
		SvgSwitch::onChange(e);
		if (paramQuantity) {
			int currTrk = (int)(trackParamSrc->getValue() + 0.5f);
			bool state = paramQuantity->getValue() >= 0.5f;
			if (isLF) {
				trackEqsSrc[currTrk].setLowPeak(state);
			}
			else {
				trackEqsSrc[currTrk].setHighPeak(state);
			}
		}
	}
};
struct ShelfLowSwitch : PeakShelfBase {
	ShelfLowSwitch() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/eq/low-shelf-on.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/eq/low-shelf-off.svg")));
	}
};
struct ShelfHighSwitch : PeakShelfBase {
	ShelfHighSwitch() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/eq/high-shelf-on.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/eq/high-shelf-off.svg")));
	}
};






#endif