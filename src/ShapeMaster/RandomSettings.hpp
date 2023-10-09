//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#pragma once

#include "../MindMeldModular.hpp"


struct RandomSettings {
	static constexpr float RAND_NODES_MAX = 128.0f;
	static constexpr float RAND_NODES_MIN = 1.0f;// excluding end points
	static constexpr float RAND_NODES_MIN_DEF = 5.0f;
	static constexpr float RAND_NODES_MAX_DEF = 30.0f;
	static constexpr float RAND_CTRL_DEF = 100.0f;
	static constexpr float RAND_ZERO_DEF = 0.0f;
	static constexpr float RAND_DELTA_CHANGE_DEF = 50.0f;
	static constexpr float RAND_DELTA_NODES_DEF = 50.0f;

	float numNodesMin;
	float numNodesMax;
	float ctrlMax;// 0.0f to 100.0f (percent of crtl range allowed to be selected during random)
	float zeroV;// 0.0f to 100.0f (percent of segments that will be 0V)
	float maxV;// 0.0f to 100.0f (percent of segments that will be maxV)
	float deltaChange;// 0.0f to 100.0f (percent of range that will be used for randomized offsets in delta mode)
	float deltaNodes;// 0.0f to 100.0f (percent of nodes that will be randomized in delta mode)
	uint16_t scale;
	int8_t stepped;
	int8_t grid;
	int8_t quantized;
	int8_t deltaMode;
	
	RandomSettings() {
		reset();
	}
	
	void reset() {
		numNodesMin = RAND_NODES_MIN_DEF;
		numNodesMax = RAND_NODES_MAX_DEF;
		ctrlMax = RAND_CTRL_DEF;
		zeroV = RAND_ZERO_DEF;
		maxV = RAND_ZERO_DEF;
		deltaChange = RAND_DELTA_CHANGE_DEF;
		deltaNodes = RAND_DELTA_NODES_DEF;
		scale = 0xFFF;// all notes on, C = bit 0, C# = bit 1, B = bit 11, etc. When scale == 0, same as 0xFFF
		stepped = 0;
		grid = 0;
		quantized = 0;
		deltaMode = 0;
	}
	
	bool getScaleKey(int k) {// k = 0 means C, k = 1 means C#, etc.
		return ((scale & (1 << k)) != 0);
	}	
	void toggleScaleKey(int k) {// k = 0 means C, k = 1 means C#, etc.
		scale ^= (1 << k);
	}
	
	
	void dataToJson(json_t* rootJ) {
		json_object_set_new(rootJ, "r_numNodesMin", json_real(numNodesMin));
		json_object_set_new(rootJ, "r_numNodesMax", json_real(numNodesMax));
		json_object_set_new(rootJ, "r_ctrlMax", json_real(ctrlMax));
		json_object_set_new(rootJ, "r_zeroV", json_real(zeroV));
		json_object_set_new(rootJ, "r_maxV", json_real(maxV));
		json_object_set_new(rootJ, "r_deltaChange", json_real(deltaChange));
		json_object_set_new(rootJ, "r_deltaNodes", json_real(deltaNodes));
		json_object_set_new(rootJ, "r_scale", json_integer(scale));
		json_object_set_new(rootJ, "r_stepped", json_integer(stepped));
		json_object_set_new(rootJ, "r_grid", json_integer(grid));
		json_object_set_new(rootJ, "r_quantized", json_integer(quantized));
		json_object_set_new(rootJ, "r_deltaMode", json_integer(deltaMode));
	}
	
	void dataFromJson(json_t* rootJ) {
		json_t *numNodesMinJ = json_object_get(rootJ, "r_numNodesMin");
		if (numNodesMinJ) numNodesMin = json_number_value(numNodesMinJ);

		json_t *numNodesMaxJ = json_object_get(rootJ, "r_numNodesMax");
		if (numNodesMaxJ) numNodesMax = json_number_value(numNodesMaxJ);

		json_t *ctrlMaxJ = json_object_get(rootJ, "r_ctrlMax");
		if (ctrlMaxJ) ctrlMax = json_number_value(ctrlMaxJ);

		json_t *zeroVJ = json_object_get(rootJ, "r_zeroV");
		if (zeroVJ) zeroV = json_number_value(zeroVJ);

		json_t *maxVJ = json_object_get(rootJ, "r_maxV");
		if (maxVJ) maxV = json_number_value(maxVJ);

		json_t *deltaChangeJ = json_object_get(rootJ, "r_deltaChange");
		if (deltaChangeJ) deltaChange = json_number_value(deltaChangeJ);

		json_t *deltaNodesJ = json_object_get(rootJ, "r_deltaNodes");
		if (deltaNodesJ) deltaNodes = json_number_value(deltaNodesJ);

		json_t *scaleJ = json_object_get(rootJ, "r_scale");
		if (scaleJ) scale = json_integer_value(scaleJ);

		json_t *steppedJ = json_object_get(rootJ, "r_stepped");
		if (steppedJ) stepped = json_integer_value(steppedJ);

		json_t *gridJ = json_object_get(rootJ, "r_grid");
		if (gridJ) grid = json_integer_value(gridJ);

		json_t *quantizedJ = json_object_get(rootJ, "r_quantized");
		if (quantizedJ) quantized = json_integer_value(quantizedJ);

		json_t *deltaModeJ = json_object_get(rootJ, "r_deltaMode");
		if (deltaModeJ) deltaMode = json_integer_value(deltaModeJ);
	}
	
	bool isDirty(const RandomSettings* refRand) {
		if (std::round(numNodesMin) != std::round(refRand->numNodesMin)) return true;// float value with decimals, but meaning is int
		if (std::round(numNodesMax) != std::round(refRand->numNodesMax)) return true;// float value with decimals, but meaning is int
		if (std::round(ctrlMax * 10.0f) != std::round(refRand->ctrlMax * 10.0f)) return true;// percent comparison to one decimal
		if (std::round(zeroV * 10.0f) != std::round(refRand->zeroV * 10.0f)) return true;// percent comparison to one decimal
		if (std::round(maxV * 10.0f) != std::round(refRand->maxV * 10.0f)) return true;// percent comparison to one decimal
		if (std::round(deltaChange * 10.0f) != std::round(refRand->deltaChange * 10.0f)) return true;// percent comparison to one decimal
		if (std::round(deltaNodes * 10.0f) != std::round(refRand->deltaNodes * 10.0f)) return true;// percent comparison to one decimal
		if (scale != refRand->scale) return true;// int comparison
		if (stepped != refRand->stepped) return true;// int comparison
		if (grid != refRand->grid) return true;// int comparison
		if (quantized != refRand->quantized) return true;// int comparison
		if (deltaMode != refRand->deltaMode) return true;// int comparison
		return false;
	}
};

