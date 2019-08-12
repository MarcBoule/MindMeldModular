//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//Based on code from the Fundamental plugins by Andrew Belt 
//See ./LICENSE.txt for all licenses
//***********************************************************************************************

#include "MindMeldModular.hpp"

#include "util/SqTime.h"
#ifdef _USE_WINDOWS_PERFTIME
    double SqTime::frequency = 0;
#endif

Plugin *pluginInstance;


void init(rack::Plugin *p) {
	pluginInstance = p;

	p->addModel(modelMixMasterJr);
}


// General objects
// none


// General functions
// none
