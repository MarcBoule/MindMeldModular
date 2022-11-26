//***********************************************************************************************
//Configurable multi-controller with parameter mapping
//For VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "../MindMeldModular.hpp"
#include "PatchSetWidgets.hpp"
#include "PatchMasterUtil.hpp"


static const int NUM_CTRL = 8;
static const int NUM_SEP = 8;


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
	uint16_t lit;// bit0 is orders[0], bit1 is orders[1], etc, must have room for NUM_CTRL + NUM_SEP
	// the lit bitfield is indexed by order number, shows lit override resulting from radio buttons (more than one hot when two groups of radio-trig buttons)
	
	
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
		lit = 0;
	}

	void clear() {
		for (int o = 0; o < NUM_CTRL + NUM_SEP; o++) {
			orders[o] = 0;
		}
		lit = 0;
	}

	bool isLit(int o) {
		return (lit & (0x1 << o)) != 0;
	}
	
	void setLitForOrder(int o) {
		lit |= (0x1 << o);
	}
	void clearLitForOrder(int o) {
		lit &= ~(0x1 << o);
	}
	void writeLit(int o, bool val) {
		clearLitForOrder(o);
		if (val) {
			setLitForOrder(o);
		}
	}
	
	void copyFrom(TileOrders* src) {
		for (int o = 0; o < NUM_CTRL + NUM_SEP; o++) {
			orders[o] = src->orders[o];
		}	
		lit = src->lit;
	}

	void moveIndex(int src, int dest) {
		int8_t tmp = orders[src];
		bool tmpLit = isLit(src);
		if (src != dest) {
			if (dest < src) {
				for (int o = src - 1; o >= dest; o--) {
					orders[o + 1] = orders[o];
					writeLit(o + 1, isLit(o));
				}
			}
			else {
				for (int o = src; o < dest; o++) {
					orders[o] = orders[o + 1];
					writeLit(o, isLit(o + 1));
				}	
			}
			orders[dest] = tmp;
			writeLit(dest, tmpLit);
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
	
	
	void deleteLitForOrder(int o) {
		// rightshift lit by 1, but keep bits 0 to o-1 intact if o > 0
		uint16_t newLit = 0;
		for (int i = 0; i < o; i++) {
			newLit |= (lit & (0x1 << i));
		}
		for (int i = o; i < NUM_CTRL + NUM_SEP - 2; i++) {
			newLit |= (lit & (0x1 << (i + 1))) >> 1;
		}
		lit = newLit;
	}
};


//-----------------------------------------------------------------------------


struct PatchMaster : Module {
	
	enum ParamIds {
		ENUMS(TILE_PARAMS, NUM_CTRL),// these params are assumed to be 0.0f to 1.0f, and the first one must be index 0
		NUM_PARAMS
	};
	
	enum InputIds {
		NUM_INPUTS
	};
	
	enum OutputIds {
		NUM_OUTPUTS
	};
	
	enum LightIds {
		ENUMS(TILE_LIGHTS, NUM_CTRL),
		ENUMS(MAP_LIGHTS, NUM_CTRL * 4 * 2), // number of tiles * maps per tile * room for green+red
		NUM_LIGHTS
	};

	// Constants
	NVGcolor paramCol = nvgRGB(0xff, 0xff, 0x40);
	
	// Need to save, no reset
	// none
	
	// Need to save, with reset
	PackedBytes4 miscSettings;
	PackedBytes4 miscSettings2;
	TileInfos tileInfos;// a tile "i" is defined by its tileInfos.infos[i], tileNames.names[i] and when i < NUM_CTRL its tileConfigs[i]
	TileNames tileNames;
	TileConfig tileConfigs[NUM_CTRL];
	TileSettings tileSettings;
	TileOrders tileOrders;// managed by the ModuleWidget, only save/load/init in Module
	
	// No need to save, with reset
	int learningId;
	bool learnedParam;
	int updateControllerLabelsRequest;// 0 when nothing to do, 1 for read meldName in widget
	float oldParams[NUM_CTRL];

	// No need to save, no reset
	RefreshCounter refresh;	
	
	
	PatchMaster() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
				
		for (int tc = 0; tc < NUM_CTRL; tc++) {
			configParam<MappedParamQuantity>(TILE_PARAMS + tc, 0.0f, 1.0f, 0.0f, string::f("Controller %i", tc + 1), "");// all params assumed to be normalized, if not, must update code in createTileChoiceMenuOne()
			((MappedParamQuantity*)paramQuantities[TILE_PARAMS + tc])->paramHandleMapDest = &(tileConfigs[tc].parHandles[0]);
			for (int m = 0; m < 4; m++) {
				tileConfigs[tc].parHandles[m].color = SCHEME_MM_BLUE_LOGO;
				APP->engine->addParamHandle(&(tileConfigs[tc].parHandles[m]));
			}
		}
		
		onReset();
	}

	~PatchMaster() {
		for (int t = 0; t < NUM_CTRL; t++) {
			for (int m = 0; m < 4; m++) {
				APP->engine->removeParamHandle(&(tileConfigs[t].parHandles[m]));
			}
		}
	}

  
	void onReset() override {
		miscSettings.cc4[0] = 0x1;// show knob arcs
		miscSettings.cc4[1] = 0x1;// show mapping lights
		miscSettings.cc4[2] = 0x1;// 0 = eco mode off, 1 = eco level 1 (/4), 2 = eco level 2 (/128)
		miscSettings.cc4[3] = 0x0;// transparent blanks
		miscSettings2.cc4[0] = 0x1;// on changes only
		miscSettings2.cc4[1] = 0x0;// unused
		miscSettings2.cc4[2] = 0x0;// unused
		miscSettings2.cc4[3] = 0x0;// unused
		tileInfos.init();
		tileNames.init();
		for (int cc = 0; cc < NUM_CTRL; cc++) {
			tileConfigs[cc].initAllExceptParHandles();
		}
		tileSettings.init();
		tileOrders.init();
		clearMaps_NoLock();

		resetNonJson();
	}
	void resetNonJson() {
		learningId = -1;
		learnedParam = false;
		updateControllerLabelsRequest = 1;
		for (int cc = 0; cc < NUM_CTRL; cc++) {
			oldParams[cc] = -1.0f;
		}
	}


	void onRandomize() override {
		sanitizeRadios();
	}


	void copyTileToClipboard(int t) {
		// does not copy mappings
		bool isCtrl = t < NUM_CTRL;
		
		json_t* pmTileJ = json_object();
		
		// special isController bool so that pasting can be filtered for proper destination tile type
		json_object_set_new(pmTileJ, "isCtrl", json_boolean(isCtrl));
		
		// tileInfos.infos[t]
		json_object_set_new(pmTileJ, "info", json_integer(tileInfos.infos[t]));
		
		// tileNames.names[t]
		json_object_set_new(pmTileJ, "name", json_string(tileNames.names[t].c_str()));
		
		// tileConfigs[t]
		if (isCtrl) {
			tileConfigs[t].toJson(pmTileJ);
		}
		
		// tileSettings.settings[t]
		json_object_set_new(pmTileJ, "settings", json_integer(tileSettings.settings[t].cc1));

		// clipboard
		json_t* clipboardJ = json_object();		
		json_object_set_new(clipboardJ, "patch-master-tile", pmTileJ);
		char* tileClip = json_dumps(clipboardJ, JSON_INDENT(2) | JSON_REAL_PRECISION(9));
		json_decref(clipboardJ);
		glfwSetClipboardString(APP->window->win, tileClip);
		free(tileClip);
	}


	void pasteTileFromClipboard(int t) {
		const char* tileClip = glfwGetClipboardString(APP->window->win);

		if (!tileClip) {
			WARN("PatchMaster error getting clipboard string");
			return;
		}

		json_error_t error;
		json_t* clipboardJ = json_loads(tileClip, 0, &error);
		if (!clipboardJ) {
			WARN("PatchMaster error json parsing clipboard");
			return;
		}
		DEFER({json_decref(clipboardJ);});

		json_t* pmTileJ = json_object_get(clipboardJ, "patch-master-tile");
		if (!pmTileJ) {
			WARN("PatchMaster error no patch-master-tile present in clipboard");
			return;
		}
		
		// special isController bool so that pasting can be filtered for proper destination tile type
		bool isCtrlDest = t < NUM_CTRL;
		json_t* jsonIsCtrl = json_object_get(pmTileJ, "isCtrl");
		if (!jsonIsCtrl) {
			WARN("PatchMaster error patch-master-tile isCtrl missing");
			return;
		}
		bool isCtrlSrc = json_is_true(jsonIsCtrl);
		if (isCtrlDest != isCtrlSrc) {
			WARN("PatchMaster error patch-master-tile wrong type for selected operation");
			return;
		}
		
		// tileInfos.infos[t]
		json_t* jsonInfo = json_object_get(pmTileJ, "info");
		if (!jsonInfo) {
			WARN("PatchMaster error patch-master-tile info missing");
			return;
		}
		tileInfos.infos[t] = json_integer_value(jsonInfo);

		// tileNames.names[t]
		json_t* jsonName = json_object_get(pmTileJ, "name");
		if (!jsonName) {
			WARN("PatchMaster error patch-master-tile name missing");
			return;
		}
		tileNames.names[t] = json_string_value(jsonName);
		updateControllerLabelsRequest = 1;

		// tileConfigs[t]
		if (isCtrlDest) {
			tileConfigs[t].fromJson(pmTileJ);
		}
		
		// tileSettings.settings[t]
		json_t *tileSettingsJ = json_object_get(pmTileJ, "settings");
		if (tileSettingsJ) {
			tileSettings.settings[t].cc1 = json_integer_value(tileSettingsJ);
		}
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// miscSettings
		json_object_set_new(rootJ, "miscSettings", json_integer(miscSettings.cc1));

		// miscSettings2
		json_object_set_new(rootJ, "miscSettings2", json_integer(miscSettings2.cc1));

		// tileInfos
		json_t *tileInfosJ = json_array();
		for (int i = 0; i < NUM_CTRL + NUM_SEP; i++) {
			json_array_insert_new(tileInfosJ, i, json_integer(tileInfos.infos[i]));
		}	
		json_object_set_new(rootJ, "tileInfos", tileInfosJ);

		// tileNames
		json_t* tileNamesJ = json_array();
		for (int n = 0; n < NUM_CTRL + NUM_SEP; n++) {
			json_array_insert_new(tileNamesJ, n, json_string(tileNames.names[n].c_str()));
		}
		json_object_set_new(rootJ, "tileNames", tileNamesJ);
		
		// TileConfigs[].parHandles[]
		json_t* mapsJ = json_array();
		for (int cc = 0; cc < NUM_CTRL; cc++) {
			for (int m = 0; m < 4; m++) {
				json_t* mapJ = json_object();
				json_object_set_new(mapJ, "moduleId", json_integer(tileConfigs[cc].parHandles[m].moduleId));
				json_object_set_new(mapJ, "paramId", json_integer(tileConfigs[cc].parHandles[m].paramId));
				json_object_set_new(mapJ, "rangeMax", json_real(tileConfigs[cc].rangeMax[m]));
				json_object_set_new(mapJ, "rangeMin", json_real(tileConfigs[cc].rangeMin[m]));
				json_array_append_new(mapsJ, mapJ);
			}
		}
		json_object_set_new(rootJ, "maps", mapsJ);

		// tileOrders
		json_t *tileOrdersJ = json_array();
		for (int o = 0; o < NUM_CTRL + NUM_SEP; o++) {
			json_array_insert_new(tileOrdersJ, o, json_integer(tileOrders.orders[o]));
		}	
		json_object_set_new(rootJ, "tileOrders", tileOrdersJ);
		// tileSettings
		json_t *tileSettingsJ = json_array();
		for (int s = 0; s < NUM_CTRL + NUM_SEP; s++) {
			json_array_insert_new(tileSettingsJ, s, json_integer(tileSettings.settings[s].cc1));
		}	
		json_object_set_new(rootJ, "tileSettings", tileSettingsJ);

		// radioLit
		json_object_set_new(rootJ, "radioLit", json_integer(tileOrders.lit));

		return rootJ;
	}


	void dataFromJson(json_t *rootJ) override {
		clearMaps_NoLock();

		// miscSettings
		json_t *miscSettingsJ = json_object_get(rootJ, "miscSettings");
		if (miscSettingsJ) {
			miscSettings.cc1 = json_integer_value(miscSettingsJ);
		}

		// miscSettings2
		json_t *miscSettings2J = json_object_get(rootJ, "miscSettings2");
		if (miscSettings2J) {
			miscSettings2.cc1 = json_integer_value(miscSettings2J);
		}

		// tileInfos
		json_t *tileInfosJ = json_object_get(rootJ, "tileInfos");
		if (tileInfosJ) {
			for (int i = 0; i < NUM_CTRL + NUM_SEP; i++) {
				json_t *tileInfosArrayJ = json_array_get(tileInfosJ, i);
				if (tileInfosArrayJ)
					tileInfos.infos[i] = json_integer_value(tileInfosArrayJ);
			}
		}
	
		// tileNames
		json_t* tileNamesJ = json_object_get(rootJ, "tileNames");
		if (tileNamesJ) {
			for (int n = 0; n < NUM_CTRL + NUM_SEP; n++) {
				json_t *tileNamesArrayJ = json_array_get(tileNamesJ, n);
				if (tileNamesArrayJ)
					tileNames.names[n] = json_string_value(tileNamesArrayJ);
			}
		}

		// TileConfigs[].parHandles[]
		json_t* mapsJ = json_object_get(rootJ, "maps");
		if (mapsJ) {
			json_t* mapJ;
			size_t mapIndex;
			json_array_foreach(mapsJ, mapIndex, mapJ) {
				json_t* moduleIdJ = json_object_get(mapJ, "moduleId");
				json_t* paramIdJ = json_object_get(mapJ, "paramId");
				json_t* rangeMaxJ = json_object_get(mapJ, "rangeMax");
				json_t* rangeMinJ = json_object_get(mapJ, "rangeMin");
				if (!(moduleIdJ && paramIdJ && rangeMaxJ && rangeMinJ))
					continue;
				tileConfigs[mapIndex / 4].rangeMax[mapIndex % 4] = json_number_value(rangeMaxJ);
				tileConfigs[mapIndex / 4].rangeMin[mapIndex % 4] = json_number_value(rangeMinJ);
				int64_t moduleId = json_integer_value(moduleIdJ);
				int paramId = json_integer_value(paramIdJ);
				if (mapIndex >= NUM_CTRL * 4 || moduleId < 0)
					continue;
				APP->engine->updateParamHandle_NoLock(&(tileConfigs[mapIndex / 4].parHandles[mapIndex % 4]), moduleId, paramId, false);
			}
		}
		
		// tileOrders
		json_t *tileOrdersJ = json_object_get(rootJ, "tileOrders");
		if (tileOrdersJ) {
			for (int o = 0; o < NUM_CTRL + NUM_SEP; o++) {
				json_t *tileOrdersArrayJ = json_array_get(tileOrdersJ, o);
				if (tileOrdersArrayJ)
					tileOrders.orders[o] = json_integer_value(tileOrdersArrayJ);
			}
		}
		// tileSettings
		json_t *tileSettingsJ = json_object_get(rootJ, "tileSettings");
		if (tileSettingsJ) {
			for (int s = 0; s < NUM_CTRL + NUM_SEP; s++) {
				json_t *tileSettingsArrayJ = json_array_get(tileSettingsJ, s);
				if (tileSettingsArrayJ)
					tileSettings.settings[s].cc1 = json_integer_value(tileSettingsArrayJ);
			}
		}
	
		// radioLit
		json_t *radioLitJ = json_object_get(rootJ, "radioLit");
		if (radioLitJ) {
			tileOrders.lit = json_integer_value(radioLitJ);
		}
	
		resetNonJson();	
	}


	void onSampleRateChange() override {
	}
	

	void process(const ProcessArgs &args) override {
		
		// Inputs and slow
		if (refresh.processInputs()) {
		}// userInputs refresh
		
		
		// Outputs
		// Step each controller tile and its 4 maps
		for (int cc = 0; cc < NUM_CTRL; cc++) {
			if (miscSettings.cc4[2] == 1 && (cc & 0x3) != (refresh.refreshCounter & 0x3)) {
				// SR / 4
				continue;
			}
			else if (miscSettings.cc4[2] == 2 && (cc & 0x7F) != (refresh.refreshCounter & 0x7F)) {
				// SR / 128
				continue;
			}
			else if (miscSettings2.cc4[0] != 0 && oldParams[cc] == params[cc].getValue()) {
				// On changes only
				continue;
			}
			for (int m = 0; m < 4; m++) {
				Module* module = tileConfigs[cc].parHandles[m].module;
				if (module) {
					int paramId = tileConfigs[cc].parHandles[m].paramId;
					ParamQuantity* paramQuantity = module->paramQuantities[paramId];
					if (paramQuantity && paramQuantity->isBounded()) {
						float value = math::rescale(params[cc].getValue(), 0.0f, 1.0f, tileConfigs[cc].rangeMin[m], tileConfigs[cc].rangeMax[m]);
						paramQuantity->setScaledValue(value);
					}
				}
			}
			oldParams[cc] = params[cc].getValue();// used when "On changes only" is active
		}
		

		// Lights
		if (refresh.processLights()) {
			// see module's widget
		}
	}// process()
	
	
	void commitLearn() {
		if (learningId < 0)
			return;
		if (!learnedParam)
			return;
		// Reset learned state
		learnedParam = false;
		learningId = -1;
	}
	void enableLearn(int id) {
		if (learningId != id) {
			learningId = id;
			learnedParam = false;
		}
	}

	void disableLearn(int id) {
		if (learningId == id) {
			learningId = -1;
		}
	}

	void learnParam(int id, int64_t moduleId, int paramId) {
		APP->engine->updateParamHandle(&(tileConfigs[id / 4].parHandles[id % 4]), moduleId, paramId, true);
		learnedParam = true;
		commitLearn();
		// updateMapLen();
		oldParams[id / 4] = -1.0f;// force updating of new mapped knob in case we are in "on changes only" mode (not thread safe?)
	}

	void clearMap(int id) {
		learningId = -1;
		APP->engine->updateParamHandle(&(tileConfigs[id / 4].parHandles[id % 4]), -1, 0, true);
	}

	void clearMaps_NoLock() {
		learningId = -1;
		for (int cc = 0; cc < NUM_CTRL; cc++) {
			for (int m = 0; m < 4; m++) {
				APP->engine->updateParamHandle_NoLock(&(tileConfigs[cc].parHandles[m]), -1, 0, true);
			}
		}
	}

	std::string getParamName(int id) {
		// this method is by Andrew Belt, in MIDIMap.cpp
		ParamHandle* paramHandle = &(tileConfigs[id / 4].parHandles[id % 4]);
		if (paramHandle->moduleId < 0)
			return "";
		ModuleWidget* mw = APP->scene->rack->getModule(paramHandle->moduleId);
		if (!mw)
			return "";
		// Get the Module from the ModuleWidget instead of the ParamHandle.
		// I think this is more elegant since this method is called in the app world instead of the engine world.
		Module* m = mw->module;
		if (!m)
			return "";
		int paramId = paramHandle->paramId;
		if (paramId >= (int) m->params.size())
			return "";
		ParamQuantity* paramQuantity = m->paramQuantities[paramId];
		std::string s;
		s += paramQuantity->name;
		s += " (";
		s += mw->model->name;
		s += ")";
		return s;
	}	
	
	int modifyOrCreateNewTile(int t, int order, uint8_t info, float defPar) {
		// returns order (will ne a new order when create)
		int retOrder = order;
		
		if (order == -1) {
			// create new tile
			for (int o = 0; o < NUM_CTRL + NUM_SEP; o++) {
				if (tileOrders.orders[o] == -1) {
					tileInfos.infos[t] = TI_VIS_MASK;
					if (o < NUM_CTRL + NUM_SEP - 1) {
						tileOrders.orders[o + 1] = -1;
					}
					tileOrders.orders[o] = t;
					tileOrders.clearLitForOrder(o);
					
					if (t >= NUM_CTRL) {
						tileNames.names[t] = string::f("Separator %i", t - NUM_CTRL + 1);
					}
					else {
						tileConfigs[t].initAllExceptParHandles();
						for (int m = 0; m < 4; m++) {
							clearMap(t * 4 + m);// parHandles
						}
						tileNames.names[t] = string::f("Controller %i", t + 1);
					}
					
					updateControllerLabelsRequest = 1;
					retOrder = o;
					break;
				}
			}
		}
		tileInfos.infos[t] &= ~(TI_TYPE_MASK | TI_SIZE_MASK);
		tileInfos.infos[t] |= info;
		if (t < NUM_CTRL) {
			params[t].setValue(defPar);
		}
		
		return retOrder;
	}
	
	void sanitizeRadios() {
		// this will ensure exactly one lit bit is set in each group (group is a set of consecutive TT_BUTN_RL or TT_BUTN_RT)
		// useful after a deleteOrder() or when an existing tile type has changed (that merges two groups for example)
		//   as a result of a tile paste, or tile type changed in tile menu
		
		// do radio trigs; this entails managing lit bits in tileOrders
		int lastGroupStart = -1;
		bool hasLit = false;
		for (int o = 0; o < NUM_CTRL + NUM_SEP; o++) {
			int8_t t = tileOrders.orders[o];
			if (t == -1) {
				break;
			}
			uint8_t ti = tileInfos.infos[t];
			if ((ti & TI_TYPE_MASK) == TT_BUTN_RT) {
				if (lastGroupStart == -1) {
					lastGroupStart = o;
					hasLit = false;
				}
				if (tileOrders.isLit(o)) {
					if (hasLit) {
						 tileOrders.clearLitForOrder(o);
					}
					else {
						hasLit = true;
					}
				}
			}
			else {
				if (lastGroupStart != -1 && !hasLit) {
					tileOrders.setLitForOrder(lastGroupStart);
				}
				lastGroupStart = -1;
				hasLit = false;
				tileOrders.clearLitForOrder(o);// make sure to kill lit bit for non-TT_BUTN_RT, in case bug happens 
			}
		}
		if (lastGroupStart != -1 && !hasLit) {
			tileOrders.setLitForOrder(lastGroupStart);
		}		
		
		// do radio latches; this entails managing params in module
		lastGroupStart = -1;
		hasLit = false;
		for (int o = 0; o < NUM_CTRL + NUM_SEP; o++) {
			int8_t t = tileOrders.orders[o];
			if (t == -1) {
				break;
			}
			uint8_t ti = tileInfos.infos[t];
			if ((ti & TI_TYPE_MASK) == TT_BUTN_RL) {
				if (lastGroupStart == -1) {
					lastGroupStart = t;
					hasLit = false;
				}
				if (params[t].getValue() >= 0.5f) {
					if (hasLit) {
						params[t].setValue(0.0f);
					}
					else {
						hasLit = true;
					}
				}
			}
			else {
				if (lastGroupStart != -1 && !hasLit) {
					params[lastGroupStart].setValue(1.0f);
				}
				lastGroupStart = -1;
				hasLit = false;
			}
		}
		if (lastGroupStart != -1 && !hasLit) {
			params[lastGroupStart].setValue(1.0f);
		}			
	}
};// module


