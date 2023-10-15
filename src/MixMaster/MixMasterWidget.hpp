//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


typedef MixMaster<N_TRK, N_GRP> TMixMaster;

MasterDisplay* masterDisplay;
TrackDisplay<TMixMaster::MixerTrack>* trackDisplays[N_TRK];
GroupDisplay<TMixMaster::MixerGroup>* groupDisplays[N_GRP];
PortWidget* inputWidgets[N_TRK * 4];// Left, Right, Volume, Pan
PanelBorder* panelBorder;
time_t oldTime = 0;



// Module's context menu
// --------------------

struct MixerInterchangeItem : MenuItem {
	TMixMaster* module;
	
	struct MixerChangeCopyItem : MenuItem {
		TMixMaster* module;
		void onAction(const event::Action &e) override {
			module->swapCopyToClipboard();
		}
	};

	struct MixerChangePasteItem : MenuItem {
		TMixMaster* module;
		void onAction(const event::Action &e) override {
			module->swapPasteFromClipboard();
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		
		MixerChangeCopyItem *mcCopyItem = createMenuItem<MixerChangeCopyItem>("Copy mixer", "");
		mcCopyItem->module = module;
		menu->addChild(mcCopyItem);
		
		menu->addChild(new MenuSeparator());

		MixerChangePasteItem *mcPasteItem = createMenuItem<MixerChangePasteItem>("Paste mixer", "");
		mcPasteItem->module = module;
		menu->addChild(mcPasteItem);

		return menu;
	}
};



void appendContextMenu(Menu *menu) override {		
	TMixMaster* module = static_cast<TMixMaster*>(this->module);
	assert(module);
	
	MixerInterchangeItem *interchangeItem = createMenuItem<MixerInterchangeItem>("MixMaster swap", RIGHT_ARROW);
	interchangeItem->module = module;
	menu->addChild(interchangeItem);


	menu->addChild(new MenuSeparator());
	
	menu->addChild(createMenuLabel("Audio settings"));
	
	FilterPosItem *filterPosItem = createMenuItem<FilterPosItem>("Filters", RIGHT_ARROW);
	filterPosItem->filterPosSrc = &(module->gInfo->filterPos);
	filterPosItem->isGlobal = true;
	filterPosItem->localOp = &(module->globalToLocalOp);
	menu->addChild(filterPosItem);
	
	PanLawMonoItem *panLawMonoItem = createMenuItem<PanLawMonoItem>("Mono pan law", RIGHT_ARROW);
	panLawMonoItem->panLawMonoSrc = &(module->gInfo->panLawMono);
	menu->addChild(panLawMonoItem);
	
	PanLawStereoItem *panLawStereoItem = createMenuItem<PanLawStereoItem>("Stereo pan mode", RIGHT_ARROW);
	panLawStereoItem->panLawStereoSrc = &(module->gInfo->directOutPanStereoMomentCvLinearVol.cc4[1]);
	panLawStereoItem->isGlobal = true;
	panLawStereoItem->localOp = &(module->globalToLocalOp);
	menu->addChild(panLawStereoItem);
	
	ChainItem *chainItem = createMenuItem<ChainItem>("Chain input", RIGHT_ARROW);
	chainItem->chainModeSrc = &(module->gInfo->chainMode);
	menu->addChild(chainItem);
	
	DirectOutsModeItem *directOutsItem = createMenuItem<DirectOutsModeItem>("Direct outs", RIGHT_ARROW);
	directOutsItem->tapModePtr = &(module->gInfo->directOutPanStereoMomentCvLinearVol.cc4[0]);
	directOutsItem->isGlobal = true;
	directOutsItem->localOp = &(module->globalToLocalOp);
	directOutsItem->directOutsSkipGroupedTracksPtr = &(module->gInfo->directOutsSkipGroupedTracks);
	menu->addChild(directOutsItem);
	
	MomentaryCvModeItem *momentItem = createMenuItem<MomentaryCvModeItem>("Mute/Solo CV", RIGHT_ARROW);
	momentItem->momentaryCvButtonsSrc = &(module->gInfo->directOutPanStereoMomentCvLinearVol.cc4[2]);
	momentItem->isGlobal = true;
	momentItem->localOp = &(module->globalToLocalOp);
	menu->addChild(momentItem);

	FadeSettingsItem *fadItem = createMenuItem<FadeSettingsItem>("Fades", RIGHT_ARROW);
	fadItem->symmetricalFadeSrc = &(module->gInfo->symmetricalFade);
	fadItem->fadeCvOutsWithVolCvSrc = &(module->gInfo->fadeCvOutsWithVolCv);
	menu->addChild(fadItem);
	
	LinCvItem *lincv0Item = createMenuItem<LinCvItem>("Vol CV inputs", RIGHT_ARROW);
	lincv0Item->linearVolCvInputsSrc = &(module->gInfo->directOutPanStereoMomentCvLinearVol.cc4[3]);
	menu->addChild(lincv0Item);
	
	menu->addChild(createCheckMenuItem("Poly-spread V and P track 1", "",
		[=]() {return module->gInfo->polySpreadVandP != 0;},
		[=]() {module->gInfo->polySpreadVandP ^= 0x1;}
	));

	menu->addChild(createCheckMenuItem("Eco mode", "",
		[=]() {return module->gInfo->ecoMode != 0;},
		[=]() {module->gInfo->ecoMode = ~module->gInfo->ecoMode;}
	));

	if (module->auxExpanderPresent) {
		menu->addChild(new MenuSeparator());

		menu->addChild(createMenuLabel("AuxSpander"));
		
		AuxSendsItem *auxSendsItem = createMenuItem<AuxSendsItem>("Aux sends", RIGHT_ARROW);
		auxSendsItem->tapModePtr = &(module->gInfo->auxSendsMode);
		auxSendsItem->isGlobal = true;
		auxSendsItem->localOp = &(module->globalToLocalOp);
		auxSendsItem->groupsControlTrackSendLevelsSrc = &(module->gInfo->groupsControlTrackSendLevels);
		menu->addChild(auxSendsItem);
		
		AuxReturnItem *auxRetunsItem = createMenuItem<AuxReturnItem>("Aux returns", RIGHT_ARROW);
		auxRetunsItem->auxReturnsMutedWhenMainSoloPtr = &(module->gInfo->auxReturnsMutedWhenMainSolo);
		auxRetunsItem->auxReturnsSolosMuteDryPtr = &(module->gInfo->auxReturnsSolosMuteDry);
		menu->addChild(auxRetunsItem);
	
		AuxRetFbProtItem *fbpItem = createMenuItem<AuxRetFbProtItem>("Routing returns to groups", RIGHT_ARROW);
		fbpItem->groupedAuxReturnFeedbackProtectionSrc = &(module->gInfo->groupedAuxReturnFeedbackProtection);
		menu->addChild(fbpItem);
	}
	
	
	menu->addChild(new MenuSeparator());
	
	menu->addChild(createMenuLabel("Visual settings"));
	
	DispColorItem *dispColItem = createMenuItem<DispColorItem>("Display colour", RIGHT_ARROW);
	dispColItem->srcColor = &(module->gInfo->colorAndCloak.cc4[dispColorGlobal]);
	dispColItem->isGlobal = true;
	dispColItem->localOp = &(module->globalToLocalOp);
	menu->addChild(dispColItem);
	
	VuColorItem *vuColItem = createMenuItem<VuColorItem>("VU colour", RIGHT_ARROW);
	vuColItem->srcColor = &(module->gInfo->colorAndCloak.cc4[vuColorGlobal]);
	vuColItem->isGlobal = true;
	vuColItem->localOp = &(module->globalToLocalOp);
	menu->addChild(vuColItem);
	
	KnobArcShowItem *knobArcShowItem = createMenuItem<KnobArcShowItem>("Knob arcs", RIGHT_ARROW);
	knobArcShowItem->srcDetailsShow = &(module->gInfo->colorAndCloak.cc4[detailsShow]);
	menu->addChild(knobArcShowItem);
	
	CvPointerShowItem *cvPointerShowItem = createMenuItem<CvPointerShowItem>("Fader CV pointers", RIGHT_ARROW);
	cvPointerShowItem->srcDetailsShow = &(module->gInfo->colorAndCloak.cc4[detailsShow]);
	menu->addChild(cvPointerShowItem);
	
	menu->addChild(createCheckMenuItem("Cloaked mode", "",
		[=]() {return module->gInfo->colorAndCloak.cc4[cloakedMode];},
		[=]() {module->gInfo->colorAndCloak.cc4[cloakedMode] ^= 0xFF;}
	));
}


inline bool calcAuxPresent(Module* expModule) {
	return (expModule && (
			expModule->model == modelAuxExpander || 
			expModule->model == modelAuxExpanderJr));
}


void draw(const DrawArgs& args) override {
	if (module && calcAuxPresent(module->rightExpander.module)) {
		DrawArgs newDrawArgs = args;
		newDrawArgs.clipBox.size.x += mm2px(0.3f);// mixers panels have their base rectangle this much larger, to kill gap artifacts
		ModuleWidget::draw(newDrawArgs);
	}
	else {
		ModuleWidget::draw(args);
	}
}
	



void step() override {
	if (module) {
		TMixMaster* module = static_cast<TMixMaster*>(this->module);
		
		// Track labels (pull from module)
		if (module->updateTrackLabelRequest != 0) {// pull request from module
			// master display
			masterDisplay->text = std::string(&(module->master.masterLabel[0]), 6);
			// track displays
			for (int trk = 0; trk < N_TRK; trk++) {
				trackDisplays[trk]->text = std::string(&(module->trackLabels[trk * 4]), 4);
			}
			// group displays
			for (int grp = 0; grp < N_GRP; grp++) {
				groupDisplays[grp]->text = std::string(&(module->trackLabels[(N_TRK + grp) * 4]), 4);
			}
			module->updateTrackLabelRequest = 0;// all done pulling
		}
		
		// Borders
		int newSizeAdd = (module->auxExpanderPresent ? 3 : 0);
		if (panelBorder->box.size.x != (box.size.x + newSizeAdd)) {
			panelBorder->box.size.x = (box.size.x + newSizeAdd);
			SvgPanel* svgPanel = static_cast<SvgPanel*>(getPanel());
			svgPanel->fb->dirty = true;
		}
		
		// GlobalToLocal operation
		if (module->globalToLocalOp.opCodeMixer != GTOL_NOP) {
			// only mixer locals set here, auxspander locals set in ToExpander code in module->process()
			switch (module->globalToLocalOp.opCodeMixer) {
				case (GTOL_VUCOL) : {
					for (int i = 0; i < N_TRK; i++) {
						module->tracks[i].vuColorThemeLocal = module->globalToLocalOp.operand;
					}
					for (int i = 0; i < N_GRP; i++) {
						module->groups[i].vuColorThemeLocal = module->globalToLocalOp.operand;
					}
					module->master.vuColorThemeLocal = module->globalToLocalOp.operand;
					break;
				}
				case (GTOL_LABELCOL) : {
					for (int i = 0; i < N_TRK; i++) {
						module->tracks[i].dispColorLocal = module->globalToLocalOp.operand;
					}
					for (int i = 0; i < N_GRP; i++) {
						module->groups[i].dispColorLocal = module->globalToLocalOp.operand;
					}
					module->master.dispColorLocal = module->globalToLocalOp.operand;
					break;
				}
				case (GTOL_STEREOPAN) : {
					for (int i = 0; i < N_TRK; i++) {
						module->tracks[i].panLawStereo = module->globalToLocalOp.operand;
					}
					for (int i = 0; i < N_GRP; i++) {
						module->groups[i].panLawStereo = module->globalToLocalOp.operand;
					}
					break;
				}
				case (GTOL_AUXSENDS) : {
					for (int i = 0; i < N_TRK; i++) {
						module->tracks[i].auxSendsMode = module->globalToLocalOp.operand;
					}
					for (int i = 0; i < N_GRP; i++) {
						module->groups[i].auxSendsMode = module->globalToLocalOp.operand;
					}
					break;
				}
				case (GTOL_DIRECTOUTS) : {
					for (int i = 0; i < N_TRK; i++) {
						module->tracks[i].directOutsMode = module->globalToLocalOp.operand;
					}
					for (int i = 0; i < N_GRP; i++) {
						module->groups[i].directOutsMode = module->globalToLocalOp.operand;
					}
					break;
				}
				case (GTOL_FILTERPOS) : {
					for (int i = 0; i < N_TRK; i++) {
						module->tracks[i].filterPos = module->globalToLocalOp.operand;
					}
					for (int i = 0; i < N_GRP; i++) {
						module->groups[i].filterPos = module->globalToLocalOp.operand;
					}
					break;
				}
				case (GTOL_MOMENTCV) : {
					for (int i = 0; i < N_TRK; i++) {
						module->tracks[i].momentCvMuteLocal = module->globalToLocalOp.operand;
						module->tracks[i].momentCvSoloLocal = module->globalToLocalOp.operand;
					}
					for (int i = 0; i < N_GRP; i++) {
						module->groups[i].momentCvMuteLocal = module->globalToLocalOp.operand;
						module->groups[i].momentCvSoloLocal = module->globalToLocalOp.operand;
					}
					module->master.momentCvMuteLocal = module->globalToLocalOp.operand;
					module->master.momentCvDimLocal = module->globalToLocalOp.operand;
					module->master.momentCvMonoLocal = module->globalToLocalOp.operand;
					break;
				}
			}
			module->globalToLocalOp.opCodeMixer = GTOL_NOP;
		}
		
		// Update param and port tooltips and message bus at 1Hz (and filter lights also)
		time_t currentTime = time(0);
		if (currentTime != oldTime) {
			oldTime = currentTime;
			char strBuf[32];
			for (int i = 0; i < N_TRK; i++) {
				std::string trackLabel = std::string(&(module->trackLabels[i * 4]), 4);
				module->inputInfos[TMixMaster::TRACK_SIGNAL_INPUTS + 2 * i + 0]->name = string::f("%s left", trackLabel.c_str());
				module->inputInfos[TMixMaster::TRACK_SIGNAL_INPUTS + 2 * i + 1]->name = string::f("%s right", trackLabel.c_str());
				// Pan
				snprintf(strBuf, 32, "%s pan", trackLabel.c_str());
				module->paramQuantities[TMixMaster::TRACK_PAN_PARAMS + i]->name = strBuf;
				module->inputInfos[TMixMaster::TRACK_PAN_INPUTS + i]->name = strBuf;
				// Fader
				snprintf(strBuf, 32, "%s level", trackLabel.c_str());
				module->paramQuantities[TMixMaster::TRACK_FADER_PARAMS + i]->name = strBuf;
				module->inputInfos[TMixMaster::TRACK_VOL_INPUTS + i]->name = strBuf;
				// Mute/fade
				if (module->tracks[i].isFadeMode()) {
					snprintf(strBuf, 32, "%s fade", trackLabel.c_str());
				}
				else {
					snprintf(strBuf, 32, "%s mute", trackLabel.c_str());
				}
				module->paramQuantities[TMixMaster::TRACK_MUTE_PARAMS + i]->name = strBuf;
				// Solo
				snprintf(strBuf, 32, "%s solo", trackLabel.c_str());
				module->paramQuantities[TMixMaster::TRACK_SOLO_PARAMS + i]->name = strBuf;
				// Group select
				snprintf(strBuf, 32, "%s group", trackLabel.c_str());
				module->paramQuantities[TMixMaster::GROUP_SELECT_PARAMS + i]->name = strBuf;
				
				// HPF cutoff
				snprintf(strBuf, 32, "%s HPF cutoff", trackLabel.c_str());
				module->paramQuantities[TMixMaster::TRACK_HPCUT_PARAMS + i]->name = strBuf;
				// LPF cutoff
				snprintf(strBuf, 32, "%s LPF cutoff", trackLabel.c_str());
				module->paramQuantities[TMixMaster::TRACK_LPCUT_PARAMS + i]->name = strBuf;

			}
			// Group
			for (int i = 0; i < N_GRP; i++) {
				std::string groupLabel = std::string(&(module->trackLabels[(N_TRK + i) * 4]), 4);
				// Pan
				snprintf(strBuf, 32, "%s pan", groupLabel.c_str());
				module->paramQuantities[TMixMaster::GROUP_PAN_PARAMS + i]->name = strBuf;
				module->inputInfos[TMixMaster::GROUP_PAN_INPUTS + i]->name = strBuf;
				// Fader
				snprintf(strBuf, 32, "%s level", groupLabel.c_str());
				module->paramQuantities[TMixMaster::GROUP_FADER_PARAMS + i]->name = strBuf;
				module->inputInfos[TMixMaster::GROUP_VOL_INPUTS + i]->name = strBuf;
				// Mute/fade
				if (module->groups[i].isFadeMode()) {
					snprintf(strBuf, 32, "%s fade", groupLabel.c_str());
				}
				else {
					snprintf(strBuf, 32, "%s mute", groupLabel.c_str());
				}
				module->paramQuantities[TMixMaster::GROUP_MUTE_PARAMS + i]->name = strBuf;
				// Solo
				snprintf(strBuf, 32, "%s solo", groupLabel.c_str());
				module->paramQuantities[TMixMaster::GROUP_SOLO_PARAMS + i]->name = strBuf;
				
				// HPF cutoff
				snprintf(strBuf, 32, "%s HPF cutoff", groupLabel.c_str());
				module->paramQuantities[TMixMaster::GROUP_HPCUT_PARAMS + i]->name = strBuf;
				// LPF cutoff
				snprintf(strBuf, 32, "%s LPF cutoff", groupLabel.c_str());
				module->paramQuantities[TMixMaster::GROUP_LPCUT_PARAMS + i]->name = strBuf;
			}
			std::string masterLabel = std::string(module->master.masterLabel, 6);
			// Fader
			snprintf(strBuf, 32, "%s level", masterLabel.c_str());
			module->paramQuantities[TMixMaster::MAIN_FADER_PARAM]->name = strBuf;
			// Mute/fade
			if (module->master.isFadeMode()) {
				snprintf(strBuf, 32, "%s fade", masterLabel.c_str());
			}
			else {
				snprintf(strBuf, 32, "%s mute", masterLabel.c_str());
			}
			module->paramQuantities[TMixMaster::MAIN_MUTE_PARAM]->name = strBuf;
			// Dim
			snprintf(strBuf, 32, "%s dim", masterLabel.c_str());
			module->paramQuantities[TMixMaster::MAIN_DIM_PARAM]->name = strBuf;
			// Mono
			snprintf(strBuf, 32, "%s mono", masterLabel.c_str());
			module->paramQuantities[TMixMaster::MAIN_MONO_PARAM]->name = strBuf;

			// more input port tooltips
			module->inputInfos[TMixMaster::CHAIN_INPUTS + 0]->name = "Chain left";
			module->inputInfos[TMixMaster::CHAIN_INPUTS + 1]->name = "Chain right";
			module->inputInfos[TMixMaster::INSERT_TRACK_INPUTS + 0]->name = "Insert 1-8";
			if (N_TRK > 8) {
				module->inputInfos[TMixMaster::INSERT_TRACK_INPUTS + 1]->name = "Insert 9-16";
			}
			module->inputInfos[TMixMaster::INSERT_GRP_AUX_INPUT]->name = "Insert group/aux";
			if (N_TRK > 8) {
				module->inputInfos[TMixMaster::TRACK_MUTESOLO_INPUTS + 0]->name = "Track mute";
				module->inputInfos[TMixMaster::TRACK_MUTESOLO_INPUTS + 1]->name = "Track solo";
			}
			else {
				module->inputInfos[TMixMaster::TRACK_MUTESOLO_INPUTS + 0]->name = "Track mute/solo";
			}
			module->inputInfos[TMixMaster::GRPM_MUTESOLO_INPUT]->name = "Mute/solo group, CV master";
			
			//output port tooltips
			module->outputInfos[TMixMaster::DIRECT_OUTPUTS + 0]->name = "Direct 1-8";
			if (N_TRK > 8) {
				module->outputInfos[TMixMaster::DIRECT_OUTPUTS + 1]->name = "Direct 9-16";
				module->outputInfos[TMixMaster::DIRECT_OUTPUTS + 2]->name = "Direct group/aux";
			}
			else {
				module->outputInfos[TMixMaster::DIRECT_OUTPUTS + 1]->name = "Direct group/aux";
			}
			module->outputInfos[TMixMaster::MAIN_OUTPUTS + 0]->name = "Main left";
			module->outputInfos[TMixMaster::MAIN_OUTPUTS + 1]->name = "Main right";
			module->outputInfos[TMixMaster::INSERT_TRACK_OUTPUTS + 0]->name = "Insert 1-8";
			if (N_TRK > 8) {		
				module->outputInfos[TMixMaster::INSERT_TRACK_OUTPUTS + 1]->name = "Insert 9-16";
			}
			module->outputInfos[TMixMaster::INSERT_GRP_AUX_OUTPUT]->name = "Insert group/aux";
			module->outputInfos[TMixMaster::FADE_CV_OUTPUT]->name = "Fade CV";
			
			
			// Mixer Message Bus (for EQ and others)
			module->sendToMessageBus(false);// do not do track move init (would overwrite async track move writing)
			
			
			// Lights
			// ------
		
			for (int i = 0; i < N_TRK; i++) {
				module->lights[TMixMaster::TRACK_HPF_LIGHTS + i].setBrightness(module->tracks[i].getHPFCutoffFreq() >= GlobalConst::minHPFCutoffFreq ? 1.0f : 0.0f);
				module->lights[TMixMaster::TRACK_LPF_LIGHTS + i].setBrightness(module->tracks[i].getLPFCutoffFreq() <= GlobalConst::maxLPFCutoffFreq ? 1.0f : 0.0f);
			}
			for (int i = 0; i < N_GRP; i++) {
				module->lights[TMixMaster::GROUP_HPF_LIGHTS + i].setBrightness(module->groups[i].getHPFCutoffFreq() >= GlobalConst::minHPFCutoffFreq ? 1.0f : 0.0f);
				module->lights[TMixMaster::GROUP_LPF_LIGHTS + i].setBrightness(module->groups[i].getLPFCutoffFreq() <= GlobalConst::maxLPFCutoffFreq ? 1.0f : 0.0f);
			}
		}
	}			
	
	ModuleWidget::step();
}// void step()

