//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "GenericComponents.hpp"



// Screws

// nothing



// Ports

// nothing



// Buttons and switches

// none



// Knobs and sliders

void MmKnobWithArc::drawArc(const DrawArgs &args, float a0, float a1, NVGcolor* color) {
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

void MmKnobWithArc::draw(const DrawArgs &args) {
	MmKnob::draw(args);
	ParamQuantity* paramQuantity = getParamQuantity();
	if (paramQuantity) {
		float aParam = -10000.0f;
		float aBase = TOP_ANGLE;
		if (!topCentered) {
			if (rightWhenNottopCentered) {
				aBase -= minAngle;
			}
			else {
				aBase += minAngle;
			}
		}
		int8_t showMask = (*detailsShowSrc & ~*cloakedModeSrc & 0x3); // 0 = off, 0x1 = cv_only, 0x3 = cv+param
		// param
		float param = paramQuantity->getValue();
		if (showMask == 0x3) {
			aParam = TOP_ANGLE + math::rescale(param, paramQuantity->getMinValue(), paramQuantity->getMaxValue(), minAngle, maxAngle);
			drawArc(args, aBase, aParam, &arcColorDarker);
		}
		// cv
		if (paramWithCV && (*paramCvConnected) && showMask != 0) {
			if (aParam == -10000.0f) {
				aParam = TOP_ANGLE + math::rescale(param, paramQuantity->getMinValue(), paramQuantity->getMaxValue(), minAngle, maxAngle);
			}
			float aCv = TOP_ANGLE + math::rescale(*paramWithCV, paramQuantity->getMinValue(), paramQuantity->getMaxValue(), minAngle, maxAngle);
			drawArc(args, aParam, aCv, &arcColor);
		}
	}
}


// Lights

// nothing