//-----------------------------------------------------------------------------


struct TileChoiceItem : MenuItem {
	PatchMaster* module = NULL;
	int t = -1;
	int order = -1;
	uint8_t info = 0;
	float defPar = 0.0f;
	
	void construct(PatchMaster* _module, int _t, int _o, uint8_t _info, float _defPar = 0.0f) {
		module = _module;
		t = _t;
		order = _o;
		info = _info;
		defPar = _defPar;
	}
	
	void onAction(const event::Action &e) override {
		module->modifyOrCreateNewTile(t, order, info, defPar);
		if (order == -1) {
			e.consume(this);// don't allow ctrl-click to keep menu open when isCreate
		}
		else {
			// changing existing tile
			module->tileOrders.clearLitForOrder(order);
			// needs sanitize (necessary when changing tile)
			module->sanitizeRadios();
		}
	}
	
	void step() override {
		rightText = CHECKMARK(order != -1 && (module->tileInfos.infos[t] & (TI_TYPE_MASK | TI_SIZE_MASK)) == info);
		MenuItem::step();
	}

};


void createControllerChoiceMenuOne(ui::Menu* menu, int t, int o, PatchMaster* module) {
	// when o == -1, it means new tile must be created and added at the end of orders (already know the tile number, which caller must find and give as parameter t, and already know that there is enough space in orders, since caller has to check this), else means only changing an existing tile t
	// this only sets visible when creating new tile (i.e. o == -1)	
	
	// KNOBS
	// --------
	menu->addChild(createSubmenuItem("Knobs", "",
		[=](Menu* menu) {			
			menu->addChild(createSubmenuItem("Small", "",
				[=](Menu* menu) {
					TileChoiceItem *newItem;
					newItem = createMenuItem<TileChoiceItem>("Unipolar", "");
					newItem->construct(module, t, o, TT_KNOB_L | TS_SMALL, 0.0f);
					menu->addChild(newItem);
					newItem = createMenuItem<TileChoiceItem>("Bipolar", "");
					newItem->construct(module, t, o, TT_KNOB_C | TS_SMALL, 0.5f);
					menu->addChild(newItem);
				}
			));
			
			menu->addChild(createSubmenuItem("Medium", "",
				[=](Menu* menu) {
					TileChoiceItem *newItem;
					newItem = createMenuItem<TileChoiceItem>("Unipolar", "");
					newItem->construct(module, t, o, TT_KNOB_L | TS_MEDIUM, 0.0f);
					menu->addChild(newItem);
					newItem = createMenuItem<TileChoiceItem>("Bipolar", "");
					newItem->construct(module, t, o, TT_KNOB_C | TS_MEDIUM, 0.5f);
					menu->addChild(newItem);
				}
			));
			
			menu->addChild(createSubmenuItem("Large", "",
				[=](Menu* menu) {
					TileChoiceItem *newItem;
					newItem = createMenuItem<TileChoiceItem>("Unipolar", "");
					newItem->construct(module, t, o, TT_KNOB_L | TS_LARGE, 0.0f);
					menu->addChild(newItem);
					newItem = createMenuItem<TileChoiceItem>("Bipolar", "");
					newItem->construct(module, t, o, TT_KNOB_C | TS_LARGE, 0.5f);
					menu->addChild(newItem);
				}
			));
		}
	));
	
	
	// BUTTONS
	// --------
	menu->addChild(createSubmenuItem("Buttons", "",
		[=](Menu* menu) {			
			menu->addChild(createSubmenuItem("Small", "",
				[=](Menu* menu) {
					TileChoiceItem *newItem;
					newItem = createMenuItem<TileChoiceItem>("Momentary", "");
					newItem->construct(module, t, o, TT_BUTN_M | TS_SMALL, 0.0f);
					menu->addChild(newItem);
					newItem = createMenuItem<TileChoiceItem>("Latched", "");
					newItem->construct(module, t, o, TT_BUTN_N | TS_SMALL, 0.0f);
					menu->addChild(newItem);
					newItem = createMenuItem<TileChoiceItem>("Latched (inverted light)", "");
					newItem->construct(module, t, o, TT_BUTN_I | TS_SMALL, 0.0f);
					menu->addChild(newItem);
					newItem = createMenuItem<TileChoiceItem>("Radio button trig", "");
					newItem->construct(module, t, o, TT_BUTN_RT | TS_SMALL, 0.0f);
					menu->addChild(newItem);
					newItem = createMenuItem<TileChoiceItem>("Radio button latched", "");
					newItem->construct(module, t, o, TT_BUTN_RL | TS_SMALL, 0.0f);
					menu->addChild(newItem);
				}
			));
							
			menu->addChild(createSubmenuItem("Medium", "",
				[=](Menu* menu) {
					TileChoiceItem *newItem;
					newItem = createMenuItem<TileChoiceItem>("Momentary", "");
					newItem->construct(module, t, o, TT_BUTN_M | TS_MEDIUM, 0.0f);
					menu->addChild(newItem);
					newItem = createMenuItem<TileChoiceItem>("Latched", "");
					newItem->construct(module, t, o, TT_BUTN_N | TS_MEDIUM, 0.0f);
					menu->addChild(newItem);
					newItem = createMenuItem<TileChoiceItem>("Latched (inverted light)", "");
					newItem->construct(module, t, o, TT_BUTN_I | TS_MEDIUM, 0.0f);
					menu->addChild(newItem);
					newItem = createMenuItem<TileChoiceItem>("Radio button trig", "");
					newItem->construct(module, t, o, TT_BUTN_RT | TS_MEDIUM, 0.0f);
					menu->addChild(newItem);
					newItem = createMenuItem<TileChoiceItem>("Radio button latched", "");
					newItem->construct(module, t, o, TT_BUTN_RL | TS_MEDIUM, 0.0f);
					menu->addChild(newItem);
				}
			));
							
			menu->addChild(createSubmenuItem("Large", "",
				[=](Menu* menu) {
					TileChoiceItem *newItem;
					newItem = createMenuItem<TileChoiceItem>("Momentary", "");
					newItem->construct(module, t, o, TT_BUTN_M | TS_LARGE, 0.0f);
					menu->addChild(newItem);
					newItem = createMenuItem<TileChoiceItem>("Latched", "");
					newItem->construct(module, t, o, TT_BUTN_N | TS_LARGE, 0.0f);
					menu->addChild(newItem);
					newItem = createMenuItem<TileChoiceItem>("Latched (inverted light)", "");
					newItem->construct(module, t, o, TT_BUTN_I | TS_LARGE, 0.0f);
					menu->addChild(newItem);
					newItem = createMenuItem<TileChoiceItem>("Radio button trig", "");
					newItem->construct(module, t, o, TT_BUTN_RT | TS_LARGE, 0.0f);
					menu->addChild(newItem);
					newItem = createMenuItem<TileChoiceItem>("Radio button latched", "");
					newItem->construct(module, t, o, TT_BUTN_RL | TS_LARGE, 0.0f);
					menu->addChild(newItem);
				}
			));
		}
	));
	
	// FADERS
	// --------
	menu->addChild(createSubmenuItem("Faders", "",
		[=](Menu* menu) {			
			menu->addChild(createSubmenuItem("Large", "",
				[=](Menu* menu) {
					TileChoiceItem *newItem;
					newItem = createMenuItem<TileChoiceItem>("Unipolar", "");
					newItem->construct(module, t, o, TT_FADER_B | TS_LARGE, 0.0f);
					menu->addChild(newItem);
					newItem = createMenuItem<TileChoiceItem>("Bipolar", "");
					newItem->construct(module, t, o, TT_FADER_C | TS_LARGE, 0.5f);
					menu->addChild(newItem);
				}
			));
			menu->addChild(createSubmenuItem("X-Large", "",
				[=](Menu* menu) {
					TileChoiceItem *newItem;
					newItem = createMenuItem<TileChoiceItem>("Unipolar", "");
					newItem->construct(module, t, o, TT_FADER_B | TS_XLARGE, 0.0f);
					menu->addChild(newItem);
					newItem = createMenuItem<TileChoiceItem>("Bipolar", "");
					newItem->construct(module, t, o, TT_FADER_C | TS_XLARGE, 0.5f);
					menu->addChild(newItem);
				}
			));			
			menu->addChild(createSubmenuItem("XX-Large", "",
				[=](Menu* menu) {
					TileChoiceItem *newItem;
					newItem = createMenuItem<TileChoiceItem>("Unipolar", "");
					newItem->construct(module, t, o, TT_FADER_B | TS_XXLARGE, 0.0f);
					menu->addChild(newItem);
					newItem = createMenuItem<TileChoiceItem>("Bipolar", "");
					newItem->construct(module, t, o, TT_FADER_C | TS_XXLARGE, 0.5f);
					menu->addChild(newItem);
				}
			));
		}
	));		
}


