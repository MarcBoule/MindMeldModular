//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//See ./LICENSE.md for all licenses
//***********************************************************************************************

#ifndef MMM_DYNAMICCOMP_HPP
#define MMM_DYNAMICCOMP_HPP


#include "rack.hpp"
#include "GenericComponents.hpp"

using namespace rack;

extern Plugin *pluginInstance;


static const float blurRadiusRatio = 0.06f;



// ******** Dynamic Widgets ********

// General Dynamic Widget creation
template <class TWidget>
TWidget* createDynamicWidget(Vec pos, int* mode) {
	TWidget *dynWidget = createWidget<TWidget>(pos);
	dynWidget->mode = mode;
	return dynWidget;
}
template <class TWidget>
TWidget* createDynamicWidgetCentered(Vec pos, int* mode) {
	TWidget *dynWidget = createWidgetCentered<TWidget>(pos);
	dynWidget->mode = mode;
	return dynWidget;
}

struct DynamicPanelBorder : widget::TransparentWidget {
	int* mode = NULL;// bit 0 is flag to draw left side border, bit 1 is flag to draw right side border; top and bottom borders are always drawn
    int oldMode = -1;
	void draw(const DrawArgs& args) override;
    void step() override;
};




// ******** Dynamic Ports ********

// General Dynamic Port creation
template <class TDynamicPort>
TDynamicPort* createDynamicPort(Vec pos, bool isInput, Module *module, int portId, int* mode) {
	TDynamicPort *dynPort = isInput ? 
		createInput<TDynamicPort>(pos, module, portId) :
		createOutput<TDynamicPort>(pos, module, portId);
	dynPort->mode = mode;
	return dynPort;
}
template <class TDynamicPort>
TDynamicPort* createDynamicPortCentered(Vec pos, bool isInput, Module *module, int portId, int* mode) {
	TDynamicPort *dynPort = createDynamicPort<TDynamicPort>(pos, isInput, module, portId, mode);
	dynPort->box.pos = dynPort->box.pos.minus(dynPort->box.size.div(2));// centering
	return dynPort;
}

struct DynamicSVGPort : SvgPort {
    int* mode = NULL;
    int oldMode = -2;
    std::vector<std::shared_ptr<Svg>> frames;
	std::string frameAltName;

    void addFrame(std::shared_ptr<Svg> svg);
    void addFrameAlt(std::string filename) {frameAltName = filename;}
    void step() override;
};


struct DynPort : DynamicSVGPort {
	DynPort() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/jack.svg")));
		//addFrameAlt(asset::plugin(pluginInstance, "res/dark/comp/PJ301M.svg"));
		shadow->blurRadius = 1.0f;
		shadow->opacity = 0.0f;// Turn off shadows
	}
};
struct DynPortGold : DynamicSVGPort {
	DynPortGold() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/jack-poly.svg")));
		//addFrameAlt(asset::plugin(pluginInstance, "res/dark/comp/PJ301M.svg"));
		shadow->blurRadius = 1.0f;
		shadow->opacity = 0.0f;// Turn off shadows
	}
};


// ******** Dynamic Params ********

template <class TDynamicParam>
TDynamicParam* createDynamicParam(Vec pos, Module *module, int paramId, int* mode) {
	TDynamicParam *dynParam = createParam<TDynamicParam>(pos, module, paramId);
	dynParam->mode = mode;
	return dynParam;
}
template <class TDynamicParam>
TDynamicParam* createDynamicParamCentered(Vec pos, Module *module, int paramId, int* mode) {
	TDynamicParam *dynParam = createDynamicParam<TDynamicParam>(pos, module, paramId, mode);
	dynParam->box.pos = dynParam->box.pos.minus(dynParam->box.size.div(2));// centering
	return dynParam;
}

struct DynamicSVGSwitch : SvgSwitch {
    int* mode = NULL;
    int oldMode = -1;
	std::vector<std::shared_ptr<Svg>> framesAll;
	std::string frameAltName0;
	std::string frameAltName1;
	
	void addFrameAll(std::shared_ptr<Svg> svg);
    void addFrameAlt0(std::string filename) {frameAltName0 = filename;}
    void addFrameAlt1(std::string filename) {frameAltName1 = filename;}
    void step() override;
};

struct DynamicSVGSwitchNoParam : MomentarySvgSwitchNoParam {
    int* mode = NULL;
    int oldMode = -1;
	std::vector<std::shared_ptr<Svg>> framesAll;
	std::string frameAltName0;
	std::string frameAltName1;
	
