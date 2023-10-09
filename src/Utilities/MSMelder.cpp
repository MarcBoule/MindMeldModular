//***********************************************************************************************
//Split/merge to/from Left/Right that spaces out the signals for compatibility with EQ master
//Used for mid/side eq'ing with two EqMasters, using MixMaster inserts and VCV Mid/Side modules 
//For VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "../MindMeldModular.hpp"


struct MSMelder : Module {
	
	enum ParamIds {
		NUM_PARAMS
	};
	
	enum InputIds {
		A_INPUT,
		B_INPUT,
		C_INPUT,
		AL_INPUT,
		BL_INPUT,
		CL_INPUT,
		AR_INPUT,
		BR_INPUT,
		CR_INPUT,
		NUM_INPUTS
	};
	
	enum OutputIds {
		A_OUTPUT,
		B_OUTPUT,
		C_OUTPUT,
		AL_OUTPUT,
		BL_OUTPUT,
		CL_OUTPUT,
		AR_OUTPUT,
		BR_OUTPUT,
		CR_OUTPUT,
		NUM_OUTPUTS
	};
	
	enum LightIds {
		NUM_LIGHTS
	};
	

	// Constants
	// none


	// Need to save, no reset
	// none
	
	// Need to save, with reset
	// none
	
	// No need to save, with reset
	// none

	// No need to save, no reset
	RefreshCounter refresh;	
	
	
	MSMelder() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		const std::string channels[3] = {"1-8", "9-16", "Grp/aux"};
		
		for (int i = 0; i < 3; i++) {
			configInput(A_INPUT + i, std::string(" aggregate").insert(0, channels[i]));
			configInput(AL_INPUT + i, std::string(" left").insert(0, channels[i]));
			configInput(AR_INPUT + i, std::string(" right").insert(0, channels[i]));
			configOutput(A_OUTPUT + i, std::string(" aggregate").insert(0, channels[i]));
			configOutput(AL_OUTPUT + i, std::string(" left").insert(0, channels[i]));
			configOutput(AR_OUTPUT + i, std::string(" right").insert(0, channels[i]));
		}
		onReset();
	}
  
	void onReset() override final {
		resetNonJson(false);
	}
	void resetNonJson(bool recurseNonJson) {
	}


	void onRandomize() override {
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		return rootJ;
	}


	void dataFromJson(json_t *rootJ) override {
		resetNonJson(true);	
	}


	void onSampleRateChange() override {
	}
	

	void process(const ProcessArgs &args) override {
		
		// Inputs and slow
		if (refresh.processInputs()) {
			// Channel sizes of output ports
			for (int i = 0; i < 3; i++) {
				// split
				int inSplitChans = inputs[A_INPUT + i].getChannels();
				int outSplitChans = (inSplitChans + 1) & ~0x1;// make even (up)
				outputs[AL_OUTPUT + i].setChannels(outSplitChans);
				outputs[AR_OUTPUT + i].setChannels(outSplitChans);
				
				// merge
				// int inMergeLChans = inputs[AL_INPUT + i].getChannels();
				// int inMergeRChans = inputs[AR_INPUT + i].getChannels();
				// int outMergeChans = std::max((inMergeLChans + 1) & ~0x1, (inMergeRChans + 1) & ~0x1);// make even (up)
				outputs[A_OUTPUT + i].setChannels(outSplitChans);//outMergeChans);
			}
		}// userInputs refresh
		
		
		// Outputs
		for (int i = 0; i < 3; i++) {
			int chansDiv2 = inputs[A_INPUT + i].getChannels() >> 1;
			for (int c = 0; c < chansDiv2; c++) {
				// split
				outputs[AL_OUTPUT + i].setVoltage(inputs[A_INPUT + i].getVoltage(c * 2 + 0), c * 2 + 0);
				outputs[AL_OUTPUT + i].setVoltage(0.0f, c * 2 + 1);
				
				outputs[AR_OUTPUT + i].setVoltage(inputs[A_INPUT + i].getVoltage(c * 2 + 1), c * 2 + 0);
				outputs[AR_OUTPUT + i].setVoltage(0.0f, c * 2 + 1);
				
				// merge
				outputs[A_OUTPUT + i].setVoltage(inputs[AL_INPUT + i].getVoltage(c * 2 + 0), c * 2 + 0);
				outputs[A_OUTPUT + i].setVoltage(inputs[AR_INPUT + i].getVoltage(c * 2 + 0), c * 2 + 1);
			}
		}
		
		
		// Lights
		if (refresh.processLights()) {
			// none
		}
	}// process()
};


//-----------------------------------------------------------------------------


struct MSMelderWidget : ModuleWidget {
	MSMelderWidget(MSMelder *module) {
		setModule(module);

		// Main panels from Inkscape
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/ms-melder.svg")));
		
		static const float top = 18.95f;// first row of jacks
		static const float dy2 = 36.95f;// sections
		
		for (int i = 0; i < 3; i++) {
			addInput(createInputCentered<MmPortGold>(mm2px(Vec(10.33f, top + dy2 * i)), module, MSMelder::A_INPUT + i));
			addOutput(createOutputCentered<MmPortGold>(mm2px(Vec(20.15f, top + dy2 * i)), module, MSMelder::A_OUTPUT + i));
			addOutput(createOutputCentered<MmPortGold>(mm2px(Vec(10.33f, top + dy2 * i + 10.75f)), module, MSMelder::AL_OUTPUT + i));
			addOutput(createOutputCentered<MmPortGold>(mm2px(Vec(20.15f, top + dy2 * i + 10.75f)), module, MSMelder::AR_OUTPUT + i));
			addInput(createInputCentered<MmPortGold>(mm2px(Vec(10.33f, top + dy2 * i + 10.75f + 9.85f)), module, MSMelder::AL_INPUT + i));
			addInput(createInputCentered<MmPortGold>(mm2px(Vec(20.15f, top + dy2 * i + 10.75f + 9.85f)), module, MSMelder::AR_INPUT + i));
		}	
	}
};


Model *modelMSMelder = createModel<MSMelder, MSMelderWidget>("MSMelder");
