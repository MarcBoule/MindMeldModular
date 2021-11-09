//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#pragma once


#include "rack.hpp"

using namespace rack;

extern Plugin *pluginInstance;


// Display (label) colors
static const int numDispThemes = 7;
static const NVGcolor DISP_COLORS[numDispThemes] = {
	nvgRGB(0xff, 0xd7, 0x14),// yellow
	nvgRGB(240, 240, 240),// light-gray			
	nvgRGB(140, 235, 107),// green
	nvgRGB(102, 245, 207),// aqua
	nvgRGB(102, 207, 245),// cyan
	nvgRGB(102, 183, 245),// blue
	nvgRGB(177, 107, 235)// purple
};
static const std::string dispColorNames[numDispThemes + 1] = {
			"Yellow (default)",
			"Light-grey",
			"Green",
			"Aqua",
			"Cyan",
			"Blue",
			"Purple",
			"Set per track"
		};



// Variations on existing knobs, lights, etc

// Ports

struct MmPort : SvgPort {
	MmPort() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/jack.svg")));
		shadow->blurRadius = 1.0f;
		shadow->opacity = 0.0f;// Turn off shadows
	}
};
struct MmPortGold : SvgPort {
	MmPortGold() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/jack-poly.svg")));
		shadow->blurRadius = 1.0f;
		shadow->opacity = 0.0f;// Turn off shadows
	}
};


// Buttons and switches

void drawRectHalo(const Widget::DrawArgs &args, Vec boxSize, NVGcolor haloColor, float posX);
void drawRoundHalo(const Widget::DrawArgs &args, Vec boxSize, NVGcolor haloColor);


struct SvgSwitchWithHalo : SvgSwitch {
	// internal
	bool manualDrawTopOverride = false;
	
	// derived classes must set up
	NVGcolor haloColor = nvgRGB(0xFF, 0xFF, 0xFF);// this should match the color of fill of the on button
	bool isRect = false;

	void draw(const DrawArgs &args) override {
		ParamQuantity* paramQuantity = getParamQuantity();
		if (!paramQuantity || paramQuantity->getValue() < 0.5f || manualDrawTopOverride) {
			SvgSwitch::draw(args);
		}
	}	
	
	void drawLayer(const DrawArgs &args, int layer) override {
		if (layer == 1) {
			ParamQuantity* paramQuantity = getParamQuantity();
			if (!paramQuantity || paramQuantity->getValue() < 0.5f) {
				// if no module or if switch is off, no need to do anything in layer 1
				return;
			}

			if (settings::haloBrightness != 0.f) {
				if (isRect) {
					drawRectHalo(args, box.size, haloColor, 0.0f);
				}
				else {
					drawRoundHalo(args, box.size, haloColor);
				}
			}
			manualDrawTopOverride = true;
			draw(args);
			manualDrawTopOverride = false;
		}
		SvgSwitch::drawLayer(args, layer);
	}
};

struct MmSwitch : SvgSwitch {
	MmSwitch() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/switch-off.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/switch-on.svg")));
	}
};

struct LedButton : SvgSwitch {
	LedButton() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/led-button.svg")));
	}
};

struct MomentarySvgSwitchNoParam : OpaqueWidget {
	int state = 0;

	// From Switch.hpp/cpp
	bool momentaryPressed = false;
	bool momentaryReleased = false;
	
	// From SvgSwitch.hpp/cpp
	widget::FramebufferWidget* fb;
	widget::SvgWidget* sw;
	std::vector<std::shared_ptr<Svg>> frames;
	
	// From ParamWidget.hpp/cpp (adapted)
	int dirtyValue = INT_MAX;


	MomentarySvgSwitchNoParam() {
		// From SvgSwitch.hpp/cpp
		fb = new widget::FramebufferWidget;
		addChild(fb);
		sw = new widget::SvgWidget;
		fb->addChild(sw);
	}

	// From Switch.hpp/cpp
	void step() override {
		// From Switch.hpp/cpp
		if (momentaryPressed) {
			momentaryPressed = false;
			// Wait another frame.
		}
		else if (momentaryReleased) {
			momentaryReleased = false;
			state = 0;//paramQuantity->setMin();
		}
		
		// From ParamWidget.hpp/cpp
		int value = state;
		// Trigger change event when paramQuantity value changes
		if (value != dirtyValue) {
			dirtyValue = value;
			event::Change eChange;
			onChange(eChange);
		}
		
		OpaqueWidget::step();
	}
	// From Switch.hpp/cpp
	void onDragStart(const event::DragStart& e) override {
		// From Switch.hpp/cpp
		if (e.button != GLFW_MOUSE_BUTTON_LEFT)
			return;
		state = 1;//paramQuantity->setMax();
		momentaryPressed = true;
	}
	// From Switch.hpp/cpp
	void onDragEnd(const event::DragEnd& e) override {
		// From Switch.hpp/cpp
		if (e.button != GLFW_MOUSE_BUTTON_LEFT)
			return;
		momentaryReleased = true;
	}
	
