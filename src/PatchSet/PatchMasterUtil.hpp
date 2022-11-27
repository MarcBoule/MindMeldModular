//***********************************************************************************************
//Configurable multi-controller with parameter mapping
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************

#pragma once

#include "../MindMeldModular.hpp"

static const int NUM_CTRL = 8;
static const int NUM_SEP = 8;


// tile info bitfield:
// bits 0-3: type
// bits 4-6: size
// bit  7  : visible
// all zeros is not a valid value (because TC_X starts at 0x01), so 0x00 can be used as un unseen code

static const uint8_t TI_TYPE_MASK = 0x0F;
static const uint8_t TI_SIZE_MASK = 0x70;
static const uint8_t TI_VIS_MASK = 0x80;

// type:
static const uint8_t TT_UNUSED =  0x01;// 
static const uint8_t TT_KNOB_C =  0x02;// default click is center (three sizes)
static const uint8_t TT_KNOB_L =  0x03;// default click is left (three sizes)
static const uint8_t TT_BUTN_M =  0x04;// momentary (three sizes)
static const uint8_t TT_BUTN_N =  0x05;// latch with normal polarity (three sizes)
static const uint8_t TT_BUTN_I =  0x06;// latch with inverted polarity (three sizes)
static const uint8_t TT_BUTN_RT = 0x07;// radio button trig (three sizes)
static const uint8_t TT_FADER_C = 0x08;// default center (TS_LARGE, TS_XLARGE and TS_XXLARGE)
static const uint8_t TT_FADER_B = 0x09;// default bottom (TS_LARGE, TS_XLARGE and TS_XXLARGE)
static const uint8_t TT_BLANK =   0x0A;// used for blank tile (a separator with no label)
static const uint8_t TT_SEP =     0x0B;// used for separator
static const uint8_t TT_BUTN_RL = 0x0C;// radio button latch (three sizes), same as TT_BUTN_RT but the mapping signal is kept high for the hot controller in the group

// sizes:
//   (can be "(x & ~TI_VIS_MASK) >> 4" to index TILE_HEIGHT, LABEL_DY, CTRL_DY
static const uint8_t TS_XXSMALL = 0x00;// blank only
static const uint8_t TS_XSMALL = 0x10;// used for separator in particular, so its LABEL_DY[] item is not -1.0f
static const uint8_t TS_SMALL = 0x20; // 8 tiles
static const uint8_t TS_MEDIUM = 0x30;// 6 tiles
static const uint8_t TS_LARGE = 0x40;// 4 tiles
static const uint8_t TS_XLARGE = 0x50;// 2 tiles in module, used for fader only
static const uint8_t TS_XXLARGE = 0x60;// 1 tile in module + room for two TS_SMALL, used for fader only
static const uint8_t TS_SMALLER = 0x70;// 1 tile in module + room for two TS_SMALL, used for fader only
static constexpr float TILE_HEIGHT[8] = {2.214f, 5.678f, 12.606f, 17.222f, 26.462f, 54.175f, 81.887f, 7.990f};// in mm
static constexpr float LABEL_DY[8]    = {-1.00f, 2.740f, 10.000f, 14.4f, 22.60f, 51.27f, 79.00f, -1.0f};// in mm; y offset for labels
static constexpr float CTRL_DY[8]     = {-1.00f, -1.00f, 5.0000f, 7.30f, 11.85f, 25.4f, 39.3f, -1.0f};// in mm; y offset of control
static const float TILE_VMARGIN = 1.25f;// in mm (SB: 1.25mm)


// visible macros:
inline bool isTileVisible(uint8_t ti) {return (ti & TI_VIS_MASK) != 0;}
inline void toggleVisible(uint8_t* ti) {*ti ^= TI_VIS_MASK;}
inline void clearVisible(uint8_t* ti) {*ti &= ~TI_VIS_MASK;}

