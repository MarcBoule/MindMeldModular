//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//Based on code from the Fundamental plugin by Andrew Belt
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "DisplayUtil.hpp"


void ScopeBuffers::populate(Channel* channel, int8_t scopeSettings) {
	int newState = channel->getState();
	int8_t newTrigMode = channel->getTrigMode();
	bool needClear = (lastChannel != channel) || (newState == PlayHead::STEPPING && lastState == PlayHead::STOPPED) || lastTrigMode != newTrigMode;
	if (needClear) {
		lastChannel = channel;
		lastTrigMode = newTrigMode;
		clear();
	}
	lastState = newState;// must be outside since newState can be different from lastState but not have triggered a clear
	
	float scpIf = channel->getScopePosition();
	if (scpIf != -1.0f && channel->getChannelActive() && (scopeSettings & SCOPE_MASK_ON) != 0) {
		scopeOn = true;
		scopeVca = ((scopeSettings & SCOPE_MASK_VCA_nSC) != 0);
		if (newState == PlayHead::STEPPING) {
			int scpI = (int)(scpIf * SCOPE_PTS + 0.5f);
			setPoint(scpI);
			float frontVal;
			float backVal;
			if (scopeVca) {
				int8_t polySelect = channel->channelSettings2.cc4[0];
				
				// VcaPost
				int postSize = channel->getVcaPostSize();
				if (polySelect < 16 && postSize > polySelect) {
					frontVal = channel->getVcaPost(polySelect);
				}
				else if (polySelect == 16 && postSize > 0) {
					frontVal = channel->getVcaPost(0);
					if (postSize > 1) {
						frontVal = (frontVal + channel->getVcaPost(1)) * 0.5f;
					}
				}
				else {
					frontVal = 0.0f;
				}
				
				// VcaPre
				int preSize = channel->getVcaPreSize();
				if (polySelect < 16 && preSize > polySelect) {
					backVal = channel->getVcaPre(polySelect);
				}
				else if (polySelect == 16 && preSize > 0) {
					backVal = channel->getVcaPre(0);
					if (preSize > 1) {
						backVal = (backVal + channel->getVcaPre(1)) * 0.5f;
					}
				}
				else {
					backVal = 0.0f;
				}
			}
			else {
				frontVal = channel->getScEnvelope();
				backVal = channel->getScSignal();			
			}
			if (lastScpI != scpI) {
				lastScpI = scpI;
				// new point, so set min and max to new val
				scpFrontYmin[scpI] = scpFrontYmax[scpI] = frontVal;
				scpBackYmin[scpI] = scpBackYmax[scpI] = backVal;
			}
			else {
				// same point, write val to min or max
				if (frontVal > scpFrontYmax[scpI]) {
					scpFrontYmax[scpI] = frontVal;
				}
				else if (frontVal < scpFrontYmin[scpI]) {
					scpFrontYmin[scpI] = frontVal;
				}	
				if (backVal > scpBackYmax[scpI]) {
					scpBackYmax[scpI] = backVal;
				}
				else if (backVal < scpBackYmin[scpI]) {
					scpBackYmin[scpI] = backVal;
				}	
			}
		}
	}
	else {
		scopeOn = false;
	}
}