	// From SvgSwitch.hpp/cpp
	void addFrame(std::shared_ptr<Svg> svg) {
		frames.push_back(svg);
		// If this is our first frame, automatically set SVG and size
		if (!sw->svg) {
			sw->setSvg(svg);
			box.size = sw->box.size;
			fb->box.size = sw->box.size;
		}
	}

	// From SvgSwitch.hpp/cpp
	void onChange(const event::Change& e) override {
		if (!frames.empty()) {
			int index = state;
			index = math::clamp(index, 0, (int) frames.size() - 1);
			sw->setSvg(frames[index]);
			fb->dirty = true;
		}
		OpaqueWidget::onChange(e);
	}	
};

struct MmSoloRoundButton : SvgSwitchWithHalo {
	MmSoloRoundButton() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/bass/solo-round-off.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/bass/solo-round-on.svg")));
		haloColor = nvgRGB(0x7A, 0xC9, 0x43);// this should match the color of fill of the on button
		shadow->opacity = 0.0;
	}
};

struct MmBypassRoundButton : SvgSwitchWithHalo {
	MmBypassRoundButton() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/bass/bypass-round-off.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/bass/bypass-round-on.svg")));
		haloColor = nvgRGB(0xFF, 0x1D, 0x25);// this should match the color of fill of the on button
		shadow->opacity = 0.0;
	}
};

struct MmMuteButton : SvgSwitchWithHalo {
	MmMuteButton() {
		momentary = false;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/mute-off.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/mute-on.svg")));
		haloColor = nvgRGB(0xD4, 0x13, 0X08);// used in MixerWidgets also for special mute/fade button. this should match the color of fill of the on button
		isRect = true;
		shadow->opacity = 0.0;
	}
};

struct MmSoloButton : SvgSwitchWithHalo {
	MmSoloButton() {
		momentary = false;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/solo-off.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/solo-on.svg")));
		haloColor = nvgRGB(0x37, 0xA2, 0x2B);// this should match the color of fill of the on button
		isRect = true;
		shadow->opacity = 0.0;
	}	
};


struct MmDimButton : SvgSwitchWithHalo {
	MmDimButton() {
		momentary = false;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/dim-off.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/dim-on.svg")));
		haloColor = nvgRGB(0x72, 0x3A, 0x93);// this should match the color of fill of the on button
		isRect = true;
		shadow->opacity = 0.0;
	}
};

struct MmMonoButton : SvgSwitchWithHalo {
	MmMonoButton() {
		momentary = false;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/mono-off.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/mono-on.svg")));
		haloColor = nvgRGB(0x40, 0x9A, 0xA8);// this should match the color of fill of the on button
		isRect = true;
		shadow->opacity = 0.0;
	}
};

struct MmBypassButton : SvgSwitchWithHalo {
	MmBypassButton() {
		momentary = false;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/eq/global-bypass-off.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/eq/global-bypass-on.svg")));
		haloColor = nvgRGB(0xFB, 0xB2, 0x40);// this should match the color of fill of the on button
		isRect = true;
		shadow->opacity = 0.0;
	}
};

struct MmGroupMinusButtonNoParam : MomentarySvgSwitchNoParam {
	MmGroupMinusButtonNoParam() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/group-minus.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/group-minus-active.svg")));
	}
};

struct MmGroupPlusButtonNoParam : MomentarySvgSwitchNoParam {
	MmGroupPlusButtonNoParam() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/group-plus.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/group-plus-active.svg")));
	}
};

// Knobs and sliders


struct MmKnob : SvgKnob {
	MmKnob() {
		minAngle = -0.83 * float(M_PI);
		maxAngle = 0.83 * float(M_PI);
		shadow->opacity = 0.0;
	}
};

struct MmBigKnobWhite : MmKnob {
	MmBigKnobWhite() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/big-knob-pointer.svg")));
	}
};	

struct MmBiggerKnobWhite : MmKnob {
	MmBiggerKnobWhite() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/bigger-knob-pointer.svg")));
	}
};	

struct MmSmallKnobGrey8mm : MmKnob {
	MmSmallKnobGrey8mm() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/knob-grey-8.svg")));
		SvgWidget* bg = new SvgWidget;
		fb->addChildBelow(bg, tw);
		bg->setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/knob-bg-8.svg")));
	}
};


struct MmKnobWithArc : MmKnob {
	// internal
	NVGcolor arcColorDarker = nvgRGB(120, 120, 120);// grey
	static constexpr float arcThickness = 1.6f;
	static constexpr float TOP_ANGLE = 3.0f * float(M_PI) / 2.0f;

	// derived class must setup
	NVGcolor arcColor;
	bool topCentered = false;
	bool rightWhenNottopCentered = false;

	// user must setup
	float *paramWithCV = NULL;
	bool *paramCvConnected;
	int8_t *detailsShowSrc;
	int8_t *cloakedModeSrc;

	
	void drawArc(const DrawArgs &args, float a0, float a1, NVGcolor* color);
	void drawLayer(const DrawArgs &args, int layer) override;
};

