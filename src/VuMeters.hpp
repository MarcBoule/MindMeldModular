//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#ifndef MMM_VUMETERS_HPP
#define MMM_MIXERWIDGETS_HPP



// Colors

static const NVGcolor DISP_COLORS[] = {
	nvgRGB(0xff, 0xd7, 0x14),// yellow
	nvgRGB(240, 240, 240),// light-gray			
	nvgRGB(140, 235, 107),// green
	nvgRGB(102, 245, 207),// aqua
	nvgRGB(102, 207, 245),// cyan
	nvgRGB(102, 183, 245),// blue
	nvgRGB(177, 107, 235)// purple
};

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
	
	// instantiator must setup:
	VuMeterAllDual *srcLevels;// from 0 to 10 V, with 10 V = 0dB (since -10 to 10 is the max)
	int8_t *colorThemeGlobal;
	int8_t *colorThemeLocal;
	float faderMaxLinearGain;
	float faderScalingExponent;
	
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
	
	void prepareYellowAndRedThresholds(float yellowMinDb, float redMinDb) {
		float maxLin = std::pow(faderMaxLinearGain, 1.0f / faderScalingExponent);
		float yellowLin = std::pow(std::pow(10.0f, yellowMinDb / 20.0f), 1.0f / faderScalingExponent);
		yellowThreshold = barY * (yellowLin / maxLin);
		float redLin = std::pow(std::pow(10.0f, redMinDb / 20.0f), 1.0f / faderScalingExponent);
		redThreshold = barY * (redLin / maxLin);
	}
	
	
	void processPeakHold() {
		long newTime = time(0);
		if ( (newTime != oldTime) && ((newTime & 0x1) == 0) ) {
			oldTime = newTime;
			peakHold[0] = 0.0f;
			peakHold[1] = 0.0f;
		}		
		for (int i = 0; i < 2; i++) {
			if (srcLevels->getPeak(i) > peakHold[i]) {
				peakHold[i] = srcLevels->getPeak(i);
			}
		}
	}

	
	void draw(const DrawArgs &args) override {
		processPeakHold();
		
		colorTheme = (*colorThemeGlobal >= numThemes) ? *colorThemeLocal : *colorThemeGlobal;
		
		// PEAK
		drawVu(args, srcLevels->getPeak(0), 0, 0);
		drawVu(args, srcLevels->getPeak(1), barX + gapX, 0);

		// RMS
		drawVu(args, srcLevels->getRms(0), 0, 1);
		drawVu(args, srcLevels->getRms(1), barX + gapX, 1);
		
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
	}
	
	// used for RMS or PEAK
	void drawVu(const DrawArgs &args, float vuValue, float posX, int colorIndex) override {
		if (posX == 0) { // draw the separator for master since depends on softclip on/off. draw only once per vu pair
			nvgBeginPath(args.vg);
			nvgRect(args.vg, 0 - 1, barY - redThreshold - sepYmaster, box.size.x + 2, sepYmaster);
			nvgFillColor(args.vg, nvgRGB(53, 53, 53));
			nvgFill(args.vg);
		}

		if (vuValue >= epsilon) {
			float vuHeight = vuValue / (faderMaxLinearGain * zeroDbVoltage);
			vuHeight = std::pow(vuHeight, 1.0f / faderScalingExponent);
			vuHeight = std::min(vuHeight, 1.0f);// normalized is now clamped
			vuHeight *= barY;
			
			float peakHoldVal = (posX == 0 ? peakHold[0] : peakHold[1]);
			if (vuHeight > redThreshold || peakHoldVal > hardRedVoltage) {
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
			if (vuHeight > redThreshold || peakHoldVal > hardRedVoltage) {
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