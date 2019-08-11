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
static const NVGcolor VU_GREEN_TOP[3][2] =  {{nvgRGB(110, 130, 70), 	nvgRGB(178, 235, 107)}, // green: peak (darker), rms (lighter)
										{nvgRGB(64, 155, 160), 	nvgRGB(102, 233, 245)}, // blue: peak (darker), rms (lighter)
										{nvgRGB(110, 70, 130), 	nvgRGB(178, 107, 235)}};// purple: peak (darker), rms (lighter)
static const NVGcolor VU_GREEN_BOT[3][2] =  {{nvgRGB(50, 130, 70), 	nvgRGB(97, 235, 107)}, // green: peak (darker), rms (lighter)
										{nvgRGB(64, 108, 160), 	nvgRGB(102, 183, 245)}, // blue: peak (darker), rms (lighter)
										{nvgRGB(50, 70, 130), 	nvgRGB(97, 107, 235)}};// purple: peak (darker), rms (lighter)
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
	int *colorTheme;
	
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
		faderMaxLinearGain = MixerTrack::trackFaderMaxLinearGain;
		faderScalingExponent = MixerTrack::trackFaderScalingExponent;
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
			
			NVGpaint gradGreen = nvgLinearGradient(args.vg, 0, barY - redThreshold, 0, barY, VU_GREEN_TOP[*colorTheme][colorIndex], VU_GREEN_BOT[*colorTheme][colorIndex]);
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
				NVGpaint gradGreen = nvgLinearGradient(args.vg, 0, barY - redThreshold, 0, barY, VU_GREEN_TOP[*colorTheme][1], VU_GREEN_BOT[*colorTheme][1]);
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
		faderMaxLinearGain = MixerMaster::masterFaderMaxLinearGain;
		faderScalingExponent = MixerMaster::masterFaderScalingExponent;
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
				NVGpaint gradGreen = nvgLinearGradient(args.vg, 0, barY - yellowThreshold, 0, barY, VU_GREEN_TOP[*colorTheme][colorIndex], VU_GREEN_BOT[*colorTheme][colorIndex]);
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
				NVGpaint gradGreen = nvgLinearGradient(args.vg, 0, barY - yellowThreshold, 0, barY, VU_GREEN_TOP[*colorTheme][1], VU_GREEN_BOT[*colorTheme][1]);
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
		float vertPos = box.size.y - box.size.y * std::pow((*srcFadeGain), 1.0f / faderScalingExponent) * fadePosNormalized ;// in px
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
		faderMaxLinearGain = MixerTrack::trackFaderMaxLinearGain;
		faderScalingExponent = MixerTrack::trackFaderScalingExponent;
		minFadeRate = MixerTrack::minFadeRate;
		prepareMaxFader();
	}
};
struct FadePointerGroup : FadePointerBase {
	FadePointerGroup() {
		box.size = mm2px(math::Vec(2.8, 42));
		faderMaxLinearGain = MixerGroup::groupFaderMaxLinearGain;
		faderScalingExponent = MixerGroup::groupFaderScalingExponent;
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
			
			DcBlockItem *dcItem = createMenuItem<DcBlockItem>("DC blocker", CHECKMARK(srcMaster->dcBlock));
			dcItem->srcMaster = srcMaster;
			menu->addChild(dcItem);
			
			VoltLimitItem *vLimitItem = createMenuItem<VoltLimitItem>("Voltage limiter", RIGHT_ARROW);
			vLimitItem->srcMaster = srcMaster;
			menu->addChild(vLimitItem);
				
			e.consume(this);
			return;
		}
		OpaqueWidget::onButton(e);		
	}
};

	

// Track and group displays base struct
// --------------------

struct GroupAndTrackDisplayBase : LedDisplayTextField {

	GroupAndTrackDisplayBase() {
		box.size = Vec(38, 16);
		textOffset = Vec(2.6f, -2.2f);
		text = "-00-";
	};
	
	// don't want background so implement adapted version here
	void draw(const DrawArgs &args) override {
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
}; 


// Track display editable label with menu
// --------------------

struct TrackDisplay : GroupAndTrackDisplayBase {
	MixerTrack *srcTracks = NULL;
	MixerTrack *srcTrack = NULL;

