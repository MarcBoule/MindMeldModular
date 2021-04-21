//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//Based on code from the Fundamental plugin by Andrew Belt
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "Display.hpp"


// ScopeBuffers
// --------------------

void ScopeBuffers::populate(Channel* channel, int8_t scopeSettings) {
	int newState = channel->getState();
	int8_t newTrigMode = channel->getTrigMode();
	bool needClear = (lastChannel != channel) || (newState == PlayHead::STEPPING && lastState == PlayHead::STOPPED) || lastTrigMode != newTrigMode;
	if (needClear) {
		lastChannel = channel;
		lastTrigMode = newTrigMode;
		clear();
	}
	lastState = newState;// must be outside since newState can be different from lastState but not have triggered a clear
	
	float scpIf = channel->getScopePosition();
	if (scpIf != -1.0f && channel->getChannelActive() && (scopeSettings & SCOPE_MASK_ON) != 0) {
		scopeOn = true;
		scopeVca = ((scopeSettings & SCOPE_MASK_VCA_nSC) != 0);
		if (newState == PlayHead::STEPPING) {
			int scpI = (int)(scpIf * SCOPE_PTS + 0.5f);
			setPoint(scpI);
			float frontVal;
			float backVal;
			if (scopeVca) {
				int8_t polySelect = channel->channelSettings2.cc4[0];
				
				// VcaPost
				int postSize = channel->getVcaPostSize();
				if (polySelect < 16 && postSize > polySelect) {
					frontVal = channel->getVcaPost(polySelect);
				}
				else if (polySelect == 16 && postSize > 0) {
					frontVal = channel->getVcaPost(0);
					if (postSize > 1) {
						frontVal = (frontVal + channel->getVcaPost(1)) * 0.5f;
					}
				}
				else {
					frontVal = 0.0f;
				}
				
				// VcaPre
				int preSize = channel->getVcaPreSize();
				if (polySelect < 16 && preSize > polySelect) {
					backVal = channel->getVcaPre(polySelect);
				}
				else if (polySelect == 16 && preSize > 0) {
					backVal = channel->getVcaPre(0);
					if (preSize > 1) {
						backVal = (backVal + channel->getVcaPre(1)) * 0.5f;
					}
				}
				else {
					backVal = 0.0f;
				}
			}
			else {
				frontVal = channel->getScEnvelope();
				backVal = channel->getScSignal();			
			}
			if (lastScpI != scpI) {
				lastScpI = scpI;
				// new point, so set min and max to new val
				scpFrontYmin[scpI] = scpFrontYmax[scpI] = frontVal;
				scpBackYmin[scpI] = scpBackYmax[scpI] = backVal;
			}
			else {
				// same point, write val to min or max
				if (frontVal > scpFrontYmax[scpI]) {
					scpFrontYmax[scpI] = frontVal;
				}
				else if (frontVal < scpFrontYmin[scpI]) {
					scpFrontYmin[scpI] = frontVal;
				}	
				if (backVal > scpBackYmax[scpI]) {
					scpBackYmax[scpI] = backVal;
				}
				else if (backVal < scpBackYmin[scpI]) {
					scpBackYmin[scpI] = backVal;
				}	
			}
		}
	}
	else {
		scopeOn = false;
	}
}


// ShapeMasterDisplay
// --------------------

