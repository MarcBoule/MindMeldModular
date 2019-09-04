//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.txt for all licenses
//***********************************************************************************************

#ifndef MMM_MIXERWIDGETS_HPP
#define MMM_MIXERWIDGETS_HPP


#include "Mixer.hpp"
#include "MixerMenus.hpp"
#include <time.h>


// VU meters
// --------------------

// Colors
static const int numThemes = 5;
static const NVGcolor VU_THEMES_TOP[numThemes][2] =  
									   {{nvgRGB(110, 130, 70), 	nvgRGB(178, 235, 107)}, // green: peak (darker), rms (lighter)
										{nvgRGB(68, 164, 122), 	nvgRGB(102, 245, 182)}, // teal: peak (darker), rms (lighter)
										{nvgRGB(64, 155, 160), 	nvgRGB(102, 233, 245)}, // light blue: peak (darker), rms (lighter)
										{nvgRGB(68, 125, 164), 	nvgRGB(102, 180, 245)}, // blue: peak (darker), rms (lighter)
										{nvgRGB(110, 70, 130), 	nvgRGB(178, 107, 235)}};// purple: peak (darker), rms (lighter)
static const NVGcolor VU_THEMES_BOT[numThemes][2] =  
									   {{nvgRGB(50, 130, 70), 	nvgRGB(97, 235, 107)}, // green: peak (darker), rms (lighter)
										{nvgRGB(68, 164, 156), 	nvgRGB(102, 245, 232)}, // teal: peak (darker), rms (lighter)
										{nvgRGB(64, 108, 160), 	nvgRGB(102, 183, 245)}, // light blue: peak (darker), rms (lighter)
										{nvgRGB(68,  92, 164), 	nvgRGB(102, 130, 245)}, // blue: peak (darker), rms (lighter)
										{nvgRGB(85,  70, 130), 	nvgRGB(135, 107, 235)}};// purple: peak (darker), rms (lighter)
static const NVGcolor VU_YELLOW[2] = {nvgRGB(136,136,37), nvgRGB(247, 216, 55)};// peak (darker), rms (lighter)
static const NVGcolor VU_ORANGE[2] = {nvgRGB(136,89,37), nvgRGB(238, 130, 47)};// peak (darker), rms (lighter)
static const NVGcolor VU_RED[2] =    {nvgRGB(136, 37, 37), 	nvgRGB(229, 34, 38)};// peak (darker), rms (lighter)

static const float sepYtrack = 0.3f * SVG_DPI / MM_PER_IN;// height of separator at 0dB. See include/app/common.hpp for constants
static const float sepYmaster = 0.4f * SVG_DPI / MM_PER_IN;// height of separator at 0dB. See include/app/common.hpp for constants

// Base struct
struct VuMeterBase : OpaqueWidget {
	static constexpr float epsilon = 0.0001f;// don't show VUs below 0.1mV
	static constexpr float peakHoldThick = 1.0f;// in px
	
	// instantiator must setup:
	VuMeterAll *srcLevels;// from 0 to 10 V, with 10 V = 0dB (since -10 to 10 is the max)
	int *colorThemeGlobal;
	
	// derived class must setup:
	float gapX;// in px
	float barX;// in px
	float barY;// in px
	// box.size // inherited from OpaqueWidget, no need to declare
	float faderMaxLinearGain;
	float faderScalingExponent;
	float zeroDbVoltage;
	
	// local 
	float peakHold[2] = {0.0f, 0.0f};
	long oldTime = -1;
	float yellowThreshold;// in px, before vertical inversion
	float redThreshold;// in px, before vertical inversion
	int colorTheme;
	
	
	VuMeterBase() {
		faderMaxLinearGain = GlobalInfo::trkAndGrpFaderMaxLinearGain;
		faderScalingExponent = GlobalInfo::trkAndGrpFaderScalingExponent;
	}
	
	void prepareYellowAndRedThresholds(float yellowMinDb, float redMinDb) {
		float maxLin = std::pow(faderMaxLinearGain, 1.0f / faderScalingExponent);
		float yellowLin = std::pow(std::pow(10.0f, yellowMinDb / 20.0f), 1.0f / faderScalingExponent);
		yellowThreshold = barY * (yellowLin / maxLin);
		float redLin = std::pow(std::pow(10.0f, redMinDb / 20.0f), 1.0f / faderScalingExponent);
		redThreshold = barY * (redLin / maxLin);
	}
	
	
	// Contract: 
	//  * calc peakHold[]
	void processPeakHold() {
		long newTime = time(0);
		if ( (newTime != oldTime) && ((newTime & 0x1) == 0) ) {
			oldTime = newTime;
			peakHold[0] = 0.0f;
			peakHold[1] = 0.0f;
		}		
		for (int i = 0; i < 2; i++) {
			if (srcLevels[i].getPeak() > peakHold[i]) {
				peakHold[i] = srcLevels[i].getPeak();
			}
		}
	}

	
	void draw(const DrawArgs &args) override {
		processPeakHold();
		
		colorTheme = (*colorThemeGlobal >= numThemes) ? srcLevels[0].vuColorTheme : *colorThemeGlobal;
		
		// PEAK
		drawVu(args, srcLevels[0].getPeak(), 0, 0);
		drawVu(args, srcLevels[1].getPeak(), barX + gapX, 0);

		// RMS
		drawVu(args, srcLevels[0].getRms(), 0, 1);
		drawVu(args, srcLevels[1].getRms(), barX + gapX, 1);
		
		// PEAK_HOLD
		drawPeakHold(args, peakHold[0], 0);
		drawPeakHold(args, peakHold[1], barX + gapX);
				
		Widget::draw(args);
	}
	
