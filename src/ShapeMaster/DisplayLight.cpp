//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//Based on code from the Fundamental plugin by Andrew Belt
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "Display.hpp"
// #include "Menus.hpp"
#include "time.h"


// ShapeMasterDisplayLight
// --------------------

void ShapeMasterDisplayLight::drawGrid(const DrawArgs &args) {
	nvgStrokeWidth(args.vg, 0.7f);
	nvgMiterLimit(args.vg, 0.7f);
	
	nvgBeginPath(args.vg);
	nvgStrokeColor(args.vg, DARKER_GRAY);
	
	// horizontal lines (range lines)
	int rangeIndex = currChan ? channels[*currChan].getRangeIndex() : 0;
	int numHLines;
	int numHLinesWithOct = calcNumHLinesAndWithOct(rangeIndex, &numHLines);
	float yDelta = canvas.y / numHLinesWithOct;
	loopHLines(args, margins.y, yDelta, numHLinesWithOct);

	// vertical lines (gridX lines)
	int numVLines = currChan ? channels[*currChan].getGridX() : 16;
	float xDelta = canvas.x / numVLines;
	numGridXmajorX = 0;// used in loopVLines() and also in major gridX lines further below
	loopVLines(args, margins.x, xDelta, numVLines);
	
	nvgStroke(args.vg);		

	// major divisions if applicable and bounding box 
	nvgBeginPath(args.vg);
	nvgStrokeColor(args.vg, DARK_GRAY);	

	nvgRect(args.vg, margins.x, margins.y, canvas.x, canvas.y);
	
	// horizontal major lines (range lines)
	if (numHLines != numHLinesWithOct) {
		yDelta = canvas.y / numHLines;
		loopHLines(args, margins.y, yDelta, numHLines);
	}
	// center horizonal line when not part of loop above
	if (rangeIndex == 5 || rangeIndex == 6) {
		float yPos = margins.y + canvas.y / 2.0f;
		nvgMoveTo(args.vg, margins.x, yPos);
		nvgLineTo(args.vg, margins.x + canvas.x, yPos);
	}
		
	// vertical major lines (gridX lines)
	for (int i = 0; i < numGridXmajorX; i++) {
		nvgMoveTo(args.vg, gridXmajorX[i], margins.y);
		nvgLineTo(args.vg, gridXmajorX[i], margins.y + canvas.y);
	}
	nvgStroke(args.vg);
}


void ShapeMasterDisplayLight::drawScopeWaveform(const DrawArgs &args, bool isFront) {
	float* drawBufMin = isFront ? scopeBuffers->scpFrontYmin : scopeBuffers->scpBackYmin;
	float* drawBufMax = isFront ? scopeBuffers->scpFrontYmax : scopeBuffers->scpBackYmax;
	
	nvgBeginPath(args.vg);
	float dsx = 1.0f / ((float)ScopeBuffers::SCOPE_PTS);// in normalized space
	float sx = 0.0f;
	float sy = 0.5f;
	if (scopeBuffers->isDrawPoint(0)) {
		sy = clamp(0.5f - drawBufMin[0] * 0.05f, 0.0f, 1.0f);
	}
	sy = margins.y + sy * canvas.y;
	nvgMoveTo(args.vg, margins.x + sx * canvas.x, sy);
	bool yOnMin = true;
	for (int i = 0; i <= ScopeBuffers::SCOPE_PTS; i++) {
		if (scopeBuffers->isDrawPoint(i)) {
			sy = yOnMin ? drawBufMax[i] : drawBufMin[i];
			sy = margins.y + clamp(0.5f - sy * 0.05f, 0.0f, 1.0f) * canvas.y;
			nvgLineTo(args.vg, margins.x + sx * canvas.x, sy);
			if (drawBufMin[i] != drawBufMax[i]) {
				float sy2 = yOnMin ? drawBufMin[i] : drawBufMax[i];
				sy2 = margins.y + clamp(0.5f - sy2 * 0.05f, 0.0f, 1.0f) * canvas.y;
				nvgLineTo(args.vg, margins.x + sx * canvas.x, sy2);
			}
			else {
				yOnMin = !yOnMin;
			}
		}
		else {
			sy = margins.y + 0.5f * canvas.y;
			nvgLineTo(args.vg, margins.x + sx * canvas.x, sy);
		}
		sx += dsx;
	}
	nvgStroke(args.vg);		
}


