//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//Based on code from the Fundamental plugin by Andrew Belt
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "Display.hpp"
#include "Menus.hpp"


float ShapeMasterDisplay::findXWithGivenCvI(int shaI, float givenCv) {
	// returns -1.0f if givenCv is not within [shaY[shaI] ; shaY[shaI + 1]], else returns the interpolated x
	if ( (givenCv <= shaY[shaI] && givenCv >= shaY[shaI + 1]) || (givenCv >= shaY[shaI] && givenCv <= shaY[shaI + 1]) ) {
		float dY = shaY[shaI + 1] - shaY[shaI];
		if (std::fabs(dY) < 1e-5) {
			return -1.0f;
		}
		float dX = 1.0f / (float)ShapeMasterDisplayLight::SHAPE_PTS;
		return (givenCv - shaY[shaI]) * dX / dY + shaI / (float)ShapeMasterDisplayLight::SHAPE_PTS;
	}
	return -1.0f;
}


float ShapeMasterDisplay::findXWithGivenCv(float startX, float givenCv) {
	static const int NUM_SEGMENTS = 3;
	const float MAX_DIST = (float)NUM_SEGMENTS / (float)ShapeMasterDisplayLight::SHAPE_PTS;
	
	// find within +-MAX_DIST of startX only, if not found, return original startX
	int startI = std::round(startX * (float)ShapeMasterDisplayLight::SHAPE_PTS); 
	int leftI = std::max(0, startI - NUM_SEGMENTS);
	int rightI = std::min(ShapeMasterDisplayLight::SHAPE_PTS - 1, (startI + 1) + NUM_SEGMENTS);
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


void ShapeMasterDisplay::onButton(const event::Button& e) {
	OpaqueWidget::onButton(e);
	
	onButtonPos = e.pos;// used only for onDoubleClick() and onDragStart
	if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT) {
		Shape* shape = channels[*currChan].getShape();
		Vec normalizedPos = normalizePixelPoint(e.pos);
		
		int buttonPtSelect = matchPtExtra(normalizedPos, shape);
		
		if (buttonPtSelect < 0) {// if ctrl point
			ui::Menu *menu = createMenu();
			createCtrlMenu(menu, shape, -buttonPtSelect - 1);
		}
		else if (buttonPtSelect < MAX_PTS) {// normal point
			ui::Menu *menu = createMenu();
			createPointMenu(menu, &channels[*currChan], buttonPtSelect);
		}
		else {// background of display
			ui::Menu *menu = createMenu();
			createBackgroundMenu(menu, shape, normalizedPos);						
		}
		e.consume(this);
	}
}// onButton


void ShapeMasterDisplay::onDoubleClick(const event::DoubleClick &e) {
	// happens after the the second click's GLFW_PRESS, but before its GLFW_RELEASE
	// happens after the the second click's onDragStart, but before its onDragEnd
	// this happens only for double click of GLFW_MOUSE_BUTTON_LEFT
	if (dragHistoryStep != NULL) delete dragHistoryStep;
	else if (dragHistoryMisc != NULL) delete dragHistoryMisc;

	Shape* shape = channels[*currChan].getShape();
	if (dragPtSelect == MAX_PTS) {
		// double clicked empty (so create new node)
		int retPt = shape->insertPointWithSafetyAndBlock(normalizePixelPoint(onButtonPos), true);// with history
		if (retPt >= 0) {
			dragPtSelect = retPt;
		}
	}
	else if (dragPtSelect >= 0) {
		// double clicked a normal node (so delete node)
		shape->deletePointWithBlock(dragPtSelect, true);// with history
		dragPtSelect = MAX_PTS;
		altSelect = 0;
	}
	else {
		// double clicked on a ctrl node 
		shape->makeLinear(-dragPtSelect - 1);// remove if ever don't want to reset when adding nodes close to existing ctrl points and we accidentally hit the control point
		dragPtSelect = MAX_PTS;// this is needed after makeLinear() or else an instant hover brings the ctrl point back to where it was
		altSelect = 0;
	}
}



