//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//See ./LICENSE.txt for all licenses
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



// Dynamic SVGPort

void DynamicSVGPort::addFrame(std::shared_ptr<Svg> svg) {
    frames.push_back(svg);
    if(frames.size() == 1) {
        SvgPort::setSvg(svg);
	}
}

void DynamicSVGPort::step() {
	if (mode != NULL && *mode != oldMode) {
        if ((*mode) > 0 && !frameAltName.empty()) {// JIT loading of alternate skin
			frames.push_back(APP->window->loadSvg(frameAltName));
			frameAltName.clear();// don't reload!
		}
        if ((*mode) < 0 && !frameUnusedName.empty()) {// JIT loading of unused skin
			frameUnused = APP->window->loadSvg(frameUnusedName);
			frameUnusedName.clear();// don't reload!
		}
        if (*mode >= 0) {
			sw->setSvg(frames[*mode]);
		}
		else {
			sw->setSvg(frameUnused);
		}
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
	if (mode != NULL && *mode != oldMode) {
        if ((*mode) > 0 && !frameAltName0.empty() && !frameAltName1.empty()) {// JIT loading of alternate skin
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



// Dynamic SVGSlider

void DynamicSVGSlider::addFrameAll(std::shared_ptr<Svg> svg) {
    framesAll.push_back(svg);
	if (framesAll.size() == 1) {
		setHandleSvg(svg);
	}
}

void DynamicSVGSlider::step() {
    if(mode != NULL && *mode != oldMode) {
        if (*mode > 0 && !frameAltName.empty()) {// JIT loading of alternate skin
			framesAll.push_back(APP->window->loadSvg(frameAltName));
			frameAltName.clear();// don't reload!
		}
		setHandleSvg(framesAll[*mode]);
        oldMode = *mode;
		fb->dirty = true;
    }
	SvgSlider::step();
}

void DynamicSVGSlider::setupSlider() {
	maxHandlePos = Vec(0, 0);
	minHandlePos = Vec(0, background->box.size.y - 0.01f);// 0.01f is epsilon so handle doesn't disappear at bottom
	float offsetY = handle->box.size.y / 2.0f;
	background->box.pos.y = offsetY;
	box.size.y = background->box.size.y + offsetY * 2.0f;
	background->visible = false;
}