	// used for RMS or PEAK
	virtual void drawVu(const DrawArgs &args, float vuValue, float posX, int colorIndex) {};

	virtual void drawPeakHold(const DrawArgs &args, float holdValue, float posX) {};
};

// 2.8mm x 42mm VU for tracks and groups
// --------------------

struct VuMeterTrack : VuMeterBase {//
	VuMeterTrack() {
		gapX = mm2px(0.4);
		barX = mm2px(1.2);
		barY = mm2px(42.0);
		box.size = Vec(barX * 2 + gapX, barY);
		zeroDbVoltage = 5.0f;// V
		prepareYellowAndRedThresholds(-6.0f, 0.0f);// dB
	}

	// used for RMS or PEAK
	void drawVu(const DrawArgs &args, float vuValue, float posX, int colorIndex) override {
		if (vuValue >= epsilon) {

			float vuHeight = vuValue / (faderMaxLinearGain * zeroDbVoltage);
			vuHeight = std::pow(vuHeight, 1.0f / faderScalingExponent);
			vuHeight = std::min(vuHeight, 1.0f);// normalized is now clamped
			vuHeight *= barY;

			NVGpaint gradGreen = nvgLinearGradient(args.vg, 0, barY - redThreshold, 0, barY, VU_THEMES_TOP[colorTheme][colorIndex], VU_THEMES_BOT[colorTheme][colorIndex]);
			if (vuHeight > redThreshold) {
				// Yellow-Red gradient
				NVGpaint gradTop = nvgLinearGradient(args.vg, 0, 0, 0, barY - redThreshold - sepYtrack, VU_RED[colorIndex], VU_YELLOW[colorIndex]);
				nvgBeginPath(args.vg);
				nvgRect(args.vg, posX, barY - vuHeight - sepYtrack, barX, vuHeight - redThreshold);
				nvgFillPaint(args.vg, gradTop);
				nvgFill(args.vg);
				// Green
				nvgBeginPath(args.vg);
				nvgRect(args.vg, posX, barY - redThreshold, barX, redThreshold);
				//nvgFillColor(args.vg, VU_GREEN[colorIndex]);
				nvgFillPaint(args.vg, gradGreen);
				nvgFill(args.vg);			
			}
			else {
				// Green
				nvgBeginPath(args.vg);
				nvgRect(args.vg, posX, barY - vuHeight, barX, vuHeight);
				//nvgFillColor(args.vg, VU_GREEN[colorIndex]);
				nvgFillPaint(args.vg, gradGreen);
				nvgFill(args.vg);
			}

		}
	}
	
	void drawPeakHold(const DrawArgs &args, float holdValue, float posX) override {
		if (holdValue >= epsilon) {
			float vuHeight = holdValue / (faderMaxLinearGain * zeroDbVoltage);
			vuHeight = std::pow(vuHeight, 1.0f / faderScalingExponent);
			vuHeight = std::min(vuHeight, 1.0f);// normalized is now clamped
			vuHeight *= barY;
			
			if (vuHeight > redThreshold) {
				// Yellow-Red gradient
				NVGpaint gradTop = nvgLinearGradient(args.vg, 0, 0, 0, barY - redThreshold - sepYtrack, VU_RED[1], VU_YELLOW[1]);
				nvgBeginPath(args.vg);
				nvgRect(args.vg, posX, barY - vuHeight - sepYtrack - peakHoldThick, barX, peakHoldThick);
				nvgFillPaint(args.vg, gradTop);
				nvgFill(args.vg);	
			}
			else {
				// Green
				NVGpaint gradGreen = nvgLinearGradient(args.vg, 0, barY - redThreshold, 0, barY, VU_THEMES_TOP[colorTheme][1], VU_THEMES_BOT[colorTheme][1]);
				nvgBeginPath(args.vg);
				nvgRect(args.vg, posX, barY - vuHeight, barX, peakHoldThick);
				nvgFillPaint(args.vg, gradGreen);
				//nvgFillColor(args.vg, VU_GREEN[1]);
				nvgFill(args.vg);
			}
		}		
	}
};



// 3.8mm x 60mm VU for master
// --------------------

struct VuMeterMaster : VuMeterBase {
	VuMeterMaster() {
		gapX = mm2px(0.6);
		barX = mm2px(1.6);
		barY = mm2px(60.0);
		box.size = Vec(barX * 2 + gapX, barY);
		zeroDbVoltage = 10.0f;// V
		prepareYellowAndRedThresholds(-6.0f, 0.0f);// dB
	}
	
