//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//Based on code from the Fundamental plugin by Andrew Belt
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#pragma once

#include "../MindMeldModular.hpp"
#include "DisplayUtil.hpp"



struct ShapeMasterDisplayLight : LightWidget {	
	// constants
	const NVGcolor DARKER_GRAY = nvgRGB(39, 39, 39);// grid and center of control points
	const NVGcolor DARK_GRAY = nvgRGB(55, 55, 55);// major grid when applicable
	static constexpr float MINI_SHAPES_Y = 6.8f;// can be set to 0.0f
	static const int SHAPE_PTS = 300;// shadow memory, divide into this many segments
	
	// user must set up
	int* currChan = NULL;
	Channel* channels;
	DisplayInfo* displayInfo;
	PackedBytes4 *settingSrc;// cc4[2] is scope settings, cc4[3] is show points
	PackedBytes4 *setting2Src;// cc4[0] is global inverted shadow, [2] is show channel names, [3] is point tooltip
	PackedBytes4 *setting3Src;// cc4[2] is cloaked
	float* lineWidthSrc;
	int* dragPtSelect;// from ShapeMasterDisplay
	int* hoverPtSelect;// from ShapeMasterDisplay
	ScopeBuffers* scopeBuffers;	
	

	// internal
	Vec margins;
	Vec canvas;
	std::shared_ptr<Font> font;
	std::string fontPath;
	float shaY[SHAPE_PTS + 1];// points of the shadow curve, with an extra element for last end point
	int numGridXmajorX;
	float gridXmajorX[16];


	ShapeMasterDisplayLight() {
		canvas = mm2px(Vec(133.0f, 57.8f - MINI_SHAPES_Y));// inner region where actual drawing takes place
		margins = mm2px(Vec(1.3f, 1.3f));// extra space to be able to click points that are on canvas' boundary
		box.size = canvas.plus(margins.mult(2.0f));
		box.size.y += mm2px(MINI_SHAPES_Y);
		fontPath = std::string(asset::plugin(pluginInstance, "res/fonts/RobotoCondensed-Regular.ttf"));
	}
	
	void draw(const DrawArgs &args) override {
		if (!(font = APP->window->loadFont(fontPath))) {
			return;
		}
		nvgSave(args.vg);
		nvgLineCap(args.vg, NVG_ROUND);
		
		if (currChan != NULL) {
					
			// nvgScissor(args.vg, 0, 0, box.size.x, box.size.y);

			if (setting3Src->cc4[2] == 0) {// if not cloaked
				drawGrid(args);
				drawScope(args);
				drawShape(args);
			}
			
			drawMessages(args);
			
			// nvgResetScissor(args.vg);					
		}

		nvgRestore(args.vg);
	}


	void loopVLines(const DrawArgs &args, float startX, float xDelta, int numVLines) {
		// assumes "numGridXmajorX = 0" was done before calling this
		// also sets numGridXmajorX and gridXmajorX array as needed, numGridXmajorX will be 0 for numVLines < 16
		int majorGridModulo = ( (numVLines % 12) == 0 ? 6 : 8 );
		float xPos = startX + xDelta;
		for (int i = 1; i < numVLines; i++) {
			nvgMoveTo(args.vg, xPos, margins.y);
			nvgLineTo(args.vg, xPos, margins.y + canvas.y);
			if (i % majorGridModulo == 0 && (numVLines >= 16 || numVLines == 12) && numGridXmajorX < 16) {
				gridXmajorX[numGridXmajorX] = xPos;
				numGridXmajorX++;
			}
			xPos += xDelta;
		}
	}
	void loopHLines(const DrawArgs &args, float startY, float yDelta, int numHLines) {
		float yPos = startY + yDelta;
		for (int i = 0; i < numHLines - 1; i++) {
			nvgMoveTo(args.vg, margins.x, yPos);
			nvgLineTo(args.vg, margins.x + canvas.x, yPos);
			yPos += yDelta;
		}
	}

	void drawGrid(const DrawArgs &args); 

	void drawScopeWaveform(const DrawArgs &args, bool isFront);
	void drawScope(const DrawArgs &args);

	void drawShape(const DrawArgs &args);

	void drawMessages(const DrawArgs &args);
};



