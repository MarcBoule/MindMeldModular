//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


typedef AuxExpander<N_TRK, N_GRP> TAuxExpander;

AuxDisplay<TAuxExpander::AuxspanderAux>* auxDisplays[4];
TrackAndGroupLabel* trackAndGroupLabels[N_TRK + N_GRP];
PanelBorder* panelBorder;
time_t oldTime = 0;



// Module's context menu
// --------------------

struct AuxspanderInterchangeItem : MenuItem {
	TAuxExpander* module;
	
	struct AuxspanderChangeCopyItem : MenuItem {
		TAuxExpander* module;
		void onAction(const event::Action &e) override {
			module->swapCopyToClipboard();
		}
	};

	struct AuxspanderChangePasteItem : MenuItem {
		TAuxExpander* module;
		void onAction(const event::Action &e) override {
			module->swapPasteFromClipboard();
		}
	};

	Menu *createChildMenu() override {
		Menu *menu = new Menu;
		
		AuxspanderChangeCopyItem *acCopyItem = createMenuItem<AuxspanderChangeCopyItem>("Copy auxspander", "");
		acCopyItem->module = module;
		menu->addChild(acCopyItem);
		
		menu->addChild(new MenuSeparator());

		AuxspanderChangePasteItem *acPasteItem = createMenuItem<AuxspanderChangePasteItem>("Paste auxspander", "");
		acPasteItem->module = module;
		menu->addChild(acPasteItem);

		return menu;
	}
};



void appendContextMenu(Menu *menu) override {		
	TAuxExpander* module = (TAuxExpander*)(this->module);
	assert(module);
	
	AuxspanderInterchangeItem *interchangeItem = createMenuItem<AuxspanderInterchangeItem>("AuxSpander swap", RIGHT_ARROW);
	interchangeItem->module = module;
	menu->addChild(interchangeItem);
}

	

void step() override {
	if (module) {
		TAuxExpander* module = (TAuxExpander*)(this->module);
		
		// Labels (pull from module)
		if (module->updateAuxLabelRequest != 0) {// pull request from module
			// aux displays
			for (int aux = 0; aux < 4; aux++) {
				auxDisplays[aux]->text = std::string(&(module->auxLabels[aux * 4]), 4);
			}
			module->updateAuxLabelRequest = 0;// all done pulling
		}
		if (module->updateTrackLabelRequest != 0) {// pull request from module
			// track and group labels
			for (int trk = 0; trk < (N_TRK + N_GRP); trk++) {
				trackAndGroupLabels[trk]->text = std::string(&(module->trackLabels[trk * 4]), 4);
			}
			module->updateTrackLabelRequest = 0;// all done pulling
		}
		
		// Borders			
		int newSizeAdd = (module->motherPresent ? 3 : 0);
		if (panelBorder->box.size.x != (box.size.x + newSizeAdd)) {
			panelBorder->box.pos.x = -newSizeAdd;
			panelBorder->box.size.x = (box.size.x + newSizeAdd);
			SvgPanel* svgPanel = (SvgPanel*)getPanel();
			svgPanel->fb->dirty = true;
		}
		
		// Update param and port tooltips at 1Hz
		time_t currentTime = time(0);
		if (currentTime != oldTime) {
			oldTime = currentTime;
			char strBuf[32];
			std::string auxLabels[4];
			for (int i = 0; i < 4; i++) {
				auxLabels[i] = std::string(&(module->auxLabels[i * 4]), 4);
				module->inputInfos[TAuxExpander::RETURN_INPUTS + 2 * i + 0]->name = string::f("%s return left", auxLabels[i].c_str());
				module->inputInfos[TAuxExpander::RETURN_INPUTS + 2 * i + 1]->name = string::f("%s return right", auxLabels[i].c_str());
				module->outputInfos[TAuxExpander::SEND_OUTPUTS + i]->name = string::f("%s send left", auxLabels[i].c_str());
				module->outputInfos[TAuxExpander::SEND_OUTPUTS + i + 4]->name = string::f("%s send right", auxLabels[i].c_str());
			}
			
			// Track and group indiv sends
			for (int i = 0; i < (N_TRK + N_GRP); i++) {
				std::string trackLabel = std::string(&(module->trackLabels[i * 4]), 4);
				// Aux A-D
				for (int auxi = 0; auxi < 4; auxi++) {
					snprintf(strBuf, 32, "%s: send %s", trackLabel.c_str(), auxLabels[auxi].c_str());
					module->paramQuantities[TAuxExpander::TRACK_AUXSEND_PARAMS + i * 4 + auxi]->name = strBuf;
				}
				// Mutes
				snprintf(strBuf, 32, "%s: send mute", trackLabel.c_str());
				module->paramQuantities[TAuxExpander::TRACK_AUXMUTE_PARAMS + i]->name = strBuf;
			}

			for (int auxi = 0; auxi < 4; auxi++) {
				// Global send aux A-D
				snprintf(strBuf, 32, "%s: global send", auxLabels[auxi].c_str());
				module->paramQuantities[TAuxExpander::GLOBAL_AUXSEND_PARAMS + auxi]->name = strBuf;
				// Global pan return aux A-D
				snprintf(strBuf, 32, "%s: return pan", auxLabels[auxi].c_str());
				module->paramQuantities[TAuxExpander::GLOBAL_AUXPAN_PARAMS + auxi]->name = strBuf;
				// Global return aux A-D
				snprintf(strBuf, 32, "%s: return level", auxLabels[auxi].c_str());
				module->paramQuantities[TAuxExpander::GLOBAL_AUXRETURN_PARAMS + auxi]->name = strBuf;
				// Global mute/fade
				if (module->auxFadeRatesAndProfiles[auxi] >= GlobalConst::minFadeRate) {
					snprintf(strBuf, 32, "%s: return fade", auxLabels[auxi].c_str());
				}
				else {
					snprintf(strBuf, 32, "%s: return mute", auxLabels[auxi].c_str());
				}
				module->paramQuantities[TAuxExpander::GLOBAL_AUXMUTE_PARAMS + auxi]->name = strBuf;
				// Global solo
				snprintf(strBuf, 32, "%s: return solo", auxLabels[auxi].c_str());
				module->paramQuantities[TAuxExpander::GLOBAL_AUXSOLO_PARAMS + auxi]->name = strBuf;
				// Global return group select
				snprintf(strBuf, 32, "%s: return group", auxLabels[auxi].c_str());
				module->paramQuantities[TAuxExpander::GLOBAL_AUXGROUP_PARAMS + auxi]->name = strBuf;
			}
			
			// more port labels
			if (N_TRK > 8) {
				module->inputInfos[TAuxExpander::POLY_AUX_AD_CV_INPUTS + 0]->name = "Track aux A sends";
				module->inputInfos[TAuxExpander::POLY_AUX_AD_CV_INPUTS + 1]->name = "Track aux B sends";
				module->inputInfos[TAuxExpander::POLY_AUX_AD_CV_INPUTS + 2]->name = "Track aux C sends";
				module->inputInfos[TAuxExpander::POLY_AUX_AD_CV_INPUTS + 3]->name = "Track aux D sends";
				module->inputInfos[TAuxExpander::POLY_AUX_M_CV_INPUT]->name = "Track aux send mutes";
				module->inputInfos[TAuxExpander::POLY_GRPS_M_CV_INPUT]->name = "Group aux send mutes";
			}
			else {
				module->inputInfos[TAuxExpander::POLY_AUX_AD_CV_INPUTS + 0]->name = "Track aux A/B sends";
				module->inputInfos[TAuxExpander::POLY_AUX_AD_CV_INPUTS + 1]->name = "Track aux C/D sends";
				module->inputInfos[TAuxExpander::POLY_AUX_M_CV_INPUT]->name = "Track and group aux send mutes";
			}
			module->inputInfos[TAuxExpander::POLY_GRPS_AD_CV_INPUT]->name = "Group aux sends";
			module->inputInfos[TAuxExpander::POLY_BUS_SND_PAN_RET_CV_INPUT]->name = "Global bus send/pan/return";
			module->inputInfos[TAuxExpander::POLY_BUS_MUTE_SOLO_CV_INPUT]->name = "Return mute/solo";
		}
	}
	ModuleWidget::step();
}



