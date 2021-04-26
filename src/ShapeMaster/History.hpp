//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#pragma once

#include "../MindMeldModular.hpp"
#include <history.hpp>

using namespace history;


// ----------------------------------------------------------------------------
// ShapeMaster
// ----------------------------------------------------------------------------

struct ShapeMaster;


struct RunChange : ModuleAction {
	ShapeMaster* shapeMasterSrc;
	void undo() override;
	void redo() override;
	RunChange() {
		name = "toggle run button";
	}
};


struct ChannelNumChange : ModuleAction {
	int* currChanSrc;
	int oldChanNum;
	int newChanNum;
	void undo() override;
	void redo() override;
	ChannelNumChange() {
		name = "change channel";
	}
};


// ----------------------------------------------------------------------------
// Channel
// ----------------------------------------------------------------------------

class Channel;


struct TrigModeChange : ModuleAction {
	Channel* channelSrc;
	int8_t oldTrigMode;
	int8_t newTrigMode;
	void undo() override;
	void redo() override;
	TrigModeChange() {
		name = "change trig mode";
	}
};


struct PlayModeChange : ModuleAction {
	Channel* channelSrc;
	int8_t oldPlayMode;
	int8_t newPlayMode;
	void undo() override;
	void redo() override;
	PlayModeChange() {
		name = "change play mode";
	}
};


struct BipolCvModeChange : ModuleAction {
	Channel* channelSrc;
	int8_t oldBipolCvMode;
	int8_t newBipolCvMode;
	void undo() override;
	void redo() override;
	BipolCvModeChange() {
		name = "change CV mode polarity";
	}
};


struct SyncLengthChange : ModuleAction {
	Param* lengthSyncParamSrc;
	float oldSyncLength;
	float newSyncLength;
	void undo() override;
	void redo() override;
	SyncLengthChange() {
		name = "change synced length";
	}
};

struct UnsyncLengthChange : ModuleAction {
	Param* lengthUnsyncParamSrc;
	float oldUnsyncLength;
	float newUnsyncLength;
	void undo() override;
	void redo() override;
	UnsyncLengthChange() {
		name = "change unsynced length";
	}
};


struct GridXChange : ModuleAction {
	Channel* channelSrc;
	uint8_t oldGridX;
	uint8_t newGridX;
	void undo() override;
	void redo() override;
	GridXChange() {
		name = "change grid-X";
	}
};


struct RangeIndexChange : ModuleAction {
	Channel* channelSrc;
	int8_t oldRangeIndex;
	int8_t newRangeIndex;
	void undo() override;
	void redo() override;
	RangeIndexChange() {
		name = "change range";
	}
};


struct ChannelChange : ModuleAction {
	Channel* channelSrc;
	json_t* oldJson;
	json_t* newJson;
	void undo() override;
	void redo() override;
	ChannelChange() {
		name = "paste/init channel";
	}
	~ChannelChange() {
		json_decref(oldJson);
		json_decref(newJson);
	}
};	


// ----------------------------------------------------------------------------
// Shape
// ----------------------------------------------------------------------------

class Shape;


struct ShapeCompleteChange : ModuleAction {
	Shape* shapeSrc;
	Shape* oldShape;
	Shape* newShape;
	void undo() override;
	void redo() override;
	ShapeCompleteChange() {
		name = "change shape";// provisional
		oldShape = NULL;
		newShape = NULL;
	}
	~ShapeCompleteChange();
};


struct InvertOrReverseChange : ModuleAction {
	Shape* shapeSrc;
	bool isReverse;
	void undo() override;
	void redo() override;
	InvertOrReverseChange() {
		name = "reverse or invert shape";// provisional
	}
};


struct InsertPointChange : ModuleAction {
	Shape* shapeSrc;
	Vec newPointVec;
	int newPt;
	void undo() override;
	void redo() override;
	InsertPointChange() {
		name = "insert node";
	}
};

struct DeletePointChange : ModuleAction {
	Shape* shapeSrc;
	Vec oldPointVec;
	float oldCtrl;
	int8_t oldType;
	int oldPt;
	void undo() override;
	void redo() override;
	DeletePointChange() {
		name = "delete node";
	}
};


struct TypeAndCtrlChange : ModuleAction {
	Shape* shapeSrc;
	int pt;
	float oldCtrl;
	int8_t oldType;
	float newCtrl;
	int8_t newType;
	void undo() override;
	void redo() override;
	TypeAndCtrlChange() {
		name = "modify control point";
	}
};



// ----------------------------------------------------------------------------
// Mix Channel and Shape
// ----------------------------------------------------------------------------

enum DragMiscChangeIds {DM_POINT, DM_CTRL, DM_LOOP};

struct DragMiscChange : ModuleAction {
	// not all dragTypes use all fields below
	Channel* channelSrc;
	Shape* shapeSrc;
	int dragType;
	int pt;
	Vec oldVec;	
	Vec newVec;	
	void undo() override;
	void redo() override;
	DragMiscChange() {
		name = "drag in display";
	}
};


struct PresetOrShapeChange : ModuleAction {
	bool isPreset;
	// not all dragTypes use all fields below
	Channel* channelSrc;
	Shape* shapeSrc;
	json_t* oldJson;
	json_t* newJson;
	void undo() override;
	void redo() override;
	PresetOrShapeChange() {
		name = "change preset or shape";
	}
	~PresetOrShapeChange() {
		json_decref(oldJson);
		json_decref(newJson);
	}
};	


struct PresetOrShapeLoad : ModuleAction {
	bool isPreset;
	Channel* channelSrc;
	json_t* oldJson;
	json_t* newJson;
	std::string oldShapePath;
	std::string newShapePath;
	void undo() override;
	void redo() override;
	PresetOrShapeLoad() {
		name = "change preset or shape";
	}
	~PresetOrShapeLoad() {
		json_decref(oldJson);
		json_decref(newJson);
	}
};	