void ShapeMasterDisplayLight::drawScope(const DrawArgs &args) {
	if (scopeBuffers->scopeOn) {
		nvgStrokeWidth(args.vg, 1.0f);
		nvgMiterLimit(args.vg, 1.0f);
		if (scopeBuffers->scopeVca) {			
			// VCA PRE - draw scpBackY
			nvgStrokeColor(args.vg, DARK_GRAY);
			drawScopeWaveform(args, false);
			
			// VCA POST - draw scpFrontY
			nvgStrokeColor(args.vg, MID_DARKER_GRAY);
			drawScopeWaveform(args, true);
		}
		else {
			// SC AUDIO - draw scpBackY
			nvgStrokeColor(args.vg, DARK_GRAY);
			drawScopeWaveform(args, false);
			
			// SC ENV - draw scpFrontY
			nvgStrokeColor(args.vg, MID_DARKER_GRAY);
			drawScopeWaveform(args, true);

			// SC TRIG LEVEL
			nvgStrokeColor(args.vg, MID_GRAY);
			nvgStrokeWidth(args.vg, 0.7f);
			nvgBeginPath(args.vg);
			float sy = clamp(0.5f - channels[*currChan].getTrigLevel() * 0.05f, 0.0f, 1.0f);
			sy = margins.y + sy * canvas.y;
			nvgMoveTo(args.vg, margins.x, sy);
			nvgLineTo(args.vg, margins.x + canvas.x, sy);
			nvgStroke(args.vg);	
		}
	}
}


void ShapeMasterDisplayLight::drawShapeWhenModuleIsVoid(const DrawArgs &args) {
	NVGcolor chanColor = CHAN_COLORS[0];

	nvgFillColor(args.vg, chanColor);
	nvgStrokeColor(args.vg, chanColor);
	nvgStrokeWidth(args.vg, 1.0f);
	nvgMiterLimit(args.vg, 1.0f);

	// shadow
	NVGcolor shadowColBright = chanColor;
	NVGcolor shadowColDark = chanColor;
	shadowColBright.a = 0.25f;
	shadowColDark.a = 0.03f;
	NVGpaint grad = nvgLinearGradient(args.vg, 0.0f, canvas.y / 2.3f + margins.y, 0.0f, canvas.y + margins.y, shadowColBright, shadowColDark);
	float homeY = margins.y + canvas.y;
	nvgFillColor(args.vg, shadowColBright);
	nvgBeginPath(args.vg);
	nvgMoveTo(args.vg, margins.x, homeY);
	nvgLineTo(args.vg, margins.x + 0.5f * canvas.x, margins.y + 0.0f * canvas.y);
	nvgLineTo(args.vg, margins.x + 1.0f * canvas.x, homeY);
	nvgClosePath(args.vg);
	nvgFillPaint(args.vg, grad);
	nvgFill(args.vg);	
	
	// lines
	nvgStrokeColor(args.vg, chanColor);
	nvgFillColor(args.vg, chanColor);
	nvgBeginPath(args.vg);
	nvgMoveTo(args.vg, margins.x, homeY);
	nvgLineTo(args.vg, margins.x + 0.5f * canvas.x, margins.y + 0.0f * canvas.y);
	nvgLineTo(args.vg, margins.x + 1.0f * canvas.x, homeY);
	nvgStroke(args.vg);	
	
	// control points (empty)
	nvgFillColor(args.vg, DARKER_GRAY);
	nvgBeginPath(args.vg);		
	float ctrlSizeUnselected = 1.6f * 1.0f;
	nvgCircle(args.vg, margins.x + 0.25f * canvas.x, margins.y + 0.5f * canvas.y, ctrlSizeUnselected);
	nvgCircle(args.vg, margins.x + 0.75f * canvas.x, margins.y + 0.5f * canvas.y, ctrlSizeUnselected);
	nvgFill(args.vg);
	nvgStroke(args.vg);

	// normal points (full)
	nvgFillColor(args.vg, chanColor);
	nvgBeginPath(args.vg);
	float ptSizeUnselected = 2.1f * 1.0f;
	nvgCircle(args.vg, margins.x + 0.0f * canvas.x, homeY, ptSizeUnselected);
	nvgCircle(args.vg, margins.x + 0.5f * canvas.x, margins.y + 0.0f * canvas.y, ptSizeUnselected);
	nvgCircle(args.vg, margins.x + 1.0f * canvas.x, homeY, ptSizeUnselected);
	nvgFill(args.vg);	
}

