//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//See ./LICENSE.md for all licenses
//***********************************************************************************************


// Note: new code to be added to the existing code in MindMeldModular's GenericComponents.hpp


#pragma once

#include "rack.hpp"


using namespace rack;

extern Plugin *pluginInstance;


// SCHEME_LIGHT_GRAY is 230, 230, 230 in Rack's componentlibrary.hpp
static const NVGcolor MID_GRAY = nvgRGB(150, 150, 150);
static const NVGcolor MID_DARKER_GRAY = nvgRGB(130, 130, 130);
// SCHEME_DARK_GRAY is 23, 23, 23 in Rack's componentlibrary.hpp


static const int numChanColors = 9;
static const NVGcolor CHAN_COLORS[numChanColors] = {
	nvgRGB(0xff, 0xd7, 0x14),// yellow (same as MMM)
	nvgRGB(245, 147, 56),// orange (New for SM)
	nvgRGB(235, 82, 73),// red (New for SM)
	
	nvgRGB(235, 102, 161),// pink (New for SM)
	nvgRGB(177, 146, 235),// nvgRGB(177, 107, 235)// purple	(New for SM, MMM version in comments)
	nvgRGB(102, 183, 245),// blue (same as MMM)

	nvgRGB(102, 245, 207),// aqua (same as MMM)
	nvgRGB(140, 235, 107),// green (same as MMM)
	nvgRGB(240, 240, 240)// light-gray (same as MMM)		
};
static const std::string chanColorNames[numChanColors] = {
			"Yellow",
			"Orange",
			"Red",
			
			"Pink",
			"Purple",
			"Blue",
			
			"Aqua",
			"Green",
			"Light-grey"
			// "Set per track"
		};



// Variations on existing knobs, lights, etc
// ----------------


// Ports

// none



// Buttons and switches

struct LedButton2 : SvgSwitch {
	LedButton2() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/round-button4-grey.svg")));
	}
};


struct MmSyncButton : SvgSwitch {
	MmSyncButton() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/shape/sync-off.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/shape/sync-on.svg")));
		shadow->opacity = 0.0;
	}
};

struct MmLockButton : SvgSwitch {
	MmLockButton() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/shape/lock-off.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/shape/lock-on.svg")));
		shadow->opacity = 0.0;
	}
};

struct MmPlayButton : SvgSwitch {
	MmPlayButton() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/shape/play-off.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/shape/play-on.svg")));
		shadow->opacity = 0.0;
	}
};

struct MmFreezeButton : SvgSwitch {
	MmFreezeButton() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/shape/freeze-off.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/shape/freeze-on.svg")));
		shadow->opacity = 0.0;
	}
};

struct MmLoopButton : SvgSwitch {
	MmLoopButton() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/shape/SL-off.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/shape/sustain-on.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/shape/loop-on.svg")));
		shadow->opacity = 0.0;
	}
};

struct MmHeadphonesButton : SvgSwitch {
	MmHeadphonesButton() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/shape/headphones-off.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/shape/headphones-on.svg")));
		shadow->opacity = 0.0;
	}
};

struct MmGearButton : SvgSwitch {
	MmGearButton() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/shape/sidechain-settings-off.svg")));
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/shape/sidechain-settings-on.svg")));
		shadow->opacity = 0.0;
	}
};



// Knobs and sliders

// none


// Lights

template <typename TBase = app::ModuleLightWidget>
struct TSmModuleLightWidget : TBase {
	TSmModuleLightWidget() {
		this->bgColor = nvgRGB(0x35, 0x35, 0x35);
		this->borderColor = nvgRGBA(0, 0, 0, 0x60);
	}
};
typedef TSmModuleLightWidget<> SmModuleLightWidget;



// Misc

#ifdef SM_PRO
struct ProSvg : SvgWidget {
	ProSvg() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/shape/pro.svg")));
	}
};
#endif

struct SyncSvg : SvgWidget {
	SyncSvg() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/shape/sync-on.svg")));
	}
};
struct LockSvg : SvgWidget {
	LockSvg() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/shape/lock-on.svg")));
	}
};

