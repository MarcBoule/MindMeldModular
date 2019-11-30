//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//Based on code from the Fundamental plugins by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************

#include "MindMeldModular.hpp"


Plugin *pluginInstance;
MessageBus<MixerPayload> mixerMessageBus;

void init(rack::Plugin *p) {
	pluginInstance = p;

	p->addModel(modelMixMasterJr);
	p->addModel(modelAuxExpanderJr);
	p->addModel(modelMixMaster);
	p->addModel(modelAuxExpander);
	p->addModel(modelMeld);
	p->addModel(modelUnmeld);
	p->addModel(modelLabelTester);
}


// General objects
// none


// General functions
// none
