//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#pragma once

#include "MixerMenus.hpp"


// Filter cutoffs

struct FilterCutWidget : ParamWidget {
	FilterCutWidget() {
		box.size = Vec(6.0f, 6.0f);
	};
};

struct HPFCutoffParamQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		float valCut = getValue();
		if (valCut >= GlobalConst::minHPFCutoffFreq) {
			return string::f("%i", (int)(math::normalizeZero(valCut) + 0.5f));
		}
		else {
			return "OFF";
		}
	}
	// std::string getLabel() override {return "HPF Cutoff";}
	std::string getUnit() override {
		if (getValue() >= GlobalConst::minHPFCutoffFreq) {
			return " Hz";
		}
		else {
			return "";
		}
	}
};
struct LPFCutoffParamQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		float valCut = getValue();
		if (valCut <= GlobalConst::maxLPFCutoffFreq) {
			valCut =  std::round(valCut / 100.0f);
			return string::f("%g", math::normalizeZero(valCut / 10.0f));
		}
		else {
			return "OFF";
		}
	}
	// std::string getLabel() override {return "LPF Cutoff";}
	std::string getUnit() override {
		if (getValue() <= GlobalConst::maxLPFCutoffFreq) {
			return " kHz";
		}
		else {
			return "";
		}
	}
};



// Fade pointer
// --------------------


struct CvAndFadePointerBase : TransparentWidget {
	// instantiator must setup:
	Param *srcParam;// to know where the fader is
	float *srcParamWithCV = NULL;// for cv pointer, NULL indicates when cv pointers are not used (no cases so far)
	PackedBytes4* colorAndCloak;// cv pointers have same color as displays
	float *srcFadeGain = NULL;// to know where to position the pointer, NULL indicates when fader pointers are not used (aux ret faders for example)
	float *srcFadeRate;// mute when < minFadeRate, fade when >= minFadeRate
	int8_t* dispColorLocalPtr;
	
	// derived class must setup:
	// box.size // inherited from TransparentWidget, no need to declare
	float faderMaxLinearGain;
	int faderScalingExponent;
	
	// local 
	float maxTFader;

	
	void prepareMaxFader() {
		maxTFader = std::pow(faderMaxLinearGain, 1.0f / faderScalingExponent);
	}


	void drawLayer(const DrawArgs &args, int layer) override {
		if (layer == 1) {
			static const float prtHeight = 2.72f  * SVG_DPI / MM_PER_IN;// height of pointer, width is determined by box.size.x in derived struct
			// cv pointer (draw only when cv has en effect)
			if (srcParamWithCV != NULL && *srcParamWithCV != -100.0f && (colorAndCloak->cc4[detailsShow] & ~colorAndCloak->cc4[cloakedMode] & 0x4) != 0) {// -1.0f indicates not to show cv pointer
				float cvPosNormalized = *srcParamWithCV / maxTFader;
				float vertPos = box.size.y - box.size.y * cvPosNormalized;// in px
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg, 0, vertPos - prtHeight / 2.0f);
				nvgLineTo(args.vg, box.size.x, vertPos);
				nvgLineTo(args.vg, 0, vertPos + prtHeight / 2.0f);
				nvgClosePath(args.vg);
				int colorIndex = (colorAndCloak->cc4[dispColorGlobal] < 7) ? colorAndCloak->cc4[dispColorGlobal] : (*dispColorLocalPtr);
				nvgFillColor(args.vg, DISP_COLORS[colorIndex]);
				nvgFill(args.vg);
				nvgStrokeColor(args.vg, SCHEME_BLACK);
				nvgStrokeWidth(args.vg, mm2px(0.11f));
				nvgStroke(args.vg);
			}
			// fade pointer (draw only when in mute mode, or when in fade mode and less than unity gain)
			if (srcFadeGain != NULL && *srcFadeRate >= GlobalConst::minFadeRate && *srcFadeGain < 1.0f  && colorAndCloak->cc4[cloakedMode] == 0) {
				float fadePosNormalized;
				if (srcParamWithCV == NULL || *srcParamWithCV == -100.0f) {
					fadePosNormalized = srcParam->getValue();
				}
				else {
					fadePosNormalized = *srcParamWithCV;
				}
				fadePosNormalized /= maxTFader;
				float vertPos = box.size.y - box.size.y * fadePosNormalized * (*srcFadeGain);// in px
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg, 0, vertPos - prtHeight / 2.0f);
				nvgLineTo(args.vg, box.size.x, vertPos);
				nvgLineTo(args.vg, 0, vertPos + prtHeight / 2.0f);
				nvgClosePath(args.vg);
				nvgFillColor(args.vg, FADE_POINTER_FILL);
				nvgFill(args.vg);
				nvgStrokeColor(args.vg, SCHEME_BLACK);
				nvgStrokeWidth(args.vg, mm2px(0.11f));
				nvgStroke(args.vg);
			}
		}
	}	
};


struct CvAndFadePointerTrack : CvAndFadePointerBase {
	CvAndFadePointerTrack() {
		box.size = mm2px(math::Vec(2.24, 42));
		faderMaxLinearGain = GlobalConst::trkAndGrpFaderMaxLinearGain;
		faderScalingExponent = GlobalConst::trkAndGrpFaderScalingExponent;
		prepareMaxFader();
	}
};
struct CvAndFadePointerGroup : CvAndFadePointerBase {
	CvAndFadePointerGroup() {
		box.size = mm2px(math::Vec(2.24, 42));
		faderMaxLinearGain = GlobalConst::trkAndGrpFaderMaxLinearGain;
		faderScalingExponent = GlobalConst::trkAndGrpFaderScalingExponent;
		prepareMaxFader();
	}
};
struct CvAndFadePointerMaster : CvAndFadePointerBase {
	CvAndFadePointerMaster() {
		box.size = mm2px(math::Vec(2.24, 60));
		faderMaxLinearGain = GlobalConst::masterFaderMaxLinearGain;
		faderScalingExponent = GlobalConst::masterFaderScalingExponent;
		prepareMaxFader();
	}
};
struct CvAndFadePointerAuxRet : CvAndFadePointerBase {
	CvAndFadePointerAuxRet() {
		box.size = mm2px(math::Vec(2.24, 30));
		faderMaxLinearGain = GlobalConst::globalAuxReturnMaxLinearGain;
		faderScalingExponent = GlobalConst::globalAuxReturnScalingExponent;
		prepareMaxFader();
	}
};



	

