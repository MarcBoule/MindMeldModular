//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#ifndef MMM_EQWIDGETS_HPP
#define MMM_EQWIDGETS_HPP

#include "EqMenus.hpp"


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
				menu->addChild(tsItem);
			}
			
			e.consume(this);
			return;
		}
		LedDisplayChoice::onButton(e);		
	}	
};


// Knobs and buttons

struct TrackKnob : DynSnapKnob {
	int* updateTrackLabelRequestSrc = NULL;
	
	TrackKnob() {
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/knob-grey.svg")));
	}
	void onChange(const event::Change& e) override {
		if (updateTrackLabelRequestSrc) {
			*updateTrackLabelRequestSrc = 1;
		}
		DynSnapKnob::onChange(e);
	}
};

#endif