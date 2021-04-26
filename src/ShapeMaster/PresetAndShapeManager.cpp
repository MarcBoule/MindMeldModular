//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "PresetAndShapeManager.hpp"
#include "OsdialogTemp.h"


static const std::string factoryPrefix = "res/ShapeMaster/";

static const char PRESET_FILTER[] = "ShapeMaster preset (.smpr):smpr";// if extension changes, other code must be updated below
static const char SHAPE_FILTER[] = "ShapeMaster shape (.smsh):smsh";// if extension changes, other code must be updated below

std::string getUserPath(bool isPreset) {
	return asset::user("MindMeldModular").append("/ShapeMaster").append(isPreset ? "/UserPresets" : "/UserShapes");
}


void appendDirMenu(std::string dirPath, Menu* menu, Channel* channel, bool isPreset);// defined in this file


bool loadPresetOrShape(const std::string path, Channel* dest, bool isPreset, bool* unsupportedSync, bool withHistory) {
	// returns success
	// unsupportedSync must only be non-null when isPreset is true and we are loading for the dirty cache
	// 
	FILE* file = std::fopen(path.c_str(), "r");
	if (!file) {
		// Exit silently
		return false;
	}
	DEFER({
		std::fclose(file);
	});

	json_error_t error;
	json_t* presetOrShapeFileJ = json_loadf(file, 0, &error);
	if (!presetOrShapeFileJ) {
		std::string message = string::f("JSON parsing error at %s %d:%d %s", error.source, error.line, error.column, error.text);
		osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, message.c_str());
		return false;
	}
	DEFER({
		json_decref(presetOrShapeFileJ);
	});
	
	json_t *channelPresetOrShapeJ = json_object_get(presetOrShapeFileJ, isPreset ? "ShapeMaster channel preset" : "ShapeMaster shape");
	if (!channelPresetOrShapeJ) {
		std::string message = isPreset ? "INVALID ShapeMaster channel preset file" : "INVALID ShapeMaster shape file";
		osdialog_message(OSDIALOG_WARNING, OSDIALOG_OK, message.c_str());
		return false;
	}

	// Push PresetOrShapeLoad history action (rest is done in onDragEnd())
	PresetOrShapeLoad* h = NULL;
	if (withHistory) {
		h = new PresetOrShapeLoad;
		h->isPreset = isPreset;
		h->channelSrc = dest;
	}

	if (isPreset) {
		if (h) {
			h->oldJson = dest->dataToJsonChannel(WITH_PARAMS, WITHOUT_PRO_UNSYNC_MATCH, WITHOUT_FULL_SETTINGS);
		}
		
		bool isDirtyCacheLoad = unsupportedSync != NULL;
		bool loadedUnsupportedSync = dest->dataFromJsonChannel(channelPresetOrShapeJ, WITH_PARAMS, isDirtyCacheLoad, WITHOUT_FULL_SETTINGS);
		if (unsupportedSync) {
			*unsupportedSync = loadedUnsupportedSync;
		}
		dest->setPresetPath(path);
		
		if (h) {
			h->newJson = dest->dataToJsonChannel(WITH_PARAMS, WITHOUT_PRO_UNSYNC_MATCH, WITHOUT_FULL_SETTINGS);
			h->name = "load preset";
		}
	}
	else {// shape
		if (h) {
			h->oldJson = dest->getShape()->dataToJsonShape();
			h->oldShapePath = dest->getShapePath();
		}
		
		dest->dataFromJsonShape(channelPresetOrShapeJ);
		dest->setShapePath(path);
		
		if (h) {
			h->newJson = dest->getShape()->dataToJsonShape();
			h->newShapePath = dest->getShapePath();
			h->name = "load shape";
		}
	}
	if (h) {
		APP->history->push(h);
	}
	return true;
}


