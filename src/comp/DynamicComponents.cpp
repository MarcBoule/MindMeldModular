//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "DynamicComponents.hpp"


// Dynamic PanelBorder

void DynamicPanelBorder::draw(const DrawArgs& args) {
	if (mode != NULL && *mode == 0) {
		NVGcolor borderColor = nvgRGBAf(0.5, 0.5, 0.5, 0.5);
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0.5, 0.5, box.size.x - 1.0, box.size.y - 1.0);
		nvgStrokeColor(args.vg, borderColor);
		nvgStrokeWidth(args.vg, 1.0);
		nvgStroke(args.vg);
	}
}
void DynamicPanelBorder::step() {
	if (mode != NULL && *mode != oldMode) {
        oldMode = *mode;
		SvgPanel *panelParent = dynamic_cast<SvgPanel*>(parent);
		if (panelParent)
			panelParent->dirty = true;
    }
	TransparentWidget::step();
}



// Dynamic SVGKnob

void DynamicSVGKnob::addFrameAll(std::shared_ptr<Svg> svg) {
    framesAll.push_back(svg);
	if (framesAll.size() == 1) {
		setSvg(svg);
	}
}

void DynamicSVGKnob::step() {
    if(mode != NULL && *mode != oldMode) {
        if (*mode > 0 && !frameAltName.empty()) {// JIT loading of alternate skin
			framesAll.push_back(APP->window->loadSvg(frameAltName));
			frameAltName.clear();// don't reload!
		}
		setSvg(framesAll[*mode]);
        oldMode = *mode;
		fb->dirty = true;
    }
	SvgKnob::step();
}