	// used for RMS or PEAK
	void drawVu(const DrawArgs &args, float vuValue, float posX, int colorIndex) override {
		if (vuValue >= epsilon) {
			float vuHeight = vuValue / (faderMaxLinearGain * zeroDbVoltage);
			vuHeight = std::pow(vuHeight, 1.0f / faderScalingExponent);
			vuHeight = std::min(vuHeight, 1.0f);// normalized is now clamped
			vuHeight *= barY;
			
			float peakHoldVal = (posX == 0 ? peakHold[0] : peakHold[1]);
			if (vuHeight > redThreshold || peakHoldVal > 10.0f) {
				// Full red
				nvgBeginPath(args.vg);
				if (vuHeight > redThreshold) {
					nvgRect(args.vg, posX, barY - vuHeight - sepYmaster, barX, vuHeight - redThreshold);
					nvgRect(args.vg, posX, barY - redThreshold, barX, redThreshold);
				}
				else {
					nvgRect(args.vg, posX, barY - vuHeight, barX, vuHeight);
				}
				nvgFillColor(args.vg, VU_RED[colorIndex]);
				nvgFill(args.vg);
			}
			else {
				NVGpaint gradGreen = nvgLinearGradient(args.vg, 0, barY - yellowThreshold, 0, barY, VU_THEMES_TOP[colorTheme][colorIndex], VU_THEMES_BOT[colorTheme][colorIndex]);
				if (vuHeight > yellowThreshold) {
					// Yellow-Orange gradient
					NVGpaint gradTop = nvgLinearGradient(args.vg, 0, barY - redThreshold, 0, barY - yellowThreshold, VU_ORANGE[colorIndex], VU_YELLOW[colorIndex]);
					nvgBeginPath(args.vg);
					nvgRect(args.vg, posX, barY - vuHeight, barX, vuHeight - yellowThreshold);
					nvgFillPaint(args.vg, gradTop);
					nvgFill(args.vg);
					// Green
					nvgBeginPath(args.vg);
					nvgRect(args.vg, posX, barY - yellowThreshold, barX, yellowThreshold);
					//nvgFillColor(args.vg, VU_GREEN[colorIndex]);
					nvgFillPaint(args.vg, gradGreen);
					nvgFill(args.vg);			
				}
				else {
					// Green
					nvgBeginPath(args.vg);
					nvgRect(args.vg, posX, barY - vuHeight, barX, vuHeight);
					//nvgFillColor(args.vg, VU_GREEN[colorIndex]);
					nvgFillPaint(args.vg, gradGreen);
					nvgFill(args.vg);
				}
			}
		}
	}	
	
	void drawPeakHold(const DrawArgs &args, float holdValue, float posX) override {
		if (holdValue >= epsilon) {
			float vuHeight = holdValue / (faderMaxLinearGain * zeroDbVoltage);
			vuHeight = std::pow(vuHeight, 1.0f / faderScalingExponent);
			vuHeight = std::min(vuHeight, 1.0f);// normalized is now clamped
			vuHeight *= barY;
			
			float peakHoldVal = (posX == 0 ? peakHold[0] : peakHold[1]);
			if (vuHeight > redThreshold || peakHoldVal > 10.0f) {
				// Full red
				nvgBeginPath(args.vg);
				nvgRect(args.vg, posX, barY - vuHeight - sepYmaster - peakHoldThick, barX, peakHoldThick);
				nvgFillColor(args.vg, VU_RED[1]);
				nvgFill(args.vg);
			} else if (vuHeight > yellowThreshold) {
				// Yellow-Orange gradient
				NVGpaint gradTop = nvgLinearGradient(args.vg, 0, barY - redThreshold, 0, barY - yellowThreshold, VU_ORANGE[1], VU_YELLOW[1]);
				nvgBeginPath(args.vg);
				nvgRect(args.vg, posX, barY - vuHeight, barX, 1.0);
				nvgFillPaint(args.vg, gradTop);
				nvgFill(args.vg);	
			}
			else {
				// Green
				NVGpaint gradGreen = nvgLinearGradient(args.vg, 0, barY - yellowThreshold, 0, barY, VU_THEMES_TOP[colorTheme][1], VU_THEMES_BOT[colorTheme][1]);
				nvgBeginPath(args.vg);
				nvgRect(args.vg, posX, barY - vuHeight, barX, 1.0);
				//nvgFillColor(args.vg, VU_GREEN[1]);
				nvgFillPaint(args.vg, gradGreen);
				nvgFill(args.vg);
			}
		}
	}
	
};


// Fade pointer
// --------------------

static const float prtHeight = 3.4f  * SVG_DPI / MM_PER_IN;// height of pointer, width is determined by box.size.x in derived struct
static const NVGcolor POINTER_FILL = nvgRGB(255, 106, 31);

struct FadePointerBase : OpaqueWidget {
	// instantiator must setup:
	Param *srcParam;// to know where the fader is
	float *srcFadeGain;// to know where to position the pointer
	float *srcFadeRate;// mute when < minFadeRate, fade when >= minFadeRate