void savePresetOrShape(const std::string path, Channel* channel, bool isPreset, Channel* channelDirtyCache) {
	INFO((isPreset ? "Saving ShapeMaster channel preset %s" : "Saving ShapeMaster shape %s"), path.c_str());
	json_t* channelPresetOrShapeJ = isPreset ? 
		channel->dataToJsonChannel(WITH_PARAMS, WITH_PRO_UNSYNC_MATCH, WITHOUT_FULL_SETTINGS) : 
		channel->dataToJsonShape();
	json_t* presetOrShapeFileJ = json_object();		
	DEFER({
		json_decref(presetOrShapeFileJ);
	});
	json_object_set_new(presetOrShapeFileJ, isPreset ? "ShapeMaster channel preset" : "ShapeMaster shape", channelPresetOrShapeJ);

	// Write to temporary path and then rename it to the correct path
	std::string tmpPath = path + ".tmp";
	FILE* file = std::fopen(tmpPath.c_str(), "w");
	if (!file) {
		// Fail silently
		return;
	}

	json_dumpf(presetOrShapeFileJ, file, JSON_INDENT(2) | JSON_REAL_PRECISION(9));
	std::fclose(file);
	system::moveFile(tmpPath, path);

	if (isPreset) {
		channel->setPresetPath(path);
		channelDirtyCache->setPresetPath("");// force reload for dirty comparison (or else star will stay since not reloaded as path not changed)
	}
	else {
		channel->setShapePath(path);
		channelDirtyCache->setShapePath("");// force reload for dirty comparison (or else star will stay since not reloaded as path not changed)
	}
}


// ----------------------------------------------------------------------------
// Preset and Shape Manager
// ----------------------------------------------------------------------------


void PresetAndShapeManager::construct(Channel* _channels, Channel* _channelDirtyCacheSrc, PackedBytes4* _miscSettings3) {
	channels = _channels;
	channelDirtyCacheSrc = _channelDirtyCacheSrc;
	miscSettings3 = _miscSettings3;
	std::string factoryPath = asset::plugin(pluginInstance, factoryPrefix);
	factoryPath.resize(factoryPath.size() - 1);// remove trailing "/"
	std::list<std::string> factoryEntries = system::getEntriesRecursive(factoryPath, 3);// 3 is max depth (1 = current path)
	
	// create vector while ignoring anything that is not an .smpr file
	for (auto it = factoryEntries.begin(); it != factoryEntries.end(); it++) {
		if (system::isFile(*it)) {
			if (string::filenameExtension(*it) == "smpr") {
				factoryPresetVector.push_back(*it);
			}
			else if (string::filenameExtension(*it) == "smsh") {
				factoryShapeVector.push_back(*it);
			}
		} 
	}	
	clearAllWorkloads();
}


void PresetAndShapeManager::executeOrStageWorkload(int c, int _workType, bool _withHistory, bool stage) {
	if (_workType <= WT_NEXT_SHAPE) {
		// file operation
		if ( stage && (miscSettings3->cc4[_workType <= WT_NEXT_PRESET ? 0 : 1] != 0) ) {
			if (requestWork[c] != WS_TODO) {
				workType[c] = _workType;
				withHistory[c] = _withHistory;
				requestWork[c] = WS_STAGED;
			}
		}
		else {
			if (requestWork[c] == WS_NONE) {
				workType[c] = _workType;
				withHistory[c] = _withHistory;
				requestWork[c] = WS_TODO;
				cv.notify_one();
			}
		}
	}
	else {
		// other operation (reverse, inverse, random); _withHistory and stage are ignored (assumed false)
		if (requestWork[c] == WS_NONE) {
			workType[c] = _workType;
			withHistory[c] = false;
			requestWork[c] = WS_TODO;
			cv.notify_one();
		}
	}
}


PresetAndShapeManager::PresetAndShapeManager() : worker(&PresetAndShapeManager::file_worker, this) {}


