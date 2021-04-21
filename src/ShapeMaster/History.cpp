//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "History.hpp"
#include "ShapeMaster.hpp"

// ----------------------------------------------------------------------------
// ShapeMaster
// ----------------------------------------------------------------------------

void RunChange::undo() {
	shapeMasterSrc->processRunToggled();
}
void RunChange::redo() {
	shapeMasterSrc->processRunToggled();
}


void ChannelNumChange::undo() {
	*currChanSrc = oldChanNum;
}
void ChannelNumChange::redo() {
	*currChanSrc = newChanNum;
}



// ----------------------------------------------------------------------------
// Channel
// ----------------------------------------------------------------------------

void TrigModeChange::undo() {
	channelSrc->setTrigMode(oldTrigMode);
}
void TrigModeChange::redo() {
	channelSrc->setTrigMode(newTrigMode);
}


void PlayModeChange::undo() {
	channelSrc->setPlayMode(oldPlayMode);
}
void PlayModeChange::redo() {
	channelSrc->setPlayMode(newPlayMode);
}


void BipolCvModeChange::undo() {
	channelSrc->setBipolCvMode(oldBipolCvMode);
}
void BipolCvModeChange::redo() {
	channelSrc->setBipolCvMode(newBipolCvMode);
}


void SyncLengthChange::undo() {
	lengthSyncParamSrc->setValue(oldSyncLength);
}
void SyncLengthChange::redo() {
	lengthSyncParamSrc->setValue(newSyncLength);
}


void UnsyncLengthChange::undo() {
	lengthUnsyncParamSrc->setValue(oldUnsyncLength);
}
void UnsyncLengthChange::redo() {
	lengthUnsyncParamSrc->setValue(newUnsyncLength);
}


void GridXChange::undo() {
	channelSrc->setGridX(oldGridX, false);// without history
}
void GridXChange::redo() {
	channelSrc->setGridX(newGridX, false);// without history
}


void RangeIndexChange::undo() {
	channelSrc->setRangeIndex(oldRangeIndex, false);// without history
}
void RangeIndexChange::redo() {
	channelSrc->setRangeIndex(newRangeIndex, false);// without history
}


void ChannelChange::undo() {
	channelSrc->dataFromJsonChannel(oldJson, WITH_PARAMS, ISNOT_DIRTY_CACHE_LOAD, WITH_FULL_SETTINGS);
}
void ChannelChange::redo() {
	channelSrc->dataFromJsonChannel(newJson, WITH_PARAMS, ISNOT_DIRTY_CACHE_LOAD, WITH_FULL_SETTINGS);
}



// ----------------------------------------------------------------------------
// Shape
// ----------------------------------------------------------------------------

void ShapeCompleteChange::undo() {
	shapeSrc->pasteShapeFrom(oldShape, false);// without history
}
void ShapeCompleteChange::redo() {
	shapeSrc->pasteShapeFrom(newShape, false);// without history
}
ShapeCompleteChange::~ShapeCompleteChange() {
	delete oldShape;
	delete newShape;
}


void InsertPointChange::undo() {
	shapeSrc->deletePointWithBlock(newPt, false);// without history
}
void InsertPointChange::redo() {
	shapeSrc->insertPointWithSafetyAndBlock(newPointVec, false, false);// without history nor safety
}


void TypeAndCtrlChange::undo() {
	shapeSrc->setCtrlWithSafety(pt, oldCtrl);
	shapeSrc->setType(pt, oldType);
}
void TypeAndCtrlChange::redo() {
	shapeSrc->setCtrlWithSafety(pt, newCtrl);
	shapeSrc->setType(pt, newType);
}


void DeletePointChange::undo() {
	shapeSrc->insertPointWithSafetyAndBlock(oldPointVec, false, false, oldCtrl, oldType);// without history nor safety
}
void DeletePointChange::redo() {
	shapeSrc->deletePointWithBlock(oldPt, false);// without history
}



// ----------------------------------------------------------------------------
// Mix Channel and Shape
// ----------------------------------------------------------------------------

void DragMiscChange::undo() {
	if (dragType == DM_POINT) {
		shapeSrc->setPoint(pt, oldVec);
	}
	else if (dragType == DM_CTRL) {
		shapeSrc->setCtrlWithSafety(pt, oldVec.x);
	}
	else if (dragType == DM_LOOP) {
		channelSrc->setLoopEndAndSustain(oldVec.y);
		channelSrc->setLoopStart(oldVec.x);
	}
}
void DragMiscChange::redo() {
	if (dragType == DM_POINT) {
		shapeSrc->setPoint(pt, newVec);
	}
	else if (dragType == DM_CTRL) {
		shapeSrc->setCtrlWithSafety(pt, newVec.x);
	}
	else if (dragType == DM_LOOP) {
		channelSrc->setLoopEndAndSustain(newVec.y);
		channelSrc->setLoopStart(newVec.x);
	}
}


void PresetOrShapeChange::undo() {
	if (isPreset) {
		channelSrc->dataFromJsonChannel(oldJson, WITH_PARAMS, ISNOT_DIRTY_CACHE_LOAD, WITHOUT_FULL_SETTINGS);
	}
	else {
		shapeSrc->dataFromJsonShape(oldJson);
	}
}
void PresetOrShapeChange::redo() {
	if (isPreset) {
		channelSrc->dataFromJsonChannel(newJson, WITH_PARAMS, ISNOT_DIRTY_CACHE_LOAD, WITHOUT_FULL_SETTINGS);
	}
	else {
		shapeSrc->dataFromJsonShape(oldJson);
	}
}

void PresetOrShapeLoad::undo() {
	if (isPreset) {
		channelSrc->dataFromJsonChannel(oldJson, WITH_PARAMS, ISNOT_DIRTY_CACHE_LOAD, WITHOUT_FULL_SETTINGS);
	}
	else {
		channelSrc->getShape()->dataFromJsonShape(oldJson);
		channelSrc->setShapePath(oldShapePath);
	}
}
void PresetOrShapeLoad::redo() {
	if (isPreset) {
		channelSrc->dataFromJsonChannel(newJson, WITH_PARAMS, ISNOT_DIRTY_CACHE_LOAD, WITHOUT_FULL_SETTINGS);
	}
	else {
		channelSrc->getShape()->dataFromJsonShape(oldJson);
		channelSrc->setShapePath(newShapePath);
	}
}

