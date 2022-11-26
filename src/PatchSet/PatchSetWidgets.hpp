//***********************************************************************************************
//Configurable multi-controller with parameter mapping
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************

#pragma once

#include "../MindMeldModular.hpp"

// COMMON
// --------

static const NVGcolor SCHEME_MM_BLUE_LOGO = nvgRGB(0x44, 0xA4, 0xF1);// MM light blue
static const NVGcolor SCHEME_MM_RED_LOGO = nvgRGB(0xE8, 0x22, 0x7C);// MM light blue (232, 34, 124)
static const NVGcolor SCHEME_PS_BG = nvgRGB(0x42, 0x42, 0x42);// tile bg colour

static constexpr float psFontSpacing = 0.14f;

static const int numPsColors = 9;
static const NVGcolor PATCHSET_COLORS[numPsColors] = {
	// these are copied over from ShapeMaster, but made distinct in case they need tweaking or extra colours
	nvgRGB(0xff, 0xd7, 0x14),// yellow (same as MMM)
	nvgRGB(240, 240, 240),// light-gray (same as MMM)		
	nvgRGB(245, 147, 56),// orange (New for SM)
	nvgRGB(235, 82, 73),// red (New for SM)
	
	nvgRGB(235, 102, 161),// pink (New for SM)
	nvgRGB(177, 146, 235),// nvgRGB(177, 107, 235)// purple	(New for SM, MMM version in comments)
	nvgRGB(102, 183, 245),// blue (same as MMM)

	nvgRGB(102, 245, 207),// aqua (same as MMM)
	nvgRGB(140, 235, 107)// green (same as MMM)
};
static const std::string psColorNames[numPsColors] = {
			"Yellow",
			"Light-grey",
			"Orange",
			"Red",
			
			"Pink",
			"Purple",
			"Blue",
			
			"Aqua",
			"Green"
		};


struct LogoSvg : SvgWidget {
	LogoSvg() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/logo.svg")));
	}
};
struct OmriLogoSvg : SvgWidget {
	OmriLogoSvg() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/patchset/Omri-logo.svg")));
	}
};

struct PsMapLight : GrayModuleLightWidget {
	PsMapLight() {
		addBaseColor(SCHEME_MM_BLUE_LOGO);
		addBaseColor(SCHEME_RED);
	}
};

struct PsBlueLight : GrayModuleLightWidget {
	PsBlueLight() {
		addBaseColor(SCHEME_MM_BLUE_LOGO);
	}
};


inline bool calcFamilyPresent(Module* expModule, bool includeBlank = true) {
	return (expModule && (
			expModule->model == modelMasterChannel || 
			expModule->model == modelPatchMaster || 
			(includeBlank && expModule->model == modelPatchMasterBlank)));
}


// BACKGROUNDS
// --------

// see PatchMaster.cpp



// FADERS
// --------


struct PmSliderWithHighlight : MmSlider {
	int8_t* highlightColor = NULL;
	
	void drawLayer(const DrawArgs &args, int layer) override {
		if (layer == 1 && highlightColor) {
			// ParamQuantity* paramQuantity = getParamQuantity();
			// if (paramQuantity) {
				// float v = paramQuantity->getScaledValue();
				float offsetY = handle->box.size.y / 2.0f;
				// float ypos = math::rescale(v, 0.f, 1.f, minHandlePos.y, maxHandlePos.y) + offsetY;
				float ypos = handle->box.pos.y + offsetY;
				
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg, 0, ypos);
				nvgLineTo(args.vg, box.size.x, ypos);
				nvgClosePath(args.vg);
				nvgStrokeColor(args.vg, PATCHSET_COLORS[*highlightColor]);
				nvgStrokeWidth(args.vg, mm2px(0.4f));
				nvgStroke(args.vg);
			// }
		}
		MmSlider::drawLayer(args, layer);
	}
};

struct PsLargeFader : PmSliderWithHighlight {
	PsLargeFader() {
		// no adjustment needed in this code, simply adjust the background svg's width to match the width of the handle by temporarily making it visible in the code below, and tweaking the svg's width as needed (when scaling not 100% between inkscape and Rack)
		setBackgroundSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/patchset/fader-large-bg.svg")));
		setHandleSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/fader-channel.svg")));
		setupSlider();
	}
};