// --------------------
// Displays 
// --------------------


// Non-Editable track and group (used by auxspander)
// --------------------

struct TrackAndGroupLabel : LedDisplayChoice {
	int8_t *trackOrGroupMomentCvSendMutePtr;
	PackedBytes4* directOutPanStereoMomentCvLinearVol = NULL;
	int8_t* dispColorPtr = NULL;
	int8_t* dispColorLocalPtr;
	
	TrackAndGroupLabel() {
		box.size = mm2px(Vec(14.6f, 5.0f));
		textOffset = Vec(4.2f, 11.3f);
		text = "-00-";
	};
	
	void draw(const DrawArgs &args) override {}	// don't want background, which is in draw, actual text is in drawLayer
	
	void drawLayer(const DrawArgs &args, int layer) override {
		if (layer == 1) {
			if (dispColorPtr) {
				int colorIndex = *dispColorPtr < 7 ? *dispColorPtr : *dispColorLocalPtr;
				color = DISP_COLORS[colorIndex];
			}	
		}
		LedDisplayChoice::drawLayer(args, layer);
	}
	
	void onButton(const event::Button &e) override {
		if (e.button == GLFW_MOUSE_BUTTON_RIGHT && e.action == GLFW_PRESS) {
			if (directOutPanStereoMomentCvLinearVol->cc4[2] < 2) return; // remove this if ever there are permanent settings
			
			ui::Menu *menu = createMenu();
			
			menu->addChild(createMenuLabel("Settings: "));// + std::string(srcTrack->trackName, 4)));
			
			if (directOutPanStereoMomentCvLinearVol->cc4[2] >= 2) {
				MomentaryCvModeItem *momentSendMuteItem = createMenuItem<MomentaryCvModeItem>("Send mute CV", RIGHT_ARROW);
				momentSendMuteItem->momentaryCvButtonsSrc = trackOrGroupMomentCvSendMutePtr;
				momentSendMuteItem->isGlobal = false;
				menu->addChild(momentSendMuteItem);
			}
			
			e.consume(this);
			return;
		}
		LedDisplayChoice::onButton(e);
	}
};



// Group select parameter with non-editable label, but will respond to hover key press
// --------------------

struct GroupSelectDisplay : ParamWidget {
	LedDisplayChoice ldc;
	int oldDispColor = -1;
	PackedBytes4 *srcColor = NULL;
	int8_t* srcColorLocal;
	int numGroups;// N_GRP
	
	GroupSelectDisplay() {
		box.size = Vec(15, 16);
		ldc.box.size = box.size;
		ldc.textOffset = math::Vec(4.9f, 11.7f);
		ldc.bgColor.a = 0.0f;
		ldc.text = "-";
	};
	
	void drawLayer(const DrawArgs &args, int layer) override {
		if (layer == 1) {
			int grp = 0;
			ParamQuantity* paramQuantity = getParamQuantity();
			if (paramQuantity) {
				grp = (int)(paramQuantity->getValue() + 0.5f);
			}
			ldc.text[0] = (char)(grp >= 1 &&  grp <= 4 ? grp + 0x30 : '-');
			ldc.text[1] = 0;
			if (srcColor) {
				int colorIndex = srcColor->cc4[dispColorGlobal] < 7 ? srcColor->cc4[dispColorGlobal] : *srcColorLocal;
				if (colorIndex != oldDispColor) {
					ldc.color = DISP_COLORS[colorIndex];
					oldDispColor = colorIndex;
				}
			}
		}
		ldc.drawLayer(args, layer);
		ParamWidget::drawLayer(args, layer);
	};

	void onHoverKey(const event::HoverKey &e) override {
		ParamQuantity* paramQuantity = getParamQuantity();
		if (paramQuantity) {
			if (e.action == GLFW_PRESS) {
				if (e.key >= GLFW_KEY_1 && e.key <= (GLFW_KEY_0 + numGroups)) {
					paramQuantity->setValue((float)(e.key - GLFW_KEY_0));
				}
				else if (e.key >= GLFW_KEY_KP_1 && e.key <= (GLFW_KEY_KP_0 + numGroups)) {
					paramQuantity->setValue((float)(e.key - GLFW_KEY_KP_0));
				}
				else if ( ((e.mods & RACK_MOD_MASK) == 0) && (
						(e.key >= GLFW_KEY_A && e.key <= GLFW_KEY_Z) ||
						e.key == GLFW_KEY_SPACE || e.key == GLFW_KEY_MINUS || 
						e.key == GLFW_KEY_0 || e.key == GLFW_KEY_KP_0 || 
						(e.key >= (GLFW_KEY_1 + numGroups) && e.key <= GLFW_KEY_9) || 
						(e.key >= (GLFW_KEY_KP_1 + numGroups) && e.key <= GLFW_KEY_KP_9) ) ){
					paramQuantity->setValue(0.0f);
				}
			}
		}
	}
};


// Buttons that don't have their own param but linked to a param, and don't need to be polled by the dsp engine, 
//   they will do thier own processing in here and update their source automatically
// --------------------

struct MmGroupMinusButtonNotify : MmGroupMinusButtonNoParam {
	Param* sourceParam = NULL;// param that is mapped to this
	float numGroups;// (float)N_GRP
	
	void onChange(const event::Change &e) override {// called after value has changed
		MmGroupMinusButtonNoParam::onChange(e);
		if (sourceParam && state != 0) {
			float group = sourceParam->getValue();// separate variable so no glitch
			if (group < 0.5f) group = numGroups;
			else group -= 1.0f;
			sourceParam->setValue(group);
		}		
	}
};


struct MmGroupPlusButtonNotify : MmGroupPlusButtonNoParam {
	Param* sourceParam = NULL;// param that is mapped to this
	int numGroups;// N_GRP
	
	void onChange(const event::Change &e) override {// called after value has changed
		MmGroupPlusButtonNoParam::onChange(e);
		if (sourceParam && state != 0) {
			float group = sourceParam->getValue();// separate variable so no glitch
			if (group > (numGroups - 0.5f)) group = 0.0f;
			else group += 1.0f;
			sourceParam->setValue(group);
		}		
	}
};



// switch with dual display types (for Mute/Fade buttons)
// --------------------
static const NVGcolor muteFadeOnCols[2] = {nvgRGB(0xD4, 0x13, 0X08), nvgRGB(0xFF, 0x6A, 0x1F)};// this should match the color of fill of mute-on/fade-on buttons