void PresetAndShapeManager::file_worker() {
	random::init();// Rack doc says to call once per thread, or else random::u32() etc will always return 0
	while (true) {
		std::unique_lock<std::mutex> lk(mtx);
		while (!(requestWork[0] == WS_TODO || requestWork[1] == WS_TODO || 
				requestWork[2] == WS_TODO || requestWork[3] == WS_TODO || 
				requestWork[4] == WS_TODO || requestWork[5] == WS_TODO || 
				requestWork[6] == WS_TODO || requestWork[7] == WS_TODO || requestStop)) {
			cv.wait(lk);
		}
		lk.unlock();
		if (requestStop) break;
		
		for (int chan = 0; chan < 8; chan++) {
			if (requestWork[chan] == WS_TODO) {		
				Channel* channel = &channels[chan];
				if (workType[chan] <= WT_NEXT_SHAPE) {
					// file operation
					bool isPreset = workType[chan] <= WT_NEXT_PRESET;
					bool getPrev = (workType[chan] == WT_PREV_PRESET || workType[chan] == WT_PREV_SHAPE);
					std::string path = isPreset ? channel->getPresetPath() : channel->getShapePath();
					if (!path.empty()) {
						std::string assetPluginPath = asset::plugin(pluginInstance, "");
						if (path.compare(0, assetPluginPath.size(), assetPluginPath) == 0) {
							// factory
							const std::vector<std::string>* factoryVector = isPreset ? &(factoryPresetVector) : &(factoryShapeVector);
							for (size_t i = 0; i < factoryVector->size(); i++) {
								if (path == (*factoryVector)[i]) {
									int newIndex = i + factoryVector->size() + (getPrev ? -1 : 1);
									newIndex %= factoryVector->size();
									loadPresetOrShape((*factoryVector)[newIndex], channel, isPreset, NULL, withHistory[chan]);
									break;
								}
							}
						}
						else {
							// user
							std::list<std::string> userEntries = system::getEntries(string::directory(path));
							std::vector<std::string> userVector;
							int pathIndex = -1;
							std::string presetOrShapeExt = (isPreset ? "smpr" : "smsh");
							for (std::string entry : userEntries) {
								if (!(system::isFile(entry) && string::filenameExtension(entry) == presetOrShapeExt)) {
									continue;
								}
								if (path == entry) {
									pathIndex = userVector.size();
								}					
								userVector.push_back(entry);
							}
							if (pathIndex != -1) {
								int newIndex = pathIndex + userVector.size() + (getPrev ? -1 : 1);
								newIndex %= userVector.size();
								loadPresetOrShape(userVector[newIndex], channel, isPreset, NULL, withHistory[chan]);
							}
						}
					}
				}
				else {
					// other operation (reverse, inverse, random)
					if (workType[chan] == WT_REVERSE) {
						channel->reverseShape(false);
					}
					else if (workType[chan] == WT_INVERT) {
						channel->invertShape(false);
					}
					else if (workType[chan] == WT_RANDOM) {
						channel->randomizeShape(false);
					}
				}
				requestWork[chan] = WS_NONE;
			}//if TODO
		}// for chan
	}// while(true)
}// file_worker()



struct SaveUserSubItem : MenuItem {
	Channel* channel;
	Channel* channelDirtyCache;
	bool isPreset = false;
	
	void onAction(const event::Action &e) override {
		// most of the code in here is from PatchManager::saveAsDialog() and PatchManager::save() by Andrew Belt (in Rack/src/patch.cpp)
		
		std::string dir;
		std::string filename;
		std::string presetOrShapePath = isPreset ? channel->getPresetPath() : channel->getShapePath();
		std::string factoryPath = asset::plugin(pluginInstance, factoryPrefix);
		if (presetOrShapePath.empty() || presetOrShapePath.rfind(factoryPath, 0) == 0) {
			dir = asset::user("MindMeldModular");
			system::createDirectory(dir);
			dir.append("/ShapeMaster");
			system::createDirectory(dir);
			if (isPreset) {
				dir.append("/UserPresets");
			}
			else {
				dir.append("/UserShapes");
			}				
			system::createDirectory(dir);
			filename = "Untitled";
		}
		else {
			dir = string::directory(presetOrShapePath);
			filename = string::filename(presetOrShapePath);
		}

		osdialog_filters* filters = osdialog_filters_parse(isPreset ? PRESET_FILTER : SHAPE_FILTER);
		DEFER({
			osdialog_filters_free(filters);
		});

		#ifdef ARCH_WIN 
		// temporary until RackV2, see ../OsdialogTemp.h (when removing, don't forget to remove additions to makefile, which are listed in comments at the top of ../OsdialogTemp.h
		char* pathC = osdialog_file_win(OSDIALOG_SAVE, dir.c_str(), filename.c_str(), filters);
		#else
		char* pathC = osdialog_file(OSDIALOG_SAVE, dir.c_str(), filename.c_str(), filters);
		#endif
		if (!pathC) {
			// Fail silently
			return;
		}
		DEFER({
			free(pathC);
		});
		
		// Append .smpr or .smsh extension if no extension was given.
		std::string pathStr = pathC;
		std::replace(pathStr.begin(), pathStr.end(), '\\', '/');
		if (string::filenameExtension(string::filename(pathStr)) == "") {
			pathStr += isPreset ? ".smpr" : ".smsh";
		}

		savePresetOrShape(pathStr, channel, isPreset, channelDirtyCache);
	}
};