void ShapeMasterDisplayLight::drawShape(const DrawArgs &args) {
	Shape* shape = channels[*currChan].getShape();
	
	int ptSelect = *dragPtSelect != MAX_PTS ? *dragPtSelect : *hoverPtSelect;
	
	NVGcolor chanColor = CHAN_COLORS[channels[*currChan].channelSettings.cc4[1]];

	nvgFillColor(args.vg, chanColor);
	nvgStrokeColor(args.vg, chanColor);
	nvgStrokeWidth(args.vg, *lineWidthSrc);
	nvgMiterLimit(args.vg, *lineWidthSrc);

	int numPts = shape->getNumPts();
	
	// shadow
	NVGcolor shadowColBright = chanColor;
	NVGcolor shadowColDark = chanColor;
	shadowColBright.a = 0.25f;
	shadowColDark.a = 0.03f;
	NVGpaint grad;
	float homeY;
	if (setting2Src->cc4[0] < 2 ? setting2Src->cc4[0] == 0 : !channels[*currChan].getLocalInvertShadow()) {
		// normal
		grad = nvgLinearGradient(args.vg, 0.0f, canvas.y / 2.3f + margins.y, 0.0f, canvas.y + margins.y, shadowColBright, shadowColDark);
		homeY = margins.y + canvas.y + 0.5f;
	}
	else {
		// inverted
		grad = nvgLinearGradient(args.vg, 0.0f, margins.y, 0.0f, (canvas.y - canvas.y / 2.3f) + margins.y, shadowColDark, shadowColBright);	
		homeY = margins.y - 0.5f;			
	}
	nvgFillColor(args.vg, shadowColBright);
	int epc = 0;
	float dsx = 1.0f / ((float)SHAPE_PTS);// in normalized space
	float sx = 0.0f;
	nvgBeginPath(args.vg);
	nvgMoveTo(args.vg, margins.x, homeY);
	for (int i = 0; i < SHAPE_PTS; i++) {
		shaY[i] = channels[*currChan].evalShapeForShadow(sx, &epc);
		float sy = 1.0f - shaY[i];
		sy = margins.y + sy * canvas.y;
		nvgLineTo(args.vg, margins.x + sx * canvas.x, sy);
		sx += dsx;
	}
	shaY[SHAPE_PTS] = channels[*currChan].evalShapeForShadow(1.0f, &epc);// [SHAPE_PTS] not an error since the array was declared with room for last point
	float sy = 1.0f - shaY[SHAPE_PTS];
	sy = margins.y + sy * canvas.y;
	nvgLineTo(args.vg, margins.x + canvas.x, sy);
	nvgLineTo(args.vg, margins.x + canvas.x, homeY);
	nvgClosePath(args.vg);
	nvgFillPaint(args.vg, grad);
	nvgFill(args.vg);	


	// lines
	nvgStrokeColor(args.vg, chanColor);
	nvgFillColor(args.vg, chanColor);
	nvgBeginPath(args.vg);
	nvgMoveTo(args.vg, margins.x, shape->getPointYFlip(0) * canvas.y + margins.y);
	for (int pt = 0; pt < (numPts - 1); pt++) {
		Vec nextPoint = (shape->getPointVectFlipY(pt + 1).mult(canvas)).plus(margins);
		if (shape->isLinear(pt)) {
			nvgLineTo(args.vg, nextPoint.x, nextPoint.y);
		}
		else {
			float stepX = 0.003f;// in normalized space
			if (shape->getCtrl(pt) > 0.9f || shape->getCtrl(pt) < 0.1f) {
				stepX /= 2.0f;
			}
			float dx = shape->getPointX(pt + 1) - shape->getPointX(pt);// in normalized space
			for (float _x = stepX; _x < dx; _x += stepX) {// _x normalized and relative to point pt
				Vec point1 = (shape->getPointVectFlipY(pt, _x).mult(canvas)).plus(margins);
				nvgLineTo(args.vg, point1.x, point1.y);
			}					
			nvgLineTo(args.vg, nextPoint.x, nextPoint.y);// just in case over/under-shoot in loop above
		}
	}
	nvgStroke(args.vg);
	
	// control points (empty)
	nvgFillColor(args.vg, DARKER_GRAY);
	nvgBeginPath(args.vg);		
	float ctrlSizeSelected = 2.7f * *lineWidthSrc;
	int ctrlPt = ptSelect < 0 ? -ptSelect - 1 : -1;// will be -1 if normal point or none is currently selected
	if (settingSrc->cc4[3] != 0) {// if showing points
		float ctrlSizeUnselected = 1.6f * *lineWidthSrc;
		for (int pt = 0; pt < (numPts - 1); pt++) {
			if (shape->isCtrlVisible(pt) && pt != ctrlPt) {// hide control points for horiz/vertical segments
				Vec point = (shape->getCtrlVectFlipY(pt).mult(canvas)).plus(margins);
				nvgCircle(args.vg, point.x, point.y, ctrlSizeUnselected);
			}
		}
	}
	if (ctrlPt != -1) {
		Vec point = (shape->getCtrlVectFlipY(ctrlPt).mult(canvas)).plus(margins);
		nvgCircle(args.vg, point.x, point.y, ctrlSizeSelected);
	}
	nvgFill(args.vg);
	nvgStroke(args.vg);

	// normal points (full)
	nvgFillColor(args.vg, chanColor);
	nvgBeginPath(args.vg);
	float ptSizeSelected = 3.2f * *lineWidthSrc;
	if (settingSrc->cc4[3] != 0) {// if showing points
		float ptSizeUnselected = 2.1f * *lineWidthSrc;
		for (int pt = 0; pt < numPts; pt++) {
			Vec point = (shape->getPointVectFlipY(pt).mult(canvas)).plus(margins);
			nvgCircle(args.vg, point.x, point.y, ptSizeUnselected);
		}
	}
	if (ptSelect >= 0 && ptSelect != MAX_PTS) {
		Vec point = (shape->getPointVectFlipY(ptSelect).mult(canvas)).plus(margins);
		nvgCircle(args.vg, point.x, point.y,  ptSizeSelected);
	}
	nvgFill(args.vg);


	// loop/sustain cursors
	nvgStrokeWidth(args.vg, 1.0f);
	if (channels[*currChan].isSustain() || channels[*currChan].isLoopWithModeGuard()) {
		const NVGcolor LOOPCV_CURSOR_COLOR = nvgRGB(85, 85, 85);
		float loopRightXt = channels[*currChan].getLoopEndAndSustain();
		float loopRight = margins.x + loopRightXt * canvas.x;
		float loopLeftXt = channels[*currChan].getLoopStart();
		float loopLeft = margins.x + loopLeftXt * canvas.x;

		// loop/sustain Cvs first
		float loopRightXtWithCv = channels[*currChan].getLoopEndAndSustainWithCv();
		if (loopRightXtWithCv != loopRightXt) {
			float loopRightWithCv = margins.x + loopRightXtWithCv * canvas.x;
			nvgBeginPath(args.vg);
			nvgStrokeColor(args.vg, LOOPCV_CURSOR_COLOR);
			nvgFillColor(args.vg, LOOPCV_CURSOR_COLOR);
			nvgMoveTo(args.vg, loopRightWithCv, margins.y);
			nvgLineTo(args.vg, loopRightWithCv, margins.y + canvas.y);
			nvgRect(args.vg, loopRightWithCv, margins.y + canvas.y * 0.49f, canvas.y * 0.02f, canvas.y * 0.02f);
			nvgStroke(args.vg);
			nvgFill(args.vg);
			// Loop start with CV
			if (channels[*currChan].isLoopWithModeGuard()){
				float loopLeftXtWithCv = channels[*currChan].getLoopStartWithCv();
				if (loopLeftXtWithCv != loopLeftXt) {
					float loopLeftWithCv = margins.x + loopLeftXtWithCv * canvas.x;
					nvgBeginPath(args.vg);
					nvgMoveTo(args.vg, loopLeftWithCv, margins.y);
					nvgLineTo(args.vg, loopLeftWithCv, margins.y + canvas.y);
					nvgRect(args.vg, loopLeftWithCv, margins.y + canvas.y * 0.49f, canvas.y * -0.02f, canvas.y * 0.02f);
					nvgStroke(args.vg);
					nvgFill(args.vg);					
				}
			}
		}

		// normal loop/sustain cursors
		nvgBeginPath(args.vg);
		nvgStrokeColor(args.vg, MID_GRAY);
		nvgFillColor(args.vg, MID_GRAY);
		nvgMoveTo(args.vg, loopRight, margins.y);
		nvgLineTo(args.vg, loopRight, margins.y + canvas.y);
		nvgStroke(args.vg);
		nvgBeginPath(args.vg);
		nvgRect(args.vg, loopRight, margins.y + canvas.y * 0.49f, canvas.y * 0.02f, canvas.y * 0.02f);
		nvgFill(args.vg);
		nvgStroke(args.vg);
		// LoopStart
		if (channels[*currChan].isLoopWithModeGuard()) {
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, loopLeft, margins.y);
			nvgLineTo(args.vg, loopLeft, margins.y + canvas.y);
			nvgStroke(args.vg);
			nvgBeginPath(args.vg);
			nvgRect(args.vg, loopLeft, margins.y + canvas.y * 0.49f, canvas.y * -0.02f, canvas.y * 0.02f);
			nvgFill(args.vg);
			nvgStroke(args.vg);
			// H line between cursors, at level of right cursor's CV
			// nvgStrokeWidth(args.vg, 0.5f);
			// epc = 0;
			// float loopRightCv = channels[*currChan].evalShapeForShadow(loopRightXt, &epc);
			// loopRightCv = margins.y + (1.0f - loopRightCv) * canvas.y;
			// nvgBeginPath(args.vg);
			// nvgMoveTo(args.vg, loopLeft, loopRightCv);
			// nvgLineTo(args.vg, loopRight, loopRightCv);
			// nvgStroke(args.vg);
		}		
	}

	// play head
	nvgStrokeWidth(args.vg, 1.0f);
	float playHead = channels[*currChan].getPlayHeadPosition();
	epc = 0;
	if (playHead != -1.0f) {
		// vertical line
		nvgBeginPath(args.vg);
		nvgStrokeColor(args.vg, SCHEME_WHITE);
		nvgMoveTo(args.vg, margins.x + playHead * canvas.x, margins.y);
		nvgLineTo(args.vg, margins.x + playHead * canvas.x, margins.y + canvas.y);
		nvgStroke(args.vg);
		// circle
		float cvs = channels[*currChan].evalShapeForShadow(playHead, &epc);
		cvs = margins.y + (1.0f - cvs) * canvas.y;
		nvgBeginPath(args.vg);
		nvgFillColor(args.vg, SCHEME_WHITE);
		nvgCircle(args.vg, margins.x + playHead * canvas.x, cvs, 3.0f);
		nvgFill(args.vg);
	}
	
	// tooltip on hovered normal point
	if (setting2Src->cc4[3] != 0 && ptSelect >= 0 && ptSelect != MAX_PTS && font->handle >= 0) {
		Vec ptVec = shape->getPointVect(ptSelect);
		
		// horizontal label
		float length = 0;
		std::string timeText = "";
		if (channels[*currChan].getTrigMode() == TM_CV) {
			length = ptVec.x * 10.0f;
			if (channels[*currChan].getBipolCvMode() != 0) {
				length -= 5.0f;
			}
			timeText = string::f("%.3gV", length);
		}
		else {
			#ifdef SM_PRO
			if (channels[*currChan].isSync()) {
				length = channels[*currChan].calcLengthSyncTime();
			}
			else 
			#endif
			{
				length = channels[*currChan].calcLengthUnsyncTime();
			}
			float time = length * ptVec.x;	
			timeText = timeToString(time, true).append("s");
		}
		
		// vertical label
		std::string voltRangedText;
		float voltRanged = channels[*currChan].applyRange(ptVec.y);
		int8_t tooltipVoltMode = channels[*currChan].getShowTooltipVoltsAs();
		if (tooltipVoltMode == 1) {// show volts
			voltRangedText = string::f("%.3gV", voltRanged);
		}
		else if (tooltipVoltMode == 0) {// show volts as voct freq
			float freq = dsp::FREQ_C4 * std::pow(2.0f, voltRanged);
			if (freq >= 1000.0f) {
				voltRangedText = string::f("%.3gkHz", freq / 1000.0f);
			}
			else if (freq >= 1.0f) {
				voltRangedText = string::f("%.4gHz", freq);
			}
			else if (freq >= 0.001f) {
				voltRangedText = string::f("%.3gmHz", freq * 1000.0f);
			}
			else {
				voltRangedText = string::f("%.2gmHz", freq * 1000.0f);
			}
		}
		else {// show volts as note
			char note[8];
			printNote(voltRanged, note, true);
			voltRangedText = note;
		}

	
		// auto position
		int textAlign = NVG_ALIGN_MIDDLE | (ptVec.x <= 0.5f ? NVG_ALIGN_LEFT : NVG_ALIGN_RIGHT);
		float toolOffsetX = margins.x;
		float toolOffsetVoltsY = margins.y * 2.3f;
		float toolOffsetTimeY = toolOffsetVoltsY;
		float toolOffsetNodeY = toolOffsetVoltsY;
		bool tooltipAboveNode = ptVec.y < 0.5f;
		if (tooltipAboveNode) {
			toolOffsetTimeY *= -1.0f;
			toolOffsetVoltsY *= -1.78f;
		}
		else {
			toolOffsetTimeY *= 1.78f;
		}
		if (ptVec.x > 0.5f) {
			toolOffsetX *= -1.0f;
		}
		
		bool showNodeNumber = channels[*currChan].getNodeTriggers() != 0;
		if (showNodeNumber && ptSelect > 0) {
			if (tooltipAboveNode) {
				toolOffsetNodeY *= (-1.78f - 0.78f);
			}
			else {
				toolOffsetTimeY += toolOffsetVoltsY * 0.78f;
				toolOffsetVoltsY *= 1.78f;
			}
		}
		
		ptVec.y = 1.0f - ptVec.y;
		ptVec = ptVec.mult(canvas).plus(margins);

		nvgFillColor(args.vg, nvgRGB(0xFF, 0xFF, 0xFF));
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, 0.0);
		nvgFontSize(args.vg, 9.0f);
		nvgTextAlign(args.vg, textAlign);
		nvgText(args.vg, ptVec.x + toolOffsetX, ptVec.y + toolOffsetVoltsY, voltRangedText.c_str(), NULL);
		nvgText(args.vg, ptVec.x + toolOffsetX, ptVec.y + toolOffsetTimeY, timeText.c_str(), NULL);
		
		if (showNodeNumber && ptSelect > 0) {
			nvgText(args.vg, ptVec.x + toolOffsetX, ptVec.y + toolOffsetNodeY, string::f("Node %d", ptSelect).c_str(), NULL);
		}
	}
}