struct SvgSwitchDual : SvgSwitch {

	float* type = NULL;// mute when < minFadeRate, fade when >= minFadeRate
    float oldType = -1.0f;
	std::vector<std::shared_ptr<Svg>> framesAll;
	std::vector<std::string> frameAltNames;
	bool manualDrawTopOverride = false;
	NVGcolor haloColor = muteFadeOnCols[0];

	SvgSwitchDual() {
		shadow->opacity = 0.0f;// Turn off shadows
	}
	
	void addFrameAll(std::shared_ptr<Svg> svg) {
		framesAll.push_back(svg);
		if (framesAll.size() == 2) {
			addFrame(framesAll[0]);
			addFrame(framesAll[1]);
		}
	}
    void addFrameAlt(std::string filename) {frameAltNames.push_back(filename);}
	void step() override {
		if( (type != NULL) && (*type != oldType) ) {
			if (!frameAltNames.empty()) {
				for (std::string strName : frameAltNames) {
					framesAll.push_back(APP->window->loadSvg(strName));
				}
				frameAltNames.clear();
			}
			int typeOffset = (*type < GlobalConst::minFadeRate ? 0 : 2);
			frames[0]=framesAll[typeOffset + 0];
			frames[1]=framesAll[typeOffset + 1];
			haloColor = muteFadeOnCols[typeOffset >> 1];
			oldType = *type;
			onChange(*(new event::Change()));// required because of the way SVGSwitch changes images, we only change the frames above.
			fb->dirty = true;// dirty is not sufficient when changing via frames assignments above (i.e. onChange() is required)
		}
		SvgSwitch::step();
	}
	
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
				drawRectHalo(args, box.size, haloColor, 0.0f);
			}
			manualDrawTopOverride = true;
			draw(args);
			manualDrawTopOverride = false;
		}
		SvgSwitch::drawLayer(args, layer);
	}		
};



struct MmMuteFadeButton : SvgSwitchDual {
	MmMuteFadeButton() {
		momentary = false;
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/mute-off.svg")));
		addFrameAll(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/mixer/mute-on.svg")));
		addFrameAlt(asset::plugin(pluginInstance, "res/comp/mixer/fade-off.svg"));
		addFrameAlt(asset::plugin(pluginInstance, "res/comp/mixer/fade-on.svg"));
		shadow->opacity = 0.0;
	}
};



// knobs with color theme arc
// --------------------


struct MmSmallKnobGreyWithArc : MmKnobWithArc {
	// internal
	int oldDispColor = -1;
	
	// user must setup
	int8_t *dispColorGlobalSrc = NULL;
	int8_t *dispColorLocalSrc;
	
	
	MmSmallKnobGreyWithArc() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/knob-grey-7.5.svg")));
		SvgWidget* bg = new SvgWidget;
		fb->addChildBelow(bg, tw);
		bg->setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/comp/knob-bg-7.5.svg")));
		topCentered = true;
	}
	
	void drawLayer(const DrawArgs &args, int layer) override {
		if (layer == 1) {
			if (dispColorGlobalSrc) {
				int colorIndex = *dispColorGlobalSrc < 7 ? *dispColorGlobalSrc : *dispColorLocalSrc;
				if (colorIndex != oldDispColor) {
					arcColor = DISP_COLORS[colorIndex];// arc color, same as displays
					oldDispColor = colorIndex;
				}
			}
		}
		MmKnobWithArc::drawLayer(args, layer);
	}
};



// Editable track, group and aux displays base struct
// --------------------

struct EditableDisplayBase : LedDisplayTextField {
	int numChars = 4;
	bool doubleClick = false;
	Widget* tabNextFocus = NULL;
	PackedBytes4* colorAndCloak = NULL;
	int8_t* dispColorLocal;

	EditableDisplayBase() {
		box.size = mm2px(Vec(12.0f, 5.0f));// svg is 10.6 wide
		textOffset = mm2px(Vec(0.0f, -0.9144));  // was Vec(6.0f, -2.7f); which is 2.032000, -0.914400 in mm
		text = "-00-";
		// DEBUG("%f, %f", 9.0f * MM_PER_IN / SVG_DPI, -2.0 * MM_PER_IN / SVG_DPI);
	};
	
	void draw(const DrawArgs &args) override {}	// don't want background, which is in draw, actual text is in drawLayer
	
	void drawLayer(const DrawArgs &args, int layer) override {
		if (layer == 1) {
			if (colorAndCloak) {
				int colorIndex = colorAndCloak->cc4[dispColorGlobal] < 7 ? colorAndCloak->cc4[dispColorGlobal] : *dispColorLocal;
				color = DISP_COLORS[colorIndex];
			}
			if (cursor > numChars) {
				text.resize(numChars);
				cursor = numChars;
				selection = numChars;
			}
			// nvgBeginPath(args.vg);
			// nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
			// nvgFillColor(args.vg, nvgRGBAf(0.0f, 0.0f, 0.8f, 1.0f));
			// nvgFill(args.vg);		
		}
		LedDisplayTextField::drawLayer(args, layer);
	}

	void onSelectKey(const event::SelectKey& e) override {
		if (e.action == GLFW_PRESS && e.key == GLFW_KEY_TAB && tabNextFocus != NULL) {
			APP->event->setSelectedWidget(tabNextFocus);
			e.consume(this);
			return;			
		}
		LedDisplayTextField::onSelectKey(e);
	}
	
	// don't want spaces since leading spaces are stripped by nanovg (which oui-blendish calls), so convert to dashes
	void onSelectText(const event::SelectText &e) override {
		if (e.codepoint < 128) {
			char letter = (char) e.codepoint;
			if (letter == 0x20) {// space
				letter = 0x2D;// hyphen
			}
			std::string newText(1, letter);
			insertText(newText);
		}
		e.consume(this);	
		
		if (text.length() > (unsigned)numChars) {
			text = text.substr(0, numChars);
			cursor = numChars;
			selection = numChars;
		}
	}
	
	void onDoubleClick(const event::DoubleClick& e) override {
		doubleClick = true;
	}
	
	void onButton(const event::Button &e) override {
		if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_RELEASE) {
			if (doubleClick) {
				doubleClick = false;
				selectAll();
			}
		}
		LedDisplayTextField::onButton(e);
	}
}; 


// Master display editable label with menu
// --------------------