	// derived class must setup:
	// box.size // inherited from OpaqueWidget, no need to declare
	float faderMaxLinearGain;
	float faderScalingExponent;
	float minFadeRate;
	
	// local 
	float maxTFader;

	
	void prepareMaxFader() {
		maxTFader = std::pow(faderMaxLinearGain, 1.0f / faderScalingExponent);
	}


	void draw(const DrawArgs &args) override {
		if (*srcFadeRate < minFadeRate || *srcFadeGain >= 1.0f) {
			return;
		}
		float fadePosNormalized = srcParam->getValue() / maxTFader;
		float vertPos = box.size.y - box.size.y * std::pow((*srcFadeGain), 1.0f/* / faderScalingExponent*/) * fadePosNormalized ;// in px
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, 0, vertPos - prtHeight / 2.0f);
		nvgLineTo(args.vg, box.size.x, vertPos);
		nvgLineTo(args.vg, 0, vertPos + prtHeight / 2.0f);
		nvgClosePath(args.vg);
		nvgFillColor(args.vg, POINTER_FILL);
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, SCHEME_BLACK);
		nvgStrokeWidth(args.vg, mm2px(0.11f));
		nvgStroke(args.vg);
	}	
};


struct FadePointerTrack : FadePointerBase {
	FadePointerTrack() {
		box.size = mm2px(math::Vec(2.8, 42));
		faderMaxLinearGain = GlobalInfo::trkAndGrpFaderMaxLinearGain;
		faderScalingExponent = GlobalInfo::trkAndGrpFaderScalingExponent;
		minFadeRate = MixerTrack::minFadeRate;
		prepareMaxFader();
	}
};
struct FadePointerGroup : FadePointerBase {
	FadePointerGroup() {
		box.size = mm2px(math::Vec(2.8, 42));
		faderMaxLinearGain = GlobalInfo::trkAndGrpFaderMaxLinearGain;
		faderScalingExponent = GlobalInfo::trkAndGrpFaderScalingExponent;
		minFadeRate = MixerGroup::minFadeRate;
		prepareMaxFader();
	}
};
struct FadePointerMaster : FadePointerBase {
	FadePointerMaster() {
		box.size = mm2px(math::Vec(2.8, 60));
		faderMaxLinearGain = MixerMaster::masterFaderMaxLinearGain;
		faderScalingExponent = MixerMaster::masterFaderScalingExponent;
		minFadeRate = MixerMaster::minFadeRate;
		prepareMaxFader();
	}
};


// Master invisible widget with menu
// --------------------

struct MasterDisplay : OpaqueWidget {
	MixerMaster *srcMaster;
	
	MasterDisplay() {
		box.size = mm2px(math::Vec(15.4, 5.1));
	}
	
	void onButton(const event::Button &e) override {
		if (e.button == GLFW_MOUSE_BUTTON_RIGHT && e.action == GLFW_PRESS) {
			ui::Menu *menu = createMenu();

			MenuLabel *mastSetLabel = new MenuLabel();
			mastSetLabel->text = "Master settings: ";
			menu->addChild(mastSetLabel);
			
			FadeRateSlider *fadeSlider = new FadeRateSlider(&(srcMaster->fadeRate), MixerMaster::minFadeRate);
			fadeSlider->box.size.x = 200.0f;
			menu->addChild(fadeSlider);
			
			FadeProfileSlider *fadeProfSlider = new FadeProfileSlider(&(srcMaster->fadeProfile));
			fadeProfSlider->box.size.x = 200.0f;
			menu->addChild(fadeProfSlider);
			
			DimGainSlider *dimSliderItem = new DimGainSlider(srcMaster);
			dimSliderItem->box.size.x = 200.0f;
			menu->addChild(dimSliderItem);
			
			DcBlockItem *dcItem = createMenuItem<DcBlockItem>("DC blocker", CHECKMARK(srcMaster->dcBlock));
			dcItem->srcMaster = srcMaster;
			menu->addChild(dcItem);
			
			VoltLimitItem *vLimitItem = createMenuItem<VoltLimitItem>("Voltage limiter", RIGHT_ARROW);
			vLimitItem->srcMaster = srcMaster;
			menu->addChild(vLimitItem);
				
			if (srcMaster->gInfo->vuColor >= numThemes) {	
				VuColorItem *vuColItem = createMenuItem<VuColorItem>("VU Colour", RIGHT_ARROW);
				vuColItem->srcColor = &(srcMaster->vu[0].vuColorTheme);
				vuColItem->isGlobal = false;
				menu->addChild(vuColItem);
			}
			
			e.consume(this);
			return;
		}
		OpaqueWidget::onButton(e);		
	}
};

	

// Track and group displays base struct
// --------------------

static const NVGcolor DISP_COLORS[] = {nvgRGB(0xff, 0xd7, 0x14), nvgRGB(102, 183, 245), nvgRGB(140, 235, 107), nvgRGB(240, 240, 240)};// yellow, blue, green, light-gray

struct GroupAndTrackDisplayBase : LedDisplayTextField {
	bool doubleClick = false;
	GlobalInfo *gInfo = NULL;

