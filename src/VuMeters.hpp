//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#ifndef MMM_VUMETERS_HPP
#define MMM_VUMETERS_HPP

#include "MindMeldModular.hpp"


// VuMeter signal processing code for peak/rms
// ----------------------------------------------------------------------------

// Code below adapted from Andrew Belt's dsp::VuMeter2 struct in VCV Rack's source code

enum VuIds {VU_PEAK_L, VU_PEAK_R, VU_RMS_L, VU_RMS_R};

struct VuMeterAllDual {
	float vuValues[4];// organized according to VuIds
	static constexpr float lambda = 30.f;// Inverse time constant in 1/seconds

	void reset() {
		for (int i = 0; i < 4; i++) {
			vuValues[i] = 0.0f;
		}
	}

	void process(float deltaTime, const float *values) {// L and R
		for (int i = 0; i < 2; i++) {
			// RMS
			float valueSquared = std::pow(values[i], 2);
			vuValues[VU_RMS_L + i] += (valueSquared - vuValues[VU_RMS_L + i]) * lambda * deltaTime;

			// PEAK
			float valueAbs = std::fabs(values[i]);
			if (valueAbs >= vuValues[VU_PEAK_L + i]) {
				vuValues[VU_PEAK_L + i] = valueAbs;
			}
			else {
				vuValues[VU_PEAK_L + i] += (valueAbs - vuValues[VU_PEAK_L + i]) * lambda * deltaTime;
			}
		}
	}
	
	float getPeak(int chan) {// chan0 is L, chan1 is R
		return vuValues[VU_PEAK_L + chan];
	}
	float getRms(int chan) {// chan0 is L, chan1 is R
		return std::sqrt(vuValues[VU_RMS_L + chan]);
	}
};



// VuMeter menu for color selection
// ----------------------------------------------------------------------------

struct VuColorItem : MenuItem {
	int8_t *srcColor;
	bool isGlobal;// true when this is in the context menu of module, false when it is in a track/group/master context menu

	struct VuColorSubItem : MenuItem {
		int8_t *srcColor;
		int setVal;
		void onAction(const event::Action &e) override {
			*srcColor = setVal;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		std::string vuColorNames[6] = {
			"Green (default)",
			"Aqua",
			"Cyan",
			"Blue",
			"Purple",
			"Set per track"
		};
		
		for (int i = 0; i < (isGlobal ? 6 : 5); i++) {
			VuColorSubItem *vuColItem = createMenuItem<VuColorSubItem>(vuColorNames[i], CHECKMARK(*srcColor == i));
			vuColItem->srcColor = srcColor;
			vuColItem->setVal = i;
			menu->addChild(vuColItem);
		}

		return menu;
	}
};



// VuMeter displays (and colors)
// ----------------------------------------------------------------------------

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

static const NVGcolor FADE_POINTER_FILL = nvgRGB(255, 106, 31);
static const int greyArc = 120;


// Base struct
struct VuMeterBase : OpaqueWidget {
	static constexpr float epsilon = 0.0001f;// don't show VUs below 0.1mV
	static constexpr float peakHoldThick = 1.0f;// in px
	static const int faderScalingExponent = 3;
	static constexpr float faderMaxLinearGain = 2.0f;
	
	// instantiator must setup:
	VuMeterAllDual *srcLevels;// from 0 to 10 V, with 10 V = 0dB (since -10 to 10 is the max)
	int8_t *colorThemeGlobal;
	int8_t *colorThemeLocal;
	
	// derived class must setup:
	float gapX;// in px
	float barX;// in px
	float barY;// in px
	// box.size // inherited from OpaqueWidget, no need to declare
	float zeroDbVoltage;
	
	// local 
	float peakHold[2] = {0.0f, 0.0f};
	long oldTime = -1;
	float yellowThreshold;// in px, before vertical inversion
	float redThreshold;// in px, before vertical inversion
	int colorTheme;
	
	
	VuMeterBase() {
	}
	
	// derived class must call after setting its constructor values:
	void prepareYellowAndRedThresholds(float yellowMinDb, float redMinDb);
	
	void processPeakHold();
	void draw(const DrawArgs &args) override;
	
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
	void drawVu(const DrawArgs &args, float vuValue, float posX, int colorIndex) override;
	
	void drawPeakHold(const DrawArgs &args, float holdValue, float posX) override;

};



// 3.8mm x 60mm VU for master
// --------------------

struct VuMeterMaster : VuMeterBase {
	int* clippingPtr;
	int oldClipping = 1;
	float hardRedVoltage;
	
	VuMeterMaster() {
		gapX = mm2px(0.6);
		barX = mm2px(1.6);
		barY = mm2px(60.0);
		box.size = Vec(barX * 2 + gapX, barY);
		zeroDbVoltage = 10.0f;// V
		hardRedVoltage = 10.0f;
		prepareYellowAndRedThresholds(-6.0f, 0.0f);// dB
	}
	
	// used for RMS or PEAK
	void drawVu(const DrawArgs &args, float vuValue, float posX, int colorIndex) override;
	
	void drawPeakHold(const DrawArgs &args, float holdValue, float posX) override;

	void step() override {
		if (*clippingPtr != oldClipping) {
			oldClipping = *clippingPtr;
			if (*clippingPtr == 0) {// soft
				prepareYellowAndRedThresholds(-4.43697499f, 1.58362492f);// dB (6V and 12V respectively)
				hardRedVoltage = 12.0f;
			}
			else {// hard
				prepareYellowAndRedThresholds(-6.0f, 0.0f);// dB (5V and 0V respectively)
				hardRedVoltage = 10.0f;
			}
		}
	}
};



// 2.8mm x 30mm VU for aux returns
// --------------------

struct VuMeterAux : VuMeterTrack {//
	VuMeterAux() {
		barY = mm2px(30.0);
		box.size = Vec(barX * 2 + gapX, barY);
	}
};

#endif