struct MasterDisplay : EditableDisplayBase {
	bool* dcBlock;
	int* clipping;
	float* fadeRate;
	float* fadeProfile;
	int8_t* vuColorThemeLocal;
	PackedBytes4* directOutPanStereoMomentCvLinearVol;
	int8_t* momentCvMuteLocal;
	int8_t* momentCvDimLocal;
	int8_t* momentCvMonoLocal;
	int8_t* chainOnly;
	float* dimGain;
	char* masterLabel;
	float* dimGainIntegerDB;
	int64_t* idSrc;
	int8_t* masterFaderScalesSendsSrc;
	
	MasterDisplay() {
		numChars = 6;
		box.size = mm2px(Vec(16.0f, 5.3f));
		textOffset = mm2px(Vec(0.0f, -0.6773)); ; // was Vec(9.0f, -2.0f) which is 3.048000, -0.677333 in mm
		text = "-0000-";
	}
	
	void onButton(const event::Button &e) override {
		if (e.button == GLFW_MOUSE_BUTTON_RIGHT && e.action == GLFW_PRESS) {
			ui::Menu *menu = createMenu();

			menu->addChild(createMenuLabel("Master settings: " + std::string(masterLabel) + string::f("  (id %" PRId64 ")", *idSrc + 1)));
			
			FadeRateSlider *fadeSlider = new FadeRateSlider(fadeRate);
			fadeSlider->box.size.x = 200.0f;
			menu->addChild(fadeSlider);
			
			FadeProfileSlider *fadeProfSlider = new FadeProfileSlider(fadeProfile);
			fadeProfSlider->box.size.x = 200.0f;
			menu->addChild(fadeProfSlider);
			
			DimGainSlider *dimSliderItem = new DimGainSlider(dimGain, dimGainIntegerDB);
			dimSliderItem->box.size.x = 200.0f;
			menu->addChild(dimSliderItem);
			
			menu->addChild(createCheckMenuItem("DC blocker", "",
				[=]() {return *dcBlock;},
				[=]() {*dcBlock = !*dcBlock;}
			));
			
			ClippingItem *clipItem = createMenuItem<ClippingItem>("Clipping", RIGHT_ARROW);
			clipItem->clippingSrc = clipping;
			menu->addChild(clipItem);

			menu->addChild(createCheckMenuItem("Apply master fader to aux sends", "",
				[=]() {return *masterFaderScalesSendsSrc != 0;},
				[=]() {*masterFaderScalesSendsSrc ^= 0x1;}
			));
			
			if (directOutPanStereoMomentCvLinearVol->cc4[2] >= 2) {
				MomentaryCvModeItem *momentMastMuteItem = createMenuItem<MomentaryCvModeItem>("Master mute CV", RIGHT_ARROW);
				momentMastMuteItem->momentaryCvButtonsSrc = momentCvMuteLocal;
				momentMastMuteItem->isGlobal = false;
				menu->addChild(momentMastMuteItem);
				
				MomentaryCvModeItem *momentMastDimItem = createMenuItem<MomentaryCvModeItem>("Master dim CV", RIGHT_ARROW);
				momentMastDimItem->momentaryCvButtonsSrc = momentCvDimLocal;
				momentMastDimItem->isGlobal = false;
				menu->addChild(momentMastDimItem);
				
				MomentaryCvModeItem *momentMastMonoItem = createMenuItem<MomentaryCvModeItem>("Master mono CV", RIGHT_ARROW);
				momentMastMonoItem->momentaryCvButtonsSrc = momentCvMonoLocal;
				momentMastMonoItem->isGlobal = false;
				menu->addChild(momentMastMonoItem);
			}
			
			
			if (colorAndCloak->cc4[vuColorGlobal] >= numVuThemes) {	
				VuColorItem *vuColItem = createMenuItem<VuColorItem>("VU Colour", RIGHT_ARROW);
				vuColItem->srcColor = vuColorThemeLocal;
				vuColItem->isGlobal = false;
				menu->addChild(vuColItem);
			}
			
			if (colorAndCloak->cc4[dispColorGlobal] >= numDispThemes) {
				DispColorItem *dispColItem = createMenuItem<DispColorItem>("Display colour", RIGHT_ARROW);
				dispColItem->srcColor = dispColorLocal;
				dispColItem->isGlobal = false;
				menu->addChild(dispColItem);
			}
			
			menu->addChild(createCheckMenuItem("Solo chain inputs", "",
				[=]() {return *chainOnly;},
				[=]() {*chainOnly ^= 0x1;}
			));
			
			e.consume(this);
			return;
		}
		EditableDisplayBase::onButton(e);		
	}
	void onChange(const event::Change &e) override {
		snprintf(masterLabel, 7, "%s", text.c_str());
		EditableDisplayBase::onChange(e);
	};
};



// Track display editable label with menu
// --------------------

template <typename TMixerTrack>
struct TrackDisplay : EditableDisplayBase {
	TMixerTrack *tracks = NULL;
	int trackNumSrc;
	bool *auxExpanderPresentPtr;
	int numTracks;
	int *updateTrackLabelRequestPtr;
	int8_t *trackOrGroupResetInAuxPtr;
	int *trackMoveInAuxRequestPtr;
	PortWidget **inputWidgets;
	ParamQuantity* hpfParamQuantity;
	ParamQuantity* lpfParamQuantity;
	int64_t *idPtr;