void ShapeMasterDisplay::drawGrid(const DrawArgs &args) {
	nvgStrokeWidth(args.vg, 0.7f);
	nvgMiterLimit(args.vg, 0.7f);
	
	nvgBeginPath(args.vg);
	nvgStrokeColor(args.vg, DARKER_GRAY);
	
	// horizontal lines (range lines)
	int rangeIndex = channels[*currChan].getRangeIndex();
	int numHLines;
	int numHLinesWithOct = calcNumHLinesAndWithOct(rangeIndex, &numHLines);
	float yDelta = canvas.y / numHLinesWithOct;
	loopHLines(args, margins.y, yDelta, numHLinesWithOct);

	// vertical lines (gridX lines)
	int numVLines = channels[*currChan].getGridX();
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


void ShapeMasterDisplay::drawScopeWaveform(const DrawArgs &args, bool isFront) {
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


void ShapeMasterDisplay::drawScope(const DrawArgs &args) {
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



void ShapeMasterDisplay::drawShape(const DrawArgs &args) {
	Shape* shape = channels[*currChan].getShape();
	
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
	// DEBUG("%g", shaY[SHAPE_PTS]);
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
	int ctrlPt = hoverPtSelect < 0 ? -hoverPtSelect - 1 : -1;// will be -1 if normal point or none is currently selected
	if (settingSrc->cc4[3] == 0) {// if not showing shape points, then show only hovered ctrl point
		if (ctrlPt != -1) {
			Vec point = (shape->getCtrlVectFlipY(ctrlPt).mult(canvas)).plus(margins);
			nvgCircle(args.vg, point.x, point.y, ctrlSizeSelected);
		}
	}
	else {
		float ctrlSizeUnselected = 1.6f * *lineWidthSrc;
		for (int pt = 0; pt < (numPts - 1); pt++) {
			if (shape->isCtrlVisible(pt)) {// hide control points for horiz/vertical segments
				Vec point = (shape->getCtrlVectFlipY(pt).mult(canvas)).plus(margins);
				nvgCircle(args.vg, point.x, point.y, (hoverPtSelect == (-pt - 1) ? ctrlSizeSelected : ctrlSizeUnselected));
			}
		}
	}
	nvgFill(args.vg);
	nvgStroke(args.vg);

	// normal points (full)
	nvgFillColor(args.vg, chanColor);
	nvgBeginPath(args.vg);
	float ptSizeSelected = 3.2f * *lineWidthSrc;
	if (settingSrc->cc4[3] == 0) {// if not showing shape points, then show only hovered point
		if (hoverPtSelect >= 0 && hoverPtSelect != MAX_PTS) {
			Vec point = (shape->getPointVectFlipY(hoverPtSelect).mult(canvas)).plus(margins);
			nvgCircle(args.vg, point.x, point.y,  ptSizeSelected);
		}
	}
	else {	
		float ptSizeUnselected = 2.1f * *lineWidthSrc;
		for (int pt = 0; pt < numPts; pt++) {
			Vec point = (shape->getPointVectFlipY(pt).mult(canvas)).plus(margins);
			nvgCircle(args.vg, point.x, point.y, (hoverPtSelect == pt ? ptSizeSelected : ptSizeUnselected));
		}
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
		nvgRect(args.vg, loopRight, margins.y + canvas.y * 0.49f, canvas.y * 0.02f, canvas.y * 0.02f);
		nvgStroke(args.vg);
		nvgFill(args.vg);
		// LoopStart
		if (channels[*currChan].isLoopWithModeGuard()) {
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, loopLeft, margins.y);
			nvgLineTo(args.vg, loopLeft, margins.y + canvas.y);
			nvgRect(args.vg, loopLeft, margins.y + canvas.y * 0.49f, canvas.y * -0.02f, canvas.y * 0.02f);
			nvgStroke(args.vg);
			nvgFill(args.vg);
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
		nvgFillColor(args.vg, SCHEME_WHITE);
		nvgMoveTo(args.vg, margins.x + playHead * canvas.x, margins.y);
		nvgLineTo(args.vg, margins.x + playHead * canvas.x, margins.y + canvas.y);
		nvgStroke(args.vg);
		// circle
		float cvs = channels[*currChan].evalShapeForShadow(playHead, &epc);
		cvs = margins.y + (1.0f - cvs) * canvas.y;
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, margins.x + playHead * canvas.x, cvs, 3.0f);
		nvgFill(args.vg);
	}
	
	// tooltip on hovered normal point
	if (setting2Src->cc4[3] != 0 && hoverPtSelect >= 0 && hoverPtSelect != MAX_PTS && font->handle >= 0) {
		Vec ptVec = shape->getPointVect(hoverPtSelect);
		
		float length;
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
		std::string timeText = timeToString(time, true).append("s");

		float voltRanged = channels[*currChan].applyRange(ptVec.y);
		std::string voltRangedText = string::f("%.3gV", voltRanged);
		
		// auto position
		int textAlign = NVG_ALIGN_MIDDLE | (ptVec.x <= 0.5f ? NVG_ALIGN_LEFT : NVG_ALIGN_RIGHT);
		float toolOffsetX = margins.x;
		float toolOffsetVoltsY = margins.y * 2.3f;
		float toolOffsetTimeY = toolOffsetVoltsY;
		if (ptVec.y < 0.5f) {
			toolOffsetTimeY *= -1.0f;
			toolOffsetVoltsY *= -1.78f;
		}
		else {
			toolOffsetTimeY *= 1.78f;
		}
		if (ptVec.x > 0.5f) {
			toolOffsetX *= -1.0f;
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
	}
}



void ShapeMasterDisplay::drawMessages(const DrawArgs &args) {
	std::string text = "";
	if (currChan && !channels[*currChan].getChannelActive()) {
		text = "Inactive channel (connect output)";
	}
	else if (time(0) < displayInfo->displayMessageTimeOff) {
		text = displayInfo->displayMessage;
	}
	else if (currChan && setting2Src->cc4[2] != 0) {
		text = channels[*currChan].getChanName();
	}
	if (font->handle >= 0 && text.compare("") != 0) {
		nvgFillColor(args.vg, SCHEME_LIGHT_GRAY);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, 0.0);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		nvgFontSize(args.vg, 12.0f);
		nvgText(args.vg, margins.x + 0.5f * canvas.x, margins.y + 0.95f * canvas.y, text.c_str(), NULL);
	}
}	



void ShapeMasterDisplay::onButton(const event::Button& e) {
	onButtonMouse = APP->scene->rack->mousePos;
	dragMouseOld = onButtonMouse;
	onButtonPos = e.pos;
	if (e.action == GLFW_PRESS) {
		Shape* shape = channels[*currChan].getShape();
		altSelect = 0;
		if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
			// check if point or control point selected
			if (hoverPtSelect != MAX_PTS) {
				if (hoverPtSelect >= 0) {// if normal point
					// no need to setPointWithSafety since drag move will be called 
				}
				else {// else ctrl point
					int cpt = -hoverPtSelect - 1;
					onButtonOrigCtrl = shape->getCtrl(cpt);
					if (shape->getType(cpt) == 0) {
						onButtonOrigCtrl = Shape::applyScalingToCtrl(onButtonOrigCtrl, 1.0f / 3.0f);
					}
				}
			}
			// check if loop/sustain cursor(s) selected
			else if (channels[*currChan].isSustain() || channels[*currChan].isLoopWithModeGuard()) {
				float loopXtR = channels[*currChan].getLoopEndAndSustain();
				float loopXR = loopXtR * canvas.x + margins.x;
				float loopXtL = channels[*currChan].getLoopStart();
				float loopXL = loopXtL * canvas.x + margins.x;
				bool isLoop = channels[*currChan].isLoopWithModeGuard();
				int epc = 0;
				loopSnapTargetCV = -1.0f;
				if ( (onButtonPos.x > loopXR - LOOP_GRAB_X) && (onButtonPos.x < loopXR + LOOP_GRAB_X) ) {
					altSelect = 1;// sustain/rightLoop selected
					if (isLoop) {
						loopSnapTargetCV = channels[*currChan].evalShapeForShadow(loopXtL, &epc);
					}
				}
				else if (isLoop && (onButtonPos.x > loopXL - LOOP_GRAB_X) && (onButtonPos.x < loopXL + LOOP_GRAB_X) ) {
					altSelect = 2;// leftLoop selected
					loopSnapTargetCV = channels[*currChan].evalShapeForShadow(loopXtR, &epc);
				}
			}
		}
		else if (e.button == GLFW_MOUSE_BUTTON_RIGHT) {
			if (hoverPtSelect < 0) {// if ctrl point
				ui::Menu *menu = createMenu();
				createCtrlMenu(menu, shape, -hoverPtSelect - 1);
			}
			else if (hoverPtSelect < MAX_PTS) {// normal point
				ui::Menu *menu = createMenu();
				createPointMenu(menu, &channels[*currChan], hoverPtSelect);
			}
			else {// background of display
				ui::Menu *menu = createMenu();
				createBackgroundMenu(menu, shape, normalizePixelPoint(onButtonPos));						
			}
		}
	}
	e.consume(this);
}


void ShapeMasterDisplay::onDoubleClick(const event::DoubleClick &e) {
	// happens after the the second click's GLFW_PRESS, but before its GLFW_RELEASE
	// this happens only for double click of GLFW_MOUSE_BUTTON_LEFT
	dragHistoryStep = NULL;
	dragHistoryMisc = NULL;

	Shape* shape = channels[*currChan].getShape();
	if (hoverPtSelect == MAX_PTS) {
		// double clicked empty (so create new node)
		int retPt = shape->insertPointWithSafetyAndBlock(normalizePixelPoint(onButtonPos), true);// with history
		if (retPt >= 0) {
			hoverPtSelect = retPt;
		}
	}
	else if (hoverPtSelect >= 0) {
		// double clicked a normal node (so delete node)
		shape->deletePointWithBlock(hoverPtSelect, true);// with history
		hoverPtSelect = MAX_PTS;
	}
	else {
		// double clicked on a ctrl node 
		shape->makeLinear(-hoverPtSelect - 1);// remove if ever don't want to reset when adding nodes close to existing ctrl points and we accidentally hit the control point
		hoverPtSelect = MAX_PTS;// this is needed after makeLinear() or else an instant hover brings the ctrl point back to where it was
	}
	e.consume(this);
}


float ShapeMasterDisplay::findXWithGivenCvI(int shaI, float givenCv) {
	// returns -1.0f if givenCv is not within [shaY[shaI] ; shaY[shaI + 1]], else returns the interpolated x
	if ( (givenCv <= shaY[shaI] && givenCv >= shaY[shaI + 1]) || (givenCv >= shaY[shaI] && givenCv <= shaY[shaI + 1]) ) {
		float dY = shaY[shaI + 1] - shaY[shaI];
		if (std::fabs(dY) < 1e-5) {
			return -1.0f;
		}
		float dX = 1.0f / (float)SHAPE_PTS;
		return (givenCv - shaY[shaI]) * dX / dY + shaI / (float)SHAPE_PTS;
	}
	return -1.0f;
}

float ShapeMasterDisplay::findXWithGivenCv(float startX, float givenCv) {
	static const int NUM_SEGMENTS = 3;
	const float MAX_DIST = (float)NUM_SEGMENTS / (float)SHAPE_PTS;
	
	// find within +-MAX_DIST of startX only, if not found, return original startX
	int startI = std::round(startX * (float)SHAPE_PTS); 
	int leftI = std::max(0, startI - NUM_SEGMENTS);
	int rightI = std::min(SHAPE_PTS - 1, (startI + 1) + NUM_SEGMENTS);
	float distXBest = 10.0f;
	float xBest = 0.0f;// invalid while distXBest == 10.0f;
	for (int i = leftI; i <= rightI; i++) {
		float interpX = findXWithGivenCvI(i, givenCv);
		if (interpX != -1.0f) {
			float newDist = std::fabs(interpX - startX);
			if (newDist <= MAX_DIST && newDist < distXBest) {
				distXBest = newDist;
				xBest = interpX;
			}
		}
	}
	if (distXBest == 10.0f) {
		return startX;
	}
	return xBest;
}


void ShapeMasterDisplay::onDragStart(const event::DragStart& e) {
	dragHistoryStep = NULL;
	dragHistoryMisc = NULL;
	
	if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
		Shape* shape = channels[*currChan].getShape();
		int mods = APP->window->getMods();
		if (hoverPtSelect != MAX_PTS) {
			// Push DragMiscChange history action (rest is done in onDragEnd())
			dragHistoryMisc = new DragMiscChange;
			dragHistoryMisc->shapeSrc = shape;
			if (hoverPtSelect >= 0) {// if normal point
				// hoverPtSelect will start moving
				//   shape->setPointWithSafety()
				dragHistoryMisc->dragType = DM_POINT;
				dragHistoryMisc->pt = hoverPtSelect;
				dragHistoryMisc->oldVec = shape->getPointVect(hoverPtSelect);
			}
			else {// else is a ctrl point
				int cpt = -hoverPtSelect - 1;
				// cpt will start moving
				//   shape->setCtrlWithSafety()
				dragHistoryMisc->dragType = DM_CTRL;
				dragHistoryMisc->pt = cpt;
				dragHistoryMisc->oldVec.x = shape->getCtrl(cpt);
			}
		}
		else if (altSelect != 0) {
			// clicked alternate object (loop points, etc.)
			// if (altSelect == 1) {
				// loopEndSustain will start moving
				//   channels[*currChan].setLoopEndAndSustain();
			// }
			// else {//if (altSelect == 2) {
				// loopStart will start moving
				//   channels[*currChan].setLoopStart();
			// }
			// Push DragMiscChange history action (rest is done in onDragEnd())
			dragHistoryMisc = new DragMiscChange;
			dragHistoryMisc->channelSrc = &(channels[*currChan]);
			dragHistoryMisc->dragType = DM_LOOP;
			dragHistoryMisc->oldVec.x = channels[*currChan].getLoopStart();
			dragHistoryMisc->oldVec.y = channels[*currChan].getLoopEndAndSustain();
		}
		else {
			// clicked display background
			if ((mods & GLFW_MOD_SHIFT) != 0) {// if step
				// step will start moving
				// Push ShapeCompleteChange history action (rest is done in onDragEnd())
				dragHistoryStep = new ShapeCompleteChange;
				dragHistoryStep->shapeSrc = shape;
				dragHistoryStep->oldShape = new Shape();
				shape->copyShapeTo(dragHistoryStep->oldShape);
			}
		}
	}
	e.consume(this);
}


