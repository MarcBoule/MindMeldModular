//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//See ./LICENSE.txt for all licenses
//***********************************************************************************************

#ifndef IM_GENERICCOMP_HPP
#define IM_GENERICCOMP_HPP


#include "rack.hpp"

using namespace rack;

extern Plugin *pluginInstance;


// Component offset constants

// none



// Variations on existing knobs, lights, etc

// Borders

struct PanelBorderNoLeft : widget::TransparentWidget {
	void draw(const DrawArgs& args) override {
		NVGcolor borderColor = nvgRGBAf(0.5, 0.5, 0.5, 0.5);
		nvgBeginPath(args.vg);
		//nvgRect(args.vg, 0.5, 0.5, box.size.x - 1.0, box.size.y - 1.0);
		// top
		nvgMoveTo(args.vg, 0.5, 0.5);
		nvgLineTo(args.vg, box.size.x - 1.0, 0.5);
		// bot
		nvgMoveTo(args.vg, 0.5, box.size.y - 1.0);
		nvgLineTo(args.vg, box.size.x - 1.0, box.size.y - 1.0);
		// right
		nvgMoveTo(args.vg, box.size.x - 1.0, 0.5);
		nvgLineTo(args.vg, box.size.x - 1.0, box.size.y - 1.0);
		
		nvgStrokeColor(args.vg, borderColor);
		nvgStrokeWidth(args.vg, 1.0);
		nvgStroke(args.vg);
	}
};


// Screws

// none



// Ports

// none



// Buttons and switches

// none



// Knobs

// none



// Lights

// none





#endif