	void onButton(const event::Button &e) override {
		if (e.button == GLFW_MOUSE_BUTTON_RIGHT && e.action == GLFW_PRESS) {
			ui::Menu *menu = createMenu();
			TMixerTrack *srcTrack = &(tracks[trackNumSrc]);
			
			menu->addChild(createMenuLabel("Track settings: " + std::string(srcTrack->trackName, 4)));
			
			menu->addChild(createCheckMenuItem("Invert input", "",
				[=]() {return srcTrack->invertInput != 0;},
				[=]() {srcTrack->invertInput ^= 0x1;}
			));

			GainAdjustSlider *trackGainAdjustSlider = new GainAdjustSlider(&(srcTrack->gainAdjust), -20.0f, 20.0f);
			trackGainAdjustSlider->box.size.x = 200.0f;
			menu->addChild(trackGainAdjustSlider);
			
			HPFCutoffSlider2 *trackHPFAdjustSlider = new HPFCutoffSlider2(hpfParamQuantity);
			trackHPFAdjustSlider->box.size.x = 200.0f;  
			menu->addChild(trackHPFAdjustSlider);
						
			LPFCutoffSlider2 *trackLPFAdjustSlider = new LPFCutoffSlider2(lpfParamQuantity);
			trackLPFAdjustSlider->box.size.x = 200.0f;  
			menu->addChild(trackLPFAdjustSlider);
						
			if (srcTrack->stereo) {
				StereoWidthLevelSlider *widthSlider = new StereoWidthLevelSlider(&(srcTrack->stereoWidth));
				widthSlider->box.size.x = 200.0f;
				menu->addChild(widthSlider);
			}
			else {
				menu->addChild(createMenuLabel("Stereo width: N/A"));
			}
			
			PanCvLevelSlider *panCvSlider = new PanCvLevelSlider(&(srcTrack->panCvLevel));
			panCvSlider->box.size.x = 200.0f;
			menu->addChild(panCvSlider);
			
			FadeRateSlider *fadeSlider = new FadeRateSlider(srcTrack->fadeRate);
			fadeSlider->box.size.x = 200.0f;
			menu->addChild(fadeSlider);
			
			FadeProfileSlider *fadeProfSlider = new FadeProfileSlider(&(srcTrack->fadeProfile));
			fadeProfSlider->box.size.x = 200.0f;
			menu->addChild(fadeProfSlider);
			
			menu->addChild(createCheckMenuItem("Link fader and fade", "",
				[=]() {return isLinked(&(srcTrack->gInfo->linkBitMask), trackNumSrc);},
				[=]() {toggleLinked(&(srcTrack->gInfo->linkBitMask), trackNumSrc);}
			));

			PolyStereoItem *polySteItem = createMenuItem<PolyStereoItem>("Poly input behavior", RIGHT_ARROW);
			polySteItem->polyStereoSrc = &(srcTrack->polyStereo);
			menu->addChild(polySteItem);

			if (srcTrack->gInfo->directOutPanStereoMomentCvLinearVol.cc4[0] >= 4) {
				TapModeItem *directOutsItem = createMenuItem<TapModeItem>("Direct outs", RIGHT_ARROW);
				directOutsItem->tapModePtr = &(srcTrack->directOutsMode);
				directOutsItem->isGlobal = false;
				menu->addChild(directOutsItem);
			}

			if (srcTrack->gInfo->filterPos >= 2) {
				FilterPosItem *filterPosItem = createMenuItem<FilterPosItem>("Filters", RIGHT_ARROW);
				filterPosItem->filterPosSrc = &(srcTrack->filterPos);
				filterPosItem->isGlobal = false;
				menu->addChild(filterPosItem);
			}

			if (srcTrack->gInfo->auxSendsMode >= 4 && *auxExpanderPresentPtr) {
				TapModeItem *auxSendsItem = createMenuItem<TapModeItem>("Aux sends", RIGHT_ARROW);
				auxSendsItem->tapModePtr = &(srcTrack->auxSendsMode);
				auxSendsItem->isGlobal = false;
				menu->addChild(auxSendsItem);
			}

			if (srcTrack->gInfo->directOutPanStereoMomentCvLinearVol.cc4[1] >= 3) {
				PanLawStereoItem *panLawStereoItem = createMenuItem<PanLawStereoItem>("Stereo pan mode", RIGHT_ARROW);
				panLawStereoItem->panLawStereoSrc = &(srcTrack->panLawStereo);
				panLawStereoItem->isGlobal = false;
				menu->addChild(panLawStereoItem);
			}

			if (srcTrack->gInfo->directOutPanStereoMomentCvLinearVol.cc4[2] >= 2) {
				MomentaryCvModeItem *momentTrackMuteItem = createMenuItem<MomentaryCvModeItem>("Mute CV", RIGHT_ARROW);
				momentTrackMuteItem->momentaryCvButtonsSrc = &(srcTrack->momentCvMuteLocal);
				momentTrackMuteItem->isGlobal = false;
				menu->addChild(momentTrackMuteItem);
				
				MomentaryCvModeItem *momentTrackSoloItem = createMenuItem<MomentaryCvModeItem>("Solo CV", RIGHT_ARROW);
				momentTrackSoloItem->momentaryCvButtonsSrc = &(srcTrack->momentCvSoloLocal);
				momentTrackSoloItem->isGlobal = false;
				menu->addChild(momentTrackSoloItem);
			}
	
			if (srcTrack->gInfo->colorAndCloak.cc4[vuColorGlobal] >= numVuThemes) {	
				VuColorItem *vuColItem = createMenuItem<VuColorItem>("VU Colour", RIGHT_ARROW);
				vuColItem->srcColor = &(srcTrack->vuColorThemeLocal);
				vuColItem->isGlobal = false;
				menu->addChild(vuColItem);
			}

			if (srcTrack->gInfo->colorAndCloak.cc4[dispColorGlobal] >= numDispThemes) {
				DispColorItem *dispColItem = createMenuItem<DispColorItem>("Display colour", RIGHT_ARROW);
				dispColItem->srcColor = &(srcTrack->dispColorLocal);
				dispColItem->isGlobal = false;
				menu->addChild(dispColItem);
			}
			
			menu->addChild(new MenuSeparator());

			menu->addChild(createMenuLabel("Actions: " + std::string(srcTrack->trackName, 4)));

			InitializeTrackItem<TMixerTrack> *initTrackItem = createMenuItem<InitializeTrackItem<TMixerTrack>>("Initialize track settings", "");
			initTrackItem->srcTrack = srcTrack;
			initTrackItem->updateTrackLabelRequestPtr = updateTrackLabelRequestPtr;
			initTrackItem->trackOrGroupResetInAuxPtr = trackOrGroupResetInAuxPtr;
			menu->addChild(initTrackItem);			

			CopyTrackSettingsItem<TMixerTrack> *copyItem = createMenuItem<CopyTrackSettingsItem<TMixerTrack>>("Copy track menu settings to:", RIGHT_ARROW);
			copyItem->tracks = tracks;
			copyItem->trackNumSrc = trackNumSrc;
			copyItem->numTracks = numTracks;
			menu->addChild(copyItem);
			
			TrackReorderItem<TMixerTrack> *reodrerItem = createMenuItem<TrackReorderItem<TMixerTrack>>("Move to:", RIGHT_ARROW);
			reodrerItem->tracks = tracks;
			reodrerItem->trackNumSrc = trackNumSrc;
			reodrerItem->numTracks = numTracks;
			reodrerItem->updateTrackLabelRequestPtr = updateTrackLabelRequestPtr;
			reodrerItem->trackMoveInAuxRequestPtr = trackMoveInAuxRequestPtr;
			reodrerItem->inputWidgets = inputWidgets;
			reodrerItem->idPtr = idPtr;
			menu->addChild(reodrerItem);
			
			e.consume(this);
			return;
		}
		EditableDisplayBase::onButton(e);
	}
	void onChange(const event::Change &e) override {
		(*((uint32_t*)(tracks[trackNumSrc].trackName))) = 0x20202020;
		for (int i = 0; i < std::min(4, (int)text.length()); i++) {
			tracks[trackNumSrc].trackName[i] = text[i];
		}
		EditableDisplayBase::onChange(e);
	};
};