struct ClearDisplayItem : MenuItem {
	Channel* channel;
	bool isPreset;
	void onAction(const event::Action &e) override {
		if (isPreset) {
			channel->setPresetPath("");
		}
		else {
			channel->setShapePath("");
		}
	}
};


struct SaveInitPresetOrShapeItem : MenuItem {
	Channel* channel;
	bool isPreset;
	std::string initFilePath;
	Channel* channelDirtyCacheSrc;
	void onAction(const event::Action &e) override {
		savePresetOrShape(initFilePath, channel, isPreset, channelDirtyCacheSrc);
	}
};


struct LoadInitPresetOrShapeItem : MenuItem {
	Channel* channel;
	bool isPreset;
	std::string initFilePath;
	void onAction(const event::Action &e) override {
		// Push PresetOrShapeChange history action (rest is done in onDragEnd())
		PresetOrShapeChange* h = new PresetOrShapeChange;
		h->isPreset = isPreset;
		if (isPreset) {
			h->channelSrc = channel;
			h->oldJson = channel->dataToJsonChannel(WITH_PARAMS, WITHOUT_PRO_UNSYNC_MATCH, WITHOUT_FULL_SETTINGS);
		}
		else {
			h->shapeSrc = channel->getShape();
			h->oldJson = h->shapeSrc->dataToJsonShape();
		}
		
		if (!loadPresetOrShape(initFilePath, channel, isPreset, NULL, false)) {// history is managed here not in loadPresetOrShape(), since if init file not found we will do resets in the next lines
			if (isPreset) {
				channel->onReset(true);
			}
			else {
				channel->resetShape();
			}
		}
		
		if (isPreset) {
			h->newJson = channel->dataToJsonChannel(WITH_PARAMS, WITHOUT_PRO_UNSYNC_MATCH, WITHOUT_FULL_SETTINGS);
			h->name = "initialize preset";
		}
		else {
			h->newJson = h->shapeSrc->dataToJsonShape();
			h->name = "initialize shape";
		}
		APP->history->push(h);
	}
};

struct DeferredItem : MenuItem {
	int8_t *srcMiscSettingDeferral;
	void onAction(const event::Action &e) override {
		*srcMiscSettingDeferral ^= 0x1;
	}
};