void ShapeMasterDisplay::onDragStart(const event::DragStart& e) {
	dragHistoryStep = NULL;
	dragHistoryMisc = NULL;
	
	Vec dragStartPos = APP->scene->rack->mousePos.minus(parent->box.pos).minus(box.pos);
	dragStartPosY = dragStartPos.y;// used only when dragging control points
	
	if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
		Shape* shape = channels[*currChan].getShape();
		int mods = APP->window->getMods();
		
		dragPtSelect = matchPtExtra(normalizePixelPoint(onButtonPos), shape);
		altSelect = 0;
				
		// check if point or control point selected
		if (dragPtSelect != MAX_PTS) {
			if (dragPtSelect < 0) {// if ctrl point
				int cpt = -dragPtSelect - 1;
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
		
		// history
		if (dragPtSelect != MAX_PTS) {
			// Push DragMiscChange history action (rest is done in onDragEnd())
			dragHistoryMisc = new DragMiscChange;
			dragHistoryMisc->shapeSrc = shape;
			if (dragPtSelect >= 0) {// if normal point
				// dragPtSelect will start moving
				//   shape->setPointWithSafety()
				dragHistoryMisc->dragType = DM_POINT;
				dragHistoryMisc->pt = dragPtSelect;
				dragHistoryMisc->oldVec = shape->getPointVect(dragPtSelect);
			}
			else {// else is a ctrl point
				int cpt = -dragPtSelect - 1;
				// cpt will start moving
				//   shape->setCtrlWithSafety()
				dragHistoryMisc->dragType = DM_CTRL;
				dragHistoryMisc->pt = cpt;
				dragHistoryMisc->oldVec.x = shape->getCtrl(cpt);
			}
		}
		else if (altSelect != 0) {
			// clicked alternate object (loop points, etc.)
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
}


void ShapeMasterDisplay::onDragMove(const event::DragMove& e) {
	if (e.button != GLFW_MOUSE_BUTTON_LEFT) {
		return;
	}
	
	Shape* shape = channels[*currChan].getShape();
	Vec posToSet = APP->scene->rack->mousePos.minus(parent->box.pos).minus(box.pos);// posToSet is in pixel space
	
	int mods = APP->window->getMods();
	if (dragPtSelect != MAX_PTS) {
		if (dragPtSelect >= 0) {// if normal point
			int xQuant;
			int yQuant;
			calcQuants(&xQuant, &yQuant, mods);
			channels[*currChan].setPointWithSafety(dragPtSelect, normalizePixelPoint(posToSet), xQuant, yQuant);
		}
		else {// else is a ctrl point
			int cpt = -dragPtSelect - 1;
			float dy = shape->getPointY(cpt + 1) - shape->getPointY(cpt);
			if (std::fabs(dy) > 1e-5f) {
				float deltaY = (dragStartPosY - posToSet.y) / canvas.y / dy;// deltaY is +1 when mouse moves up by dy and is -1 when mouse moves down by dy
				
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
	}
	else if (altSelect != 0) {
		// clicked alternate object (loop points, etc.)
		float posToSetX = clamp((posToSet.x - margins.x) / canvas.x, 0.0f, 1.0f);
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
			int xQuant;
			int yQuant;
			calcQuants(&xQuant, &yQuant, mods | GLFW_MOD_ALT);// force calc of xQuant since step is forcibly horizontally quantized	
			Vec normPos = normalizePixelPoint(posToSet);
			int gp = std::min(mouseStepGp, shape->getNumPts() - 2);
			mouseStepGp = shape->calcPointFromX<float>(normPos.x, gp);
			shape->makeStep(mouseStepGp, normPos, xQuant, yQuant);
		}
	}
	
	OpaqueWidget::onDragMove(e);
}// onDragMove
	
	
void ShapeMasterDisplay::onDragEnd(const event::DragEnd& e) {
	Shape* shape = channels[*currChan].getShape();
	
	dragPtSelect = MAX_PTS;
	altSelect = 0;
	
	// history
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
}

	
int ShapeMasterDisplay::matchPt(Vec normalizedPos, Shape* shape, int pt) {		
	Vec point = shape->getPointVect(pt);
	Vec delta = (normalizedPos.minus(point)).abs();
	if (delta.x < grabX && delta.y < grabY) {
		return pt;
	}
	Vec ctrl = shape->getCtrlVect(pt);
	delta = (normalizedPos.minus(ctrl)).abs();
	if (delta.x < grabX && delta.y < grabY) {
		return -pt - 1;
	}
	return MAX_PTS;
}

int ShapeMasterDisplay::matchPtExtra(Vec normalizedPos, Shape* shape) {	// will also check a few neighbours
	int numPts = shape->getNumPts();
	if (normalizedPos.x <= 0.0f) {
		matchPtExtraGp = 0;
	}
	else if (normalizedPos.x >= 1.0f) {
		matchPtExtraGp = numPts - 1;
	}
	else {
		int gp = std::min(matchPtExtraGp, numPts - 2);
		matchPtExtraGp = shape->calcPointFromX<float>(normalizedPos.x, gp);
	}

	int	newpt = matchPt(normalizedPos, shape, matchPtExtraGp);
	if (newpt == MAX_PTS && matchPtExtraGp < (numPts - 1)) {
		newpt = matchPt(normalizedPos, shape, matchPtExtraGp + 1);
		if (newpt == MAX_PTS && matchPtExtraGp < (numPts - 2)) {
			newpt = matchPt(normalizedPos, shape, matchPtExtraGp + 2);
		}	
	}
	if (newpt == MAX_PTS && matchPtExtraGp > 0) {
		newpt = matchPt(normalizedPos, shape, matchPtExtraGp - 1);
		if (newpt == MAX_PTS && matchPtExtraGp > 1) {
			newpt = matchPt(normalizedPos, shape, matchPtExtraGp - 2);
		}	
	}	
	return newpt;
}

	
void ShapeMasterDisplay::onHover(const event::Hover& e) {
	hoverPtSelect = MAX_PTS;
	int mods = APP->window->getMods();
	
	if ((mods & GLFW_MOD_SHIFT) == 0 && (dragPtSelect == MAX_PTS) ) {// disallow hovering when making steps, much easier, and when dragging
		// find closest node 
		Shape* shape = channels[*currChan].getShape();						
		hoverPtSelect = matchPtExtra(normalizePixelPoint(e.pos), shape);
	}
	OpaqueWidget::onHover(e);
}