void ShapeMasterDisplay::onDragMove(const event::DragMove& e) {
	if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
		Shape* shape = channels[*currChan].getShape();
		Vec dragMouse = APP->scene->rack->mousePos;
		int mods = APP->window->getMods();
		if (hoverPtSelect != MAX_PTS) {
			if (hoverPtSelect >= 0) {// if normal point
				Vec posToSet = onButtonPos.plus(dragMouse).minus(onButtonMouse);// posToSet is in pixel space
				int xQuant;
				int yQuant;
				calcQuants(&xQuant, &yQuant, mods);
				channels[*currChan].setPointWithSafety(hoverPtSelect, normalizePixelPoint(posToSet), xQuant, yQuant);
			}
			else {// else is a ctrl point
				int cpt = -hoverPtSelect - 1;
				float dy = shape->getPointY(cpt + 1) - shape->getPointY(cpt);
				if (std::fabs(dy) > 1e-5f) {
					float deltaY = (onButtonMouse.y - dragMouse.y) / canvas.y / dy;// deltaY is +1 when mouse moves up by dy and is -1 when mouse moves down by dy
					
					float newCtrl = onButtonOrigCtrl;
					if (shape->getType(cpt) == 0) {
						newCtrl += deltaY * 0.7f;
						newCtrl = Shape::applyScalingToCtrl(newCtrl, 3.0f);
					}
					else {
						newCtrl += deltaY * 2.0f;
					}
					
					shape->setCtrlWithSafety(cpt, newCtrl);
				}
			}
			dragMouseOld = dragMouse;
		}
		else if (altSelect != 0) {
			// clicked alternate object (loop points, etc.)
			float posToSetX = onButtonPos.x + dragMouse.x - onButtonMouse.x;// posToSetX is in pixel space
			posToSetX = clamp((posToSetX - margins.x) / canvas.x, 0.0f, 1.0f);
			bool wantSnapToOtherCursor = ((mods & RACK_MOD_CTRL) != 0) && loopSnapTargetCV != -1.0f;
			if (wantSnapToOtherCursor) {// && altSelect <= 2) { 
				// look from posToSetX and find nearest PosToSetX within reasonabe grab that has CV of loopSnapTargetCV
				posToSetX = findXWithGivenCv(posToSetX, loopSnapTargetCV);
			}
			if (altSelect == 1) {
				if (channels[*currChan].isSustain() && (mods & RACK_MOD_CTRL) != 0) {
					float gridX = (float)channels[*currChan].getGridX();
					posToSetX = std::round(posToSetX * gridX) / gridX;
				}
				channels[*currChan].setLoopEndAndSustain(posToSetX);
			}
			else {//if (altSelect == 2) {
				channels[*currChan].setLoopStart(posToSetX);
			}
		}
		else {
			// clicked display background
			if ((mods & GLFW_MOD_SHIFT) != 0) {// if step
				Vec posToSet = onButtonPos.plus(dragMouse).minus(onButtonMouse);// posToSet is in pixel space
				int xQuant;
				int yQuant;
				calcQuants(&xQuant, &yQuant, mods | GLFW_MOD_ALT);// force calc of xQuant since step is forcibly horizontally quantized	
				Vec normPos = normalizePixelPoint(posToSet);
				int gp = std::min(mouseStepP, shape->getNumPts() - 2);
				mouseStepP = shape->calcPointFromX<float>(normPos.x, gp);
				shape->makeStep(mouseStepP, normPos, xQuant, yQuant);
			}
		}
	}
	e.consume(this);
}// onDragMove
	
	
void ShapeMasterDisplay::onDragEnd(const event::DragEnd& e) {
	Shape* shape = channels[*currChan].getShape();
	if (dragHistoryStep != NULL) {
		dragHistoryStep->newShape = new Shape();
		shape->copyShapeTo(dragHistoryStep->newShape);
		dragHistoryStep->name = "add/move step";
		APP->history->push(dragHistoryStep);
		dragHistoryStep = NULL;
	}
	else if (dragHistoryMisc != NULL) {
		if (dragHistoryMisc->dragType == DM_POINT) {
			dragHistoryMisc->newVec = shape->getPointVect(dragHistoryMisc->pt);
			dragHistoryMisc->name = "move node";
		}
		else if (dragHistoryMisc->dragType == DM_CTRL) {
			dragHistoryMisc->newVec.x = shape->getCtrl(dragHistoryMisc->pt);
			dragHistoryMisc->name = "move control point";
		}
		else if (dragHistoryMisc->dragType == DM_LOOP) {
			dragHistoryMisc->newVec.x = channels[*currChan].getLoopStart();
			dragHistoryMisc->newVec.y = channels[*currChan].getLoopEndAndSustain();
			dragHistoryMisc->name = "move sustain/loop cursor";
		}
		APP->history->push(dragHistoryMisc);
		dragHistoryMisc = NULL;
	}
	e.consume(this);
}

	
bool ShapeMasterDisplay::hoverMatch(Vec normalizedPos, Shape* shape, int pt) {// return value is stop search (because found or will never be found)		
	Vec point = shape->getPointVect(pt);
	Vec delta = (normalizedPos.minus(point)).abs();
	if (delta.x < grabX && delta.y < grabY) {
		hoverPtSelect = pt;
		return true;
	}
	Vec ctrl = shape->getCtrlVect(pt);
	delta = (normalizedPos.minus(ctrl)).abs();
	if (delta.x < grabX && delta.y < grabY) {
		hoverPtSelect = -pt - 1;
		return true;
	}
	return false;
}
	