void createSeparatorChoiceMenuOne(ui::Menu* menu, int t, int o, PatchMaster* module) {
	// when o == -1, it means new tile must be created and added at the end of orders (already know the tile number, which caller must find and give as parameter t, and already know that there is enough space in orders, since caller has to check this), else means only changing an existing tile t
	// this only sets visible when creating a new tile (o == -1)
	
	TileChoiceItem *newItem;
		
	newItem = createMenuItem<TileChoiceItem>("Divider label", "");
	newItem->construct(module, t, o, TS_XSMALL | TT_SEP);
	menu->addChild(newItem);
	
	menu->addChild(createSubmenuItem("Blank", "",
		[=](Menu* menu) {			
			TileChoiceItem *newItem;
			newItem = createMenuItem<TileChoiceItem>("XX-Small", "");
			newItem->construct(module, t, o, TS_XXSMALL | TT_BLANK);
			menu->addChild(newItem);
			
			newItem = createMenuItem<TileChoiceItem>("X-Small", "");
			newItem->construct(module, t, o, TS_XSMALL | TT_BLANK);
			menu->addChild(newItem);
			
			newItem = createMenuItem<TileChoiceItem>("Smaller", "");
			newItem->construct(module, t, o, TS_SMALLER | TT_BLANK);
			menu->addChild(newItem);
			
			newItem = createMenuItem<TileChoiceItem>("Small", "");
			newItem->construct(module, t, o, TS_SMALL | TT_BLANK);
			menu->addChild(newItem);
			
			newItem = createMenuItem<TileChoiceItem>("Medium", "");
			newItem->construct(module, t, o, TS_MEDIUM | TT_BLANK);
			menu->addChild(newItem);
			
			newItem = createMenuItem<TileChoiceItem>("Large", "");
			newItem->construct(module, t, o, TS_LARGE | TT_BLANK);
			menu->addChild(newItem);
		}
	));
}


struct PmBgBase : SvgWidget {
	PatchMaster* module = NULL;
	PmBgBase** tileBackgrounds = NULL;
	int8_t* highlightTileMoveDest = NULL;
	int8_t* highlightTileMoveDir = NULL;
	int tileNumber = 0;
	int tileOrder = 0;
	int* learningMapSrc = NULL;// stays NULL when tileNumber >= NUM_CTRL
	float manualBgHeight = 0.0f;// need extra special height for manual tiles so as to not mess with box.size.y, since that is needed for proper tile moving outlines (and proper intertile space for right clickign the module bg to get module menu)
	
	// local
	bool highlightTileMoveSrc = false;
	float onButtonMouseY = 0.0f;// Y pixel position of button click in rack space, used for a delta in onDragMove
	float onButtonPosY = 0.0f;// Y pixel position of button click in module; 0 = top, 380 = bot
	

	// applies only to controller tiles (when tileNumber < NUM_CTRL)
	struct MapHeaderItem : MenuItem {
		PatchMaster* module = NULL;
		int tileNumber = 0;
		int mapNumber = 0;
		
		void step() override {
			std::string mapHeader = module->getParamName(tileNumber * 4 + mapNumber);
			if (mapHeader.empty()) {
				mapHeader = "[No mapping set]";
			}			
			// mapHeader.insert(0, string::f("MAP %i: ", mapNumber + 1));
			text = mapHeader;
			MenuItem::step();
		}
	};

	// applies only to controller tiles (when tileNumber < NUM_CTRL)
	struct StartMappingItem : MenuItem {
		PatchMaster* module = NULL;
		PmBgBase** tileBackgrounds = NULL;
		int* learningMapSrc = NULL;
		int tileNumber = 0;
		int mapNumber = 0;
		
		void onAction(const event::Action &e) override {
			int id = tileNumber * 4 + mapNumber;
			module->clearMap(id);
			APP->scene->rack->touchedParam = NULL;
			module->enableLearn(id);
			*learningMapSrc = id;
			if (tileBackgrounds[tileNumber] != NULL) {
				APP->event->setSelectedWidget(tileBackgrounds[tileNumber]);
			}
			e.consume(this);// don't allow ctrl-click to keep menu open
		}