	GroupAndTrackDisplayBase() {
		box.size = Vec(38, 16);
		textOffset = Vec(2.6f, -2.2f);
		text = "-00-";
	};
	
	// don't want background so implement adapted version here
	void draw(const DrawArgs &args) override {
		if (gInfo) {
			color = DISP_COLORS[gInfo->dispColor];
		}
		if (cursor > 4) {
			text.resize(4);
			cursor = 4;
			selection = 4;
		}
		
		// the code below is LedDisplayTextField.draw() without the background rect
		nvgScissor(args.vg, RECT_ARGS(args.clipBox));
		if (font->handle >= 0) {
			bndSetFont(font->handle);

			NVGcolor highlightColor = color;
			highlightColor.a = 0.5;
			int begin = std::min(cursor, selection);
			int end = (this == APP->event->selectedWidget) ? std::max(cursor, selection) : -1;
			bndIconLabelCaret(args.vg, textOffset.x, textOffset.y,
				box.size.x - 2*textOffset.x, box.size.y - 2*textOffset.y,
				-1, color, 12, text.c_str(), highlightColor, begin, end);

			bndSetFont(APP->window->uiFont->handle);
		}
		nvgResetScissor(args.vg);
	}
	
	// don't want spaces since leading spaces are stripped by nanovg (which oui-blendish calls), so convert to dashes
	void onSelectText(const event::SelectText &e) override {
		if (e.codepoint < 128) {
			char letter = (char) e.codepoint;
			if (letter == 0x20) {// space
				letter = 0x2D;// hyphen
			}
			std::string newText(1, letter);
			insertText(newText);
		}
		e.consume(this);	
		
		if (text.length() > 4) {
			text = text.substr(0, 4);
		}
	}
	
	void onDoubleClick(const event::DoubleClick& e) override {
		doubleClick = true;
	}
	
	void onButton(const event::Button &e) override {
		if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_RELEASE) {
			if (doubleClick) {
				doubleClick = false;
				selectAll();
			}
		}
		LedDisplayTextField::onButton(e);
	}

}; 


// Track display editable label with menu
// --------------------

struct TrackDisplay : GroupAndTrackDisplayBase {
	MixerTrack *tracks = NULL;
	int trackNumSrc;
	int *resetTrackLabelRequestPtr;
	PortWidget **inputWidgets;

	void onButton(const event::Button &e) override {
		if (e.button == GLFW_MOUSE_BUTTON_RIGHT && e.action == GLFW_PRESS) {
			ui::Menu *menu = createMenu();
			MixerTrack *srcTrack = &(tracks[trackNumSrc]);

			MenuLabel *trkSetLabel = new MenuLabel();
			trkSetLabel->text = "Track settings: " + std::string(srcTrack->trackName, 4);
			menu->addChild(trkSetLabel);
			
			GainAdjustSlider *trackGainAdjustSlider = new GainAdjustSlider(srcTrack);
			trackGainAdjustSlider->box.size.x = 200.0f;
			menu->addChild(trackGainAdjustSlider);
			
			HPFCutoffSlider *trackHPFAdjustSlider = new HPFCutoffSlider(srcTrack);
			trackHPFAdjustSlider->box.size.x = 200.0f;
			menu->addChild(trackHPFAdjustSlider);
			
			LPFCutoffSlider *trackLPFAdjustSlider = new LPFCutoffSlider(srcTrack);
			trackLPFAdjustSlider->box.size.x = 200.0f;
			menu->addChild(trackLPFAdjustSlider);
			
			FadeRateSlider *fadeSlider = new FadeRateSlider(&(srcTrack->fadeRate), MixerTrack::minFadeRate);
			fadeSlider->box.size.x = 200.0f;
			menu->addChild(fadeSlider);
			
			FadeProfileSlider *fadeProfSlider = new FadeProfileSlider(&(srcTrack->fadeProfile));
			fadeProfSlider->box.size.x = 200.0f;
			menu->addChild(fadeProfSlider);
			
			LinkFaderItem<MixerTrack> *linkFadItem = createMenuItem<LinkFaderItem<MixerTrack>>("Link fader", CHECKMARK(srcTrack->isLinked()));
			linkFadItem->srcTrkGrp = srcTrack;
			menu->addChild(linkFadItem);
			
			if (srcTrack->gInfo->directOutsMode >= 2) {
				DirectOutsTrackItem<MixerTrack> *dirTrkItem = createMenuItem<DirectOutsTrackItem<MixerTrack>>("Direct outs", RIGHT_ARROW);
				dirTrkItem->srcTrkGrp = srcTrack;
				menu->addChild(dirTrkItem);
			}

			if (srcTrack->gInfo->panLawStereo >= 2) {
				PanLawStereoItem *panLawStereoItem = createMenuItem<PanLawStereoItem>("Stereo pan mode", RIGHT_ARROW);
				panLawStereoItem->panLawStereoSrc = &(srcTrack->panLawStereo);
				panLawStereoItem->isGlobal = false;
				menu->addChild(panLawStereoItem);
			}

			if (srcTrack->gInfo->vuColor >= numThemes) {	
				VuColorItem *vuColItem = createMenuItem<VuColorItem>("VU Colour", RIGHT_ARROW);
				vuColItem->srcColor = &(srcTrack->vu[0].vuColorTheme);
				vuColItem->isGlobal = false;
				menu->addChild(vuColItem);
			}
			
			menu->addChild(new MenuLabel());// empty line

			MenuLabel *settingsALabel = new MenuLabel();
			settingsALabel->text = "Actions: " + std::string(srcTrack->trackName, 4);
			menu->addChild(settingsALabel);

			TrackReorderItem *reodrerItem = createMenuItem<TrackReorderItem>("Move to:", RIGHT_ARROW);
			reodrerItem->tracks = tracks;
			reodrerItem->trackNumSrc = trackNumSrc;
			reodrerItem->resetTrackLabelRequestPtr = resetTrackLabelRequestPtr;
			reodrerItem->inputWidgets = inputWidgets;
			menu->addChild(reodrerItem);
			
			CopyTrackSettingsItem *copyItem = createMenuItem<CopyTrackSettingsItem>("Copy track menu settings to:", RIGHT_ARROW);
			copyItem->tracks = tracks;
			copyItem->trackNumSrc = trackNumSrc;
			menu->addChild(copyItem);
			
			e.consume(this);
			return;
		}
		GroupAndTrackDisplayBase::onButton(e);
	}
	void onChange(const event::Change &e) override {
		(*((uint32_t*)(tracks[trackNumSrc].trackName))) = 0x20202020;
		for (int i = 0; i < std::min(4, (int)text.length()); i++) {
			tracks[trackNumSrc].trackName[i] = text[i];
		}
		GroupAndTrackDisplayBase::onChange(e);
	};
};