void ShapeMasterDisplayLight::drawMessages(const DrawArgs &args) {
	std::string text = "";
	if (setting3Src->cc4[2] != 0) {// if cloaked
		text = "Cloaked";
	}
	else if (currChan && !channels[*currChan].getChannelActive()) {
		text = "Inactive channel (connect output)";
	}
	else if (time(0) < displayInfo->displayMessageTimeOff) {
		text = displayInfo->displayMessage;
	}
	else if (currChan && setting2Src->cc4[2] != 0) {
		text = channels[*currChan].getChanName();
	}
	if (font->handle >= 0 && !text.empty()) {
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, 0.0);
		nvgFontSize(args.vg, 12.0f);
		
		float textXc = margins.x + 0.5f * canvas.x;
		float textYc = margins.y + 0.95f * canvas.y;
		float bounds[4];// [xmin,ymin, xmax,ymax]
		nvgTextBounds(args.vg, textXc, textYc, text.c_str(), NULL, bounds);
		// DEBUG("%g, %g, %g, %g", bounds[0], bounds[1], bounds[2], bounds[3]);
		
		nvgBeginPath(args.vg);
		nvgFillColor(args.vg, nvgRGBA(39, 39, 39, 175));// alpha, low = transparent, high = opaque
		nvgRect(args.vg, bounds[0] - 1.0f, bounds[1] - 1.0f, bounds[2] - bounds[0] + 2.0f, bounds[3] - bounds[1] + 2.0f);// xmin, ymin, w, h
		nvgFill(args.vg);
		
		nvgFillColor(args.vg, SCHEME_LIGHT_GRAY);
		nvgText(args.vg, textXc, textYc, text.c_str(), NULL);
	}
}
