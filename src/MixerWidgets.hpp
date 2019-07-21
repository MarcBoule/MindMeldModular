//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.txt for all licenses
//***********************************************************************************************


#ifndef MMM_MIXERWIDGETS_HPP
#define MMM_MIXERWIDGETS_HPP

#include "Mixer.hpp"



// TODO: make this thing reference a MixerTrack directly, and store the label in there! (make sure this works re push/pull)


struct GainAdjustQuantity : Quantity {
	MixerTrack *srcTrack = NULL;
	float gainInDB = 0.0f;
	
	GainAdjustQuantity(MixerTrack *_srcTrack) {
		srcTrack = _srcTrack;
	}
	void setValue(float value) override {
		gainInDB = math::clamp(value, getMinValue(), getMaxValue());
		srcTrack->gainAdjust = std::pow(10.0f, gainInDB / 20.0f);
	}
	float getValue() override {
		gainInDB = 20.0f * std::log10(srcTrack->gainAdjust);
		return gainInDB;
	}
	float getMinValue() override {return -20.0f;}
	float getMaxValue() override {return 20.0f;}
	float getDefaultValue() override {return 0.0f;}
	float getDisplayValue() override {return getValue();}
	void setDisplayValue(float displayValue) override {setValue(displayValue);}
	std::string getLabel() override {return "Gain adjust";}
	std::string getUnit() override {return " dB";}
};

struct GainAdjustSlider : ui::Slider {
	GainAdjustSlider(MixerTrack *srcTrack) {
		quantity = new GainAdjustQuantity(srcTrack);
	}
	~GainAdjustSlider() {
		delete quantity;
	}
};


struct TrackDisplay : LedDisplayTextField {
	MixerTrack *srcTrack = NULL;

	TrackDisplay() {
		box.size = Vec(38, 16);
		textOffset = Vec(2.6f, -2.2f);
	};
	void draw(const DrawArgs &args) override {// override and do not call LedDisplayTextField.draw() since draw manually here
		if (cursor > 4) {
			text.resize(4);
			cursor = 4;
			selection = 4;
		}
		
		// the code below is from LedDisplayTextField.draw()
		nvgScissor(args.vg, RECT_ARGS(args.clipBox));
		if (font->handle >= 0) {
			bndSetFont(font->handle);

			NVGcolor highlightColor = color;
			highlightColor.a = 0.5;
			int begin = std::min(cursor, selection);
			int end = (this == APP->event->selectedWidget) ? std::max(cursor, selection) : -1;
			bndIconLabelCaret(args.vg, textOffset.x, textOffset.y,
				box.size.x - 2*textOffset.x, box.size.y - 2*textOffset.y,
				-1, color, 12, text.c_str(), highlightColor, begin, end);

			bndSetFont(APP->window->uiFont->handle);
		}
		nvgResetScissor(args.vg);
	}
	void onButton(const event::Button &e) override {
		if (e.button == GLFW_MOUSE_BUTTON_RIGHT && e.action == GLFW_PRESS) {
			ui::Menu *menu = createMenu();

			MenuLabel *lowcutLabel = new MenuLabel();
			lowcutLabel->text = "Track settings: ";
			menu->addChild(lowcutLabel);
			
			GainAdjustSlider *trackGainAdjustSlider = new GainAdjustSlider(srcTrack);
			trackGainAdjustSlider->box.size.x = 200.0f;
			menu->addChild(trackGainAdjustSlider);
			
			e.consume(this);
			return;
		}
		LedDisplayTextField::onButton(e);
	}
	void onChange(const event::Change &e) override {
		(*((uint32_t*)(srcTrack->trackName))) = 0x20202020;
		for (unsigned i = 0; i < text.length(); i++) {
			srcTrack->trackName[i] = text[i];
		}
		LedDisplayTextField::onChange(e);
	};
};



// Module's context menu
// --------------------

struct PanLawMonoItem : MenuItem {
	GlobalInfo *gInfo;
	
	struct PanLawMonoSubItem : MenuItem {
		GlobalInfo *gInfo;
		int setVal = 0;
		void onAction(const event::Action &e) override {
			gInfo->panLawMono = setVal;
		}
	};
	
	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		PanLawMonoSubItem *law0Item = createMenuItem<PanLawMonoSubItem>("+0 dB (no compensation)", CHECKMARK(gInfo->panLawMono == 0));
		law0Item->gInfo = gInfo;
		menu->addChild(law0Item);

		PanLawMonoSubItem *law1Item = createMenuItem<PanLawMonoSubItem>("+3 dB boost (equal power, default)", CHECKMARK(gInfo->panLawMono == 1));
		law1Item->gInfo = gInfo;
		law1Item->setVal = 1;
		menu->addChild(law1Item);

		PanLawMonoSubItem *law2Item = createMenuItem<PanLawMonoSubItem>("+4.5 dB boost (compromise)", CHECKMARK(gInfo->panLawMono == 2));
		law2Item->gInfo = gInfo;
		law2Item->setVal = 2;
		menu->addChild(law2Item);

		PanLawMonoSubItem *law3Item = createMenuItem<PanLawMonoSubItem>("+6 dB boost (linear)", CHECKMARK(gInfo->panLawMono == 3));
		law3Item->gInfo = gInfo;
		law3Item->setVal = 3;
		menu->addChild(law3Item);

		return menu;
	}
};

struct PanLawStereoItem : MenuItem {
	GlobalInfo *gInfo;

	struct PanLawStereoSubItem : MenuItem {
		GlobalInfo *gInfo;
		int setVal = 0;
		void onAction(const event::Action &e) override {
			gInfo->panLawStereo = setVal;
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		PanLawStereoSubItem *law0Item = createMenuItem<PanLawStereoSubItem>("Stereo balance (default)", CHECKMARK(gInfo->panLawStereo == 0));
		law0Item->gInfo = gInfo;
		menu->addChild(law0Item);

		PanLawStereoSubItem *law1Item = createMenuItem<PanLawStereoSubItem>("True panning", CHECKMARK(gInfo->panLawStereo == 1));
		law1Item->gInfo = gInfo;
		law1Item->setVal = 1;
		menu->addChild(law1Item);

		return menu;
	}
};


#endif
