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

void appendContextMenu(Menu *menu) override {		
	TMixMaster* module = (TMixMaster*)(this->module);
	assert(module);

	menu->addChild(new MenuSeparator());
	
	MenuLabel *settingsALabel = new MenuLabel();
	settingsALabel->text = "Audio settings";
	menu->addChild(settingsALabel);
	
	FilterPosItem *filterPosItem = createMenuItem<FilterPosItem>("Filters", RIGHT_ARROW);
	filterPosItem->filterPosSrc = &(module->gInfo.filterPos);
	filterPosItem->isGlobal = true;
	menu->addChild(filterPosItem);
	
	PanLawMonoItem *panLawMonoItem = createMenuItem<PanLawMonoItem>("Mono pan law", RIGHT_ARROW);
	panLawMonoItem->panLawMonoSrc = &(module->gInfo.panLawMono);
	menu->addChild(panLawMonoItem);
	
	PanLawStereoItem *panLawStereoItem = createMenuItem<PanLawStereoItem>("Stereo pan mode", RIGHT_ARROW);
	panLawStereoItem->panLawStereoSrc = &(module->gInfo.panLawStereo);
	panLawStereoItem->isGlobal = true;
	menu->addChild(panLawStereoItem);
	
	ChainItem *chainItem = createMenuItem<ChainItem>("Chain input", RIGHT_ARROW);
	chainItem->chainModeSrc = &(module->gInfo.chainMode);
	menu->addChild(chainItem);
	
	TapModeItem *directOutsItem = createMenuItem<TapModeItem>("Direct outs", RIGHT_ARROW);
	directOutsItem->tapModePtr = &(module->gInfo.directOutsMode);
	directOutsItem->isGlobal = true;
	menu->addChild(directOutsItem);
	
	MomentaryCvItem *momentItem = createMenuItem<MomentaryCvItem>("Mute/Solo CV", RIGHT_ARROW);
	momentItem->momentaryCvButtonsSrc = &(module->gInfo.momentaryCvButtons);
	menu->addChild(momentItem);

	FadeSettingsItem *fadItem = createMenuItem<FadeSettingsItem>("Fades", RIGHT_ARROW);
	fadItem->symmetricalFadeSrc = &(module->gInfo.symmetricalFade);
	fadItem->fadeCvOutsWithVolCvSrc = &(module->gInfo.fadeCvOutsWithVolCv);
	menu->addChild(fadItem);
	
	EcoItem *eco0Item = createMenuItem<EcoItem>("Eco mode", CHECKMARK(module->gInfo.ecoMode));
	eco0Item->ecoModeSrc = &(module->gInfo.ecoMode);
	menu->addChild(eco0Item);
	
	if (module->auxExpanderPresent) {
		menu->addChild(new MenuSeparator());

		MenuLabel *settingsVLabel = new MenuLabel();
		settingsVLabel->text = "AuxSpander";
		menu->addChild(settingsVLabel);
		
		TapModePlusItem *auxSendsItem = createMenuItem<TapModePlusItem>("Aux sends", RIGHT_ARROW);
		auxSendsItem->tapModePtr = &(module->gInfo.auxSendsMode);
		auxSendsItem->isGlobal = true;
		auxSendsItem->groupsControlTrackSendLevelsSrc = &(module->gInfo.groupsControlTrackSendLevels);
		menu->addChild(auxSendsItem);
		
		AuxReturnItem *auxRetunsItem = createMenuItem<AuxReturnItem>("Aux returns", RIGHT_ARROW);
		auxRetunsItem->auxReturnsMutedWhenMainSoloPtr = &(module->gInfo.auxReturnsMutedWhenMainSolo);
		auxRetunsItem->auxReturnsSolosMuteDryPtr = &(module->gInfo.auxReturnsSolosMuteDry);
		menu->addChild(auxRetunsItem);
	
		AuxRetFbProtItem *fbpItem = createMenuItem<AuxRetFbProtItem>("Routing returns to groups", RIGHT_ARROW);
		fbpItem->groupedAuxReturnFeedbackProtectionSrc = &(module->gInfo.groupedAuxReturnFeedbackProtection);
		menu->addChild(fbpItem);
	}
	
	
	menu->addChild(new MenuSeparator());
	
	MenuLabel *settingsVLabel = new MenuLabel();
	settingsVLabel->text = "Visual settings";
	menu->addChild(settingsVLabel);
	
	DispColorItem *dispColItem = createMenuItem<DispColorItem>("Display colour", RIGHT_ARROW);
	dispColItem->srcColor = &(module->gInfo.colorAndCloak.cc4[dispColorGlobal]);
	dispColItem->isGlobal = true;
	menu->addChild(dispColItem);
	
	VuColorItem *vuColItem = createMenuItem<VuColorItem>("VU colour", RIGHT_ARROW);
	vuColItem->srcColor = &(module->gInfo.colorAndCloak.cc4[vuColorGlobal]);
	vuColItem->isGlobal = true;
	menu->addChild(vuColItem);
	
	KnobArcShowItem *knobArcShowItem = createMenuItem<KnobArcShowItem>("Knob arcs", RIGHT_ARROW);
	knobArcShowItem->srcDetailsShow = &(module->gInfo.colorAndCloak.cc4[detailsShow]);
	menu->addChild(knobArcShowItem);
	
	CvPointerShowItem *cvPointerShowItem = createMenuItem<CvPointerShowItem>("Fader CV pointers", RIGHT_ARROW);
	cvPointerShowItem->srcDetailsShow = &(module->gInfo.colorAndCloak.cc4[detailsShow]);
	menu->addChild(cvPointerShowItem);
	
	CloakedModeItem *nightItem = createMenuItem<CloakedModeItem>("Cloaked mode", CHECKMARK(module->gInfo.colorAndCloak.cc4[cloakedMode]));
	nightItem->colorAndCloakSrc = &(module->gInfo.colorAndCloak);
	menu->addChild(nightItem);
}


