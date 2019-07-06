//***********************************************************************************************
//MBSB: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//See ./LICENSE.txt for all licenses
//***********************************************************************************************


#include "DynamicComponents.hpp"



// Dynamic SVGPort

void DynamicSVGPort::addFrame(std::shared_ptr<Svg> svg) {
    frames.push_back(svg);
    if(frames.size() == 1) {
        SvgPort::setSvg(svg);
	}
}

void DynamicSVGPort::step() {
    if(mode != NULL && *mode != oldMode) {
        if (*mode > 0 && !frameAltName.empty()) {// JIT loading of alternate skin
			frames.push_back(APP->window->loadSvg(frameAltName));
			frameAltName.clear();// don't reload!
		}
        sw->setSvg(frames[*mode]);
        oldMode = *mode;
        fb->dirty = true;
    }
	SvgPort::step();
}



// Dynamic SVGSwitch

void DynamicSVGSwitch::addFrameAll(std::shared_ptr<Svg> svg) {
    framesAll.push_back(svg);
	if (framesAll.size() == 2) {
		addFrame(framesAll[0]);
		addFrame(framesAll[1]);
	}
}

void DynamicSVGSwitch::step() {
    if(mode != NULL && *mode != oldMode) {
        if (*mode > 0 && !frameAltName0.empty() && !frameAltName1.empty()) {// JIT loading of alternate skin
			framesAll.push_back(APP->window->loadSvg(frameAltName0));
			framesAll.push_back(APP->window->loadSvg(frameAltName1));
			frameAltName0.clear();// don't reload!
			frameAltName1.clear();// don't reload!
		}
		if ((*mode) == 0) {
			frames[0]=framesAll[0];
			frames[1]=framesAll[1];
		}
		else {
			frames[0]=framesAll[2];
			frames[1]=framesAll[3];
		}
        oldMode = *mode;
		onChange(*(new event::Change()));// required because of the way SVGSwitch changes images, we only change the frames above.
		fb->dirty = true;// dirty is not sufficient when changing via frames assignments above (i.e. onChange() is required)
    }
	SvgSwitch::step();
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

void DynamicSVGKnob::draw(const DrawArgs &args) {
	static const float a0 = 3.0f * M_PI / 2.0f;
	
	SvgKnob::draw(args);
	if (paramQuantity) {
		float normalizedParam = paramQuantity->getScaledValue();
		if (normalizedParam != 0.5f) {
			float a1 = math::rescale(normalizedParam, 0.f, 1.f, minAngle, maxAngle) + a0;
			Vec cVec = box.size.div(2.0f);
			float r = box.size.x / 2.0f + 3.0f;// arc radius
			int dir = a0 < a1 ? NVG_CW : NVG_CCW;
			nvgBeginPath(args.vg);
			nvgArc(args.vg, cVec.x, cVec.y, r, a0, a1, dir);
			nvgStrokeWidth(args.vg, 1.5f);// arc thickness
			nvgStrokeColor(args.vg, nvgRGB(255, 245, 0));
			nvgStroke(args.vg);
		}
	}
}