void ShapeMasterDisplay::onHover(const event::Hover& e) {
	// find closest node 
	Shape* shape = channels[*currChan].getShape();
	Vec normalizedPos = e.pos.minus(margins).div(canvas);
	normalizedPos.y = 1.0f - normalizedPos.y;
	
	hoverPtSelect = MAX_PTS;
	altSelect = 0;
	
	int mods = APP->window->getMods();
	if ((mods & GLFW_MOD_SHIFT) == 0) {// disallow hovering when making steps, much easier
		int numPts = shape->getNumPts();
				
		if (normalizedPos.x <= 0.0f) {
			hoverPtMouse = 0;
		}
		else if (normalizedPos.x >= 1.0f) {
			hoverPtMouse = numPts - 1;
		}
		else {
			int gp = std::min(hoverPtMouse, numPts - 2);
			hoverPtMouse = shape->calcPointFromX<float>(normalizedPos.x, gp);
		}
	
		hoverMatch(normalizedPos, shape, hoverPtMouse);
		if (hoverPtSelect == MAX_PTS && hoverPtMouse < (numPts - 1)) {
			if (!hoverMatch(normalizedPos, shape, hoverPtMouse + 1) && hoverPtMouse < (numPts - 2)) {
				hoverMatch(normalizedPos, shape, hoverPtMouse + 2);
			}	
		}
		if (hoverPtSelect == MAX_PTS && hoverPtMouse > 0) {
			if (!hoverMatch(normalizedPos, shape, hoverPtMouse - 1) && hoverPtMouse > 1) {
				hoverMatch(normalizedPos, shape, hoverPtMouse - 2);
			}	
		}	
	}
	e.consume(this);
}