		void step() override {
			uint8_t ti = module->tileInfos.infos[tileNumber];
			disabled = ((ti & TI_TYPE_MASK) == TT_SEP) || ((ti & TI_TYPE_MASK) == TT_BLANK) || !isTileVisible(ti);
		}

	};
	
	// applies only to controller tiles (when tileNumber < NUM_CTRL)
	struct UnmapItem : MenuItem {
		PatchMaster* module = NULL;
		int tileNumber = 0;
		int mapNumber = 0;
		
		void onAction(const event::Action &e) override {
			module->clearMap(tileNumber * 4 + mapNumber);
		}
		
		void step() override {
			disabled = module->tileConfigs[tileNumber].parHandles[mapNumber].moduleId < 0;
		}
	};

	// applies only to controller tiles (when tileNumber < NUM_CTRL)
	struct RangeItem : MenuItem {
		PatchMaster* module = NULL;
		int tileNumber = 0;
		int mapNumber = 0;

		Menu *createChildMenu() override {
			Menu *menu = new Menu;
			
			// menu->addChild(createMenuLabel("Parameter range:"));
			
			RangeSlider *rangeMinSlider = new RangeSlider(&(module->tileConfigs[tileNumber].rangeMin[mapNumber]), false, &(module->oldParams[tileNumber]));
			rangeMinSlider->box.size.x = 200.0f;
			menu->addChild(rangeMinSlider);

			RangeSlider *rangeMaxSlider = new RangeSlider(&(module->tileConfigs[tileNumber].rangeMax[mapNumber]), true, &(module->oldParams[tileNumber]));
			rangeMaxSlider->box.size.x = 200.0f;
			menu->addChild(rangeMaxSlider);
			
			menu->addChild(createSubmenuItem("Range presets", "",
				[=](Menu* menu) {
					menu->addChild(createMenuItem("Default", "",
						[=]() {
							module->tileConfigs[tileNumber].rangeMax[mapNumber] = 1.0f;
							module->tileConfigs[tileNumber].rangeMin[mapNumber] = 0.0f;
							module->oldParams[tileNumber] = -1.0f;// force updating of mapped knob in case we are in "on changes only" mode (not thread safe?)
						}
					));
					menu->addChild(createMenuItem("Invert", "",
						[=]() {
							module->tileConfigs[tileNumber].rangeMax[mapNumber] = 0.0f;
							module->tileConfigs[tileNumber].rangeMin[mapNumber] = 1.0f;
							module->oldParams[tileNumber] = -1.0f;// force updating of mapped knob in case we are in "on changes only" mode (not thread safe?)
						}
					));
					menu->addChild(createMenuItem("0-50%", "",
						[=]() {
							module->tileConfigs[tileNumber].rangeMin[mapNumber] = 0.0f;
							module->tileConfigs[tileNumber].rangeMax[mapNumber] = 0.5f;
							module->oldParams[tileNumber] = -1.0f;// force updating of mapped knob in case we are in "on changes only" mode (not thread safe?)
						}
					));
					menu->addChild(createMenuItem("25-75%", "",
						[=]() {
							module->tileConfigs[tileNumber].rangeMin[mapNumber] = 0.25f;
							module->tileConfigs[tileNumber].rangeMax[mapNumber] = 0.75f;
							module->oldParams[tileNumber] = -1.0f;// force updating of mapped knob in case we are in "on changes only" mode (not thread safe?)
						}
					));
					menu->addChild(createMenuItem("50-100%", "",
						[=]() {
							module->tileConfigs[tileNumber].rangeMin[mapNumber] = 0.5f;
							module->tileConfigs[tileNumber].rangeMax[mapNumber] = 1.0f;
							module->oldParams[tileNumber] = -1.0f;// force updating of mapped knob in case we are in "on changes only" mode (not thread safe?)
						}
					));
				}
			));
			return menu;
		}
		
		void step() override {
			uint8_t ti = module->tileInfos.infos[tileNumber];
			disabled = ((ti & TI_TYPE_MASK) == TT_SEP) || ((ti & TI_TYPE_MASK) == TT_BLANK) || !isTileVisible(ti) || module->tileConfigs[tileNumber].parHandles[mapNumber].moduleId < 0;
		}
	};

	// applies only to controller tiles (when tileNumber < NUM_CTRL)
	void onDeselect(const DeselectEvent& e) override {
		if (learningMapSrc && tileNumber < NUM_CTRL) {
			// Check if a ParamWidget was touched
			ParamWidget* touchedParam = APP->scene->rack->touchedParam;
			int id = *learningMapSrc;
			if (touchedParam && id != -1 && (module->learningId == id)) {
				APP->scene->rack->touchedParam = NULL;
				int64_t moduleId = touchedParam->module->id;
				int paramId = touchedParam->paramId;
				module->learnParam(id, moduleId, paramId);
			}
			else {
				module->disableLearn(id);
			}
		}
	}	
	
	// code below adapted from ParamWidget::ParamField() by Andrew Belt
	struct TileNameValueField : ui::TextField {
		PatchMaster* module = NULL;
		int tileNumber = 0;

		TileNameValueField(PatchMaster* _module, int _tileNumber) {
			module = _module;
			tileNumber = _tileNumber;
			text = module->tileNames.names[tileNumber];
			selectAll();
		}

		void step() override {
			// Keep selected
			APP->event->setSelectedWidget(this);
			TextField::step();
		}

		void onSelectKey(const event::SelectKey& e) override {
			if (e.action == GLFW_RELEASE) {// && (e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_KP_ENTER)) {
				module->tileNames.names[tileNumber] = text;
				module->updateControllerLabelsRequest = 1;
				if (e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_KP_ENTER) {
					ui::MenuOverlay* overlay = getAncestorOfType<ui::MenuOverlay>();
					overlay->requestDelete();
					e.consume(this);
				}
			}

			if (!e.getTarget())
				TextField::onSelectKey(e);
		}
	};
	

	struct MoveSubItem : MenuItem {
		PatchMaster* module = NULL;
		int tileOrderFrom = 0;
		int tileOrderTo = 0;

		void onAction(const event::Action &e) override {
			module->tileOrders.moveIndex(tileOrderFrom, tileOrderTo);
			module->sanitizeRadios();
			e.consume(this);// don't allow ctrl-click to keep menu open
		}

	};
	struct MoveItem : MenuItem {
		PatchMaster* module = NULL;
		int tileOrder = 0;

		Menu *createChildMenu() override {
			Menu *menu = new Menu;

			for (int tview = 0; tview < NUM_CTRL + NUM_SEP; tview++) {
				int t = module->tileOrders.orders[tview];
				if (isTileVisible(module->tileInfos.infos[t])) {
					MoveSubItem *movesubItem = createMenuItem<MoveSubItem>(module->tileNames.names[t], "");
					movesubItem->module = module;
					movesubItem->tileOrderFrom = tileOrder;
					movesubItem->tileOrderTo = tview;
					movesubItem->disabled = tview == tileOrder;
					menu->addChild(movesubItem);
				}
			}

			return menu;
		}
	};
	
	struct TileHideItem : MenuItem {
		uint8_t* tileInfo = NULL;
		void onAction(const event::Action &e) override {
			clearVisible(tileInfo);
			e.consume(this);// don't allow ctrl-click to keep menu open
		}
	};
	
	struct DeleteTileItem : MenuItem {
		PatchMaster* module = NULL;
		int tileOrder = 0;
		void onAction(const event::Action &e) override {
			// reset maps of the tile number in question if it's a controller
			int td = module->tileOrders.orders[tileOrder];// tile to delete
			if (td < NUM_CTRL) {
				module->tileConfigs[td].initAllExceptParHandles();
				for (int m = 0; m < 4; m++) {
					if (module->tileConfigs[td].parHandles[m].moduleId >= 0) {
						module->clearMap(td * 4 + m);// parHandles
					}
				}					
			}
			// now remove it in the orders
			int o = tileOrder + 1;
			for ( ; o < NUM_CTRL + NUM_SEP; o++) {
				module->tileOrders.orders[o - 1] = module->tileOrders.orders[o];
				if (module->tileOrders.orders[o] == -1) {
					break;
				}
			}
			module->tileOrders.orders[o - 1] = -1;// only really needed when deleting a full orders array (with no -1 termination)
			module->tileOrders.deleteLitForOrder(tileOrder);
			// at this point two separarate radio groups could have been merged, so needs sanitizing
			module->sanitizeRadios();
			e.consume(this);// don't allow ctrl-click to keep menu open
		}
	};

	struct DuplicateControllerItem : MenuItem {
		PatchMaster* module = NULL;
		int tcSrc;// tile number of current controller we wish to duplicate
		int ocSrc;// tile order of current controller we wish to duplicate
		int tcNew;// new tile number of potential controller

		void onAction(const event::Action &e) override {
			// create duplicate tile at end
			uint8_t info = module->tileInfos.infos[tcSrc];
			float defPar = module->paramQuantities[PatchMaster::TILE_PARAMS + tcSrc]->defaultValue;
			int ocNew = module->modifyOrCreateNewTile(tcNew, -1, info, defPar);// -1 means create
			
			// now move it if needed
			if (ocNew > ocSrc + 1) {
				module->tileOrders.moveIndex(ocNew, ocSrc + 1);
				// module->sanitizeRadios(); this should not be needed
			}
			e.consume(this);// don't allow ctrl-click to keep menu open when duplicate controller
		}		
	};
	
	struct DuplicateSeparatorItem : MenuItem {
		PatchMaster* module = NULL;
		int tcSrc;// tile number of current separator we wish to duplicate
		int ocSrc;// tile order of current separator we wish to duplicate
		int tsNew;// new tile number of potential separator

		void onAction(const event::Action &e) override {
			// create duplicate tile at end
			uint8_t info = module->tileInfos.infos[tcSrc];
			int ocNew = module->modifyOrCreateNewTile(tsNew, -1, info, 0.0f);// -1 means create, 0.0f is unused
			
			// now move it if needed
			if (ocNew > ocSrc + 1) {
				module->tileOrders.moveIndex(ocNew, ocSrc + 1);
				// module->sanitizeRadios(); this should not be needed
			}
			e.consume(this);// don't allow ctrl-click to keep menu open when duplicate separator
		}		
	};
	