	void onButton(const event::Button &e) override {
		if (e.button == GLFW_MOUSE_BUTTON_RIGHT && e.action == GLFW_PRESS) {
			ui::Menu *menu = createMenu();

			MenuLabel *trkSetLabel = new MenuLabel();
			trkSetLabel->text = "Track settings: ";
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
			
			menu->addChild(new MenuLabel());// empty line

			TrackSettingsCopyItem *cpyItem = createMenuItem<TrackSettingsCopyItem>("Copy track settings", "");
			cpyItem->srcTrack = srcTrack;
			menu->addChild(cpyItem);
			
			TrackSettingsPasteItem *pstItem = createMenuItem<TrackSettingsPasteItem>("Paste track settings", "");
			pstItem->srcTrack = srcTrack;
			menu->addChild(pstItem);
			
			TrackReorderItem *reodrerItem = createMenuItem<TrackReorderItem>("Move to:", RIGHT_ARROW);
			reodrerItem->srcTracks = srcTracks;
			reodrerItem->srcTrack = srcTrack;
			menu->addChild(reodrerItem);
			
			e.consume(this);
			return;
		}
		LedDisplayTextField::onButton(e);
	}
	void onChange(const event::Change &e) override {
		(*((uint32_t*)(srcTrack->trackName))) = 0x20202020;
		for (int i = 0; i < std::min(4, (int)text.length()); i++) {
			srcTrack->trackName[i] = text[i];
		}
		LedDisplayTextField::onChange(e);
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
			trkSetLabel->text = "Group settings: ";
			menu->addChild(trkSetLabel);
			
			FadeRateSlider *fadeSlider = new FadeRateSlider(&(srcGroup->fadeRate), MixerGroup::minFadeRate);
			fadeSlider->box.size.x = 200.0f;
			menu->addChild(fadeSlider);
			
			e.consume(this);
			return;
		}
		LedDisplayTextField::onButton(e);
	}
	void onChange(const event::Change &e) override {
		(*((uint32_t*)(srcGroup->groupName))) = 0x20202020;
		for (int i = 0; i < std::min(4, (int)text.length()); i++) {
			srcGroup->groupName[i] = text[i];
		}
		LedDisplayTextField::onChange(e);
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
	
	//void onButton(const event::Button &e) override {};
	void draw(const DrawArgs &args) override {
		if (srcTrack) {
			int grp = srcTrack->group;
			text[0] = (char)(grp >= 1 &&  grp <= 4 ? grp + 0x30 : '-');
			text[1] = 0;
		}
		LedDisplayChoice::draw(args);
	};

	void onHoverKey(const event::HoverKey &e) override {
		if (e.action == GLFW_PRESS) {
			if (e.key >= GLFW_KEY_1 && e.key <= GLFW_KEY_4) {
				srcTrack->group = e.key - GLFW_KEY_0;
			}
			else if (e.key >= GLFW_KEY_KP_1 && e.key <= GLFW_KEY_KP_4) {
				srcTrack->group = e.key - GLFW_KEY_KP_0;
			}
			else if ( ((e.mods & RACK_MOD_MASK) == 0) && (
					(e.key >= GLFW_KEY_A && e.key <= GLFW_KEY_Z) ||
					e.key == GLFW_KEY_SPACE || e.key == GLFW_KEY_MINUS || 
					e.key == GLFW_KEY_0 || e.key == GLFW_KEY_KP_0 || 
					(e.key >= GLFW_KEY_5 && e.key <= GLFW_KEY_9) || 
					(e.key >= GLFW_KEY_KP_5 && e.key <= GLFW_KEY_KP_9) ) ){
				srcTrack->group = 0;
			}
		}
	}
};


// Buttons that have their own Tigger mechanism built in and don't need to be polled by the dsp engine, 
//   they will do thier own processing in here and update their source automatically
// --------------------

struct DynGroupMinusButtonNotify : DynGroupMinusButton {
	Trigger buttonTrigger;
	int *srcGroup = NULL;
	
	void onChange(const event::Change &e) override {// called after value has changed
		DynGroupMinusButton::onChange(e);
		if (paramQuantity && srcGroup) {
			if (buttonTrigger.process(paramQuantity->getValue())) {
				if ((*srcGroup) == 0) (*srcGroup) = 4;
				else (*srcGroup)--;		
			}
		}		
	}
};


struct DynGroupPlusButtonNotify : DynGroupPlusButton {
	Trigger buttonTrigger;
	int *srcGroup = NULL;
	
	void onChange(const event::Change &e) override {// called after value has changed
		DynGroupPlusButton::onChange(e);
		if (paramQuantity && srcGroup) {
			if (buttonTrigger.process(paramQuantity->getValue())) {
				if ((*srcGroup) == 4) (*srcGroup) = 0;
				else (*srcGroup)++;	
			}
		}		
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


	

#endif