// Group display editable label with menu
// --------------------

struct GroupDisplay : GroupAndTrackDisplayBase {
	MixerGroup *srcGroup = NULL;

	void onButton(const event::Button &e) override {
		if (e.button == GLFW_MOUSE_BUTTON_RIGHT && e.action == GLFW_PRESS) {
			ui::Menu *menu = createMenu();

			MenuLabel *trkSetLabel = new MenuLabel();
			trkSetLabel->text = "Group settings: " + std::string(srcGroup->groupName, 4);
			menu->addChild(trkSetLabel);
			
			FadeRateSlider *fadeSlider = new FadeRateSlider(&(srcGroup->fadeRate), MixerGroup::minFadeRate);
			fadeSlider->box.size.x = 200.0f;
			menu->addChild(fadeSlider);
			
			FadeProfileSlider *fadeProfSlider = new FadeProfileSlider(&(srcGroup->fadeProfile));
			fadeProfSlider->box.size.x = 200.0f;
			menu->addChild(fadeProfSlider);
			
			LinkFaderItem<MixerGroup> *linkFadItem = createMenuItem<LinkFaderItem<MixerGroup>>("Link fader", CHECKMARK(srcGroup->isLinked()));
			linkFadItem->srcTrkGrp = srcGroup;
			menu->addChild(linkFadItem);
			
			if (srcGroup->gInfo->directOutsMode >= 2) {
				DirectOutsTrackItem<MixerGroup> *dirTrkItem = createMenuItem<DirectOutsTrackItem<MixerGroup>>("Direct outs", RIGHT_ARROW);
				dirTrkItem->srcTrkGrp = srcGroup;
				menu->addChild(dirTrkItem);
			}

			if (srcGroup->gInfo->panLawStereo >= 2) {
				PanLawStereoItem *panLawStereoItem = createMenuItem<PanLawStereoItem>("Stereo pan mode", RIGHT_ARROW);
				panLawStereoItem->panLawStereoSrc = &(srcGroup->panLawStereo);
				panLawStereoItem->isGlobal = false;
				menu->addChild(panLawStereoItem);
			}

			if (srcGroup->gInfo->vuColor >= numThemes) {	
				VuColorItem *vuColItem = createMenuItem<VuColorItem>("VU Colour", RIGHT_ARROW);
				vuColItem->srcColor = &(srcGroup->vu[0].vuColorTheme);
				vuColItem->isGlobal = false;
				menu->addChild(vuColItem);
			}
			
			e.consume(this);
			return;
		}
		GroupAndTrackDisplayBase::onButton(e);
	}
	void onChange(const event::Change &e) override {
		(*((uint32_t*)(srcGroup->groupName))) = 0x20202020;
		for (int i = 0; i < std::min(4, (int)text.length()); i++) {
			srcGroup->groupName[i] = text[i];
		}
		GroupAndTrackDisplayBase::onChange(e);
	};
};


// Group select display, non-editable label, but will respond to hover key press
// --------------------

struct GroupSelectDisplay : LedDisplayChoice {
	MixerTrack *srcTrack = NULL;

	GroupSelectDisplay() {
		box.size = Vec(18, 16);
		textOffset = math::Vec(6.6f, 11.7f);
		bgColor.a = 0.0f;
		text = "-";
	};
	