	void onButton(const event::Button &e) override {
		int mods = APP->window->getMods();
		bool isCtrl = tileNumber < NUM_CTRL;
		
		if (e.button == GLFW_MOUSE_BUTTON_RIGHT && e.action == GLFW_PRESS) {
			// APP->event->setSelectedWidget(this);
			ui::Menu *menu = createMenu();
					
			// tile name menu entry
			TileNameValueField* nameValueField = new TileNameValueField(module, tileNumber);
			nameValueField->box.size.x = 100;
			menu->addChild(nameValueField);	
			
			// tile type
			menu->addChild(new MenuSeparator());
			if (isCtrl) {	
				menu->addChild(createMenuLabel("Controller type:"));
				createControllerChoiceMenuOne(menu, tileNumber, tileOrder, module);
			}
			else {
				menu->addChild(createMenuLabel("Separator type:"));
				createSeparatorChoiceMenuOne(menu, tileNumber, tileOrder, module);
			}
			
			// tile colour (display when !isCtrl, control highlight when isCtrl)
			menu->addChild(new MenuSeparator());
			std::string colStr = "Colour";
			if (isCtrl) {
				colStr.append(" (control's highlight)");
			}
			menu->addChild(createSubmenuItem(colStr, "", [=](Menu* menu) {
				for (int i = 0; i < numPsColors; i++) {
					std::string colName = psColorNames[i];
					if (isCtrl) {
						if (i == 0) {
							colName.resize(6);
						}
						else if (i == TileSettings::defaultControlHighlightColor) {
							colName.append(" (default)");
						}
					}
					menu->addChild(createCheckMenuItem(colName, "",
						[=]() {return module->tileSettings.settings[tileNumber].cc4[0] == i;},
						[=]() {module->tileSettings.settings[tileNumber].cc4[0] = i;}
					));
				}
			}));	

			// mapping	
			if (isCtrl) {
				menu->addChild(new MenuSeparator());
				menu->addChild(createMenuLabel("Mapping:"));
				if (tileBackgrounds[tileNumber] != NULL) {
					APP->event->setSelectedWidget(tileBackgrounds[tileNumber]);
				}
				for (int m = 0; m < 4; m++) {
					menu->addChild(createSubmenuItem(string::f("Map %i", m + 1), "",
						[=](Menu* menu) {			

							// menu->addChild(createMenuLabel());
							MapHeaderItem *headMapItem = createMenuItem<MapHeaderItem>("", "");
							headMapItem->module = module;
							headMapItem->tileNumber = tileNumber;
							headMapItem->mapNumber = m;
							headMapItem->disabled = true;
							menu->addChild(headMapItem);
				
							StartMappingItem *startMapItem = createMenuItem<StartMappingItem>("Start mapping", "");
							startMapItem->module = module;
							startMapItem->tileNumber = tileNumber;
							startMapItem->mapNumber = m;
							startMapItem->learningMapSrc = learningMapSrc;
							startMapItem->tileBackgrounds = tileBackgrounds;
							startMapItem->box.size.x = std::max(150.0f, startMapItem->box.size.x);
							menu->addChild(startMapItem);
							
							UnmapItem *unmapItem = createMenuItem<UnmapItem>("Unmap", "");
							unmapItem->module = module;
							unmapItem->tileNumber = tileNumber;
							unmapItem->mapNumber = m;
							menu->addChild(unmapItem);
							
							RangeItem *rangeItem = createMenuItem<RangeItem>("Range", RIGHT_ARROW);
							rangeItem->module = module;
							rangeItem->tileNumber = tileNumber;
							rangeItem->mapNumber = m;
							menu->addChild(rangeItem);
						}
					));
				}
			}

			menu->addChild(new MenuSeparator());	

			// move tile
			MoveItem *moveItem = createMenuItem<MoveItem>("Move to", RIGHT_ARROW);
			moveItem->module = module;
			moveItem->tileOrder = tileOrder;
			menu->addChild(moveItem);

			// copy tile
			std::string copyPastePostString = isCtrl ? " controller (without maps)" : " separator";
			menu->addChild(createMenuItem("Copy" + copyPastePostString, "",
				[=]() {
					module->copyTileToClipboard(tileNumber);
				}
			));

			// paste tile
			menu->addChild(createMenuItem("Paste" + copyPastePostString, "",
				[=]() {
					module->pasteTileFromClipboard(tileNumber);
					module->tileOrders.clearLitForOrder(tileOrder);
					// needs sanitize (necessary when changing tile)
					module->sanitizeRadios();
				}
			));

			// hide tile
			TileHideItem *hideItem = createMenuItem<TileHideItem>("Hide (module menu to show)", "");
			hideItem->tileInfo = &(module->tileInfos.infos[tileNumber]);
			menu->addChild(hideItem);
			
			// duplicate tile
			static const std::string duplicateStr("Duplicate");
			if (isCtrl) {
				int tcNew = module->tileOrders.findNextControllerTileNumber();
				if (tcNew < 0) {
					menu->addChild(createMenuLabel(duplicateStr));
				}
				else {
					DuplicateControllerItem *dcItem = createMenuItem<DuplicateControllerItem>(duplicateStr, "");
					dcItem->module = module;
					dcItem->tcSrc = tileNumber;
					dcItem->ocSrc = tileOrder;
					dcItem->tcNew = tcNew;
					menu->addChild(dcItem);
				}
			}
			else {
				int tsNew = module->tileOrders.findNextSeparatorTileNumber();
				if (tsNew < 0) {
					menu->addChild(createMenuLabel(duplicateStr));
				}
				else {
					DuplicateSeparatorItem *dsItem = createMenuItem<DuplicateSeparatorItem>(duplicateStr, "");
					dsItem->module = module;
					dsItem->tcSrc = tileNumber;
					dsItem->ocSrc = tileOrder;
					dsItem->tsNew = tsNew;
					menu->addChild(dsItem);
				}
			}
			
			// delete tile
			menu->addChild(new MenuSeparator());	
			DeleteTileItem *dtItem = createMenuItem<DeleteTileItem>("Delete", "");
			dtItem->module = module;
			dtItem->tileOrder = tileOrder;
			menu->addChild(dtItem);
			
			e.consume(this);
			return;
		}
		if (e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS && (mods & RACK_MOD_CTRL) != 0) {
			if (isPmAllowMouseTileMove()) {
				onButtonMouseY = APP->scene->rack->getMousePos().y;
				onButtonPosY = box.pos.y + e.pos.y;
				highlightTileMoveSrc = true;
				e.consume(this);
				return;
			}
		}
		Widget::onButton(e);
	}
	
	
	// returns -1 if mouse is not over any active tile, return order index if it is
	// posY is vertical mouse pos in pixels, module space (so 0 = top and 380 = bot)
	int getMouseOverTile(float posY) {
		int ret = -1;
		if (tileBackgrounds) {
			for (int tview = 0; tview < NUM_CTRL + NUM_SEP; tview++) {
				if (tileBackgrounds[tview] != NULL && posY > tileBackgrounds[tview]->box.pos.y &&
						posY < tileBackgrounds[tview]->box.pos.y + tileBackgrounds[tview]->box.size.y) {
					ret = tileBackgrounds[tview]->tileOrder;
					break;
				}
			}
		}
		return ret;
	}
	
	void onDragMove(const DragMoveEvent& e) override {
		if (e.button == GLFW_MOUSE_BUTTON_LEFT && highlightTileMoveSrc && highlightTileMoveDest) {
			float deltaMouseY = APP->scene->rack->getMousePos().y - onButtonMouseY;
			float onDragMovePosY = onButtonPosY + deltaMouseY;
			*highlightTileMoveDest = getMouseOverTile(onDragMovePosY);
			*highlightTileMoveDir = deltaMouseY < 0.0f ? 0 : 1;// 0 = moving upwards, 1 = moving downwards
		}
		SvgWidget::onDragMove(e);
	}
	

	void onDragEnd(const DragEndEvent& e) override {
		// DEBUG("drag end");
		if (e.button == GLFW_MOUSE_BUTTON_LEFT && highlightTileMoveSrc && highlightTileMoveDest) {
			float deltaMouseY = APP->scene->rack->getMousePos().y - onButtonMouseY;
			float onDragMovePosY = onButtonPosY + deltaMouseY;
			*highlightTileMoveDest = getMouseOverTile(onDragMovePosY);
			*highlightTileMoveDir = deltaMouseY < 0.0f ? 0 : 1;// 0 = moving upwards, 1 = moving downwards
			if (*highlightTileMoveDest != -1 && module) {
				module->tileOrders.moveIndex(tileOrder, *highlightTileMoveDest);
				module->sanitizeRadios();
			}
		}
		highlightTileMoveSrc = false;
		*highlightTileMoveDest = -1;
		SvgWidget::onDragEnd(e);
	}
	

	void draw(const DrawArgs &args) override {
		if (!svg) {
			nvgBeginPath(args.vg);
			nvgRoundedRect(args.vg, 0, 0, box.size.x, manualBgHeight, 1.25f);
			nvgFillColor(args.vg, SCHEME_PS_BG);
			nvgFill(args.vg);
			// nvgStrokeWidth(args.vg, 1.5f);		
			// nvgStrokeColor(args.vg, SCHEME_YELLOW);
			// nvgStroke(args.vg);	
		}
		else {
			SvgWidget::draw(args);
		}
		
		if (highlightTileMoveSrc) {
			nvgBeginPath(args.vg);
			nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 1.25f);
			nvgStrokeWidth(args.vg, 1.5f);		
			nvgStrokeColor(args.vg, SCHEME_MM_BLUE_LOGO);
			nvgStroke(args.vg);	
		}
		else  if (highlightTileMoveDest && *highlightTileMoveDest == tileOrder) {
			nvgStrokeWidth(args.vg, 1.5f);		
			nvgStrokeColor(args.vg, SCHEME_MM_RED_LOGO);
			nvgBeginPath(args.vg);
			nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 1.25f);
			nvgStroke(args.vg);
			nvgBeginPath(args.vg);
			nvgStrokeColor(args.vg, SCHEME_WHITE);
			float ly = -mm2px(TILE_VMARGIN) / 2.0f;// moving up (line on top)
			if (*highlightTileMoveDir != 0) {// 0 = moving upwards, 1 = moving downwards
				ly = -ly + box.size.y;// moving down so line on bottom
			}
			nvgMoveTo(args.vg, 0, ly);
			nvgLineTo(args.vg, box.size.x, ly);
			nvgStroke(args.vg);
		}
	}
};


// TILE BACKGROUNDS

// Separator and blank bgs

struct PmManualBg : PmBgBase {
	PmManualBg() {
		box.size = mm2px(Vec(18.32f, 0.0f));
		manualBgHeight = 0.0f;
	}
};

struct PmSeparatorBg : PmBgBase {
	PmSeparatorBg() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/patchset/title-divider.svg")));
	}
};
struct PmBlankXSmallBg : PmBgBase {
	PmBlankXSmallBg() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/patchset/pm-blank-xsm.svg")));
	}
};
struct PmBlankXXSmallBg : PmBgBase {
	PmBlankXXSmallBg() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/patchset/pm-blank-xxsm.svg")));
	}
};
struct PmBlankSmallBg : PmBgBase {
	PmBlankSmallBg() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/patchset/pm-button-sm.svg")));
	}
};
struct PmBlankSmallerBg : PmBgBase {
	PmBlankSmallerBg() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/patchset/pm-blank-smaller.svg")));
	}
};
struct PmBlankMediumBg : PmBgBase {
	PmBlankMediumBg() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/patchset/pm-button-md.svg")));
	}
};
struct PmBlankLargeBg : PmBgBase {
	PmBlankLargeBg() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/patchset/pm-button-lg.svg")));
	}
};

// Knob and button bgs

struct PmLargeKnobBg : PmBgBase {
	PmLargeKnobBg() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/patchset/pm-knob-lg.svg")));
	}
};
struct PmLargeKnobUnipolarBg : PmBgBase {
	PmLargeKnobUnipolarBg() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/patchset/pm-knob-lg-u.svg")));
	}
};
struct PmLargeButtonBg : PmBgBase {
	PmLargeButtonBg() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/patchset/pm-button-lg.svg")));
	}
};

struct PmMediumKnobBg : PmBgBase {
	PmMediumKnobBg() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/patchset/pm-knob-md.svg")));
	}
};
struct PmMediumKnobUnipolarBg : PmBgBase {
	PmMediumKnobUnipolarBg() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/patchset/pm-knob-md-u.svg")));
	}
};
struct PmMediumButtonBg : PmBgBase {
	PmMediumButtonBg() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/patchset/pm-button-md.svg")));
	}
};

struct PmSmallKnobBg : PmBgBase {
	PmSmallKnobBg() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/patchset/pm-knob-sm.svg")));
	}
};
struct PmSmallKnobUnipolarBg : PmBgBase {
	PmSmallKnobUnipolarBg() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/patchset/pm-knob-sm-u.svg")));
	}
};
struct PmSmallButtonBg : PmBgBase {
	PmSmallButtonBg() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/patchset/pm-button-sm.svg")));
	}
};

// Fader bgs

struct PmLargeFaderUnipolarBg : PmBgBase {
	PmLargeFaderUnipolarBg() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/patchset/pm-fader-xxlg-u.svg")));
	}
};
struct PmMediumFaderUnipolarBg : PmBgBase {
	PmMediumFaderUnipolarBg() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/patchset/pm-fader-xlg-u.svg")));
	}
};
struct PmSmallFaderUnipolarBg : PmBgBase {
	PmSmallFaderUnipolarBg() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/patchset/pm-fader-lg-u.svg")));
	}
};
struct PmLargeFaderBg : PmBgBase {
	PmLargeFaderBg() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/patchset/pm-fader-xxlg.svg")));
	}
};
struct PmMediumFaderBg : PmBgBase {
	PmMediumFaderBg() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/patchset/pm-fader-xlg.svg")));
	}
};
struct PmSmallFaderBg : PmBgBase {
	PmSmallFaderBg() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/patchset/pm-fader-lg.svg")));
	}
};