// Group display editable label with menu
// --------------------

template <typename TMixerGroup>
struct GroupDisplay : EditableDisplayBase {
	TMixerGroup *srcGroup = NULL;
	bool *auxExpanderPresentPtr;
	int numTracks;// used to calc group offset in linkBitMask
	int *updateTrackLabelRequestPtr;
	int8_t *trackOrGroupResetInAuxPtr;
	ParamQuantity* hpfParamQuantity;
	ParamQuantity* lpfParamQuantity;

	void onButton(const event::Button &e) override {
		if (e.button == GLFW_MOUSE_BUTTON_RIGHT && e.action == GLFW_PRESS) {
			ui::Menu *menu = createMenu();

			menu->addChild(createMenuLabel("Group settings: " + std::string(srcGroup->groupName, 4)));
			
			GainAdjustSlider *groupGainAdjustSlider = new GainAdjustSlider(&(srcGroup->gainAdjust), -20.0f, 20.0f);
			groupGainAdjustSlider->box.size.x = 200.0f;
			menu->addChild(groupGainAdjustSlider);
			
			HPFCutoffSlider2 *trackHPFAdjustSlider = new HPFCutoffSlider2(hpfParamQuantity);
			trackHPFAdjustSlider->box.size.x = 200.0f;  
			menu->addChild(trackHPFAdjustSlider);
						
			LPFCutoffSlider2 *trackLPFAdjustSlider = new LPFCutoffSlider2(lpfParamQuantity);
			trackLPFAdjustSlider->box.size.x = 200.0f;  
			menu->addChild(trackLPFAdjustSlider);
						
			StereoWidthLevelSlider *widthSlider = new StereoWidthLevelSlider(&(srcGroup->stereoWidth));
			widthSlider->box.size.x = 200.0f;
			menu->addChild(widthSlider);

			PanCvLevelSlider *panCvSlider = new PanCvLevelSlider(&(srcGroup->panCvLevel));
			panCvSlider->box.size.x = 200.0f;
			menu->addChild(panCvSlider);
			
			FadeRateSlider *fadeSlider = new FadeRateSlider(srcGroup->fadeRate);
			fadeSlider->box.size.x = 200.0f;
			menu->addChild(fadeSlider);
			
			FadeProfileSlider *fadeProfSlider = new FadeProfileSlider(&(srcGroup->fadeProfile));
			fadeProfSlider->box.size.x = 200.0f;
			menu->addChild(fadeProfSlider);
			
			int groupNumForLink = numTracks + srcGroup->groupNum;
			menu->addChild(createCheckMenuItem("Link fader and fade", "",
				[=]() {return isLinked(&(srcGroup->gInfo->linkBitMask), groupNumForLink);},
				[=]() {toggleLinked(&(srcGroup->gInfo->linkBitMask), groupNumForLink);}
			));

			if (srcGroup->gInfo->directOutPanStereoMomentCvLinearVol.cc4[0] >= 4) {
				TapModeItem *directOutsItem = createMenuItem<TapModeItem>("Direct outs", RIGHT_ARROW);
				directOutsItem->tapModePtr = &(srcGroup->directOutsMode);
				directOutsItem->isGlobal = false;
				menu->addChild(directOutsItem);
			}

			if (srcGroup->gInfo->filterPos >= 2) {
				FilterPosItem *filterPosItem = createMenuItem<FilterPosItem>("Filters", RIGHT_ARROW);
				filterPosItem->filterPosSrc = &(srcGroup->filterPos);
				filterPosItem->isGlobal = false;
				menu->addChild(filterPosItem);
			}

			if (srcGroup->gInfo->auxSendsMode >= 4 && *auxExpanderPresentPtr) {
				TapModeItem *auxSendsItem = createMenuItem<TapModeItem>("Aux sends", RIGHT_ARROW);
				auxSendsItem->tapModePtr = &(srcGroup->auxSendsMode);
				auxSendsItem->isGlobal = false;
				menu->addChild(auxSendsItem);
			}

			if (srcGroup->gInfo->directOutPanStereoMomentCvLinearVol.cc4[1] >= 3) {
				PanLawStereoItem *panLawStereoItem = createMenuItem<PanLawStereoItem>("Stereo pan mode", RIGHT_ARROW);
				panLawStereoItem->panLawStereoSrc = &(srcGroup->panLawStereo);
				panLawStereoItem->isGlobal = false;
				menu->addChild(panLawStereoItem);
			}

			if (srcGroup->gInfo->directOutPanStereoMomentCvLinearVol.cc4[2] >= 2) {
				MomentaryCvModeItem *momentGroupMuteItem = createMenuItem<MomentaryCvModeItem>("Mute CV", RIGHT_ARROW);
				momentGroupMuteItem->momentaryCvButtonsSrc = &(srcGroup->momentCvMuteLocal);
				momentGroupMuteItem->isGlobal = false;
				menu->addChild(momentGroupMuteItem);
				
				MomentaryCvModeItem *momentGroupSoloItem = createMenuItem<MomentaryCvModeItem>("Solo CV", RIGHT_ARROW);
				momentGroupSoloItem->momentaryCvButtonsSrc = &(srcGroup->momentCvSoloLocal);
				momentGroupSoloItem->isGlobal = false;
				menu->addChild(momentGroupSoloItem);
			}

			if (srcGroup->gInfo->colorAndCloak.cc4[vuColorGlobal] >= numVuThemes) {	
				VuColorItem *vuColItem = createMenuItem<VuColorItem>("VU Colour", RIGHT_ARROW);
				vuColItem->srcColor = &(srcGroup->vuColorThemeLocal);
				vuColItem->isGlobal = false;
				menu->addChild(vuColItem);
			}
			
			if (srcGroup->gInfo->colorAndCloak.cc4[dispColorGlobal] >= numDispThemes) {
				DispColorItem *dispColItem = createMenuItem<DispColorItem>("Display colour", RIGHT_ARROW);
				dispColItem->srcColor = &(srcGroup->dispColorLocal);
				dispColItem->isGlobal = false;
				menu->addChild(dispColItem);
			}
			
			menu->addChild(new MenuSeparator());

			menu->addChild(createMenuLabel("Actions: " + std::string(srcGroup->groupName, 4)));

			InitializeGroupItem<TMixerGroup> *initGroupItem = createMenuItem<InitializeGroupItem<TMixerGroup>>("Initialize group settings", "");
			initGroupItem->srcGroup = srcGroup;
			initGroupItem->groupNumForLink = groupNumForLink;
			initGroupItem->updateTrackLabelRequestPtr = updateTrackLabelRequestPtr;
			initGroupItem->trackOrGroupResetInAuxPtr = trackOrGroupResetInAuxPtr;
			menu->addChild(initGroupItem);			

			e.consume(this);
			return;
		}
		EditableDisplayBase::onButton(e);
	}
	void onChange(const event::Change &e) override {
		(*((uint32_t*)(srcGroup->groupName))) = 0x20202020;
		for (int i = 0; i < std::min(4, (int)text.length()); i++) {
			srcGroup->groupName[i] = text[i];
		}
		EditableDisplayBase::onChange(e);
	};
};