	void addFrameAll(std::shared_ptr<Svg> svg);
    void addFrameAlt0(std::string filename) {frameAltName0 = filename;}
    void addFrameAlt1(std::string filename) {frameAltName1 = filename;}
    void step() override;
};


struct DynamicSVGKnob : SvgKnob {
    int* mode = NULL;
    int oldMode = -1;
	std::vector<std::shared_ptr<Svg>> framesAll;
	std::string frameAltName;

	void addFrameAll(std::shared_ptr<Svg> svg);
    void addFrameAlt(std::string filename) {frameAltName = filename;}	
    void step() override;
	//void draw(const DrawArgs &args) override;
};

struct DynamicSVGSlider : SvgSlider {
    int* mode = NULL;
    int oldMode = -1;
	std::vector<std::shared_ptr<Svg>> framesAll;
	std::string frameAltName;

	void addFrameAll(std::shared_ptr<Svg> svg);
    void addFrameAlt(std::string filename) {frameAltName = filename;}	
    void step() override;
	void setupSlider();
};


struct DynMuteButton : DynamicSVGSwitch {
	
	DynMuteButton() {
		momentary = false;
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/mute-off.svg")));
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/mute-on.svg")));
		//addFrameAlt0(asset::plugin(pluginInstance, "res/dark/comp/TL1105_0.svg"));
		//addFrameAlt1(asset::plugin(pluginInstance, "res/dark/comp/TL1105_1.svg"));	
		shadow->opacity = 0.0;
	}
};

struct DynSoloButton : DynamicSVGSwitch {
	DynSoloButton() {
		momentary = false;
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/solo-off.svg")));
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/solo-on.svg")));
		//addFrameAlt0(asset::plugin(pluginInstance, "res/dark/comp/TL1105_0.svg"));
		//addFrameAlt1(asset::plugin(pluginInstance, "res/dark/comp/TL1105_1.svg"));	
		shadow->opacity = 0.0;
	}
};

struct DynDimButton : DynamicSVGSwitch {
	DynDimButton() {
		momentary = false;
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/dim-off.svg")));
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/dim-on.svg")));
		//addFrameAlt0(asset::plugin(pluginInstance, "res/dark/comp/TL1105_0.svg"));
		//addFrameAlt1(asset::plugin(pluginInstance, "res/dark/comp/TL1105_1.svg"));	
		shadow->opacity = 0.0;
	}
};

struct DynMonoButton : DynamicSVGSwitch {
	DynMonoButton() {
		momentary = false;
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/mono-off.svg")));
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/mono-on.svg")));
		//addFrameAlt0(asset::plugin(pluginInstance, "res/dark/comp/TL1105_0.svg"));
		//addFrameAlt1(asset::plugin(pluginInstance, "res/dark/comp/TL1105_1.svg"));	
		shadow->opacity = 0.0;
	}
};

struct DynBypassButton : DynamicSVGSwitch {
	DynBypassButton() {
		momentary = false;
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/eq/bypass-off.svg")));
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/eq/bypass-on.svg")));
		//addFrameAlt0(asset::plugin(pluginInstance, "res/dark/comp/TL1105_0.svg"));
		//addFrameAlt1(asset::plugin(pluginInstance, "res/dark/comp/TL1105_1.svg"));	
		shadow->opacity = 0.0;
	}
};


struct DynGroupMinusButton : DynamicSVGSwitch {
	DynGroupMinusButton() {
		momentary = true;
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/group-minus.svg")));
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/group-minus-active.svg")));
		//addFrameAlt0(asset::plugin(pluginInstance, "res/dark/comp/TL1105_0.svg"));
		//addFrameAlt1(asset::plugin(pluginInstance, "res/dark/comp/TL1105_1.svg"));	
		shadow->opacity = 0.0;
	}
};

struct DynGroupPlusButton : DynamicSVGSwitch {
	DynGroupPlusButton() {
		momentary = true;
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/group-plus.svg")));
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/group-plus-active.svg")));
		//addFrameAlt0(asset::plugin(pluginInstance, "res/dark/comp/TL1105_0.svg"));
		//addFrameAlt1(asset::plugin(pluginInstance, "res/dark/comp/TL1105_1.svg"));	
		shadow->opacity = 0.0;
	}
};


struct DynGroupMinusButtonNoParam : DynamicSVGSwitchNoParam {
	DynGroupMinusButtonNoParam() {
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/group-minus.svg")));
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/group-minus-active.svg")));
		//addFrameAlt0(asset::plugin(pluginInstance, "res/dark/comp/TL1105_0.svg"));
		//addFrameAlt1(asset::plugin(pluginInstance, "res/dark/comp/TL1105_1.svg"));	
	}
};