struct PatchMasterWidget : ModuleWidget {
	static constexpr float midX = 10.16f;
	TileInfos defaultTileInfos;
	TileNames defaultTileNames;
	TileOrders defaultTileOrders;
	int8_t defaultColor = 0;// yellow, used when module == NULL

	time_t oldTime = 0;
	int learningMap = -1;
	int8_t highlightTileMoveDest = -1;
	int8_t highlightTileMoveDir = 0;// 0 = moving upwards, 1 = moving downwards
	
	SvgPanel* svgPanel;
	PanelBorder* panelBorder;
	SvgWidget* logoSvg;
	SvgWidget* omriLogoSvg;
	Trigger radioTriggers[NUM_CTRL];// indexed by tile number 
		
	TileInfos tileInfosLast;// this should always match what is in the pointers below, and when it does not match tileInfosMw->info, tiles need to be repopulated and then infoMw set to match tileInfosMw->info
	TileOrders tileOrdersLast;
	PmBgBase* tileBackgrounds[NUM_CTRL + NUM_SEP];// dealloc/realloc. A tile number i gets allocated at index i in here
	TileDisplaySep* tileDisplays[NUM_CTRL + NUM_SEP];// dealloc/realloc. A tile number i gets allocated at index i in here
	ParamWidget* tileControllers[NUM_CTRL];// dealloc/realloc
	ModuleLightWidget* tileControllerLights[NUM_CTRL];// for led buttons; dealloc/realloc
	ModuleLightWidget* tileMappingLights[NUM_CTRL][4];// for mapping lights; dealloc/realloc


	struct AddControllerItem : MenuItem {
		PatchMaster* module = NULL;
		int tc;// new tile number of potential controller

		Menu *createChildMenu() override {
			Menu *menu = new Menu;
			createControllerChoiceMenuOne(menu, tc, -1, module);// -1 means need create
			// correct radioLit bit cleared when create, so nothing to do here
			return menu;
		}
	};

	struct AddSeparatorItem : MenuItem {
		PatchMaster* module = NULL;
		int ts;// new tile number of potential separator

		Menu *createChildMenu() override {
			Menu *menu = new Menu;
			createSeparatorChoiceMenuOne(menu, ts, -1, module);// -1 means need create
			// correct radioLit bit cleared when create, so nothing to do here
			return menu;
		}
	};


	void appendContextMenu(Menu *menu) override {
		PatchMaster *module = (PatchMaster*)(this->module);
		assert(module);

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Settings:"));
		
		// show mapping lights
		menu->addChild(createCheckMenuItem("Show mapping lights", "",
			[=]() {return module->miscSettings.cc4[1] != 0;},
			[=]() {module->miscSettings.cc4[1] ^= 0x1;}
		));

		// show knob arcs
		menu->addChild(createCheckMenuItem("Show knob arcs", "",
			[=]() {return module->miscSettings.cc4[0] != 0;},
			[=]() {module->miscSettings.cc4[0] ^= 0x1;}
		));

		// show blank tiles
		menu->addChild(createCheckMenuItem("Show blank tiles", "",
			[=]() {return module->miscSettings.cc4[3] == 0;},
			[=]() {module->miscSettings.cc4[3] ^= 0x1;}
		));

		// allow ctrl/cmd + click drag to move tiles
		menu->addChild(createCheckMenuItem("Allow ctrl/cmd + click drag to move tiles", "",
			[=]() {return isPmAllowMouseTileMove();},
			[=]() {togglePmAllowMouseTileMove();}
		));

		// Eco mode
		menu->addChild(createSubmenuItem("Precision", "", 
			[=](Menu* menu) {
				menu->addChild(createCheckMenuItem("Sample Rate", "",
					[=]() {return module->miscSettings.cc4[2] == 0;},
					[=]() {module->miscSettings.cc4[2] = 0;}
				));
				menu->addChild(createCheckMenuItem("SR ÷ 4 (default)", "",
					[=]() {return module->miscSettings.cc4[2] == 1;},
					[=]() {module->miscSettings.cc4[2] = 1;}
				));
				menu->addChild(createCheckMenuItem("SR ÷ 128", "",
					[=]() {return module->miscSettings.cc4[2] == 2;},
					[=]() {module->miscSettings.cc4[2] = 2;}
				));
				menu->addChild(new MenuSeparator());
				menu->addChild(createCheckMenuItem("On changes only", "",
					[=]() {return module->miscSettings2.cc4[0] != 0;},
					[=]() {module->miscSettings2.cc4[0] ^= 0x1;}
				));
			}
		));	

		// Unmap all
		menu->addChild(createMenuItem("Unmap all controllers", "",
			[=]() {
				module->clearMaps_NoLock();
			}
		));

		// visibility:
		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Visibility (if space):"));
		for (int tview = 0; tview < NUM_CTRL + NUM_SEP; tview++) {
			int t = module->tileOrders.orders[tview];
			if (t < 0) {
				break;
			}
			std::string tileName = module->tileNames.names[t];
			if (t < NUM_CTRL) {
				tileName.insert(0, "- ");
			}
			menu->addChild(createCheckMenuItem(tileName, "",
				[=]() {
					return isTileVisible(module->tileInfos.infos[t]);
				},
				[=]() {
					toggleVisible(&(module->tileInfos.infos[t]));
				}
			));
		}
		
		menu->addChild(new MenuSeparator());
		
		// add controller
		int tc = module->tileOrders.findNextControllerTileNumber();
		static const std::string newControllerStr("Add new controller");
		if (tc < 0) {
			menu->addChild(createMenuLabel(newControllerStr));
		}
		else {
			AddControllerItem *ac2Item = createMenuItem<AddControllerItem>(newControllerStr, RIGHT_ARROW);
			ac2Item->module = module;
			ac2Item->tc = tc;
			menu->addChild(ac2Item);
		}
		
		// add separator
		int ts = module->tileOrders.findNextSeparatorTileNumber();
		static const std::string newSeparatorStr("Add new separator");
		if (ts < 0) {
			menu->addChild(createMenuLabel(newSeparatorStr));
		}
		else {
			AddSeparatorItem *as2Item = createMenuItem<AddSeparatorItem>(newSeparatorStr, RIGHT_ARROW);
			as2Item->module = module;
			as2Item->ts = ts;
			menu->addChild(as2Item);
		}	
	}


