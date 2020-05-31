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


struct DynSoloRoundButton : DynamicSVGSwitch {
	DynSoloRoundButton() {
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/bass/solo-round-off.svg")));
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/bass/solo-round-on.svg")));
		//addFrameAlt0(asset::plugin(pluginInstance, "res/dark/comp/TL1105_0.svg"));
		//addFrameAlt1(asset::plugin(pluginInstance, "res/dark/comp/TL1105_1.svg"));	
		shadow->opacity = 0.0;
	}
};

struct DynBypassRoundButton : DynamicSVGSwitch {
	DynBypassRoundButton() {
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/bass/bypass-round-off.svg")));
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/bass/bypass-round-on.svg")));
		//addFrameAlt0(asset::plugin(pluginInstance, "res/dark/comp/TL1105_0.svg"));
		//addFrameAlt1(asset::plugin(pluginInstance, "res/dark/comp/TL1105_1.svg"));	
		shadow->opacity = 0.0;
	}
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
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/eq/global-bypass-off.svg")));
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/eq/global-bypass-on.svg")));
		//addFrameAlt0(asset::plugin(pluginInstance, "res/dark/comp/TL1105_0.svg"));
		//addFrameAlt1(asset::plugin(pluginInstance, "res/dark/comp/TL1105_1.svg"));	
		shadow->opacity = 0.0;
	}
	void randomize() override {}
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

struct DynKnobWithArc : DynKnob {
	// internal
	NVGcolor arcColorDarker = nvgRGB(120, 120, 120);// grey
	static constexpr float arcThickness = 1.6f;
	static constexpr float TOP_ANGLE = 3.0f * M_PI / 2.0f;

	// derived class must setup
	NVGcolor arcColor;
	bool topCentered = false;

	// user must setup
	float *paramWithCV = NULL;
	bool *paramCvConnected;
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
			float aParam = -10000.0f;
			float aBase = TOP_ANGLE;
			if (!topCentered) {
				aBase += minAngle;
			}
			int8_t showMask = (*detailsShowSrc & ~*cloakedModeSrc & 0x3); // 0 = off, 0x1 = cv_only, 0x3 = cv+param
			// param
			float param = paramQuantity->getValue();
			if (showMask == 0x3) {
				aParam = TOP_ANGLE + math::rescale(param, paramQuantity->getMinValue(), paramQuantity->getMaxValue(), minAngle, maxAngle);
				drawArc(args, aBase, aParam, &arcColorDarker);
			}
			// cv
			if ((*paramCvConnected) && paramWithCV && showMask != 0) {
				if (aParam == -10000.0f) {
					aParam = TOP_ANGLE + math::rescale(param, paramQuantity->getMinValue(), paramQuantity->getMaxValue(), minAngle, maxAngle);
				}
				float aCv = TOP_ANGLE + math::rescale(*paramWithCV, paramQuantity->getMinValue(), paramQuantity->getMaxValue(), minAngle, maxAngle);
				drawArc(args, aParam, aCv, &arcColor);
			}
		}
	}
};

struct DynBigKnobWhite : DynKnob {
	DynBigKnobWhite() {
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/big-knob-pointer.svg")));
	}
};	

struct DynBiggerKnobWhite : DynKnob {
	DynBiggerKnobWhite() {
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/bigger-knob-pointer.svg")));
	}
};	

struct DynSmallKnobGrey : DynKnob {// 7.5mm
	DynSmallKnobGrey() {
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/knob-grey.svg")));
		//addFrameAlt(asset::plugin(pluginInstance, "res/dark/comp/RoundSmallBlackKnob.svg"));
	}
};

struct DynSmallKnobGrey8mm : DynKnob {
	DynSmallKnobGrey8mm() {
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/knob-grey8mm.svg")));
		//addFrameAlt(asset::plugin(pluginInstance, "res/dark/comp/RoundSmallBlackKnob.svg"));
	}
};

#endif
