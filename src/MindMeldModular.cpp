//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//Based on code from the Fundamental plugins by Andrew Belt 
//See ./LICENSE.txt for all licenses
//***********************************************************************************************

#include "MindMeldModular.hpp"


Plugin *pluginInstance;


void init(rack::Plugin *p) {
	pluginInstance = p;

	p->addModel(modelMixMasterJr);
}


// General objects
// none


// General functions
// none