	void draw(const DrawArgs &args) override {
		if (srcTrack) {
			int grp = srcTrack->getGroup();
			text[0] = (char)(grp >= 1 &&  grp <= 4 ? grp + 0x30 : '-');
			text[1] = 0;
			color = DISP_COLORS[srcTrack->gInfo->dispColor];
		}
		LedDisplayChoice::draw(args);
	};

	void onHoverKey(const event::HoverKey &e) override {
		if (e.action == GLFW_PRESS) {
			if (e.key >= GLFW_KEY_1 && e.key <= GLFW_KEY_4) {
				srcTrack->setGroup(e.key - GLFW_KEY_0);
			}
			else if (e.key >= GLFW_KEY_KP_1 && e.key <= GLFW_KEY_KP_4) {
				srcTrack->setGroup(e.key - GLFW_KEY_KP_0);
			}
			else if ( ((e.mods & RACK_MOD_MASK) == 0) && (
					(e.key >= GLFW_KEY_A && e.key <= GLFW_KEY_Z) ||
					e.key == GLFW_KEY_SPACE || e.key == GLFW_KEY_MINUS || 
					e.key == GLFW_KEY_0 || e.key == GLFW_KEY_KP_0 || 
					(e.key >= GLFW_KEY_5 && e.key <= GLFW_KEY_9) || 
					(e.key >= GLFW_KEY_KP_5 && e.key <= GLFW_KEY_KP_9) ) ){
				srcTrack->setGroup(0);
			}
		}
	}
};


// Buttons that have their own Tigger mechanism built in and don't need to be polled by the dsp engine, 
//   they will do thier own processing in here and update their source automatically
// --------------------

struct DynGroupMinusButtonNotify : DynGroupMinusButton {
	Trigger buttonTrigger;
	MixerTrack *srcTrack;
	
	void onChange(const event::Change &e) override {// called after value has changed
		DynGroupMinusButton::onChange(e);
		if (paramQuantity) {
			if (buttonTrigger.process(paramQuantity->getValue() * 2.0f)) {
				int group = srcTrack->getGroup();
				if (group == 0) group = 4;
				else group--;
				srcTrack->setGroup(group);
			}
		}		
	}
};


struct DynGroupPlusButtonNotify : DynGroupPlusButton {
	Trigger buttonTrigger;
	MixerTrack *srcTrack;
	
	void onChange(const event::Change &e) override {// called after value has changed
		DynGroupPlusButton::onChange(e);
		if (paramQuantity) {
			if (buttonTrigger.process(paramQuantity->getValue() * 2.0f)) {
				int group = srcTrack->getGroup();
				if (group == 4) group = 0;
				else group++;	
				srcTrack->setGroup(group);
			}
		}		
	}
};


// Special solo button with mutex feature (ctrl-click)

struct DynSoloButtonMutex : DynSoloButton {
	Param *soloParams;// 19 (or 15) params in here must be cleared when mutex solo performed on a group (track)
	unsigned long soloMutexUnclickMemory;// for ctrl-unclick. Invalid when soloMutexUnclickMemory == -1
	int soloMutexUnclickMemorySize;// -1 when nothing stored

	void onButton(const event::Button &e) override {
		if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS) {
			if (((APP->window->getMods() & RACK_MOD_MASK) == RACK_MOD_CTRL)) {
				int soloParamId = paramQuantity->paramId - TRACK_SOLO_PARAMS;
				bool isTrack = (soloParamId < 16);
				int end = 16 + (isTrack ? 0 : 4);
				bool turningOnSolo = soloParams[soloParamId].getValue() < 0.5f;
				
				
				if (turningOnSolo) {// ctrl turning on solo: memorize solo states and clear all other solos 
					// memorize solo states in case ctrl-unclick happens
					soloMutexUnclickMemorySize = end;
					soloMutexUnclickMemory = 0;
					for (int i = 0; i < end; i++) {
						if (soloParams[i].getValue() > 0.5f) {
							soloMutexUnclickMemory |= (1 << i);
						}
					}
					
					// clear 19 (or 15) solos 
					for (int i = 0; i < end; i++) {
						if (soloParamId != i) {
							soloParams[i].setValue(0.0f);
						}
					}
					
				}
				else {// ctrl turning off solo: recall stored solo states
					// reinstate 19 (or 15) solos 
					if (soloMutexUnclickMemorySize >= 0) {
						for (int i = 0; i < soloMutexUnclickMemorySize; i++) {
							if (soloParamId != i) {
								soloParams[i].setValue((soloMutexUnclickMemory & (1 << i)) != 0 ? 1.0f : 0.0f);
							}
						}
						soloMutexUnclickMemorySize = -1;// nothing in soloMutexUnclickMemory
					}
				}
				e.consume(this);
				return;
			}
			else {
				soloMutexUnclickMemorySize = -1;// nothing in soloMutexUnclickMemory
				if ((APP->window->getMods() & RACK_MOD_MASK) == (RACK_MOD_CTRL | GLFW_MOD_SHIFT)) {
					for (int i = 0; i < 20; i++) {
						if (i != paramQuantity->paramId - TRACK_SOLO_PARAMS) {
							soloParams[i].setValue(0.0f);
						}
					}
					e.consume(this);
					return;
				}
			}
		}
		DynSoloButton::onButton(e);		
	}
};


