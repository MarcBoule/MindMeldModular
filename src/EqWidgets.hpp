//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#ifndef MMM_EQWIDGETS_HPP
#define MMM_EQWIDGETS_HPP

#include "EqMenus.hpp"
#include "VuMeters.hpp"
#include "dsp/fft.hpp"


// Labels
// --------------------

struct TrackLabel : LedDisplayChoice {
	int8_t* colorGlobalSrc = NULL;
	int8_t* colorLocalSrc;
	char* trackLabelsSrc;
	Param* trackParamSrc;
	TrackEq *trackEqsSrc;
	
	TrackLabel() {
		box.size = mm2px(Vec(10.6f, 5.0f));
		textOffset = Vec(4.2f, 11.3f);
		text = "-00-";
	};
	
	void draw(const DrawArgs &args) override {
		if (colorGlobalSrc) {
			int colorIndex = *colorGlobalSrc < 7 ? *colorGlobalSrc : *colorLocalSrc;
			color = DISP_COLORS[colorIndex];
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

			CopyTrackSettingsItem *copyItem = createMenuItem<CopyTrackSettingsItem>("Copy track settings to:", RIGHT_ARROW);
			copyItem->trackLabelsSrc = trackLabelsSrc;
			copyItem->trackEqsSrc = trackEqsSrc;
			copyItem->trackNumSrc = (int)(trackParamSrc->getValue() + 0.5f);
			menu->addChild(copyItem);


			MenuLabel *trkSelLabel = new MenuLabel();
			trkSelLabel->text = "Select Track: ";
			menu->addChild(trkSelLabel);
			
			for (int trk = 0; trk < 24; trk++) {
				int currTrk = (int)(trackParamSrc->getValue() + 0.5f);
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
	int8_t* colorGlobalSrc = NULL;
	int8_t* colorLocalSrc;
	char* trackLabelsSrc;
	Param* trackParamSrc = NULL;
	TrackEq *trackEqsSrc;
	int band;
	
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
		
		if (colorGlobalSrc) {
			int colorIndex = *colorGlobalSrc < 7 ? *colorGlobalSrc : *colorLocalSrc;
			color = DISP_COLORS[colorIndex];
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
			float freq = trackEqsSrc[trk].getFreq(band);
			if (freq < 10000.0f) {
				text = string::f("%i", (int)(freq + 0.5f));
			}
			else {
				freq /= 1000.0f;
				text = string::f("%.2fk", freq);
			}
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
};
struct BandLabelQ : BandLabelBase {
	void prepareText() override {
		if (trackParamSrc) {
			int trk = (int)(trackParamSrc->getValue() + 0.5f);
			if (! ( (band == 0 && !trackEqsSrc[trk].getLowPeak()) || (band == 3 && !trackEqsSrc[trk].getHighPeak())) ) {
				float q = trackEqsSrc[trk].getQ(band);
				text = string::f("%.2f", math::normalizeZero(q));
			}
			else {
				text = "---";
			}
		}
	}
};



// Displays
// --------------------

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
					float freq = trackEqsSrc[currTrk].getFreq(srcId - FREQ_PARAMS);
					if (freq < 10000.0f) {
						text = string::f("%i Hz", (int)(freq + 0.5f));
					}
					else {
						freq /= 1000.0f;
						text = string::f("%.2f kHz", freq);
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
					if (! ( (band == 0 && !trackEqsSrc[currTrk].getLowPeak()) || (band == 3 && !trackEqsSrc[currTrk].getHighPeak())) ) {
						float q = trackEqsSrc[currTrk].getQ(band);
						text = string::f("%.2f", math::normalizeZero(q));
					}
					else {
						text = "---";
					}
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
	static constexpr float minFreq = 20.0f;
	static constexpr float maxFreq = 22000.0f;
	static constexpr float minDb = -20.0f;
	static constexpr float maxDb = 20.0f;
	static const int numDrawSteps = 200;
	float stepLogFreqs[numDrawSteps + 4 + 1];// 4 for cursors, 1 since will loop with "<= numDrawSteps"
	simd::float_4 stepDbs[numDrawSteps + 4 + 1];// 4 for cursors, 1 since will loop with "<= numDrawSteps"
	
	// user must set up
	Param* trackParamSrc = NULL;
	TrackEq* trackEqsSrc;
	PackedBytes4 *miscSettingsSrc;	
	
	// internal
	float minLogFreq;
	float maxLogFreq;
	QuattroBiQuadCoeff drawEq;
	std::shared_ptr<Font> font;
	
	
	EqCurveAndGrid() {
		box.size = mm2px(Vec(109.22f, 60.943f));	
		minLogFreq = std::log10(minFreq);// 1.3
		maxLogFreq = std::log10(maxFreq);// 4.3
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/RobotoCondensed-Regular.ttf"));
	}
	
	void textAtFreqAndDb(const DrawArgs &args, float freq, float dB, std::string text) {
		float logFreq = std::log10(freq);
		float textX = math::rescale(logFreq, minLogFreq, maxLogFreq, 0.0f, box.size.x);
		float textY = math::rescale(dB, minDb, maxDb, box.size.y, 0.0f);
		nvgText(args.vg, textX + 1.5f, textY - 3.0f, text.c_str(), NULL);
	}
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
	void dotAtLogFreqAndDb(const DrawArgs &args, float logFreq, float dB) {
		float circX = math::rescale(logFreq, minLogFreq, maxLogFreq, 0.0f, box.size.x);
		float circY = math::rescale(dB, minDb, maxDb, box.size.y, 0.0f);
		nvgCircle(args.vg, circX, circY, 2.0f);
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
	
	void draw(const DrawArgs &args) override {
		// grid
		nvgStrokeColor(args.vg, nvgRGB(0x37, 0x37, 0x37));
		nvgStrokeWidth(args.vg, 1.0f);
		nvgBeginPath(args.vg);
		// vertical lines
		vertLineAtFreq(args, 30.0f);
		vertLineAtFreq(args, 40.0f);
		vertLineAtFreq(args, 50.0f);
		for (int i = 1; i <= 5; i++) {
			vertLineAtFreq(args, 100.0f * (float)i);
			vertLineAtFreq(args, 1000.0f * (float)i);
		}
		vertLineAtFreq(args, 10000.0f);
		vertLineAtFreq(args, 20000.0f);
		// horizontal lines
		horzLineAtDb(args, 20.0f);
		horzLineAtDb(args, 12.0f);
		horzLineAtDb(args, 0.0f);
		horzLineAtDb(args, -12.0f);
		//nvgRect(args.vg, 0.0f, 0.0f, box.size.x, box.size.y);
		nvgStroke(args.vg);		
		// text labels
		if (font->handle >= 0) {
			nvgFillColor(args.vg, nvgRGB(0x97, 0x97, 0x97));
			nvgFontFaceId(args.vg, font->handle);
			nvgTextLetterSpacing(args.vg, 0.0);
			nvgFontSize(args.vg, 9.0f);
			// frequency
			textAtFreqAndDb(args, 100.0f, -20.0f, "100 Hz");
			textAtFreqAndDb(args, 500.0f, -20.0f, "500 Hz");
			textAtFreqAndDb(args, 1000.0f, -20.0f, "1 kHz");
			textAtFreqAndDb(args, 5000.0f, -20.0f, "5 kHz");
			textAtFreqAndDb(args, 10000.0f, -20.0f, "10 kHz");
			// dB
			textAtFreqAndDb(args, 20.0f, -12.0f, "-12 dB");
			textAtFreqAndDb(args, 20.0f, 0.0f, "0 dB");
			textAtFreqAndDb(args, 20.0f, 12.0f, "+12 dB");
		}
		
		
		
		// curve
		if (trackParamSrc != NULL) {
			int currTrk = (int)(trackParamSrc->getValue() + 0.5f);
			float sampleRate = trackEqsSrc[0].getSampleRate();

			// set eqCoefficients of separate drawEq according to active track
			// get cursor points of each band		
			simd::float_4 logFreqCursors;
			for (int b = 0; b < 4; b++) {
				logFreqCursors[b] = trackEqsSrc[currTrk].getFreq(b);// not log yet, do that after loop
				float normalizedFreq = std::min(0.5f, trackEqsSrc[currTrk].getFreq(b) / sampleRate);	
				float linearGain = (trackEqsSrc[currTrk].getBandActive(b)) ? std::pow(10.0f, trackEqsSrc[currTrk].getGain(b) / 20.0f) : 1.0f;
				drawEq.setParameters(b, trackEqsSrc[currTrk].getBandType(b), normalizedFreq, linearGain, trackEqsSrc[currTrk].getQ(b));
			}
			logFreqCursors = sortFloat4(simd::log10(logFreqCursors));
			
			
			// fill freq response curve data
			float delLogX = (maxLogFreq - minLogFreq) / ((float)numDrawSteps);
			int c = 0;// index into logFreqCursors
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


			// draw frequency response curve
			nvgScissor(args.vg, RECT_ARGS(args.clipBox));
			nvgLineCap(args.vg, NVG_ROUND);
			nvgMiterLimit(args.vg, 1.0f);
			NVGcolor bandColors[4] = {nvgRGB(146, 32, 22), nvgRGB(0, 155, 137), nvgRGB(50, 99, 148),nvgRGB(111, 81, 113)};
			if (miscSettingsSrc->cc4[0] != 0) {
				for (int b = 0; b < 4; b++) {
					if (trackEqsSrc[currTrk].getBandActive(b)) {
						drawEqCurveBandFill(b, args, bandColors[b], trackEqsSrc[currTrk].getGain(b));
					}
				}
			}
			drawEqCurveTotal(args, SCHEME_LIGHT_GRAY);
			nvgResetScissor(args.vg);					
		}
	}
	
	void drawEqCurveTotal(const DrawArgs &args, NVGcolor col) {
		nvgStrokeColor(args.vg, col);
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
		nvgStrokeColor(args.vg, col);
		nvgStrokeWidth(args.vg, 1.0f);	
		nvgBeginPath(args.vg);
		for (int x = 0; x <= (numDrawSteps + 4); x++) {	
			if (x == 0) {
				moveToAtLogFreqAndDb(args, stepLogFreqs[x], stepDbs[x][band]);
			}
			else {
				lineToAtLogFreqAndDb(args, stepLogFreqs[x], stepDbs[x][band]);
			}
		}
		nvgStroke(args.vg);
	}	
	void drawEqCurveBandFill(int band, const DrawArgs &args, NVGcolor col, float cursorDb) {
		NVGcolor fillColCursor = col;
		NVGcolor fillCol0 = col;
		fillColCursor.a = 0.5f;
		fillCol0.a = 0.1f;
		nvgFillColor(args.vg, fillColCursor);
		nvgStrokeColor(args.vg, col);
		nvgStrokeWidth(args.vg, 1.0f);	
		nvgBeginPath(args.vg);
		moveToAtLogFreqAndDb(args, minLogFreq, 0.0f);
		for (int x = 0; x <= (numDrawSteps + 4); x++) {	
			lineToAtLogFreqAndDb(args, stepLogFreqs[x], stepDbs[x][band]);
		}
		lineToAtLogFreqAndDb(args, maxLogFreq, 0.0f);
		nvgClosePath(args.vg);
		NVGpaint grad;
		if (cursorDb > 0.0f) {
			grad = nvgLinearGradient(args.vg, 0.0f, 0.0f, 0.0f, box.size.y / 2.0f, fillColCursor, fillCol0);
		}
		else {
			grad = nvgLinearGradient(args.vg, 0.0f, box.size.y / 2.0f, 0.0f, box.size.y, fillCol0, fillColCursor);
		}

		nvgFillPaint(args.vg, grad);
		nvgFill(args.vg);
		nvgStroke(args.vg);
	}
};



// Knobs
// --------------------

static const NVGcolor COL_GRAY = nvgRGB(111, 111, 111);
static const NVGcolor COL_GREEN = nvgRGB(127, 200, 68);
static const NVGcolor COL_RED = nvgRGB(247, 23, 41);


struct TrackKnob : DynSnapKnob {
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
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/eq/track-knob-pointer.svg")));
		
		refresh = 0;
		numTracks = -1;// force fillDotPosAndDefState() in first pass in draw() where paramQuantity is non null
		cVec = box.size.div(2.0f);
		totAng = maxAngle - minAngle;
	}
	
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
		DynSnapKnob::onChange(e);
	}
	
	
	void draw(const DrawArgs &args) override {
		DynamicSVGKnob::draw(args);
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

struct TrackGainKnob : DynKnob {
	Param* trackParamSrc;
	TrackEq* trackEqsSrc;
	
	TrackGainKnob() {
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/eq/gain-knob.svg")));
	}
	void onChange(const event::Change& e) override {
		DynKnob::onChange(e);
		if (paramQuantity) {
			int currTrk = (int)(trackParamSrc->getValue() + 0.5f);
			trackEqsSrc[currTrk].setTrackGain(paramQuantity->getValue());
		}
	}
};

struct BandKnob : DynKnob {
	Param* trackParamSrc;
	TrackEq* trackEqsSrc;
	int* lastMovedKnobIdSrc;
	time_t* lastMovedKnobTimeSrc;
	
	void loadGraphics(int band) {
		if (band == 0) 
			addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/eq/lf-knob.svg")));
		else if (band == 1) 
			addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/eq/lmf-knob.svg")));
		else if (band == 2) 
			addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/eq/hmf-knob.svg")));
		else
			addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/eq/hf-knob.svg")));
	}
	
	void onDragMove(const event::DragMove& e) override {
		DynKnob::onDragMove(e);
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
		DynKnob::onChange(e);
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
	}

	void onChange(const event::Change& e) override {
		DynKnob::onChange(e);
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
		DynKnob::onChange(e);
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
			trackEqsSrc[currTrk].setTrackActive(paramQuantity->getValue() > 0.5f);
		}
	}
};

struct BandSwitch : app::SvgSwitch {
	Param* trackParamSrc;
	TrackEq* trackEqsSrc;

	void loadGraphics(int band) {
		if (band == 0) {
			addFrame(APP->window->loadSvg(asset::system("res/ComponentLibrary/TL1105_0.svg")));
			addFrame(APP->window->loadSvg(asset::system("res/ComponentLibrary/TL1105_1.svg")));
		}
		else if (band == 1) {
			addFrame(APP->window->loadSvg(asset::system("res/ComponentLibrary/TL1105_0.svg")));
			addFrame(APP->window->loadSvg(asset::system("res/ComponentLibrary/TL1105_1.svg")));
		}
		else if (band == 2) {
			addFrame(APP->window->loadSvg(asset::system("res/ComponentLibrary/TL1105_0.svg")));
			addFrame(APP->window->loadSvg(asset::system("res/ComponentLibrary/TL1105_1.svg")));
		}
		else {
			addFrame(APP->window->loadSvg(asset::system("res/ComponentLibrary/TL1105_0.svg")));
			addFrame(APP->window->loadSvg(asset::system("res/ComponentLibrary/TL1105_1.svg")));
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
			bool state = paramQuantity->getValue() > 0.5f;
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