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
bool oldMotherPresent = false;
time_t oldTime = 0;


void step() override {
	if (module) {
		AuxExpander<N_TRK, N_GRP>* moduleA = (AuxExpander<N_TRK, N_GRP>*)module;
		
		// Labels (pull from module)
		if (moduleA->updateAuxLabelRequest != 0) {// pull request from module
			// aux displays
			for (int aux = 0; aux < 4; aux++) {
				auxDisplays[aux]->text = std::string(&(moduleA->auxLabels[aux * 4]), 4);
			}
			moduleA->updateAuxLabelRequest = 0;// all done pulling
		}
		if (moduleA->updateTrackLabelRequest != 0) {// pull request from module
			// track and group labels
			for (int trk = 0; trk < (N_TRK + N_GRP); trk++) {
				trackAndGroupLabels[trk]->text = std::string(&(moduleA->trackLabels[trk * 4]), 4);
			}
			moduleA->updateTrackLabelRequest = 0;// all done pulling
		}
		
		// Borders			
		if ( moduleA->motherPresent != oldMotherPresent ) {
			oldMotherPresent = moduleA->motherPresent;
			if (oldMotherPresent) {
				panelBorder->box.pos.x = -3;
				panelBorder->box.size.x = box.size.x + 3;
			}
			else {
				panelBorder->box.pos.x = 0;
				panelBorder->box.size.x = box.size.x;
			}
			((SvgPanel*)panel)->dirty = true;// weird zoom bug: if the if/else above is commented, zoom bug when this executes
		}
		
		// Update param tooltips at 1Hz
		time_t currentTime = time(0);
		if (currentTime != oldTime) {
			oldTime = currentTime;
			char strBuf[32];
			std::string auxLabels[4];
			for (int i = 0; i < 4; i++) {
				auxLabels[i] = std::string(&(moduleA->auxLabels[i * 4]), 4);
			}
			
			// Track and group indiv sends
			for (int i = 0; i < (N_TRK + N_GRP); i++) {
				std::string trackLabel = std::string(&(moduleA->trackLabels[i * 4]), 4);
				// Aux A-D
				for (int auxi = 0; auxi < 4; auxi++) {
					snprintf(strBuf, 32, "%s: send %s", trackLabel.c_str(), auxLabels[auxi].c_str());
					moduleA->paramQuantities[TAuxExpander::TRACK_AUXSEND_PARAMS + i * 4 + auxi]->label = strBuf;
				}
				// Mutes
				snprintf(strBuf, 32, "%s: send mute", trackLabel.c_str());
				moduleA->paramQuantities[TAuxExpander::TRACK_AUXMUTE_PARAMS + i]->label = strBuf;
			}

			for (int auxi = 0; auxi < 4; auxi++) {
				// Global send aux A-D
				snprintf(strBuf, 32, "%s: global send", auxLabels[auxi].c_str());
				moduleA->paramQuantities[TAuxExpander::GLOBAL_AUXSEND_PARAMS + auxi]->label = strBuf;
				// Global pan return aux A-D
				snprintf(strBuf, 32, "%s: return pan", auxLabels[auxi].c_str());
				moduleA->paramQuantities[TAuxExpander::GLOBAL_AUXPAN_PARAMS + auxi]->label = strBuf;
				// Global return aux A-D
				snprintf(strBuf, 32, "%s: return level", auxLabels[auxi].c_str());
				moduleA->paramQuantities[TAuxExpander::GLOBAL_AUXRETURN_PARAMS + auxi]->label = strBuf;
				// Global mute/fade
				if (moduleA->auxFadeRatesAndProfiles[auxi] >= GlobalConst::minFadeRate) {
					snprintf(strBuf, 32, "%s: return fade", auxLabels[auxi].c_str());
				}
				else {
					snprintf(strBuf, 32, "%s: return mute", auxLabels[auxi].c_str());
				}
				moduleA->paramQuantities[TAuxExpander::GLOBAL_AUXMUTE_PARAMS + auxi]->label = strBuf;
				// Global solo
				snprintf(strBuf, 32, "%s: return solo", auxLabels[auxi].c_str());
				moduleA->paramQuantities[TAuxExpander::GLOBAL_AUXSOLO_PARAMS + auxi]->label = strBuf;
				// Global return group select
				snprintf(strBuf, 32, "%s: return group", auxLabels[auxi].c_str());
				moduleA->paramQuantities[TAuxExpander::GLOBAL_AUXGROUP_PARAMS + auxi]->label = strBuf;
			}
		}
	}
	Widget::step();
}