void PresetAndShapeManager::createPresetOrShapeMenu(Channel* channel, bool isPreset) {
	ui::Menu *menu = createMenu();
	std::string dir;

	MenuLabel *titleLabel = new MenuLabel();
	titleLabel->text = (isPreset ? "Channel presets" : "Shapes");
	menu->addChild(titleLabel);
		
	menu->addChild(new MenuSeparator());

	// MindMeld presets or shapes
	dir = asset::plugin(pluginInstance, factoryPrefix + (isPreset ? "MindMeldPresets" : "MindMeldShapes"));
	appendDirMenu(dir, menu, channel, isPreset);

	// Community presets or shapes
	dir = asset::plugin(pluginInstance, factoryPrefix + (isPreset ? "CommunityPresets" : "CommunityShapes"));
	appendDirMenu(dir, menu, channel, isPreset);

	// User presets or shapes
	std::string userPath = getUserPath(isPreset);
	appendDirMenu(userPath, menu, channel, isPreset);
	
	// Defer until EOC
	DeferredItem *deferredItem = createMenuItem<DeferredItem>("Defer prev/next until EOC", CHECKMARK(miscSettings3->cc4[isPreset ? 0 : 1] != 0));
	deferredItem->srcMiscSettingDeferral = &(miscSettings3->cc4[isPreset ? 0 : 1]);
	menu->addChild(deferredItem);
	
	
	menu->addChild(new MenuSeparator());

	MenuLabel *actLabel = new MenuLabel();
	actLabel->text = "Actions";
	menu->addChild(actLabel);
			
	// Save user preset or shape
	SaveUserSubItem *saveUserItem = createMenuItem<SaveUserSubItem>(isPreset ? "Save user preset" : "Save user shape", "");
	saveUserItem->channel = channel;
	saveUserItem->channelDirtyCache = channelDirtyCacheSrc;
	saveUserItem->isPreset = isPreset;
	menu->addChild(saveUserItem);
	
	ClearDisplayItem* clrItem = createMenuItem<ClearDisplayItem>("Clear label");
	clrItem->channel = channel;
	clrItem->isPreset = isPreset;
	menu->addChild(clrItem);
	
	menu->addChild(new MenuSeparator());

	SaveInitPresetOrShapeItem* saveInitPSItem = createMenuItem<SaveInitPresetOrShapeItem>(isPreset ? "Save preset as init" : "Save shape as init");
	saveInitPSItem->channel = channel;
	saveInitPSItem->isPreset = isPreset;
	saveInitPSItem->initFilePath = userPath + "/~init." + (isPreset ? "smpr" : "smsh");
	saveInitPSItem->channelDirtyCacheSrc = channelDirtyCacheSrc;
	menu->addChild(saveInitPSItem);
	
	LoadInitPresetOrShapeItem* loadInitPSItem = createMenuItem<LoadInitPresetOrShapeItem>(isPreset ? "Initialize preset" : "Initialize shape");
	loadInitPSItem->channel = channel;
	loadInitPSItem->isPreset = isPreset;
	loadInitPSItem->initFilePath = userPath + "/~init." + (isPreset ? "smpr" : "smsh");
	menu->addChild(loadInitPSItem);
	
}


struct PresetOrShapeItem : MenuItem {
	std::string path;
	Channel* channel;
	bool isPreset;

	void onAction(const event::Action &e) override {
		INFO((isPreset ? "Loading ShapeMaster channel preset %s" : "Loading ShapeMaster shape %s"), path.c_str());
		loadPresetOrShape(path, channel, isPreset, NULL, true);
	}
};


struct DirectoryItem : MenuItem {
	std::string pathToScan;
	Channel* channel;
	bool isPreset;

	Menu *createChildMenu() override {
		Menu *menu = new Menu;

		std::list<std::string> entries = system::getEntries(pathToScan);
		std::string presetOrShapeExt = (isPreset ? "smpr" : "smsh");
		
		for (std::string entry : entries) {
			if (system::isFile(entry)) {
				if (!(string::filenameExtension(entry) == presetOrShapeExt)) {
					continue;
				}
				std::string presetOrShapeBase = string::filenameBase(string::filename(entry));
				PresetOrShapeItem *prstOrShpItem = createMenuItem<PresetOrShapeItem>(presetOrShapeBase.c_str(), "");
				prstOrShpItem->path = entry;
				prstOrShpItem->channel = channel;
				prstOrShpItem->isPreset = isPreset;
				menu->addChild(prstOrShpItem);
			}
			else {
				appendDirMenu(entry, menu, channel, isPreset);	
			}
		}	

		return menu;
	}
};

void appendDirMenu(std::string dirPath, Menu* menu, Channel* channel, bool isPreset) {
	std::string dirName = string::filenameBase(string::filename(dirPath));
	DirectoryItem *dirItem = createMenuItem<DirectoryItem>(dirName, RIGHT_ARROW);
	dirItem->pathToScan = dirPath;
	dirItem->channel = channel;
	dirItem->isPreset = isPreset;
	menu->addChild(dirItem);	
}
	
	


