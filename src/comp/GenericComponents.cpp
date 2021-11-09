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

void drawRectHalo(const Widget::DrawArgs &args, Vec boxSize, NVGcolor haloColor, float posX) {
	// some of the code in this block is adapted from LightWidget::drawHalo() and the composite call is from LightWidget::drawLayer()
	
	nvgGlobalCompositeBlendFunc(args.vg, NVG_ONE_MINUS_DST_COLOR, NVG_ONE);

	const float brightness = 0.8f;
	NVGcolor color = nvgRGBAf(0, 0, 0, 0);
	NVGcolor hc = haloColor;
	hc.a *= math::clamp(brightness, 0.f, 1.f);
	color = color::screen(color, hc);
	color = color::clamp(color);
	
	nvgBeginPath(args.vg);
	nvgRect(args.vg, -12 + posX, -12, boxSize.x + 24, boxSize.y + 24);
	
	NVGcolor icol = color::mult(color, settings::haloBrightness);
	NVGcolor ocol = nvgRGBA(0, 0, 0, 0);
	NVGpaint paint = nvgBoxGradient(args.vg, -6 + posX, -6, boxSize.x + 12, boxSize.y + 12, 8, 12, icol, ocol);// tlx, tly, w, h, radius, feather, icol, ocol
	
	nvgFillPaint(args.vg, paint);
	nvgFill(args.vg);
	
	nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);
}


void drawRoundHalo(const Widget::DrawArgs &args, Vec boxSize, NVGcolor haloColor) {
	// some of the code in this block is adapted from LightWidget::drawHalo() and the composite call is from LightWidget::drawLayer()
	nvgGlobalCompositeBlendFunc(args.vg, NVG_ONE_MINUS_DST_COLOR, NVG_ONE);

	const float brightness = 0.8f;
	NVGcolor color = nvgRGBAf(0, 0, 0, 0);
	NVGcolor hc = haloColor;
	hc.a *= math::clamp(brightness, 0.f, 1.f);
	color = color::screen(color, hc);
	color = color::clamp(color);
	
	math::Vec c = boxSize.div(2);
	float radius = std::min(boxSize.x, boxSize.y) / 2.0;
	float oradius = radius + std::min(radius * 4.f, 15.f);

	nvgBeginPath(args.vg);
	nvgRect(args.vg, c.x - oradius, c.y - oradius, 2 * oradius, 2 * oradius);

	NVGcolor icol = color::mult(color, settings::haloBrightness);
	NVGcolor ocol = nvgRGBA(0, 0, 0, 0);
	NVGpaint paint = nvgRadialGradient(args.vg, c.x, c.y, radius, oradius, icol, ocol);
	nvgFillPaint(args.vg, paint);
	nvgFill(args.vg);
	
	nvgGlobalCompositeOperation(args.vg, NVG_SOURCE_OVER);
}



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

void MmKnobWithArc::drawLayer(const DrawArgs &args, int layer) {
	MmKnob::drawLayer(args, layer);
	
	if (layer == 1) {
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
}


// Lights

// nothing