struct PsXLargeFader : PmSliderWithHighlight {
	PsXLargeFader() {
		// no adjustment needed in this code, simply adjust the background svg's width to match the width of the handle by temporarily making it visible in the code below, and tweaking the svg's width as needed (when scaling not 100% between inkscape and Rack)
		setBackgroundSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/patchset/fader-xlarge-bg.svg")));
		setHandleSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/fader-channel.svg")));
		setupSlider();
	}
};

struct PsXXLargeFader : PmSliderWithHighlight {
	PsXXLargeFader() {
		// no adjustment needed in this code, simply adjust the background svg's width to match the width of the handle by temporarily making it visible in the code below, and tweaking the svg's width as needed (when scaling not 100% between inkscape and Rack)
		setBackgroundSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/patchset/fader-xxlarge-bg.svg")));
		setHandleSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/fader-channel.svg")));
		setupSlider();
	}
};



// KNOBS
// --------

struct PmKnobWithArc : MmKnob {
	// internal
	static constexpr float TOP_ANGLE = 3.0f * float(M_PI) / 2.0f;

	// derived class or user must setup
	int8_t* arcColor = NULL;
	int8_t* showArc = NULL;
	bool topCentered = false;

	void drawArc(const DrawArgs &args, float a0, float a1, int8_t arcColor) {
		// possible sizes are currently box.size.x =  15.117000, 23.622047, 38.385826, for the 3 knob sizes in PM at this time
		
		int dir = a1 > a0 ? NVG_CW : NVG_CCW;
		Vec cVec = box.size.div(2.0f);
		float r = (box.size.x * 1.2033f) / 2.0f;// arc radius
		float arcThickness = 1.6f;
		// above settings are for medium knob, tweak below for small and large
		if (box.size.x < 20.0f) {
			// small
			arcThickness = 1.3f;
			r *= 0.98f;
		}
		if (box.size.x > 30.0f) {
			// large
			arcThickness = 2.6f;
			r *= 0.975f;
		}
		
		nvgBeginPath(args.vg);
		nvgLineCap(args.vg, NVG_ROUND);
		nvgArc(args.vg, cVec.x, cVec.y, r, a0, a1, dir);
		nvgStrokeWidth(args.vg, arcThickness);
		nvgStrokeColor(args.vg, PATCHSET_COLORS[arcColor]);
		nvgStroke(args.vg);		
	}

	void drawLayer(const DrawArgs &args, int layer) override {
		MmKnob::drawLayer(args, layer);
		
		if (layer == 1 && arcColor && showArc && *showArc != 0) {
			ParamQuantity* paramQuantity = getParamQuantity();
			if (paramQuantity) {
				float aParam = -10000.0f;
				float aBase = TOP_ANGLE;
				if (!topCentered) {
					aBase += minAngle;
				}
				
				float param = paramQuantity->getValue();
				aParam = TOP_ANGLE + math::rescale(param, paramQuantity->getMinValue(), paramQuantity->getMaxValue(), minAngle, maxAngle);
				drawArc(args, aBase, aParam, *arcColor);
			}
		}
	}
};


struct PmSmallKnob : PmKnobWithArc {
	PmSmallKnob() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/patchset/Trimpot.svg")));
		SvgWidget* bg = new SvgWidget;
		fb->addChildBelow(bg, tw);
		bg->setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/patchset/Trimpot_bg.svg")));
	}
};


struct PmMediumKnob : PmKnobWithArc {
	PmMediumKnob() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/knob-grey-8.svg")));
		SvgWidget* bg = new SvgWidget;
		fb->addChildBelow(bg, tw);
		bg->setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/knob-bg-8.svg")));
	}
};


struct PmLargeKnob : PmKnobWithArc {
	PmLargeKnob() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/patchset/knob-grey-13.svg")));
		SvgWidget* bg = new SvgWidget;
		fb->addChildBelow(bg, tw);
		bg->setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/patchset/knob-bg-13.svg")));
	}
};