// switch with dual display types (for Mute/Fade buttons)
// --------------------

struct DynamicSVGSwitchDual : SvgSwitch {
	int* mode = NULL;
    float* type = NULL;// mute when < minFadeRate, fade when >= minFadeRate
    int oldMode = -1;
    float oldType = -1.0f;
	std::vector<std::shared_ptr<Svg>> framesAll;
	std::vector<std::string> frameAltNames;
	
	void addFrameAll(std::shared_ptr<Svg> svg);
    void addFrameAlt(std::string filename) {frameAltNames.push_back(filename);}
	void step() override;
};

void DynamicSVGSwitchDual::addFrameAll(std::shared_ptr<Svg> svg) {
    framesAll.push_back(svg);
	if (framesAll.size() == 2) {
		addFrame(framesAll[0]);
		addFrame(framesAll[1]);
	}
}

void DynamicSVGSwitchDual::step() {
    if( mode != NULL && type != NULL && ((*mode != oldMode) || (*type != oldType)) ) {
		if (!frameAltNames.empty()) {
			for (std::string strName : frameAltNames) {
				framesAll.push_back(APP->window->loadSvg(strName));
			}
			frameAltNames.clear();
		}
		int typeOffset = (*type < MixerTrack::minFadeRate ? 0 : 2);
		frames[0]=framesAll[(*mode) * 4 + typeOffset + 0];
		frames[1]=framesAll[(*mode) * 4 + typeOffset + 1];
        oldMode = *mode;
        oldType = *type;
		onChange(*(new event::Change()));// required because of the way SVGSwitch changes images, we only change the frames above.
		fb->dirty = true;// dirty is not sufficient when changing via frames assignments above (i.e. onChange() is required)
    }
	SvgSwitch::step();
}

struct DynMuteFadeButton : DynamicSVGSwitchDual {
	DynMuteFadeButton() {
		momentary = false;
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mute-off.svg")));
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mute-on.svg")));
		addFrameAlt(asset::plugin(pluginInstance, "res/comp/fade-off.svg"));
		addFrameAlt(asset::plugin(pluginInstance, "res/comp/fade-on.svg"));
		shadow->opacity = 0.0;
	}
};

// linked faders
// --------------------

struct DynSmallFaderWithLink : DynSmallFader {
	GlobalInfo *gInfo;
	Param *faderParams = NULL;
	float lastValue = -1.0f;
	
	void onButton(const event::Button &e) override {
		int faderIndex = paramQuantity->paramId - TRACK_FADER_PARAMS;
		if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS) {
			if ((APP->window->getMods() & RACK_MOD_MASK) == GLFW_MOD_ALT) {
				gInfo->toggleLinked(faderIndex);
				e.consume(this);
				return;
			}
			else if ((APP->window->getMods() & RACK_MOD_MASK) == (GLFW_MOD_ALT | GLFW_MOD_SHIFT)) {
				gInfo->linkBitMask = 0;
				e.consume(this);
				return;
			}
		}
		DynSmallFader::onButton(e);		
	}

	void draw(const DrawArgs &args) override {
		DynSmallFader::draw(args);
		if (paramQuantity) {
			int faderIndex = paramQuantity->paramId - TRACK_FADER_PARAMS;
			if (gInfo->isLinked(faderIndex)) {
				float v = paramQuantity->getScaledValue();
				float offsetY = handle->box.size.y / 2.0f;
				float ypos = math::rescale(v, 0.f, 1.f, minHandlePos.y, maxHandlePos.y) + offsetY;
				
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg, 0, ypos);
				nvgLineTo(args.vg, box.size.x, ypos);
				nvgClosePath(args.vg);
				nvgStrokeColor(args.vg, SCHEME_RED);
				nvgStrokeWidth(args.vg, mm2px(0.4f));
				nvgStroke(args.vg);
			}
		}
	}
};


// knob with color theme arc
// --------------------

struct DynSmallKnobGreyWithPanCol : DynSmallKnobGrey {
	GlobalInfo *gInfo = NULL;
	
	void draw(const DrawArgs &args) override {
		static const float a0 = 3.0f * M_PI / 2.0f;
		
		DynSmallKnobGrey::draw(args);
		if (paramQuantity && gInfo) {
			float normalizedParam = paramQuantity->getScaledValue();
			if (normalizedParam != 0.5f) {
				float a1 = math::rescale(normalizedParam, 0.f, 1.f, minAngle, maxAngle) + a0;
				Vec cVec = box.size.div(2.0f);
				float r = box.size.x / 2.0f + 2.6f;// arc radius
				int dir = a0 < a1 ? NVG_CW : NVG_CCW;
				nvgBeginPath(args.vg);
				nvgArc(args.vg, cVec.x, cVec.y, r, a0, a1, dir);
				nvgStrokeWidth(args.vg, 1.5f);// arc thickness
				nvgStrokeColor(args.vg, DISP_COLORS[gInfo->dispColor]);// arc color, same as displays
				nvgStroke(args.vg);
			}
		}
	}
};



#endif
