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
	ShapeMaster* shapeMasterSrc = nullptr;
	void undo() override;
	void redo() override;
	RunChange() {
		name = "toggle run button";
	}
};


struct ChannelNumChange : ModuleAction {
	int* currChanSrc = nullptr;
	int oldChanNum = 0;
	int newChanNum = 0;
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
	Channel* channelSrc = nullptr;
	int8_t oldTrigMode = 0;
	int8_t newTrigMode = 0;
	void undo() override;
	void redo() override;
	TrigModeChange() {
		name = "change trig mode";
	}
};


struct PlayModeChange : ModuleAction {
	Channel* channelSrc = nullptr;
	int8_t oldPlayMode = 0;
	int8_t newPlayMode = 0;
	void undo() override;
	void redo() override;
	PlayModeChange() {
		name = "change play mode";
	}
};


struct BipolCvModeChange : ModuleAction {
	Channel* channelSrc = nullptr;
	int8_t oldBipolCvMode = 0;
	int8_t newBipolCvMode = 0;
	void undo() override;
	void redo() override;
	BipolCvModeChange() {
		name = "change CV mode polarity";
	}
};


struct SyncLengthChange : ModuleAction {
	Param* lengthSyncParamSrc = nullptr;
	float oldSyncLength = 0.0f;
	float newSyncLength = 0.0f;
	void undo() override;
	void redo() override;
	SyncLengthChange() {
		name = "change synced length";
	}
};

struct UnsyncLengthChange : ModuleAction {
	Param* lengthUnsyncParamSrc = nullptr;
	float oldUnsyncLength = 0.0f;
	float newUnsyncLength = 0.0f;
	void undo() override;
	void redo() override;
	UnsyncLengthChange() {
		name = "change unsynced length";
	}
};


struct GridXChange : ModuleAction {
	Channel* channelSrc = nullptr;
	uint8_t oldGridX = 0;
	uint8_t newGridX = 0;
	void undo() override;
	void redo() override;
	GridXChange() {
		name = "change grid-X";
	}
};


struct RangeIndexChange : ModuleAction {
	Channel* channelSrc = nullptr;
	int8_t oldRangeIndex = 0;
	int8_t newRangeIndex = 0;
	void undo() override;
	void redo() override;
	RangeIndexChange() {
		name = "change range";
	}
};


struct ChannelChange : ModuleAction {
	Channel* channelSrc = nullptr;
	json_t* oldJson = nullptr;
	json_t* newJson = nullptr;
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
	Shape* shapeSrc = nullptr;
	Shape* oldShape = nullptr;
	Shape* newShape = nullptr;
	void undo() override;
	void redo() override;
	ShapeCompleteChange() {
		name = "change shape";// provisional
		oldShape = nullptr;
		newShape = nullptr;
	}
	~ShapeCompleteChange();
};


struct InvertOrReverseChange : ModuleAction {
	Shape* shapeSrc = nullptr;
	bool isReverse = false;
	void undo() override;
	void redo() override;
	InvertOrReverseChange() {
		name = "reverse or invert shape";// provisional
	}
};


struct InsertPointChange : ModuleAction {
	Shape* shapeSrc = nullptr;
	Vec newPointVec;
	int newPt = 0;
	void undo() override;
	void redo() override;
	InsertPointChange() {
		name = "insert node";
	}
};

struct DeletePointChange : ModuleAction {
	Shape* shapeSrc = nullptr;
	Vec oldPointVec;
	float oldCtrl = 0.0f;
	int8_t oldType = 0;
	int oldPt = 0;
	void undo() override;
	void redo() override;
	DeletePointChange() {
		name = "delete node";
	}
};


struct TypeAndCtrlChange : ModuleAction {
	Shape* shapeSrc = nullptr;
	int pt = 0;
	float oldCtrl = 0.0f;
	int8_t oldType = 0;
	float newCtrl = 0.0f;
	int8_t newType = 0;
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
	Channel* channelSrc = nullptr;
	Shape* shapeSrc = nullptr;
	int dragType = 0;
	int pt = 0;
	Vec oldVec;	
	Vec newVec;	
	void undo() override;
	void redo() override;
	DragMiscChange() {
		name = "drag in display";
	}
};


struct PresetOrShapeChange : ModuleAction {
	bool isPreset = false;
	// not all dragTypes use all fields below
	Channel* channelSrc = nullptr;
	Shape* shapeSrc = nullptr;
	json_t* oldJson = nullptr;
	json_t* newJson = nullptr;
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
	bool isPreset = false;
	Channel* channelSrc = nullptr;
	json_t* oldJson = nullptr;
	json_t* newJson = nullptr;
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