// Aux display editable label with menu
// --------------------

template <typename TAuxspanderAux>
struct AuxDisplay : EditableDisplayBase {
	TAuxspanderAux *srcAux = NULL;
	int8_t* srcVuColor = NULL;
	int8_t* srcDirectOutsModeLocal = NULL;
	int8_t* srcPanLawStereoLocal = NULL;
	int8_t* srcMomentCvRetMuteLocal = NULL;
	int8_t* srcMomentCvRetSoloLocal = NULL;
	PackedBytes4* directOutPanStereoMomentCvLinearVol = NULL;
	float* srcPanCvLevel = NULL;
	float* srcFadeRatesAndProfiles = NULL;// use [0] for fade rate, [4] for fade profile, pointer must be setup with aux indexing
	char* auxName;
	int auxNumber = 0;
	int numTracks;
	int numGroups;
	int *updateAuxLabelRequestPtr;
	
	void onButton(const event::Button &e) override {
		if (e.button == GLFW_MOUSE_BUTTON_RIGHT && e.action == GLFW_PRESS) {
			ui::Menu *menu = createMenu();

			menu->addChild(createMenuLabel("Aux settings: " + text));
			
			HPFCutoffSlider<TAuxspanderAux> *auxHPFAdjustSlider = new HPFCutoffSlider<TAuxspanderAux>(srcAux);
			auxHPFAdjustSlider->box.size.x = 200.0f;
			menu->addChild(auxHPFAdjustSlider);
			
			LPFCutoffSlider<TAuxspanderAux> *auxLPFAdjustSlider = new LPFCutoffSlider<TAuxspanderAux>(srcAux);
			auxLPFAdjustSlider->box.size.x = 200.0f;
			menu->addChild(auxLPFAdjustSlider);
			
			if (srcAux->stereo) {
				StereoWidthLevelSlider *widthSlider = new StereoWidthLevelSlider(&(srcAux->stereoWidth));
				widthSlider->box.size.x = 200.0f;
				menu->addChild(widthSlider);
			}
			
			PanCvLevelSlider *panCvSlider = new PanCvLevelSlider(srcPanCvLevel);
			panCvSlider->box.size.x = 200.0f;
			menu->addChild(panCvSlider);
			
			FadeRateSlider *fadeSlider = new FadeRateSlider(srcFadeRatesAndProfiles);
			fadeSlider->box.size.x = 200.0f;
			menu->addChild(fadeSlider);
			
			FadeProfileSlider *fadeProfSlider = new FadeProfileSlider(&(srcFadeRatesAndProfiles[4]));
			fadeProfSlider->box.size.x = 200.0f;
			menu->addChild(fadeProfSlider);
			
			if (directOutPanStereoMomentCvLinearVol->cc4[0] >= 4) {
				TapModeItem *directOutsItem = createMenuItem<TapModeItem>("Direct outs", RIGHT_ARROW);
				directOutsItem->tapModePtr = srcDirectOutsModeLocal;
				directOutsItem->isGlobal = false;
				menu->addChild(directOutsItem);
			}

			if (directOutPanStereoMomentCvLinearVol->cc4[1] >= 3) {
				PanLawStereoItem *panLawStereoItem = createMenuItem<PanLawStereoItem>("Stereo pan mode", RIGHT_ARROW);
				panLawStereoItem->panLawStereoSrc = srcPanLawStereoLocal;
				panLawStereoItem->isGlobal = false;
				menu->addChild(panLawStereoItem);
			}

			if (directOutPanStereoMomentCvLinearVol->cc4[2] >= 2) {
				MomentaryCvModeItem *momentRetMuteItem = createMenuItem<MomentaryCvModeItem>("Return mute CV", RIGHT_ARROW);
				momentRetMuteItem->momentaryCvButtonsSrc = srcMomentCvRetMuteLocal;
				momentRetMuteItem->isGlobal = false;
				menu->addChild(momentRetMuteItem);
				
				MomentaryCvModeItem *momentRetSoloItem = createMenuItem<MomentaryCvModeItem>("Return solo CV", RIGHT_ARROW);
				momentRetSoloItem->momentaryCvButtonsSrc = srcMomentCvRetSoloLocal;
				momentRetSoloItem->isGlobal = false;
				menu->addChild(momentRetSoloItem);
			}

			if (colorAndCloak->cc4[vuColorGlobal] >= numVuThemes) {	
				VuColorItem *vuColItem = createMenuItem<VuColorItem>("VU Colour", RIGHT_ARROW);
				vuColItem->srcColor = srcVuColor;
				vuColItem->isGlobal = false;
				menu->addChild(vuColItem);
			}
			
			if (colorAndCloak->cc4[dispColorGlobal] >= numDispThemes) {
				DispColorItem *dispColItem = createMenuItem<DispColorItem>("Display colour", RIGHT_ARROW);
				dispColItem->srcColor = dispColorLocal;
				dispColItem->isGlobal = false;
				menu->addChild(dispColItem);
			}
			
			menu->addChild(new MenuSeparator());

			menu->addChild(createMenuLabel("Actions: " + text));

			InitializeAuxItem<TAuxspanderAux> *initAuxItem = createMenuItem<InitializeAuxItem<TAuxspanderAux>>("Initialize aux settings", "");
			initAuxItem->srcAux = srcAux;
			initAuxItem->numTracks = numTracks;
			initAuxItem->numGroups = numGroups;
			initAuxItem->updateAuxLabelRequestPtr = updateAuxLabelRequestPtr;
			menu->addChild(initAuxItem);			

			e.consume(this);
			return;
		}
		EditableDisplayBase::onButton(e);
	}
	void onChange(const event::Change &e) override {
		(*((uint32_t*)(auxName))) = 0x20202020;
		for (int i = 0; i < std::min(4, (int)text.length()); i++) {
			auxName[i] = text[i];
		}
		EditableDisplayBase::onChange(e);
	};
};