	PatchMasterWidget(PatchMaster *module) {
		setModule(module);

		// Main panels from Inkscape
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/patchset/pm-bg.svg")));
		svgPanel = (SvgPanel*)getPanel();
		panelBorder = findBorder(svgPanel->fb);
					
		// logo
		svgPanel->addChild(logoSvg = createWidgetCentered<LogoSvg>(mm2px(Vec(midX, 123.32f))));
		omriLogoSvg = createWidgetCentered<OmriLogoSvg>(mm2px(Vec(midX, 123.32f)));
		omriLogoSvg->visible = false;
		svgPanel->addChild(omriLogoSvg);
		
		tileInfosLast.clear();// force all tiles to be re-populated (since 0 is impossible)
		tileOrdersLast.clear();// impossible to have this, but will be set below in populateTiles()
		for (int t = 0; t < NUM_CTRL + NUM_SEP; t++) {
			tileBackgrounds[t] = NULL;
			tileDisplays[t] = NULL;
		}
		for (int t = 0; t < NUM_CTRL; t++) {
			tileControllers[t] = NULL;
			tileControllerLights[t] = NULL;
			for (int m = 0; m < 4; m++) {
				tileMappingLights[t][m] = NULL;
			}
		}
		
		populateTiles(module);
	}
	
	
	void deallocTile(int t) {
		// dealloc for realloc
		if (tileBackgrounds[t] != NULL) {
			svgPanel->removeChild(tileBackgrounds[t]);
			delete tileBackgrounds[t];
			tileBackgrounds[t] = NULL;
		}
		if (tileDisplays[t] != NULL) {
			removeChild(tileDisplays[t]);
			delete tileDisplays[t];
			tileDisplays[t] = NULL;
		}
		if (t < NUM_CTRL) {
			if (tileControllers[t] != NULL) {
				removeChild(tileControllers[t]);
				delete tileControllers[t];
				tileControllers[t] = NULL;
			}
			if (tileControllerLights[t] != NULL) {
				removeChild(tileControllerLights[t]);
				delete tileControllerLights[t];
				tileControllerLights[t] = NULL;
			}
			for (int m = 0; m < 4; m++) {
				if (tileMappingLights[t][m] != NULL) {
					removeChild(tileMappingLights[t][m]);
					delete tileMappingLights[t][m];
					tileMappingLights[t][m] = NULL;
				}
			}
		}		
	}
	
	
	void populateTiles(PatchMaster *module) {
		// must support NULL module
		
		TileInfos* tileInfosMw;
		TileOrders* tileOrdersMw;
		TileNames* tileNamesMw;
		if (module) {
			tileInfosMw = &(module->tileInfos);
			tileOrdersMw = &(module->tileOrders);
			tileNamesMw = &(module->tileNames);
		}
		else {
			tileInfosMw = &(defaultTileInfos);
			tileOrdersMw = &(defaultTileOrders);
			tileNamesMw = &(defaultTileNames);
		}
		
		float y = 2.072f;// in mm 
		static const float yBot = 118.6f;
		
		for (int t = 0; t < NUM_CTRL + NUM_SEP; t++) {
			deallocTile(t);
		}
		
		int tview = 0;
		int tviewRadio = -1;// start of a radio group i.e. order of first tile in group (so that its height can be enlarged). This has been changed and now steps along the group, so it is not always the first tile in group.
		uint8_t radioType = 0; // TT_BUTN_RT when group of trig radios, TT_BUTN_RL when group of latched radios, 0 when tviewRadio == -1
		for ( ; tview < NUM_CTRL + NUM_SEP; tview++) {			
			int t = tileOrdersMw->orders[tview];
			if (t < 0) {
				break;
			}

			uint8_t sizeIndex = getSizeIndex(tileInfosMw->infos[t]);
			float tileHeight = getHeight(sizeIndex);
			bool hasRoom = (y + tileHeight <= yBot);
			bool tileVisible = isTileVisible(tileInfosMw->infos[t]);
			
			uint8_t ti = tileInfosMw->infos[t];
			uint8_t tsize = ti & TI_SIZE_MASK;
			uint8_t tctrl = ti & TI_TYPE_MASK;
			
			// new version
			if ((tctrl == TT_BUTN_RT || tctrl == TT_BUTN_RL) && hasRoom && tileVisible) {
				if (tviewRadio == -1 || tctrl != radioType) {
					tviewRadio = tview;
					radioType = tctrl;
				}
			}
			else {
				if (!(tctrl == TT_BUTN_RT || tctrl == TT_BUTN_RL)) {				
					tviewRadio = -1;
					radioType = 0;
				}
			}
			// old version
			// if (tctrl == TT_BUTN_RT && hasRoom && tileVisible) {
				// if (tviewRadio == -1) {
					// tviewRadio = tview;
				// }
			// }
			// else {
				// if (tctrl != TT_BUTN_RT) {
					// tviewRadio = -1;
				// }
			// }
			
			if (hasRoom && tileVisible) {
				
				if (tctrl == TT_SEP) {
					// separator tile (tsize not checked since not necessary)
					
					// tile background
					svgPanel->addChild(tileBackgrounds[t] = createWidget<PmSeparatorBg>(mm2px(Vec(midX, y))));
					tileBackgrounds[t]->box.pos.x = tileBackgrounds[t]->box.pos.x - tileBackgrounds[t]->box.size.x / 2.0f;
					tileBackgrounds[t]->tileBackgrounds = tileBackgrounds;
					tileBackgrounds[t]->learningMapSrc = &learningMap;
					tileBackgrounds[t]->highlightTileMoveDest = &highlightTileMoveDest;
					tileBackgrounds[t]->highlightTileMoveDir = &highlightTileMoveDir;
					tileBackgrounds[t]->tileOrder = tview;
					tileBackgrounds[t]->module = module;
					tileBackgrounds[t]->tileNumber = t;
					
					// tile display
					addChild(tileDisplays[t] = createWidgetCentered<TileDisplaySep>(mm2px(Vec(midX, y + getLabelDy(sizeIndex)))));
					tileDisplays[t]->text = tileNamesMw->names[t];
					if (module) {
						// separator tile display has color (does not revert to gray like controller display)
						tileDisplays[t]->dispColor = &(module->tileSettings.settings[t].cc4[0]);				
					}
					else {
						tileDisplays[t]->dispColor = &defaultColor;
					}
				}
				else if (tctrl == TT_BLANK) {
					// blank tile
					
					// tile background
					if (tsize == TS_LARGE) {
						svgPanel->addChild(tileBackgrounds[t] = createWidget<PmBlankLargeBg>(mm2px(Vec(midX, y))));
					}
					else if (tsize == TS_MEDIUM) {
						svgPanel->addChild(tileBackgrounds[t] = createWidget<PmBlankMediumBg>(mm2px(Vec(midX, y))));
					}
					else if (tsize == TS_SMALL) {
						svgPanel->addChild(tileBackgrounds[t] = createWidget<PmBlankSmallBg>(mm2px(Vec(midX, y))));
					}
					else if (tsize == TS_SMALLER) {
						svgPanel->addChild(tileBackgrounds[t] = createWidget<PmBlankSmallerBg>(mm2px(Vec(midX, y))));
					}
					else if (tsize == TS_XSMALL) {
						svgPanel->addChild(tileBackgrounds[t] = createWidget<PmBlankXSmallBg>(mm2px(Vec(midX, y))));
					}
					else {// tsize == TS_XXSMALL assumed
						svgPanel->addChild(tileBackgrounds[t] = createWidget<PmBlankXXSmallBg>(mm2px(Vec(midX, y))));
					}
					tileBackgrounds[t]->box.pos.x = tileBackgrounds[t]->box.pos.x - tileBackgrounds[t]->box.size.x / 2.0f;
					tileBackgrounds[t]->tileBackgrounds = tileBackgrounds;
					tileBackgrounds[t]->learningMapSrc = &learningMap;
					tileBackgrounds[t]->highlightTileMoveDest = &highlightTileMoveDest;
					tileBackgrounds[t]->highlightTileMoveDir = &highlightTileMoveDir;
					tileBackgrounds[t]->tileOrder = tview;
					tileBackgrounds[t]->module = module;
					tileBackgrounds[t]->tileNumber = t;
				}
				else {
					// non separator tile
					
					// tile background
					if (tsize == TS_XXLARGE) {
						// fader assumed (must create something)
						if (tctrl == TT_FADER_C) {
							svgPanel->addChild(tileBackgrounds[t] = createWidget<PmLargeFaderBg>(mm2px(Vec(midX, y))));
						}
						else {
							svgPanel->addChild(tileBackgrounds[t] = createWidget<PmLargeFaderUnipolarBg>(mm2px(Vec(midX, y))));
						}
					}
					else if (tsize == TS_XLARGE) {
						// fader assumed (must create something)
						if (tctrl == TT_FADER_C) {
							svgPanel->addChild(tileBackgrounds[t] = createWidget<PmMediumFaderBg>(mm2px(Vec(midX, y))));
						}
						else {
							svgPanel->addChild(tileBackgrounds[t] = createWidget<PmMediumFaderUnipolarBg>(mm2px(Vec(midX, y))));
						}
					}
					else if (tsize == TS_LARGE) {
						if (tctrl == TT_KNOB_C) {
							svgPanel->addChild(tileBackgrounds[t] = createWidget<PmLargeKnobBg>(mm2px(Vec(midX, y))));
						}
						else if (tctrl == TT_KNOB_L) {
							svgPanel->addChild(tileBackgrounds[t] = createWidget<PmLargeKnobUnipolarBg>(mm2px(Vec(midX, y))));
						}
						else if (tctrl == TT_FADER_C) {
							svgPanel->addChild(tileBackgrounds[t] = createWidget<PmSmallFaderBg>(mm2px(Vec(midX, y))));
						}
						else if (tctrl == TT_FADER_B) {
							svgPanel->addChild(tileBackgrounds[t] = createWidget<PmSmallFaderUnipolarBg>(mm2px(Vec(midX, y))));
						}
						else if (tctrl == TT_BUTN_RT || tctrl == TT_BUTN_RL) {
							svgPanel->addChild(tileBackgrounds[t] = createWidget<PmManualBg>(mm2px(Vec(midX, y))));
							tileBackgrounds[t]->box.size.y = mm2px(getHeight(tsize >> 4));
							tileBackgrounds[t]->manualBgHeight = tileBackgrounds[t]->box.size.y;
							if (tviewRadio != -1 && tviewRadio != tview && tctrl == radioType) {
								int t2 = tileOrdersMw->orders[tviewRadio];
								tileBackgrounds[t2]->manualBgHeight += mm2px(3.3f);
								tviewRadio = tview;
							}
						}
						else {// remaining buttons assumed
							svgPanel->addChild(tileBackgrounds[t] = createWidget<PmLargeButtonBg>(mm2px(Vec(midX, y))));
						}							
					}
					else if (tsize == TS_MEDIUM) {
						if (tctrl == TT_KNOB_C) {
							svgPanel->addChild(tileBackgrounds[t] = createWidget<PmMediumKnobBg>(mm2px(Vec(midX, y))));
						}
						else if (tctrl == TT_KNOB_L) {
							svgPanel->addChild(tileBackgrounds[t] = createWidget<PmMediumKnobUnipolarBg>(mm2px(Vec(midX, y))));
						}
						else if (tctrl == TT_BUTN_RT || tctrl == TT_BUTN_RL) {
							svgPanel->addChild(tileBackgrounds[t] = createWidget<PmManualBg>(mm2px(Vec(midX, y))));
							tileBackgrounds[t]->box.size.y = mm2px(getHeight(tsize >> 4));
							tileBackgrounds[t]->manualBgHeight = tileBackgrounds[t]->box.size.y;
							if (tviewRadio != -1 && tviewRadio != tview && tctrl == radioType) {
								int t2 = tileOrdersMw->orders[tviewRadio];
								tileBackgrounds[t2]->manualBgHeight += mm2px(3.3f);
								tviewRadio = tview;
							}
						}
						else {// remaining buttons assumed
							svgPanel->addChild(tileBackgrounds[t] = createWidget<PmMediumButtonBg>(mm2px(Vec(midX, y))));
						}							
					}
					else {// TM_SMALL assumed
						if (tctrl == TT_KNOB_C) {
							svgPanel->addChild(tileBackgrounds[t] = createWidget<PmSmallKnobBg>(mm2px(Vec(midX, y))));
						}
						else if (tctrl == TT_KNOB_L) {
							svgPanel->addChild(tileBackgrounds[t] = createWidget<PmSmallKnobUnipolarBg>(mm2px(Vec(midX, y))));
						}
						else if (tctrl == TT_BUTN_RT || tctrl == TT_BUTN_RL) {
							svgPanel->addChild(tileBackgrounds[t] = createWidget<PmManualBg>(mm2px(Vec(midX, y))));
							tileBackgrounds[t]->box.size.y = mm2px(getHeight(tsize >> 4));
							tileBackgrounds[t]->manualBgHeight = tileBackgrounds[t]->box.size.y;
							if (tviewRadio != -1 && tviewRadio != tview && tctrl == radioType) {
								int t2 = tileOrdersMw->orders[tviewRadio];
								tileBackgrounds[t2]->manualBgHeight += mm2px(3.3f);
								tviewRadio = tview;
							}
						}
						else {// remaining buttons assumed
							svgPanel->addChild(tileBackgrounds[t] = createWidget<PmSmallButtonBg>(mm2px(Vec(midX, y))));
						}							
					}
					tileBackgrounds[t]->box.pos.x = tileBackgrounds[t]->box.pos.x - tileBackgrounds[t]->box.size.x / 2.0f;
					tileBackgrounds[t]->tileBackgrounds = tileBackgrounds;
					tileBackgrounds[t]->learningMapSrc = &learningMap;
					tileBackgrounds[t]->highlightTileMoveDest = &highlightTileMoveDest;
					tileBackgrounds[t]->highlightTileMoveDir = &highlightTileMoveDir;
					tileBackgrounds[t]->tileOrder = tview;
					tileBackgrounds[t]->module = module;
					tileBackgrounds[t]->tileNumber = t;
					
					
					// tile display
					addChild(tileDisplays[t] = createWidgetCentered<TileDisplayController>(mm2px(Vec(midX, y + getLabelDy(sizeIndex)))));
					tileDisplays[t]->text = tileNamesMw->names[t];
					if ((tctrl == TT_FADER_C || tctrl == TT_FADER_B) && tsize == TS_LARGE) {
						tileDisplays[t]->box.pos.y += 3.5f;
					}
					
					// controller
					if (tileControllers[t] == NULL) {
						if (isCtlrAButton(ti)) {
							if ((ti & TI_SIZE_MASK) == TS_LARGE) {
								addParam(tileControllers[t] = createParamCentered<PmLargeButton>(mm2px(Vec(midX, y + getCtrlDy(sizeIndex))), module, PatchMaster::TILE_PARAMS + t));
								addChild(tileControllerLights[t] = createLightCentered<PmLargeButtonLight>(mm2px(Vec(midX, y + getCtrlDy(sizeIndex))), module, PatchMaster::TILE_LIGHTS + t));
							}
							else if ((ti & TI_SIZE_MASK) == TS_MEDIUM) {
								addParam(tileControllers[t] = createParamCentered<PmMediumButton>(mm2px(Vec(midX, y + getCtrlDy(sizeIndex))), module, PatchMaster::TILE_PARAMS + t));
								addChild(tileControllerLights[t] = createLightCentered<PmMediumButtonLight>(mm2px(Vec(midX, y + getCtrlDy(sizeIndex))), module, PatchMaster::TILE_LIGHTS + t));
							}
							else {// TS_SMALL assumed
								addParam(tileControllers[t] = createParamCentered<PmSmallButton>(mm2px(Vec(midX, y + getCtrlDy(sizeIndex))), module, PatchMaster::TILE_PARAMS + t));
								addChild(tileControllerLights[t] = createLightCentered<PmSmallButtonLight>(mm2px(Vec(midX, y + getCtrlDy(sizeIndex))), module, PatchMaster::TILE_LIGHTS + t));
							}
							bool newIsMoment = ((ti & TI_TYPE_MASK) == TT_BUTN_M) || ((ti & TI_TYPE_MASK) == TT_BUTN_RT);
							((SvgSwitch*)(tileControllers[t]))->momentary = newIsMoment;
							if (module) {
								module->paramQuantities[PatchMaster::TILE_PARAMS + t]->snapEnabled = true;
								module->paramQuantities[PatchMaster::TILE_PARAMS + t]->randomizeEnabled = !newIsMoment;
								module->paramQuantities[PatchMaster::TILE_PARAMS + t]->defaultValue = 0.0f;
								module->params[PatchMaster::TILE_PARAMS + t].setValue((module->params[PatchMaster::TILE_PARAMS + t].getValue() >= 0.5f && !newIsMoment) ? 1.0f : 0.0f);
								((PmCtrlLightWidget*)tileControllerLights[t])->highlightColor = &(module->tileSettings.settings[t].cc4[0]);
							}
							radioTriggers[t].reset();
						}
						else if (isCtlrAKnob(ti)) {
							if ((ti & TI_SIZE_MASK) == TS_LARGE) {
								addParam(tileControllers[t] = createParamCentered<PmLargeKnob>(mm2px(Vec(midX, y + getCtrlDy(sizeIndex))), module, PatchMaster::TILE_PARAMS + t));
							}
							else if ((ti & TI_SIZE_MASK) == TS_MEDIUM) {
								addParam(tileControllers[t] = createParamCentered<PmMediumKnob>(mm2px(Vec(midX, y + getCtrlDy(sizeIndex))), module, PatchMaster::TILE_PARAMS + t));
							}
							else {// TS_SMALL assumed
								addParam(tileControllers[t] = createParamCentered<PmSmallKnob>(mm2px(Vec(midX, y + getCtrlDy(sizeIndex))), module, PatchMaster::TILE_PARAMS + t));
							}
							if (module) {
								module->paramQuantities[PatchMaster::TILE_PARAMS + t]->snapEnabled = false;
								module->paramQuantities[PatchMaster::TILE_PARAMS + t]->randomizeEnabled = true;
								module->paramQuantities[PatchMaster::TILE_PARAMS + t]->resetEnabled = true;
								module->paramQuantities[PatchMaster::TILE_PARAMS + t]->smoothEnabled = true;
								float defVal = (ti & TI_TYPE_MASK) == TT_KNOB_C ? 0.5f : 0.0f;
								module->paramQuantities[PatchMaster::TILE_PARAMS + t]->defaultValue = defVal;
								((PmKnobWithArc*)tileControllers[t])->arcColor = &(module->tileSettings.settings[t].cc4[0]);
								((PmKnobWithArc*)tileControllers[t])->showArc = &(module->miscSettings.cc4[0]);
								((PmKnobWithArc*)tileControllers[t])->topCentered = ((ti & TI_TYPE_MASK) == TT_KNOB_C);
							}
						}
						else if (isCtlrAFader(ti)) { 
							if ((ti & TI_SIZE_MASK) == TS_XXLARGE) {
								addParam(tileControllers[t] = createParamCentered<PsXXLargeFader>(mm2px(Vec(midX, y + getCtrlDy(sizeIndex))), module, PatchMaster::TILE_PARAMS + t));
							}
							else if ((ti & TI_SIZE_MASK) == TS_XLARGE) {
								addParam(tileControllers[t] = createParamCentered<PsXLargeFader>(mm2px(Vec(midX, y + getCtrlDy(sizeIndex))), module, PatchMaster::TILE_PARAMS + t));
							}
							else {// TS_LARGE assumed
								addParam(tileControllers[t] = createParamCentered<PsLargeFader>(mm2px(Vec(midX, y + getCtrlDy(sizeIndex))), module, PatchMaster::TILE_PARAMS + t));
								tileControllers[t]->box.pos.y -= 0.5f;
							}
							if (module) {
								module->paramQuantities[PatchMaster::TILE_PARAMS + t]->snapEnabled = false;
								module->paramQuantities[PatchMaster::TILE_PARAMS + t]->randomizeEnabled = true;
								module->paramQuantities[PatchMaster::TILE_PARAMS + t]->resetEnabled = true;
								module->paramQuantities[PatchMaster::TILE_PARAMS + t]->smoothEnabled = true;
								float defVal = (ti & TI_TYPE_MASK) == TT_FADER_C ? 0.5f : 0.0f;
								module->paramQuantities[PatchMaster::TILE_PARAMS + t]->defaultValue = defVal;
								((PmSliderWithHighlight*)tileControllers[t])->highlightColor = &(module->tileSettings.settings[t].cc4[0]);
							}
						}
					}
					
					// map leds
					static constexpr float lofsx[4] = {-7.5f, -5.5f, 5.5f, 7.5f};
					static constexpr float lofsy =  2.0f;// measured from top of tile
					for (int m = 0; m < 4; m++) {
						if (tileMappingLights[t][m] == NULL) {
							tileMappingLights[t][m] = createLightCentered<TinyLight<PsMapLight>>(mm2px(Vec(midX + lofsx[m], y + lofsy )), module, PatchMaster::MAP_LIGHTS + (t * 4 + m) * 2 + 0);
							if (module) {
								tileMappingLights[t][m]->visible = module->miscSettings.cc4[1] != 0;
							}
							addChild(tileMappingLights[t][m]);
						}
					}
				}
				y += (tileHeight + TILE_VMARGIN);
			}// if (hasRoom && tileVisible
		}// for (int tview	
		tileOrdersLast.copyFrom(tileOrdersMw);
		tileInfosLast.copyFrom(tileInfosMw);
	}// void populateTiles(
	
	
	void draw(const DrawArgs& args) override {
		if (module && calcFamilyPresent(module->rightExpander.module)) {
			DrawArgs newDrawArgs = args;
			newDrawArgs.clipBox.size.x += mm2px(0.3f);// PM familiy panels have their base rectangle this much larger, to kill gap artifacts
			ModuleWidget::draw(newDrawArgs);
		}
		else {
			ModuleWidget::draw(args);
		}
	}
	