struct MmSmallKnobRedWithArc : MmKnobWithArc {
	MmSmallKnobRedWithArc() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/knob-red-7.5.svg")));
		SvgWidget* bg = new SvgWidget;
		fb->addChildBelow(bg, tw);
		bg->setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/knob-bg-7.5.svg")));
		arcColor = nvgRGB(219, 65, 85);
	}
};
struct MmSmallKnobRedWithArcTopCentered : MmSmallKnobRedWithArc {
	MmSmallKnobRedWithArcTopCentered() {
		topCentered = true;
	}
};	

struct MmSmallKnobOrangeWithArc : MmKnobWithArc {
	MmSmallKnobOrangeWithArc() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/knob-orange-7.5.svg")));
		SvgWidget* bg = new SvgWidget;
		fb->addChildBelow(bg, tw);
		bg->setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/knob-bg-7.5.svg")));
		arcColor = nvgRGB(255, 127, 42);
	}
};
struct MmSmallKnobOrangeWithArcTopCentered : MmSmallKnobOrangeWithArc {
	MmSmallKnobOrangeWithArcTopCentered() {
		topCentered = true;
	}
};	

struct MmSmallKnobBlueWithArc : MmKnobWithArc {
	MmSmallKnobBlueWithArc() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/knob-blue-7.5.svg")));
		SvgWidget* bg = new SvgWidget;
		fb->addChildBelow(bg, tw);
		bg->setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/knob-bg-7.5.svg")));
		arcColor = nvgRGB(113, 160, 255);
	}
};
struct MmSmallKnobBlueWithArcTopCentered : MmSmallKnobBlueWithArc {
	MmSmallKnobBlueWithArcTopCentered() {
		topCentered = true;
	}
};	

struct MmSmallKnobPurpleWithArc : MmKnobWithArc {
	MmSmallKnobPurpleWithArc() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/knob-purple-7.5.svg")));
		SvgWidget* bg = new SvgWidget;
		fb->addChildBelow(bg, tw);
		bg->setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/knob-bg-7.5.svg")));
		arcColor = nvgRGB(163, 93, 209);
	}
};
struct MmSmallKnobPurpleWithArcTopCentered : MmSmallKnobPurpleWithArc {
	MmSmallKnobPurpleWithArcTopCentered() {
		topCentered = true;
	}
};	

struct Mm8mmKnobGrayWithArc : MmKnobWithArc {
	Mm8mmKnobGrayWithArc() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/knob-grey-8.svg")));
		SvgWidget* bg = new SvgWidget;
		fb->addChildBelow(bg, tw);
		bg->setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/knob-bg-8.svg")));
		arcColor = DISP_COLORS[1];// yellow knob by default
	}
};
struct Mm8mmKnobGrayWithArcTopCentered : Mm8mmKnobGrayWithArc {
	Mm8mmKnobGrayWithArcTopCentered() {
		topCentered = true;
	}
};

struct MmSlider : SvgSlider {
	void setupSlider() {
		maxHandlePos = Vec(0, 0);
		minHandlePos = Vec(0, background->box.size.y - 0.01f);// 0.01f is epsilon so handle doesn't disappear at bottom
		float offsetY = handle->box.size.y / 2.0f;
		background->box.pos.y = offsetY;
		box.size.y = background->box.size.y + offsetY * 2.0f;
		background->setVisible(false);
	}
};



struct MmSmallFader : MmSlider {
	MmSmallFader() {
		// no adjustment needed in this code, simply adjust the background svg's width to match the width of the handle by temporarily making it visible in the code below, and tweaking the svg's width as needed (when scaling not 100% between inkscape and Rack)
		setBackgroundSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/fader-channel-bg.svg")));
		setHandleSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/fader-channel.svg")));
		setupSlider();
	}
};

struct MmSmallerFader : MmSlider {
	MmSmallerFader() {
		// no adjustment needed in this code, simply adjust the background svg's width to match the width of the handle by temporarily making it visible in the code below, and tweaking the svg's width as needed (when scaling not 100% between inkscape and Rack)
		setBackgroundSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/fader-aux-bg.svg")));
		setHandleSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/fader-channel.svg")));
		setupSlider();
	}
};

struct MmBigFader : MmSlider {
	MmBigFader() {
		// no adjustment needed in this code, simply adjust the background svg's width to match the width of the handle by temporarily making it visible in the code below, and tweaking the svg's width as needed (when scaling not 100% between inkscape and Rack)
		setBackgroundSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/fader-master-bg.svg")));
		setHandleSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/fader-master.svg")));
		setupSlider();
	}
};


// Lights


template <typename TBase = GrayModuleLightWidget>
struct TMMWhiteBlueLight : TBase {
	TMMWhiteBlueLight() {
		this->addBaseColor(SCHEME_WHITE);
		this->addBaseColor(SCHEME_BLUE);
	}
};
typedef TMMWhiteBlueLight<> MMWhiteBlueLight;