// controller macros:
inline bool isCtlrAKnob(uint8_t ti) {return (ti & TI_TYPE_MASK) >= TT_KNOB_C && (ti & TI_TYPE_MASK) <= TT_KNOB_L;}
inline bool isCtlrAButton(uint8_t ti) {
	uint8_t sz = (ti & TI_TYPE_MASK); 
	return (sz >= TT_BUTN_M && sz <= TT_BUTN_RT) || sz == TT_BUTN_RL;}
inline bool isCtlrAFader(uint8_t ti) {return (ti & TI_TYPE_MASK) == TT_FADER_C || (ti & TI_TYPE_MASK) == TT_FADER_B;}
inline bool isButtonParamMomentary(uint8_t ti) {return ((ti & TI_TYPE_MASK) == TT_BUTN_M) || ((ti & TI_TYPE_MASK) == TT_BUTN_RT) || ((ti & TI_TYPE_MASK) == TT_BUTN_RL);}

// size macros:
inline uint8_t getSizeIndex(uint8_t ti) {return (ti & ~TI_VIS_MASK) >> 4;}
inline float getHeight(uint8_t sizeIndex) {return TILE_HEIGHT[sizeIndex];}
inline float getLabelDy(uint8_t sizeIndex) {return LABEL_DY[sizeIndex];}
inline float getCtrlDy(uint8_t sizeIndex) {return CTRL_DY[sizeIndex];}



struct TileInfos {
	static const uint8_t defaultControllerInfo = TT_KNOB_L | TS_MEDIUM | TI_VIS_MASK;
	static const uint8_t defaultSeparatorInfo = TT_SEP | TS_XSMALL | TI_VIS_MASK;
	uint8_t infos[NUM_CTRL + NUM_SEP];// first are controllers, then separators (blanks are seps but with no label). The index number into this array is refered to as a tile number
	
	TileInfos() {
		init();
	}
	
	void init() {
		// tiles 0 to NUM_CTRL - 1 are controllers
		infos[0] = defaultControllerInfo;
		infos[1] = TT_BUTN_M | TS_MEDIUM | TI_VIS_MASK; 
		for (int ic = 2; ic < NUM_CTRL; ic++) {
			infos[ic] = TT_KNOB_L | TS_MEDIUM;
		}
		// tile NUM_CTRL to NUM_SEP - 1 are separators (and blanks)
		for (int is = NUM_CTRL; is < NUM_CTRL + NUM_SEP; is++) {
			infos[is] = defaultSeparatorInfo; 
		}
	}
	
	void clear() {
		for (int i = 0; i < NUM_CTRL + NUM_SEP; i++) {
			infos[i] = 0;
		}
	}
	
	void copyFrom(TileInfos* src) {
		for (int i = 0; i < NUM_CTRL + NUM_SEP; i++) {
			infos[i] = src->infos[i];
		}		
	}
};


struct TileNames {
	std::string names[NUM_CTRL + NUM_SEP];
	
	TileNames() {
		init();
	}
	
	void init() {
		// tiles 0 to NUM_CTRL - 1 are controllers
		names[0] = "Controller 1";
		names[1] = "Controller 2";
		for (int nc = 2; nc < NUM_CTRL; nc++) {
			names[nc] = "No name";
		}
		// tile NUM_CTRL to NUM_SEP - 1 are separators (and blanks)
		names[NUM_CTRL] = "PatchMaster";
		for (int ns = NUM_CTRL + 1; ns < NUM_CTRL + NUM_SEP; ns++) {
			names[ns] = "No name";
		}
	}
};


struct TileSettings {
	static const int8_t defaultControlHighlightColor = 6; 
	PackedBytes4 settings[NUM_CTRL + NUM_SEP];
	
	TileSettings() {
		init();
	}
	
	void initTile(int s) {
		settings[s].cc4[0] = s < NUM_CTRL ? defaultControlHighlightColor : 0;// label color: blue for controllers, yellow for separators 
		settings[s].cc4[1] = 0;// not used
		settings[s].cc4[2] = 0;// not used
		settings[s].cc4[3] = 0;// not used
	}
	