struct DynGroupPlusButtonNoParam : DynamicSVGSwitchNoParam {
	DynGroupPlusButtonNoParam() {
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/group-plus.svg")));
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/group-plus-active.svg")));
		//addFrameAlt0(asset::plugin(pluginInstance, "res/dark/comp/TL1105_0.svg"));
		//addFrameAlt1(asset::plugin(pluginInstance, "res/dark/comp/TL1105_1.svg"));	
	}
};

struct DynKnob : DynamicSVGKnob {
	DynKnob() {
		minAngle = -0.83*M_PI;
		maxAngle = 0.83*M_PI;
		shadow->opacity = 0.0;
	}
};
struct DynSnapKnob : DynKnob {
	DynSnapKnob() {
		snap = true;
	}
};
struct DynKnobWithArc : DynKnob {
	// internal
	NVGcolor arcColorDarker = nvgRGB(120, 120, 120);// grey
	static constexpr float arcThickness = 1.6f;
	static constexpr float TOP_ANGLE = 3.0f * M_PI / 2.0f;

	// derived class must setup
	NVGcolor arcColor;
	bool topCentered = false;

	// user must setup
	float* paramWithCV = NULL;
	int8_t *detailsShowSrc;
	int8_t *cloakedModeSrc;

	
	void drawArc(const DrawArgs &args, float a0, float a1, NVGcolor* color) {
		int dir = a1 > a0 ? NVG_CW : NVG_CCW;
		Vec cVec = box.size.div(2.0f);
		float r = (box.size.x * 1.2033f) / 2.0f;// arc radius
		nvgBeginPath(args.vg);
		nvgLineCap(args.vg, NVG_ROUND);
		nvgArc(args.vg, cVec.x, cVec.y, r, a0, a1, dir);
		nvgStrokeWidth(args.vg, arcThickness);
		nvgStrokeColor(args.vg, *color);
		nvgStroke(args.vg);		
	}
	
	void draw(const DrawArgs &args) override {
		DynamicSVGKnob::draw(args);
		if (paramQuantity) {
			float normalizedParam = paramQuantity->getScaledValue();
			float aParam = -10000.0f;
			float aBase = TOP_ANGLE;
			if (!topCentered) {
				aBase += minAngle;
			}
			// param
			if ((!topCentered || normalizedParam != 0.5f) && (*detailsShowSrc & ~*cloakedModeSrc & 0x3) == 0x3) {
				aParam = TOP_ANGLE + math::rescale(normalizedParam, 0.f, 1.f, minAngle, maxAngle);
				drawArc(args, aBase, aParam, &arcColorDarker);
			}
			// cv
			if (paramWithCV && *paramWithCV != -1.0f && (*detailsShowSrc & ~*cloakedModeSrc & 0x3) != 0) {
				if (aParam == -10000.0f) {
					aParam = TOP_ANGLE + math::rescale(normalizedParam, 0.f, 1.f, minAngle, maxAngle);
				}
				float aCv = TOP_ANGLE + math::rescale(*paramWithCV, paramQuantity->getMinValue(), paramQuantity->getMaxValue(), minAngle, maxAngle);
				drawArc(args, aParam, aCv, &arcColor);
			}
		}
	}
};


struct DynSmallFader : DynamicSVGSlider {
	DynSmallFader() {
		// no adjustment needed in this code, simply adjust the background svg's width to match the width of the handle by temporarily making it visible in the code below, and tweaking the svg's width as needed (when scaling not 100% between inkscape and Rack)
		setBackgroundSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/fader-channel-bg.svg")));
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/fader-channel.svg")));
		setupSlider();
	}
};

struct DynSmallerFader : DynamicSVGSlider {
	DynSmallerFader() {
		// no adjustment needed in this code, simply adjust the background svg's width to match the width of the handle by temporarily making it visible in the code below, and tweaking the svg's width as needed (when scaling not 100% between inkscape and Rack)
		setBackgroundSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/fader-aux-bg.svg")));
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/fader-channel.svg")));
		setupSlider();
	}
};

struct DynBigFader : DynamicSVGSlider {
	DynBigFader() {
		// no adjustment needed in this code, simply adjust the background svg's width to match the width of the handle by temporarily making it visible in the code below, and tweaking the svg's width as needed (when scaling not 100% between inkscape and Rack)
		setBackgroundSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/fader-master-bg.svg")));
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/fader-master.svg")));
		setupSlider();
	}
};




#endif
