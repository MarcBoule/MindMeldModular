//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//Based on code from the Fundamental plugins by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************

#include "MindMeldModular.hpp"


Plugin *pluginInstance;
// MessageBus<Payload> *messages;

void init(rack::Plugin *p) {
	pluginInstance = p;
	// messages = new MessageBus<Payload>();

	p->addModel(modelMixMaster);
	p->addModel(modelAuxExpander);
}


// General objects
// none


// General functions
// none