	void step() override {
		if (module) {
			PatchMaster* module = (PatchMaster*)(this->module);
			
			// see if layout needs updating
			bool needsLayoutUpdate = false;
			for (int tview = 0; tview < NUM_CTRL + NUM_SEP; tview++) {
				int t = module->tileOrders.orders[tview];
				if (t != tileOrdersLast.orders[tview]) {
					needsLayoutUpdate = true;
					break;
				}
				if (t < 0) {
					break;
				}
				if (tileInfosLast.infos[t] != module->tileInfos.infos[t]) {
					needsLayoutUpdate = true;
					break;
				}
				
			}
			if (needsLayoutUpdate) {
				populateTiles(module);
				svgPanel->fb->dirty = true;				
			}
						
			// labels (pull from module)
			if (module->updateControllerLabelsRequest != 0) {// pull request from module
				for (int tview = 0; tview < NUM_CTRL + NUM_SEP; tview++) {
					int t = module->tileOrders.orders[tview];
					if (t < 0) {
						break;
					}
					if (tileDisplays[t] != NULL) {
						tileDisplays[t]->text = module->tileNames.names[t];
					}
				}
				module->updateControllerLabelsRequest = 0;// all done pulling
			}

			// Borders	
			bool familyPresentLeft  = calcFamilyPresent(module->leftExpander.module);
			bool familyPresentRight = calcFamilyPresent(module->rightExpander.module);
			float newPosX = svgPanel->box.pos.x;
			float newSizeX = svgPanel->box.size.x;
			//
			if (familyPresentLeft) {
				newPosX -= 3.0f;
				newSizeX += 3.0f;
			}
			if (familyPresentRight) {
				newSizeX += 3.0f;
			}
			if (panelBorder->box.pos.x != newPosX || panelBorder->box.size.x != newSizeX) {
				panelBorder->box.pos.x = newPosX;
				panelBorder->box.size.x = newSizeX;
				svgPanel->fb->dirty = true;
			}
			
			// Logos
			bool rightIsBlank = (module->rightExpander.module && (module->rightExpander.module->model == modelPatchMasterBlank));
			bool newLogoVisible = !familyPresentRight || (rightIsBlank && !calcFamilyPresent(module->rightExpander.module->rightExpander.module, false));
			//
			bool leftIsBlank = (module->leftExpander.module && (module->leftExpander.module->model == modelPatchMasterBlank));
			bool newOmriVisible = !newLogoVisible && (!familyPresentLeft || (leftIsBlank && !calcFamilyPresent(module->leftExpander.module->leftExpander.module, false)));
			//
			if (logoSvg->visible != newLogoVisible || omriLogoSvg->visible != newOmriVisible) {
				logoSvg->visible = newLogoVisible;
				omriLogoSvg->visible = newOmriVisible;
				svgPanel->fb->dirty = true;
			}
			
			// Update param tooltips, paramHandle texts, blanks transparent and mapping light visibility at 1Hz
			time_t currentTime = time(0);
			if (currentTime != oldTime) {
				oldTime = currentTime;
				for (int ic = 0; ic < NUM_CTRL; ic++) {
					uint8_t ti = module->tileInfos.infos[ic];
					
					// param tooltips
					module->paramQuantities[PatchMaster::TILE_PARAMS + ic]->name = module->tileNames.names[ic]; 
					
					// paramHandle texts
					for (int m = 0; m < 4; m++) {
						module->tileConfigs[ic].parHandles[m].text = module->tileNames.names[ic];
					}
					
					// show mapping lights
					if (isTileVisible(ti)) {
						for (int m = 0; m < 4; m++) {
							if (tileMappingLights[ic][m] != NULL) {
								tileMappingLights[ic][m]->visible = module->miscSettings.cc4[1] != 0;
							}
						}
					}
				}
				for (int is = NUM_CTRL; is < NUM_CTRL + NUM_SEP; is++) {
					uint8_t ti = module->tileInfos.infos[is];

					// blank transparent
					if ((ti & TI_TYPE_MASK) == TT_BLANK) {
						if (tileBackgrounds[is] != NULL) {
							tileBackgrounds[is]->visible = module->miscSettings.cc4[3] == 0;
						}
					}
				}
			}
			
			// radioTriggers for module->radioLit, and mapping and led button lights
			for (int tview = 0; tview < NUM_CTRL + NUM_SEP; tview++) {
				int t = module->tileOrders.orders[tview];
				if (t < 0) {
					break;
				}
				if (t < NUM_CTRL) {
					uint8_t ti = module->tileInfos.infos[t];
					if (radioTriggers[t].process(module->params[t].getValue())) {
						if ((ti & TI_TYPE_MASK) == TT_BUTN_RT) {
							module->tileOrders.setLitForOrder(tview);
							// force one-hot around order tview:
							// up (no visibility check, since groups enforced without taking visibility into account)
							for (int tv2 = tview + 1; tv2 < NUM_CTRL + NUM_SEP; tv2++) {
								int8_t t2 = module->tileOrders.orders[tv2];
								if (t2 == -1) {
									break;
								}
								uint8_t ti2 = module->tileInfos.infos[t2];
								if ((ti2 & TI_TYPE_MASK) == TT_BUTN_RT) {
									module->tileOrders.clearLitForOrder(tv2);
								}
								else break;
							}
							// down (no visibility check, since groups enforced without taking visibility into account)
							for (int tv2 = tview - 1; tv2 >= 0; tv2--) {
								int8_t t2 = module->tileOrders.orders[tv2];
								uint8_t ti2 = module->tileInfos.infos[t2];
								if ((ti2 & TI_TYPE_MASK) == TT_BUTN_RT) {
									module->tileOrders.clearLitForOrder(tv2);
								}
								else break;
							}
						}
						else if ((ti & TI_TYPE_MASK) == TT_BUTN_RL) {
							// force one-hot around order tview:
							// up (no visibility check, since groups enforced without taking visibility into account)
							for (int tv2 = tview + 1; tv2 < NUM_CTRL + NUM_SEP; tv2++) {
								int8_t t2 = module->tileOrders.orders[tv2];
								if (t2 == -1) {
									break;
								}
								uint8_t ti2 = module->tileInfos.infos[t2];
								if ((ti2 & TI_TYPE_MASK) == TT_BUTN_RL) {
									module->params[t2].setValue(0.0f);
								}
								else break;
							}
							// down (no visibility check, since groups enforced without taking visibility into account)
							for (int tv2 = tview - 1; tv2 >= 0; tv2--) {
								int t2 = module->tileOrders.orders[tv2];
								uint8_t ti2 = module->tileInfos.infos[t2];
								if ((ti2 & TI_TYPE_MASK) == TT_BUTN_RL) {
									module->params[t2].setValue(0.0f);
								}
								else break;
							}
						}
					}
					if (isTileVisible(ti)) {
						// mapping lights
						for (int m = 0; m < 4; m++) {
							int id = t * 4 + m;
							module->lights[PatchMaster::MAP_LIGHTS + (t * 4 + m) * 2 + 0].setBrightness(module->tileConfigs[t].parHandles[m].moduleId >= 0 && module->learningId != id ? 1.0f : 0.0f);// green
							module->lights[PatchMaster::MAP_LIGHTS + (t * 4 + m) * 2 + 1].setBrightness(module->learningId == id ? 1.0f : 0.0f);// red
						}
						// led-button lights
						if (isCtlrAButton(ti)) {
							float lbv = module->params[t].getValue();
							if (module->tileOrders.isLit(tview)) {
								lbv = 1.0f;
							}
							if ((ti & TI_TYPE_MASK) == TT_BUTN_I) {
								lbv = 1.0f - lbv;
							}
							module->lights[PatchMaster::TILE_LIGHTS + t].setBrightness(lbv);
						}
					}
				}
			}
		}			
		
		ModuleWidget::step();
	}// void step()
};


Model *modelPatchMaster = createModel<PatchMaster, PatchMasterWidget>("PatchMaster");
