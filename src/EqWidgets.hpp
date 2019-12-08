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


// Labels
// --------------------

struct TrackLabel : LedDisplayChoice {
	// int8_t* dispColorPtr = NULL;
	// int8_t* dispColorLocalPtr;
	char* trackLabelsSrc;
	Param* trackParamSrc;
	
	TrackLabel() {
		box.size = mm2px(Vec(10.6f, 5.0f));
		textOffset = Vec(4.2f, 11.3f);
		text = "-00-";
	};
	
	// void draw(const DrawArgs &args) override {
		// if (dispColorPtr) {
			// int colorIndex = *dispColorPtr < 7 ? *dispColorPtr : *dispColorLocalPtr;
			// color = DISP_COLORS[colorIndex];
		// }	
		// LedDisplayChoice::draw(args);
	// }
	
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

			MenuLabel *trkSelLabel = new MenuLabel();
			trkSelLabel->text = "Select Track: ";
			menu->addChild(trkSelLabel);
			
			for (int i = 0; i < 24; i++) {
				int currTrk = (int)(trackParamSrc->getValue() + 0.5f);
				TrackSelectItem *tsItem = createMenuItem<TrackSelectItem>(std::string(&(trackLabelsSrc[i * 4]), 4), CHECKMARK(i == currTrk));
				tsItem->trackParamSrc = trackParamSrc;
				tsItem->trackNumber = i;
				tsItem->disabled = i == currTrk;
				menu->addChild(tsItem);
			}
			
			e.consume(this);
			return;
		}
		LedDisplayChoice::onButton(e);		
	}	
};


// Knobs and buttons
// --------------------
static const NVGcolor COL_GRAY = nvgRGB(111, 111, 111);
static const NVGcolor COL_GREEN = nvgRGB(127, 200, 68);
static const NVGcolor COL_RED = nvgRGB(247, 23, 41);


struct TrackKnob : DynSnapKnob {
	// user must setup:
	int* updateTrackLabelRequestSrc = NULL;
	TrackEq* trackEqsSrc;
	
	// internal
	int refresh = 0;// 0 to 23
	float px[24];
	float py[24];
	Vec cVec;
	float totAng;
	
	TrackKnob() {
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/eq/track-knob.svg")));
		cVec = box.size.div(2.0f);
		totAng = maxAngle - minAngle;
	}
	void onChange(const event::Change& e) override {
		if (updateTrackLabelRequestSrc) {
			*updateTrackLabelRequestSrc = 1;
		}
		DynSnapKnob::onChange(e);
	}
	
	
	void draw(const DrawArgs &args) override {
		static const float radius = 18.0f;
		DynamicSVGKnob::draw(args);
		if (paramQuantity) {
			float maxTrack = paramQuantity->getMaxValue();
			int selectedTrack = (int)(paramQuantity->getValue() + 0.5f);
			float deltAng = totAng / maxTrack;
			float ang = minAngle;
			for (int trk = 0; trk <= (int)(maxTrack + 0.5f); trk++) {
				//if (refresh == trk) { // TODO optimize this more
					px[trk] = cVec.x + radius * std::sin(ang);
					py[trk] = cVec.y - radius * std::cos(ang);
				//}
				nvgBeginPath(args.vg);
				nvgCircle(args.vg, px[trk], py[trk], 1.1f);
				if (trk == selectedTrack) {
					nvgFillColor(args.vg, SCHEME_WHITE);
				}
				else if (trackEqsSrc[trk].getActive()) {
					nvgFillColor(args.vg, COL_GREEN);
				}
				else if (trackEqsSrc[trk].isNonDefaultState()) {
					nvgFillColor(args.vg, COL_RED);
				}
				else {
					nvgFillColor(args.vg, COL_GRAY);
				}
				nvgFill(args.vg);		
				ang += deltAng;
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

struct ActiveSwitch : MmSwitch {
	Param* trackParamSrc;
	TrackEq* trackEqsSrc;

	void onChange(const event::Change& e) override {
		MmSwitch::onChange(e);
		if (paramQuantity) {
			int currTrk = (int)(trackParamSrc->getValue() + 0.5f);
			trackEqsSrc[currTrk].setActive(paramQuantity->getValue() > 0.5f);
		}
	}
};

struct BandKnob : DynKnob {
	Param* trackParamSrc;
	TrackEq* trackEqsSrc;
	
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


#endif