struct MismatchedAuxOverlay : widget::Widget {
	Module* module;
	static const int margin = 90;
	
	MismatchedAuxOverlay(Vec boxsize, Module* _module) {
		box.size = boxsize;
		module = _module;
	}
	
	void drawLayer(const DrawArgs& args, int layer) override {
		Widget::drawLayer(args, layer);
		if (layer == 1 && module != NULL) {
			bool badMother = (module->leftExpander.module && module->leftExpander.module->model == (N_TRK == 16 ? modelMixMasterJr : modelMixMaster));
			if (badMother) {
				nvgBeginPath(args.vg);
				nvgRect(args.vg, 0 + margin, 0 + margin, box.size.x - 2 * margin, box.size.y - 2 * margin);
				nvgFillColor(args.vg, nvgRGBAf(0, 0, 0, 0.6));
				nvgFill(args.vg);

				std::string text = "Mixer - AuxSpander mismatch";
				std::string text2 = N_TRK == 16 ? "Please use the 8-track AuxSpander Jr" : "Please use the 16-track AuxSpander";
				float ofx = bndLabelWidth(args.vg, -1, text.c_str()) + 2;
				float ofy = bndLabelHeight(args.vg, -1, text.c_str(), ofx);
				Rect r;
				r.pos = box.size.div(2).minus(Vec(ofx, ofy).div(2));
				r.size = Vec(ofx, ofy);
				bndLabel(args.vg, RECT_ARGS(r), -1, text.c_str());
				ofx = bndLabelWidth(args.vg, -1, text2.c_str()) + 2;
				ofy = bndLabelHeight(args.vg, -1, text2.c_str(), ofx);
				r.pos = box.size.div(2).minus(Vec(ofx, -ofy).div(2));
				r.size = Vec(ofx, ofy);
				bndLabel(args.vg, RECT_ARGS(r), -1, text2.c_str());
			}
		}
	}
};