struct ShapeMasterDisplay : OpaqueWidget {	
	// constants
	static constexpr float MINI_SHAPES_Y = 6.8f;// can be set to 0.0f
	static constexpr float LOOP_GRAB_X = 3.0f;
	
	// user must set up
	int* currChan = NULL;
	Channel* channels;
	float* lineWidthSrc;
	float* shaY;// from ShapeMasterDisplayLight
	PackedBytes4 *setting3Src;// cc4[2] is cloaked
	
	// internal
	float dragStartPosY;// used only when dragging control points
	Vec onButtonPos;// used only for onDoubleClick() and onDragStart()
	ShapeCompleteChange* dragHistoryStep = NULL;
	DragMiscChange* dragHistoryMisc = NULL;
	int dragPtSelect = MAX_PTS;// MAX_PTS when none, [0:MAX_PTS-1] when dragging normal point, [-MAX_PTS:-1] when dragging ctrl point
	int altSelect = 0;// alternate select 0=none, 1=loopEndAndSustain, 2=loopStart, this is only used when dragPtSelect == MAX_PTS; if other altSelects are added, review code since != 0 currently assumes 1 or 2 
	int hoverPtSelect = MAX_PTS;// MAX_PTS when none, [0:MAX_PTS-1] when hovering normal point, [-MAX_PTS:-1] when hovering ctrl point
	float loopSnapTargetCV = -1.0f;// used only when altSelect != 0;
	int matchPtExtraGp = 0;// used in matchPtExtra() as a memory for its guess point;
	int mouseStepGp = 0;// used in onDragMove() to improve guess point; using hoverPtMouse is not perfect (not synced when move mouse fast)
	float onButtonOrigCtrl;// only used when hoverPtSelect < 0
	Vec margins;
	Vec canvas;
	float grabX;
	float grabY;


	ShapeMasterDisplay() {
		canvas = mm2px(Vec(133.0f, 57.8f - MINI_SHAPES_Y));// inner region where actual drawing takes place
		margins = mm2px(Vec(1.3f, 1.3f));// extra space to be able to click points that are on canvas' boundary
		box.size = canvas.plus(margins.mult(2.0f));
		box.size.y += mm2px(MINI_SHAPES_Y);
	}
	
	
	void step() override {
		grabX = 0.01f;
		grabY = 0.02f;		
		
		if (currChan != NULL) {
			grabX *= *lineWidthSrc;
			grabY *= *lineWidthSrc;
		}
		OpaqueWidget::step();
	}
	
		
	
	
	Vec normalizePixelPoint(Vec pixelPoint) {
		pixelPoint = pixelPoint.minus(margins).div(canvas);// after this line we are in normalized space but inverted Y
		pixelPoint.x = clamp(pixelPoint.x, 0.0f, 1.0f);
		pixelPoint.y = clamp(1.0f - pixelPoint.y, 0.0f, 1.0f);
		return pixelPoint;
	}
		
		
	void calcQuants(int* xQ, int* yQ, int mods) {
		*xQ = -1;
		// x quantize (snap)
		if ((mods & GLFW_MOD_ALT) != 0) {
			*xQ = channels[*currChan].getGridX();
		}
		*yQ = -1;
		// y quantize (range)
		if ((mods & RACK_MOD_CTRL) != 0) {
			int rangeIndex = channels[*currChan].getRangeIndex();
			int numHLines;// unused here
			*yQ = calcNumHLinesAndWithOct(rangeIndex, &numHLines);
		}
	}
	
	float findXWithGivenCvI(int shaI, float givenCv);
	float findXWithGivenCv(float startX, float givenCv);
	
	void onButton(const event::Button& e) override;
	void onDoubleClick(const event::DoubleClick &e) override;

	void onDragStart(const event::DragStart& e) override;
	void onDragMove(const event::DragMove& e) override;
	void onDragEnd(const event::DragEnd& e) override;

	int matchPt(Vec normalizedPos, Shape* shape, int pt);
	int matchPtExtra(Vec normalizedPos, Shape* shape);
	void onHover(const event::Hover& e) override;

	
	void onLeave(const event::Leave& e) override {
		hoverPtSelect = MAX_PTS;
		OpaqueWidget::onLeave(e);
	}
};
