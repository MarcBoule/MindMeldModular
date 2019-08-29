//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//See ./LICENSE.txt for all licenses
//***********************************************************************************************

#ifndef MMM_DYNAMICCOMP_HPP
#define MMM_DYNAMICCOMP_HPP


#include "rack.hpp"

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
	std::shared_ptr<Svg> frameUnused;
	std::string frameUnusedName;

    void addFrame(std::shared_ptr<Svg> svg);
    void addFrameAlt(std::string filename) {frameAltName = filename;}
    void addFrameUnused(std::string filename) {frameUnusedName = filename;}
    void step() override;
};


struct DynPort : DynamicSVGPort {
	DynPort() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/jack.svg")));
		//addFrameAlt(asset::plugin(pluginInstance, "res/dark/comp/PJ301M.svg"));
		addFrameUnused(asset::plugin(pluginInstance, "res/comp/jack-unused.svg"));
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

struct DynamicSVGKnob : SvgKnob {
    int* mode = NULL;
    int oldMode = -1;
	std::vector<std::shared_ptr<Svg>> framesAll;
	std::string frameAltName;

	void addFrameAll(std::shared_ptr<Svg> svg);
    void addFrameAlt(std::string filename) {frameAltName = filename;}	
    void step() override;
	void draw(const DrawArgs &args) override;
};

struct DynamicSVGSlider : SvgSlider {
    int* mode = NULL;
    int oldMode = -1;
	std::vector<std::shared_ptr<Svg>> framesAll;
	std::string frameAltName;

	void addFrameAll(std::shared_ptr<Svg> svg);
    void addFrameAlt(std::string filename) {frameAltName = filename;}	
    void step() override;
};


struct DynMuteButton : DynamicSVGSwitch {
	
	DynMuteButton() {
		momentary = false;
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mute-off.svg")));
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mute-on.svg")));
		//addFrameAlt0(asset::plugin(pluginInstance, "res/dark/comp/TL1105_0.svg"));
		//addFrameAlt1(asset::plugin(pluginInstance, "res/dark/comp/TL1105_1.svg"));	
		shadow->opacity = 0.0;
	}
};

struct DynSoloButton : DynamicSVGSwitch {
	DynSoloButton() {
		momentary = false;
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/solo-off.svg")));
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/solo-on.svg")));
		//addFrameAlt0(asset::plugin(pluginInstance, "res/dark/comp/TL1105_0.svg"));
		//addFrameAlt1(asset::plugin(pluginInstance, "res/dark/comp/TL1105_1.svg"));	
		shadow->opacity = 0.0;
	}
	
};

struct DynDimButton : DynamicSVGSwitch {
	DynDimButton() {
		momentary = false;
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/dim-off.svg")));
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/dim-on.svg")));
		//addFrameAlt0(asset::plugin(pluginInstance, "res/dark/comp/TL1105_0.svg"));
		//addFrameAlt1(asset::plugin(pluginInstance, "res/dark/comp/TL1105_1.svg"));	
		shadow->opacity = 0.0;
	}
};

struct DynMonoButton : DynamicSVGSwitch {
	DynMonoButton() {
		momentary = false;
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mono-off.svg")));
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mono-on.svg")));
		//addFrameAlt0(asset::plugin(pluginInstance, "res/dark/comp/TL1105_0.svg"));
		//addFrameAlt1(asset::plugin(pluginInstance, "res/dark/comp/TL1105_1.svg"));	
		shadow->opacity = 0.0;
	}
};


struct DynGroupMinusButton : DynamicSVGSwitch {
	DynGroupMinusButton() {
		momentary = true;
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/group-minus.svg")));
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/group-minus-active.svg")));
		//addFrameAlt0(asset::plugin(pluginInstance, "res/dark/comp/TL1105_0.svg"));
		//addFrameAlt1(asset::plugin(pluginInstance, "res/dark/comp/TL1105_1.svg"));	
		shadow->opacity = 0.0;
	}
};

struct DynGroupPlusButton : DynamicSVGSwitch {
	DynGroupPlusButton() {
		momentary = true;
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/group-plus.svg")));
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/group-plus-active.svg")));
		//addFrameAlt0(asset::plugin(pluginInstance, "res/dark/comp/TL1105_0.svg"));
		//addFrameAlt1(asset::plugin(pluginInstance, "res/dark/comp/TL1105_1.svg"));	
		shadow->opacity = 0.0;
	}
};


struct DynKnob : DynamicSVGKnob {
	DynKnob() {
		minAngle = -0.83*M_PI;
		maxAngle = 0.83*M_PI;
		shadow->opacity = 0.0;
	}
};
struct DynSmallKnob : DynKnob {
	DynSmallKnob() {
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/knob-grey.svg")));
		//addFrameAlt(asset::plugin(pluginInstance, "res/dark/comp/RoundSmallBlackKnob.svg"));
	}
};
struct DynSmallKnobNoRandom : DynSmallKnob {
	void randomize() override {}
};
struct DynSmallSnapKnob : DynSmallKnob {
	DynSmallSnapKnob() {
		snap = true;
	}
};

struct DynSmallFader : DynamicSVGSlider {
	DynSmallFader() {
		// no adjustment needed in this code, simply adjust the background svg's width to match the width of the handle by temporarily making it visible in the code below, and tweaking the svg's width as needed (when scaling not 100% between inkscape and Rack)
		setBackgroundSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/fader-channel-bg.svg")));
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/fader-channel.svg")));
		maxHandlePos = Vec(0, 0);
		minHandlePos = Vec(0, background->box.size.y - 0.01f);// 0.01f is epsilon so handle doesn't disappear at bottom
		float offsetY = handle->box.size.y / 2.0f;
		background->box.pos.y = offsetY;
		box.size.y = background->box.size.y + offsetY * 2.0f;
		background->visible = false;
	}
};

struct DynBigFader : DynamicSVGSlider {
	DynBigFader() {
		// no adjustment needed in this code, simply adjust the background svg's width to match the width of the handle by temporarily making it visible in the code below, and tweaking the svg's width as needed (when scaling not 100% between inkscape and Rack)
		setBackgroundSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/fader-master-bg.svg")));
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/fader-master.svg")));
		maxHandlePos = Vec(0, 0);
		minHandlePos = Vec(0, background->box.size.y - 0.01f);// 0.01f is epsilon so handle doesn't disappear at bottom
		float offsetY = handle->box.size.y / 2.0f;
		background->box.pos.y = offsetY;
		box.size.y = background->box.size.y + offsetY * 2.0f;
		background->visible = false;
	}
};




#endif