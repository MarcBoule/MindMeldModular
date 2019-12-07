//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//See ./LICENSE.md for all licenses
//***********************************************************************************************

#ifndef IM_GENERICCOMP_HPP
#define IM_GENERICCOMP_HPP


#include "rack.hpp"

using namespace rack;

extern Plugin *pluginInstance;


// Component offset constants

// none



// Variations on existing knobs, lights, etc

struct MmSwitch : app::SvgSwitch {
	MmSwitch() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/eq/switch-bypass.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/eq/switch-active.svg")));
	}
};



// Ports

// none



// Buttons and switches

struct LedButton : app::SvgSwitch {
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


// Knobs

// none



// Lights


template <typename TBase = GrayModuleLightWidget>
struct TMMWhiteBlueLight : TBase {
	TMMWhiteBlueLight() {
		this->addBaseColor(SCHEME_WHITE);
		this->addBaseColor(SCHEME_BLUE);
	}
};
typedef TMMWhiteBlueLight<> MMWhiteBlueLight;





#endif