	void init() {
		for (int s = 0; s < NUM_CTRL + NUM_SEP; s++) {
			initTile(s);
		}
	}
};


struct TileOrders {
	int8_t orders[NUM_CTRL + NUM_SEP];// ordered list of tile numbers, as they would appear if all visible and enough space, terminated by -1 or implicit size of array when full (full = all available controllers and separators added, but likely not all shown in module widget though since no space)
	// A tile number between 0 to NUM_CTRL - 1 indicates a controller
	// A tile number between NUM_CTRL to NUM_CTRL + NUM_SEP - 1 indicates a separator tile (and blanks)
	
	
	TileOrders() {
		init();
	}
	
	void init() {
		orders[0] = NUM_CTRL;
		orders[1] = 0;
		orders[2] = 1;
		for (int o = 3; o < NUM_CTRL + NUM_SEP; o++) {
			orders[o] = -1;
		}
	}

	void clear() {
		for (int o = 0; o < NUM_CTRL + NUM_SEP; o++) {
			orders[o] = 0;
		}
	}

	void copyFrom(TileOrders* src) {
		for (int o = 0; o < NUM_CTRL + NUM_SEP; o++) {
			orders[o] = src->orders[o];
		}	
	}

	void moveIndex(int src, int dest) {
		int8_t tmp = orders[src];
		if (src != dest) {
			if (dest < src) {
				for (int o = src - 1; o >= dest; o--) {
					orders[o + 1] = orders[o];
				}
			}
			else {
				for (int o = src; o < dest; o++) {
					orders[o] = orders[o + 1];
				}	
			}
			orders[dest] = tmp;
		}		
	}
	
	int findNextControllerTileNumber() {
		// returns -1 when all controllers already used
		uint8_t used[NUM_CTRL] = {};
		for (int o = 0; o < NUM_CTRL + NUM_SEP; o++) {
			if (orders[o] == -1) {
				break;
			}
			if (orders[o] < NUM_CTRL) {
				used[orders[o]] = 1;
			}	
		}	
		
		for (int i = 0; i < NUM_CTRL; i++) {
			if (used[i] == 0) {
				return i;
			}
		}
		return -1;
	}
	
	int findNextSeparatorTileNumber() {
		// returns -1 when all separators already used
		uint8_t used[NUM_SEP] = {};
		for (int o = 0; o < NUM_CTRL + NUM_SEP; o++) {
			if (orders[o] == -1) {
				break;
			}
			if (orders[o] >= NUM_CTRL) {
				used[orders[o] - NUM_CTRL] = 1;
			}	
		}	
		
		for (int i = 0; i < NUM_SEP; i++) {
			if (used[i] == 0) {
				return i + NUM_CTRL;
			}
		}
		return -1;
	}
};


struct TileConfig {
	ParamHandle parHandles[4] = {};// 4 possible maps for a given tile's controller. no onReset needed, since it's constructor/destr. Do not copy or move these, they should be fixed location
	float rangeMax[4] = {};
	float rangeMin[4] = {};
	uint8_t lit = 0;// this is a bool, used for radio buttons (both latched or trig)

	
	void initAllExceptParHandles() {
		for (int m = 0; m < 4; m++) {
			rangeMax[m] = 1.0f;
			rangeMin[m] = 0.0f;
		}
		lit = 0;
	}
		
	void toJson(json_t* pmTileJ) {
		// used only when copy pasting tiles 
		json_t* configsJ = json_array();
		for (size_t m = 0; m < 4; m++) {
			json_t* configJ = json_object();
			// parHandles not copied
			json_object_set_new(configJ, "rangeMax", json_real(rangeMax[m]));
			json_object_set_new(configJ, "rangeMin", json_real(rangeMin[m]));
			json_array_append_new(configsJ, configJ);
		}
		json_object_set_new(pmTileJ, "configs", configsJ);
		// lit not copied
	}
	