// Special solo button with mutex feature (ctrl-click)

struct MmSoloButtonMutex : MmSoloButton {
	Param *soloParams;// 19 or 15 params in here must be cleared when mutex solo performed on a group (track)
	//  (9 or 7 for jr)
	int baseSoloParamId;
	unsigned long soloMutexUnclickMemory;// for ctrl-unclick. Invalid when soloMutexUnclickMemory == -1
	int soloMutexUnclickMemorySize;// -1 when nothing stored
	int numTracks;
	int numGroups;

	void onButton(const event::Button &e) override {
		if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS) {
			if (((APP->window->getMods() & RACK_MOD_MASK) == RACK_MOD_CTRL)) {
				ParamQuantity* paramQuantity = getParamQuantity();
				int soloParamId = paramQuantity->paramId - baseSoloParamId;
				bool isTrack = (soloParamId < numTracks);
				int end = numTracks + (isTrack ? 0 : numGroups);
				bool turningOnSolo = soloParams[soloParamId].getValue() < 0.5f;
				
				
				if (turningOnSolo) {// ctrl turning on solo: memorize solo states and clear all other solos 
					// memorize solo states in case ctrl-unclick happens
					soloMutexUnclickMemorySize = end;
					soloMutexUnclickMemory = 0;
					for (int i = 0; i < end; i++) {
						if (soloParams[i].getValue() >= 0.5f) {
							soloMutexUnclickMemory |= (1 << i);
						}
					}
					
					// clear 19 or 15 solos (9 or 7 for jr)
					for (int i = 0; i < end; i++) {
						if (soloParamId != i) {
							soloParams[i].setValue(0.0f);
						}
					}
					
				}
				else {// ctrl turning off solo: recall stored solo states
					// reinstate 19 or 15 solos (9 or 7 for jr) 
					if (soloMutexUnclickMemorySize >= 0) {
						for (int i = 0; i < soloMutexUnclickMemorySize; i++) {
							if (soloParamId != i) {
								soloParams[i].setValue((soloMutexUnclickMemory & (1 << i)) != 0 ? 1.0f : 0.0f);
							}
						}
						soloMutexUnclickMemorySize = -1;// nothing in soloMutexUnclickMemory
					}
				}
				e.consume(this);
				return;
			}
			else {
				soloMutexUnclickMemorySize = -1;// nothing in soloMutexUnclickMemory
				if ((APP->window->getMods() & RACK_MOD_MASK) == (RACK_MOD_CTRL | GLFW_MOD_SHIFT)) {
					ParamQuantity* paramQuantity = getParamQuantity();
					for (int i = 0; i < (numTracks + numGroups); i++) {
						if (i != paramQuantity->paramId - baseSoloParamId) {
							soloParams[i].setValue(0.0f);
						}
					}
					e.consume(this);
					return;
				}
			}
		}
		MmSoloButton::onButton(e);		
	}
};



struct MmMuteFadeButtonWithClear : MmMuteFadeButton {
	Param *muteParams;// 19 or 15 params in here must be cleared when mutex mute performed on a group (track)
	//  (9 or 7 for jr)
	int baseMuteParamId;
	int numTracksAndGroups;

	void onButton(const event::Button &e) override {
		if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS) {
			if ((APP->window->getMods() & RACK_MOD_MASK) == (RACK_MOD_CTRL | GLFW_MOD_SHIFT)) {
				ParamQuantity* paramQuantity = getParamQuantity();
				for (int i = 0; i < (numTracksAndGroups); i++) {
					if (i != paramQuantity->paramId - baseMuteParamId) {
						muteParams[i].setValue(0.0f);
					}
				}
				e.consume(this);
				return;
			}
		}
		MmMuteFadeButton::onButton(e);		
	}	
};



// linked faders
// --------------------

struct MmSmallFaderWithLink : MmSmallFader {
	unsigned long* linkBitMaskSrc;
	int baseFaderParamId;
	
	void onButton(const event::Button &e) override {
		ParamQuantity* paramQuantity = getParamQuantity();
		int faderIndex = paramQuantity->paramId - baseFaderParamId;
		if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS) {
			if ((APP->window->getMods() & RACK_MOD_MASK) == RACK_MOD_CTRL) {
				toggleLinked(linkBitMaskSrc, faderIndex);
				e.consume(this);
				return;
			}
			else if ((APP->window->getMods() & RACK_MOD_MASK) == (RACK_MOD_CTRL | GLFW_MOD_SHIFT)) {
				*linkBitMaskSrc = 0;
				e.consume(this);
				return;
			}
		}
		MmSmallFader::onButton(e);		
	}

	void drawLayer(const DrawArgs &args, int layer) override {
		if (layer == 1) {
			ParamQuantity* paramQuantity = getParamQuantity();
			if (paramQuantity) {
				int faderIndex = paramQuantity->paramId - baseFaderParamId;
				if (isLinked(linkBitMaskSrc, faderIndex)) {
					float v = paramQuantity->getScaledValue();
					float offsetY = handle->box.size.y / 2.0f;
					float ypos = math::rescale(v, 0.f, 1.f, minHandlePos.y, maxHandlePos.y) + offsetY;
					
					nvgBeginPath(args.vg);
					nvgMoveTo(args.vg, 0, ypos);
					nvgLineTo(args.vg, box.size.x, ypos);
					nvgClosePath(args.vg);
					nvgStrokeColor(args.vg, SCHEME_RED);
					nvgStrokeWidth(args.vg, mm2px(0.4f));
					nvgStroke(args.vg);
				}
			}
		}
		MmSmallFader::drawLayer(args, layer);
	}
};