void step() override {
	if (module) {
		TMixMaster* module = (TMixMaster*)(this->module);
		
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
			((SvgPanel*)panel)->dirty = true;// weird zoom bug: if the if/else above is commented, zoom bug when this executes
		}
		
		// Update param tooltips and message bus at 1Hz
		time_t currentTime = time(0);
		if (currentTime != oldTime) {
			oldTime = currentTime;
			char strBuf[32];
			for (int i = 0; i < N_TRK; i++) {
				std::string trackLabel = std::string(&(module->trackLabels[i * 4]), 4);
				// Pan
				snprintf(strBuf, 32, "%s: pan", trackLabel.c_str());
				module->paramQuantities[TMixMaster::TRACK_PAN_PARAMS + i]->label = strBuf;
				// Fader
				snprintf(strBuf, 32, "%s: level", trackLabel.c_str());
				module->paramQuantities[TMixMaster::TRACK_FADER_PARAMS + i]->label = strBuf;
				// Mute/fade
				if (module->tracks[i].isFadeMode()) {
					snprintf(strBuf, 32, "%s: fade", trackLabel.c_str());
				}
				else {
					snprintf(strBuf, 32, "%s: mute", trackLabel.c_str());
				}
				module->paramQuantities[TMixMaster::TRACK_MUTE_PARAMS + i]->label = strBuf;
				// Solo
				snprintf(strBuf, 32, "%s: solo", trackLabel.c_str());
				module->paramQuantities[TMixMaster::TRACK_SOLO_PARAMS + i]->label = strBuf;
				// Group select
				snprintf(strBuf, 32, "%s: group", trackLabel.c_str());
				module->paramQuantities[TMixMaster::GROUP_SELECT_PARAMS + i]->label = strBuf;
				
				// HPF cutoff
				snprintf(strBuf, 32, "%s: HPF cutoff", trackLabel.c_str());
				module->paramQuantities[TMixMaster::TRACK_HPCUT_PARAMS + i]->label = strBuf;
				// LPF cutoff
				snprintf(strBuf, 32, "%s: LPF cutoff", trackLabel.c_str());
				module->paramQuantities[TMixMaster::TRACK_LPCUT_PARAMS + i]->label = strBuf;

			}
			// Group
			for (int i = 0; i < N_GRP; i++) {
				std::string groupLabel = std::string(&(module->trackLabels[(N_TRK + i) * 4]), 4);
				// Pan
				snprintf(strBuf, 32, "%s: pan", groupLabel.c_str());
				module->paramQuantities[TMixMaster::GROUP_PAN_PARAMS + i]->label = strBuf;
				// Fader
				snprintf(strBuf, 32, "%s: level", groupLabel.c_str());
				module->paramQuantities[TMixMaster::GROUP_FADER_PARAMS + i]->label = strBuf;
				// Mute/fade
				if (module->groups[i].isFadeMode()) {
					snprintf(strBuf, 32, "%s: fade", groupLabel.c_str());
				}
				else {
					snprintf(strBuf, 32, "%s: mute", groupLabel.c_str());
				}
				module->paramQuantities[TMixMaster::GROUP_MUTE_PARAMS + i]->label = strBuf;
				// Solo
				snprintf(strBuf, 32, "%s: solo", groupLabel.c_str());
				module->paramQuantities[TMixMaster::GROUP_SOLO_PARAMS + i]->label = strBuf;
			}
			std::string masterLabel = std::string(module->master.masterLabel, 6);
			// Fader
			snprintf(strBuf, 32, "%s: level", masterLabel.c_str());
			module->paramQuantities[TMixMaster::MAIN_FADER_PARAM]->label = strBuf;
			// Mute/fade
			if (module->master.isFadeMode()) {
				snprintf(strBuf, 32, "%s: fade", masterLabel.c_str());
			}
			else {
				snprintf(strBuf, 32, "%s: mute", masterLabel.c_str());
			}
			module->paramQuantities[TMixMaster::MAIN_MUTE_PARAM]->label = strBuf;
			// Dim
			snprintf(strBuf, 32, "%s: dim", masterLabel.c_str());
			module->paramQuantities[TMixMaster::MAIN_DIM_PARAM]->label = strBuf;
			// Mono
			snprintf(strBuf, 32, "%s: mono", masterLabel.c_str());
			module->paramQuantities[TMixMaster::MAIN_MONO_PARAM]->label = strBuf;

			// Mixer Message Bus (for EQ and others)
			module->sendToMessageBus();
		}
	}			
	
	ModuleWidget::step();
}// void step()