	void fromJson(json_t* pmTileJ) {
		// used only when copy pasting tiles 
		json_t* configsJ = json_object_get(pmTileJ, "configs");
		if ( !configsJ || !json_is_array(configsJ) ) {
			WARN("PatchMaster error patch-master-tile configs array malformed or missing");
			return;
		}
		for (int m = 0; m < std::min(4, (int)json_array_size(configsJ)); m++) {
			json_t* configJ = json_array_get(configsJ, m);
			if (!configJ) {
				WARN("PatchMaster error missing config in configs array");
				return;		
			}

			// parHandles not copied

			// rangeMax
			json_t* rangeMaxJ = json_object_get(configJ, "rangeMax");
			if (!rangeMaxJ) {
				WARN("PatchMaster missing rangeMax in config, skipping it");
				continue;
			}
			rangeMax[m] = json_number_value(rangeMaxJ);				
	
			// rangeMin
			json_t* rangeMinJ = json_object_get(configJ, "rangeMin");
			if (!rangeMinJ) {
				WARN("PatchMaster missing rangeMin in config, skipping it");
				continue;
			}
			rangeMin[m] = json_number_value(rangeMinJ);
		}
		// lit not copied (but has to be reset)
		lit = 0;
	}
};// struct TileConfig



struct MappedParamQuantity : ParamQuantity {
	ParamHandle* paramHandleMapDest = NULL;
	
	std::string getDescription() override {
		std::string ret = "";
		
		if (paramHandleMapDest) {
			for (int m = 0; m < 4; m++) {
				if (paramHandleMapDest[m].moduleId >= 0) {
					ModuleWidget* mw = APP->scene->rack->getModule(paramHandleMapDest[m].moduleId);
					if (mw) {
						Module* mp = mw->module;
						if (mp) {
							int paramId = paramHandleMapDest[m].paramId;
							if (paramId < (int) mp->params.size()) {
								ParamQuantity* paramQuantity = mp->paramQuantities[paramId];
								if (ret != "") {
									ret += "\n";
								}
								ret += string::f("Map %i: ", m + 1);
								ret += paramQuantity->getDisplayValueString();
								ret += paramQuantity->getUnit();
							}
						}
					}
				}
			}
		}
		
		return ret;
	}
};



struct RangeQuantity : Quantity {
	float* srcRange = NULL;
	bool isMax = true;
	float* srcOldParam = NULL;
	  
	RangeQuantity(float* _srcRange, bool _isMax, float* _srcOldParam) {
		srcRange = _srcRange;
		isMax = _isMax;
		srcOldParam = _srcOldParam;
	}
	void setValue(float value) override {
		*srcRange = math::clamp(value, getMinValue(), getMaxValue());
		*srcOldParam = -1.0f;// force updating of mapped knob in case we are in "on changes only" mode (not thread safe?)
	}
	float getValue() override {
		return *srcRange;
	}
	float getMinValue() override {return 0.0f;}
	float getMaxValue() override {return 1.0f;}
	float getDefaultValue() override {return isMax ? 1.0f : 0.0f;}
	float getDisplayValue() override {return getValue();}
	std::string getDisplayValueString() override {
		float valRange = getDisplayValue();
		return string::f("%.1f", valRange * 100.0f);
	}
	void setDisplayValue(float displayValue) override {setValue(displayValue);}
	std::string getLabel() override {return isMax ? "Max" : "Min";}
	std::string getUnit() override {
		return "%";
	}
};
struct RangeSlider : ui::Slider {
	RangeSlider(float *_srcRange, bool _isMax, float* _srcOldParam) {
		quantity = new RangeQuantity(_srcRange, _isMax, _srcOldParam);
	}
	~RangeSlider() {
		delete quantity;
	}
};