// BUTTONS (with lights)
// --------

struct PmCtrlLightWidget : GrayModuleLightWidget {
	int8_t* highlightColor = NULL;
	int8_t oldHighLightColor = -1;
	
	void step() override {
		GrayModuleLightWidget::step();
		if (highlightColor && oldHighLightColor != *highlightColor) {
			baseColors[0] = PATCHSET_COLORS[*highlightColor];
			oldHighLightColor = *highlightColor;
		}
	}
	
};


struct PmSmallButton : SvgSwitch {
	PmSmallButton() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/patchset/button-sm.svg")));
		shadow->opacity = 0.0f;// Turn off shadows
	}
};
struct PmSmallButtonLight : PmCtrlLightWidget {
	PmSmallButtonLight() {
		this->addBaseColor(SCHEME_MM_BLUE_LOGO);
		this->borderColor = color::BLACK_TRANSPARENT;
		this->bgColor = color::BLACK_TRANSPARENT;
		this->box.size = mm2px(Vec(4.126f, 4.126f));// size excludes region that is to remain unlit
	}
};


struct PmMediumButton : SvgSwitch {
	PmMediumButton() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/patchset/button-md.svg")));
		shadow->opacity = 0.0f;// Turn off shadows
	}
};
struct PmMediumButtonLight : PmCtrlLightWidget {
	PmMediumButtonLight() {
		this->addBaseColor(SCHEME_MM_BLUE_LOGO);
		this->borderColor = color::BLACK_TRANSPARENT;
		this->bgColor = color::BLACK_TRANSPARENT;
		this->box.size = mm2px(Vec(6.602f, 6.602f));// size excludes region that is to remain unlit
	}
};


struct PmLargeButton : SvgSwitch {
	PmLargeButton() {
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/patchset/button-lg.svg")));
		shadow->opacity = 0.0f;// Turn off shadows
	}
};
struct PmLargeButtonLight : PmCtrlLightWidget {
	PmLargeButtonLight() {
		this->addBaseColor(SCHEME_MM_BLUE_LOGO);
		this->borderColor = color::BLACK_TRANSPARENT;
		this->bgColor = color::BLACK_TRANSPARENT;
		this->box.size = mm2px(Vec(10.728f, 10.728f));// size excludes region that is to remain unlit
	}
};



// DISPLAYS
// --------

struct TileDisplaySep : LedDisplayChoice {
	int8_t* dispColor = NULL;
	
	TileDisplaySep() {
		box.size = mm2px(Vec(16.32f, 4.0f));
		textOffset = Vec(23.92f, 6.1f);
		text = "----";
		fontPath = std::string(asset::plugin(pluginInstance, "res/fonts/RobotoCondensed-Regular.ttf"));
		dispColor = NULL;
	}
	
	void onButton(const event::Button &e) override {}	
	
	void draw(const DrawArgs &args) override {}// don't want any backgrounds (comment this line out for debugging label)
	
	void drawLayer(const DrawArgs& args, int layer) override {
		nvgScissor(args.vg, RECT_ARGS(args.clipBox));

		if (layer == 1) {
			color = PATCHSET_COLORS[dispColor ? *dispColor : 1];// 1 = light gray
			std::shared_ptr<window::Font> font = APP->window->loadFont(fontPath);
			if (font && font->handle >= 0) {
				nvgFillColor(args.vg, color);
				nvgFontFaceId(args.vg, font->handle);
				nvgTextLetterSpacing(args.vg, psFontSpacing);

				nvgFontSize(args.vg, 11);
				nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
				nvgText(args.vg, textOffset.x, textOffset.y, text.c_str(), NULL);
			}
		}

		Widget::drawLayer(args, layer);
		nvgResetScissor(args.vg);
	}
	
	void onHoverKey(const HoverKeyEvent& e) override {
		// don't want OpaqueWidget to consume this, so that the tile hotkeys can fall through the tile display labels
	}

};

struct TileDisplayController : TileDisplaySep {
	// for now, same instantiation as TileDisplaySep
	// don't setup the dispColor pointer to revert to gray 
};