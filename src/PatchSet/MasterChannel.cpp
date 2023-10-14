//***********************************************************************************************
//Split/merge to/from Left/Right that spaces out the signals for compatibility with EQ master
//Used for mid/side eq'ing with two EqMasters, using MixMaster inserts and VCV Mid/Side modules 
//For VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "../MindMeldModular.hpp"
#include "../MixMaster/MixerWidgets.hpp"
#include "PatchSetWidgets.hpp"


static const std::string defLabelName = "Master Chan";


// managed by Mixer, not by tracks (tracks read only)
struct McGlobalInfo {
	// constants
	enum McParamIds {
		MAIN_FADER_PARAM,
		MAIN_MUTE_PARAM,
		MAIN_DIM_PARAM,
		MAIN_MONO_PARAM,
		NUM_PARAMS
	};	
	
	
	// need to save, no reset
	// none
	
	// need to save, with reset
	PackedBytes4 directOutPanStereoMomentCvLinearVol;
		// directOutsMode: 0 is pre-insert, 1 is post-insert, 2 is post-fader, 3 is post-solo, 4 is per-track choice
		// panLawStereo: Stereo balance linear (default), Stereo balance equal power (+3dB boost since one channel lost),  True pan (linear redistribution but is not equal power), Per-track
		// momentaryCvButtons: 1 = yes (original rising edge only version), 0 = level sensitive (emulated with rising and falling detection)
		// linearVolCvInputs: 0 means powN, 1 means linear
	PackedBytes4 colorAndCloak;// see enum called ccIds for fields
	bool symmetricalFade;
	uint16_t ecoMode;// all 1's means yes, 0 means no
	
	// no need to save, with reset
	float sampleTime;

	// no need to save, no reset
	// none
	
	
	McGlobalInfo() {
		onReset();
	}
		
	void onReset() {
		directOutPanStereoMomentCvLinearVol.cc4[0] = 3; // directOutsMode: post-solo by default
		directOutPanStereoMomentCvLinearVol.cc4[1] = 1; // panLawStereo
		directOutPanStereoMomentCvLinearVol.cc4[2] = 1; // momentaryCvButtons: 0=gatelevel, 1=momentary, 2=pertrack
		directOutPanStereoMomentCvLinearVol.cc4[3] = 0; // linearVolCvInputs: 0 means powN, 1 means linear		
		colorAndCloak.cc4[cloakedMode] = 0;
		colorAndCloak.cc4[vuColorGlobal] = numVuThemes;// force use local value
		colorAndCloak.cc4[dispColorGlobal] = 0;// unused since manually done in MasterChannelWidget, will use local value
		colorAndCloak.cc4[detailsShow] = 0x7;
		symmetricalFade = false;
		ecoMode = 0xFFFF;// all 1's means yes, 0 means no
		resetNonJson();
	}


	void resetNonJson() {
		sampleTime = APP->engine->getSampleTime();
	}


	void dataToJson(json_t *rootJ) {		
		// colorAndCloak
		json_object_set_new(rootJ, "colorAndCloak", json_integer(colorAndCloak.cc1));
		
		// symmetricalFade
		json_object_set_new(rootJ, "symmetricalFade", json_boolean(symmetricalFade));
		
		// ecoMode
		json_object_set_new(rootJ, "ecoMode", json_integer(ecoMode));
		
		// momentaryCvButtons
		json_object_set_new(rootJ, "momentaryCvButtons", json_integer(directOutPanStereoMomentCvLinearVol.cc4[2]));

		// linearVolCvInputs
		json_object_set_new(rootJ, "linearVolCvInputs", json_integer(directOutPanStereoMomentCvLinearVol.cc4[3]));
	}


	void dataFromJson(json_t *rootJ) {
		// colorAndCloak
		json_t *colorAndCloakJ = json_object_get(rootJ, "colorAndCloak");
		if (colorAndCloakJ)
			colorAndCloak.cc1 = json_integer_value(colorAndCloakJ);
		
		// symmetricalFade
		json_t *symmetricalFadeJ = json_object_get(rootJ, "symmetricalFade");
		if (symmetricalFadeJ)
			symmetricalFade = json_is_true(symmetricalFadeJ);

		// ecoMode
		json_t *ecoModeJ = json_object_get(rootJ, "ecoMode");
		if (ecoModeJ)
			ecoMode = json_integer_value(ecoModeJ);
		
		// momentaryCvButtons
		json_t *momentaryCvButtonsJ = json_object_get(rootJ, "momentaryCvButtons");
		if (momentaryCvButtonsJ)
			directOutPanStereoMomentCvLinearVol.cc4[2] = json_integer_value(momentaryCvButtonsJ);
		
		// linearVolCvInputs
		json_t *linearVolCvInputsJ = json_object_get(rootJ, "linearVolCvInputs");
		if (linearVolCvInputsJ)
			directOutPanStereoMomentCvLinearVol.cc4[3] = json_integer_value(linearVolCvInputsJ);
		
		// extern must call resetNonJson()
	}	
		
};// struct McGlobalInfo


struct McCore {
	// Constants
	
	// need to save, no reset
	// none
	
	// need to save, with reset
	bool dcBlock;
	int clipping; // 0 is soft, 1 is hard (must be single ls bit)
	float fadeRate; // mute when < minFadeRate, fade when >= minFadeRate. This is actually the fade time in seconds
	float fadeProfile; // exp when +1, lin when 0, log when -1
	int8_t vuColorThemeLocal;
	int8_t dispColorLocal;
	int8_t momentCvMuteLocal;
	int8_t momentCvDimLocal;
	int8_t momentCvMonoLocal;
	float dimGain;// slider uses this gain, but displays it in dB instead of linear
	std::string masterLabel;
	
	// no need to save, with reset
	private:
	float mute;
	simd::float_4 gainMatrix;// L, R, RinL, LinR (used for fader-mono block)
	// float volCv;
	public:
	TSlewLimiterSingle<simd::float_4> gainMatrixSlewers;
	SlewLimiterSingle muteSlewer;
	private:
	FirstOrderStereoFilter dcBlockerStereo;// 6dB/oct
	public:
	VuMeterAllDual vu;// use mix[0..1]
	float fadeGain; // target of this gain is the value of the mute/fade button's param (i.e. 0.0f or 1.0f)
	float target;// used detect button press (needed to reset fadeGainXr and VUs)
	float fadeGainX;// absolute X value of fade, between 0.0f and 1.0f (for symmetrical fade)
	float fadeGainXr;// reset X value of fade, between 0.0f and 1.0f (for asymmetrical fade)
	float fadeGainScaled;
	float paramWithCV;
	float dimGainIntegerDB;// corresponds to dimGain, converted to dB, then rounded, then back to linear

	// no need to save, no reset
	McGlobalInfo *gInfo = nullptr;
	Param *params = nullptr;



	float calcFadeGain() {return params[McGlobalInfo::MAIN_MUTE_PARAM].getValue() >= 0.5f ? 0.0f : 1.0f;}
	bool isFadeMode() {return fadeRate >= GlobalConst::minFadeRate;}

	
	McCore(McGlobalInfo *_gInfo, Param *_params) {
		gInfo = _gInfo;
		params = _params;
		gainMatrixSlewers.setRiseFall(simd::float_4(GlobalConst::antipopSlewSlow)); // slew rate is in input-units per second (ex: V/s)
		muteSlewer.setRiseFall(GlobalConst::antipopSlewFast); // slew rate is in input-units per second (ex: V/s)
		dcBlockerStereo.setParameters(true, 0.1f);
		
		onReset();
	}


	void onReset() {
		dcBlock = false;
		clipping = 0;
		fadeRate = 0.0f;
		fadeProfile = 0.0f;
		vuColorThemeLocal = 0;
		dispColorLocal = 0;
		momentCvMuteLocal = 1;
		momentCvDimLocal = 1;
		momentCvMonoLocal = 1;
		dimGain = 0.25119f;// 0.1 = -20 dB, 0.25119 = -12 dB
		masterLabel = defLabelName;
		resetNonJson();
	}


	void resetNonJson() {
		mute = 0.0f;
		gainMatrix = simd::float_4(0.0f);
		gainMatrixSlewers.reset();
		muteSlewer.reset();
		setupDcBlocker();// dcBlockerStereo
		dcBlockerStereo.reset();
		vu.reset();
		fadeGain = calcFadeGain();
		target = fadeGain;
		fadeGainX = fadeGain;
		fadeGainXr = 0.0f;
		fadeGainScaled = fadeGain;// no pow needed here since 0.0f or 1.0f
		paramWithCV = -100.0f;
		dimGainIntegerDB = calcDimGainIntegerDB(dimGain);
	}


	void dataToJson(json_t *rootJ) {
		// dcBlock
		json_object_set_new(rootJ, "dcBlock", json_boolean(dcBlock));

		// clipping
		json_object_set_new(rootJ, "clipping", json_integer(clipping));
		
		// fadeRate
		json_object_set_new(rootJ, "fadeRate", json_real(fadeRate));
		
		// fadeProfile
		json_object_set_new(rootJ, "fadeProfile", json_real(fadeProfile));
		
		// vuColorThemeLocal
		json_object_set_new(rootJ, "vuColorThemeLocal", json_integer(vuColorThemeLocal));
		
		// dispColorLocal
		json_object_set_new(rootJ, "dispColorLocal", json_integer(dispColorLocal));
		
		// momentCvMuteLocal
		json_object_set_new(rootJ, "momentCvMuteLocal", json_integer(momentCvMuteLocal));
		
		// momentCvDimLocal
		json_object_set_new(rootJ, "momentCvDimLocal", json_integer(momentCvDimLocal));
		
		// momentCvMonoLocal
		json_object_set_new(rootJ, "momentCvMonoLocal", json_integer(momentCvMonoLocal));
		
		// dimGain
		json_object_set_new(rootJ, "dimGain", json_real(dimGain));
		
		// masterLabel
		json_object_set_new(rootJ, "masterLabel", json_string(masterLabel.c_str()));
	}


	void dataFromJson(json_t *rootJ) {
		// dcBlock
		json_t *dcBlockJ = json_object_get(rootJ, "dcBlock");
		if (dcBlockJ)
			dcBlock = json_is_true(dcBlockJ);
		
		// clipping
		json_t *clippingJ = json_object_get(rootJ, "clipping");
		if (clippingJ)
			clipping = json_integer_value(clippingJ);
		
		// fadeRate
		json_t *fadeRateJ = json_object_get(rootJ, "fadeRate");
		if (fadeRateJ)
			fadeRate = json_number_value(fadeRateJ);
		
		// fadeProfile
		json_t *fadeProfileJ = json_object_get(rootJ, "fadeProfile");
		if (fadeProfileJ)
			fadeProfile = json_number_value(fadeProfileJ);
		
		// vuColorThemeLocal
		json_t *vuColorThemeLocalJ = json_object_get(rootJ, "vuColorThemeLocal");
		if (vuColorThemeLocalJ)
			vuColorThemeLocal = json_integer_value(vuColorThemeLocalJ);
		
		// dispColorLocal
		json_t *dispColorLocalJ = json_object_get(rootJ, "dispColorLocal");
		if (dispColorLocalJ)
			dispColorLocal = json_integer_value(dispColorLocalJ);
		
		// momentCvMuteLocal
		json_t *momentCvMuteLocalJ = json_object_get(rootJ, "momentCvMuteLocal");
		if (momentCvMuteLocalJ)
			momentCvMuteLocal = json_integer_value(momentCvMuteLocalJ);
		
		// momentCvDimLocal
		json_t *momentCvDimLocalJ = json_object_get(rootJ, "momentCvDimLocal");
		if (momentCvDimLocalJ)
			momentCvDimLocal = json_integer_value(momentCvDimLocalJ);
		
		// momentCvMonoLocal
		json_t *momentCvMonoLocalJ = json_object_get(rootJ, "momentCvMonoLocal");
		if (momentCvMonoLocalJ)
			momentCvMonoLocal = json_integer_value(momentCvMonoLocalJ);
		
		// dimGain
		json_t *dimGainJ = json_object_get(rootJ, "dimGain");
		if (dimGainJ)
			dimGain = json_number_value(dimGainJ);

		// masterLabel
		json_t *textJ = json_object_get(rootJ, "masterLabel");
		if (textJ)
			masterLabel = json_string_value(textJ);
		
		// extern must call resetNonJson()
	}			
	
	void setupDcBlocker() {
		float fc = 10.0f;// Hz
		fc *= gInfo->sampleTime;// fc is in normalized freq for rest of method
		dcBlockerStereo.setParameters(true, fc);
	}
	
	
	void onSampleRateChange() {
		setupDcBlocker();
	}
	
	
	// Soft-clipping polynomial function
	// piecewise portion that handles inputs between 6 and 12 V
	// unipolar only, caller must take care of signs
	// assumes that 6 <= x <= 12
	// assumes f(x) := x  when x < 6
	// assumes f(x) := 10  when x > 12
	//
	// Chosen polynomial:
	// f(x) := a + b*x + c*x^2 + d*x^3
	//
	// Coefficient solving constraints:
	// f(6) = 6
	// f(12) = 10
	// f'(6) = 1 (unity slope at 6V)
	// f'(12) = 0 (horizontal at 12V)
	// where:
	// f'(x) := b + 2*c*x + 3*d*x^2
	// 
	// solve(system(f(6)=6,f(12)=10,f'(6)=1,f'(12)=0),{a,b,c,d})
	// 
	// solution:
	// a=2 and b=0 and c=(1/6) and d=(-1/108)
	float clipPoly(float inX) {
		return 2.0f + inX * inX * (1.0f/6.0f - inX * (1.0f/108.0f));
	}
	
	float clip(float inX) {// 0 = soft, 1 = hard
		if (inX <= 6.0f && inX >= -6.0f) {
			return inX;
		}
		if (clipping == 1) {// hard clip
			return clamp(inX, -10.0f, 10.0f);
		}
		// here clipping is 0, so do soft clip
		inX = clamp(inX, -12.0f, 12.0f);
		if (inX >= 0.0f)
			return clipPoly(inX);
		return -clipPoly(-inX);
	}
	
	
	void process(float *mix, bool eco) {// master
		// takes mix[0..1] and redeposits post in same place
		
		if (eco) {
			// calc ** fadeGain, fadeGainX, fadeGainXr, target, fadeGainScaled **
			float newTarget = calcFadeGain();
			if (newTarget != target) {
				fadeGainXr = 0.0f;
				target = newTarget;
				vu.reset();
			}
			if (fadeGain != target) {
				if (isFadeMode()) {
					float deltaX = (gInfo->sampleTime / fadeRate) * (1 + (gInfo->ecoMode & 0x3));// last value is sub refresh
					fadeGain = updateFadeGain(fadeGain, target, &fadeGainX, &fadeGainXr, deltaX, fadeProfile, gInfo->symmetricalFade);
					fadeGainScaled = std::pow(fadeGain, GlobalConst::masterFaderScalingExponent);
				}
				else {// we are in mute mode
					fadeGain = target;
					fadeGainX = target;
					fadeGainScaled = target;// no pow needed here since 0.0f or 1.0f
				}	
			}
			// dim (this affects fadeGainScaled only, so treated like a partial mute, but no effect on fade pointers or other just effect on sound
			mute = fadeGainScaled;
			if (params[McGlobalInfo::MAIN_DIM_PARAM].getValue() >= 0.5f) {
				mute *= dimGainIntegerDB;
			}
			
			// calc ** fader, paramWithCV **
			float fader = params[McGlobalInfo::MAIN_FADER_PARAM].getValue();
			// if (inVol->isConnected() && inVol->getChannels() >= (N_GRP * 2 + 4)) {
				// volCv = clamp(inVol->getVoltage(N_GRP * 2 + 4 - 1) * 0.1f, 0.0f, 1.0f);
				// paramWithCV = fader * volCv;
				// if (gInfo->directOutPanStereoMomentCvLinearVol.cc4[3] == 0) {
					// fader = paramWithCV;
				// }			
			// }
			// else {
				// volCv = 1.0f;
				// paramWithCV = -100.0f;
			// }

			// scaling
			fader = std::pow(fader, GlobalConst::masterFaderScalingExponent);
			
			// calc ** gainMatrix **
			// mono
			if (params[McGlobalInfo::MAIN_MONO_PARAM].getValue() >= 0.5f) {
				gainMatrix = simd::float_4(0.5f * fader);
			}
			else {
				gainMatrix = simd::float_4(fader, fader, 0.0f, 0.0f);
			}
		}
		
		// Calc gains for chain input (with antipop when signal connect, impossible for disconnect)
		if (mute != muteSlewer.out) {
			muteSlewer.process(gInfo->sampleTime, mute);
		}
		
		// Calc master gain with slewer and apply it
		simd::float_4 sigs(mix[0], mix[1], mix[1], mix[0]);// L, R, RinL, LinR
		if (movemask(gainMatrix == gainMatrixSlewers.out) != 0xF) {// movemask returns 0xF when 4 floats are equal
			gainMatrixSlewers.process(gInfo->sampleTime, gainMatrix);
		}
		sigs = sigs * gainMatrixSlewers.out;
		sigs[0] += sigs[2];// pre mute, do not change VU needs
		sigs[1] += sigs[3];// pre mute, do not change VU needs
		// if (gInfo->directOutPanStereoMomentCvLinearVol.cc4[3] != 0) {
			// sigs[0] *= volCv;
			// sigs[1] *= volCv;
		// }		
		
		// Set mix
		mix[0] = sigs[0] * muteSlewer.out;
		mix[1] = sigs[1] * muteSlewer.out;
		
		// VUs (no cloaked mode for master, always on)
		if (eco) {
			float sampleTimeEco = gInfo->sampleTime * (1 + (gInfo->ecoMode & 0x3));
			vu.process(sampleTimeEco, fadeGainScaled == 0.0f ? &sigs[0] : mix);
		}
						
		// DC blocker (post VU)
		if (dcBlock) {
			dcBlockerStereo.process(mix, mix);
		}
		
		// Clipping (post VU, so that we can see true range)
		mix[0] = clip(mix[0]);
		mix[1] = clip(mix[1]);
	}		
};// struct McCore


//*****************************************************************************


struct MasterChannel : Module {
	
	// ParamIds
	// global at top of file
	
	enum InputIds {
		ENUMS(IN_INPUTS, 2),
		NUM_INPUTS
	};
	
	enum OutputIds {
		ENUMS(OUT_OUTPUTS, 2),
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
	McGlobalInfo gInfo;
	McCore master;
	
	// No need to save, with reset
	int updateTrackLabelRequest;// 0 when nothing to do, 1 for read names in widget

	// No need to save, no reset
	RefreshCounter refresh;	
	
	
	MasterChannel() : master(&gInfo, &params[0]) {
		config(McGlobalInfo::NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		// Fader
		float maxMFader = std::pow(GlobalConst::masterFaderMaxLinearGain, 1.0f / GlobalConst::masterFaderScalingExponent);
		configParam(McGlobalInfo::MAIN_FADER_PARAM, 0.0f, maxMFader, 1.0f, "Master: level", " dB", -10, 20.0f * GlobalConst::masterFaderScalingExponent);
		// Mute
		configParam(McGlobalInfo::MAIN_MUTE_PARAM, 0.0f, 1.0f, 0.0f, "Master: mute");
		// Dim
		configParam(McGlobalInfo::MAIN_DIM_PARAM, 0.0f, 1.0f, 0.0f, "Master: dim");
		// Mono
		configParam(McGlobalInfo::MAIN_MONO_PARAM, 0.0f, 1.0f, 0.0f, "Master: mono");

		configInput(IN_INPUTS + 0, "Audio left");
		configInput(IN_INPUTS + 1, "Audio right");

		configOutput(OUT_OUTPUTS + 0, "Audio left");
		configOutput(OUT_OUTPUTS + 1, "Audio right");

		configBypass(IN_INPUTS + 0, OUT_OUTPUTS + 0);
		configBypass(IN_INPUTS + 1, OUT_OUTPUTS + 1);
				
		onReset();
	}
  
	void onReset() override final {
		gInfo.onReset();
		master.onReset();
		resetNonJson(false);
	}
	void resetNonJson(bool recurseNonJson) {
		updateTrackLabelRequest = 1;
		if (recurseNonJson) {
			gInfo.resetNonJson();
			master.resetNonJson();
		}
	}


	void onRandomize() override {
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		
		// gInfo
		gInfo.dataToJson(rootJ);

		// master
		master.dataToJson(rootJ);

		return rootJ;
	}


	void dataFromJson(json_t *rootJ) override {
		// gInfo
		gInfo.dataFromJson(rootJ);

		// master
		master.dataFromJson(rootJ);

		resetNonJson(true);	
	}


	void onSampleRateChange() override {
		gInfo.sampleTime = APP->engine->getSampleTime();
		master.onSampleRateChange();
	}
	

	void process(const ProcessArgs &args) override {

		// Inputs and slow
		if (refresh.processInputs()) {
			// none
		}	


		// McGlobalInfo
		// none
		
		uint16_t ecoCode = (refresh.refreshCounter & 0x3 & gInfo.ecoMode);
		bool ecoStagger4 = (gInfo.ecoMode == 0 || ecoCode == 3);

		// Master
		float mix[2];
		mix[0] = inputs[IN_INPUTS + 0].getVoltageSum();
		mix[1] = inputs[IN_INPUTS + 1].isConnected() ? inputs[IN_INPUTS + 1].getVoltageSum() : mix[0];
		master.process(mix, ecoStagger4);
		
		// Set master outputs
		outputs[OUT_OUTPUTS + 0].setVoltage(mix[0]);
		outputs[OUT_OUTPUTS + 1].setVoltage(mix[1]);
		
		
		// Lights
		if (refresh.processLights()) {
			// none
		}
	}// process()
};


//-----------------------------------------------------------------------------


struct MasterChannelWidget : ModuleWidget {
	SvgPanel* svgPanel;
	PanelBorder* panelBorder;
	SvgWidget* logoSvg;
	SvgWidget* omriLogoSvg;
	TileDisplaySep* masterDisplay;
	time_t oldTime = 0;
	int8_t defaultColor = 0;// yellow, used when module == NULL
	
	
	struct NameOrLabelValueField : ui::TextField {
		MasterChannel* module = NULL;

		NameOrLabelValueField(MasterChannel* _module) {
			module = _module;
			text = module->master.masterLabel;
			selectAll();
		}

		void step() override {
			// Keep selected
			// APP->event->setSelectedWidget(this);
			// TextField::step();
		}

		void onSelectKey(const event::SelectKey& e) override {
			if (e.action == GLFW_RELEASE){// && (e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_KP_ENTER)) {
				module->master.masterLabel = text;
				module->updateTrackLabelRequest = 1;
				if (e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_KP_ENTER) {
					ui::MenuOverlay* overlay = getAncestorOfType<ui::MenuOverlay>();
					overlay->requestDelete();
					e.consume(this);
				}
			}

			if (!e.getTarget())
				TextField::onSelectKey(e);
		}
	};	
	
	
	void appendContextMenu(Menu *menu) override {
		MasterChannel *module = static_cast<MasterChannel*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator());
		menu->addChild(createMenuLabel("Settings:"));
				
		// name menu entry
		NameOrLabelValueField* nameValueField = new NameOrLabelValueField(module);
		nameValueField->box.size.x = 100;
		menu->addChild(nameValueField);	

		menu->addChild(createCheckMenuItem("Symmetrical fades", "",
			[=]() {return module->gInfo.symmetricalFade;},
			[=]() {module->gInfo.symmetricalFade = !module->gInfo.symmetricalFade;}
		));
		
		FadeRateSlider *fadeSlider = new FadeRateSlider(&(module->master.fadeRate));
		fadeSlider->box.size.x = 200.0f;
		menu->addChild(fadeSlider);
		
		FadeProfileSlider *fadeProfSlider = new FadeProfileSlider(&(module->master.fadeProfile));
		fadeProfSlider->box.size.x = 200.0f;
		menu->addChild(fadeProfSlider);
		
		DimGainSlider *dimSliderItem = new DimGainSlider(&(module->master.dimGain), &(module->master.dimGainIntegerDB));
		dimSliderItem->box.size.x = 200.0f;
		menu->addChild(dimSliderItem);
		
		menu->addChild(createCheckMenuItem("DC blocker", "",
			[=]() {return module->master.dcBlock;},
			[=]() {module->master.dcBlock = !module->master.dcBlock;}
		));
		
		ClippingItem *clipItem = createMenuItem<ClippingItem>("Clipping", RIGHT_ARROW);
		clipItem->clippingSrc = &(module->master.clipping);
		menu->addChild(clipItem);
		
		VuColorItem *vuColItem = createMenuItem<VuColorItem>("VU Colour", RIGHT_ARROW);
		vuColItem->srcColor = &(module->master.vuColorThemeLocal);
		vuColItem->isGlobal = false;
		menu->addChild(vuColItem);

		menu->addChild(createSubmenuItem("Display colour", "", [=](Menu* menu) {
			for (int i = 0; i < numPsColors; i++) {
				menu->addChild(createCheckMenuItem(psColorNames[i], "",
					[=]() {return module->master.dispColorLocal == i;},
					[=]() {module->master.dispColorLocal = i;}
				));
			}
		}));			
	}	
	
	
	MasterChannelWidget(MasterChannel *module) {
		setModule(module);

		// Main panels from Inkscape
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/patchset/master-channel.svg")));
		svgPanel = static_cast<SvgPanel*>(getPanel());
		panelBorder = findBorder(svgPanel->fb);
		static constexpr float midX = 25.4f / 2.0f;
		
		// logo
		addChild(logoSvg = createWidgetCentered<LogoSvg>(mm2px(Vec(midX, 123.32f))));
		omriLogoSvg = createWidgetCentered<OmriLogoSvg>(mm2px(Vec(midX, 123.32f)));
		omriLogoSvg->visible = false;
		addChild(omriLogoSvg);

		static constexpr float jkoX = 10.5 / 2.0f;
		
		// Master inputs
		addInput(createInputCentered<MmPort>(mm2px(Vec(midX - jkoX + 0.2f, 12.8f)), module, MasterChannel::IN_INPUTS + 0));			
		addInput(createInputCentered<MmPort>(mm2px(Vec(midX - jkoX + 0.2f, 21.8f)), module, MasterChannel::IN_INPUTS + 1));			
		
		// Master outputs
		addOutput(createOutputCentered<MmPort>(mm2px(Vec(midX + jkoX + 0.3f, 12.8f)), module, MasterChannel::OUT_OUTPUTS + 0));			
		addOutput(createOutputCentered<MmPort>(mm2px(Vec(midX + jkoX + 0.3f, 21.8f)), module, MasterChannel::OUT_OUTPUTS + 1));			
		
		// Master label
		addChild(masterDisplay = createWidgetCentered<TileDisplaySep>(mm2px(Vec(midX, 31.36f))));
		masterDisplay->text = module ? module->master.masterLabel : defLabelName;
		if (module) {
			masterDisplay->dispColor = &(module->master.dispColorLocal);
		}
		else {
			masterDisplay->dispColor = &defaultColor;
		}
		
		// Master fader
		addParam(createParamCentered<MmBigFader>(mm2px(Vec(midX + jkoX + 0.05f, 70.3f)), module, McGlobalInfo::MAIN_FADER_PARAM));
		if (module) {
			// VU meter
			VuMeterMaster *newVU = createWidgetCentered<VuMeterMaster>(mm2px(Vec(midX + jkoX + 0.05f - 5.35f, 70.3f)));
			newVU->srcLevels = module->master.vu.vuValues;
			newVU->srcMuteGhost = &(module->master.fadeGainScaled);
			newVU->colorThemeGlobal = &(module->gInfo.colorAndCloak.cc4[vuColorGlobal]);
			newVU->colorThemeLocal = &(module->master.vuColorThemeLocal);
			newVU->clippingPtr = &(module->master.clipping);
			addChild(newVU);
			// Fade pointer
			CvAndFadePointerMaster *newFP = createWidgetCentered<CvAndFadePointerMaster>(mm2px(Vec(midX + jkoX + 0.05f - 5.35f - 3.4f, 70.3f)));
			newFP->srcParam = &(module->params[McGlobalInfo::MAIN_FADER_PARAM]);
			newFP->srcParamWithCV = &(module->master.paramWithCV);
			newFP->colorAndCloak = &(module->gInfo.colorAndCloak);
			newFP->srcFadeGain = &(module->master.fadeGain);
			newFP->srcFadeRate = &(module->master.fadeRate);
			addChild(newFP);				
		}
		
		// Master mute
		MmMuteFadeButton* newMuteFade;
		addParam(newMuteFade = createParamCentered<MmMuteFadeButton>(mm2px(Vec(midX, 109.8f)), module, McGlobalInfo::MAIN_MUTE_PARAM));
		if (module) {
			newMuteFade->type = &(module->master.fadeRate);
		}
		
		// Master dim
		addParam(createParamCentered<MmDimButton>(mm2px(Vec(midX - jkoX - 0.1f, 116.1f)), module, McGlobalInfo::MAIN_DIM_PARAM));
		
		// Master mono
		addParam(createParamCentered<MmMonoButton>(mm2px(Vec(midX + jkoX + 0.1f, 116.1f)), module, McGlobalInfo::MAIN_MONO_PARAM));
	}
	
	
	void draw(const DrawArgs& args) override {
		if (module && calcFamilyPresent(module->rightExpander.module)) {
			DrawArgs newDrawArgs = args;
			newDrawArgs.clipBox.size.x += mm2px(0.3f);// PM familiy panels have their base rectangle this much larger, to kill gap artifacts
			ModuleWidget::draw(newDrawArgs);
		}
		else {
			ModuleWidget::draw(args);
		}
	}

	
	void step() override {
		if (module) {
			MasterChannel* module = static_cast<MasterChannel*>(this->module);
			
			// Track labels (pull from module)
			if (module->updateTrackLabelRequest != 0) {// pull request from module
				// master display
				masterDisplay->text = module->master.masterLabel;
				module->updateTrackLabelRequest = 0;// all done pulling
			}
			

			// Borders	
			bool familyPresentLeft  = calcFamilyPresent(module->leftExpander.module);
			bool familyPresentRight = calcFamilyPresent(module->rightExpander.module);
			float newPosX = svgPanel->box.pos.x;
			float newSizeX = svgPanel->box.size.x;
			//
			if (familyPresentLeft) {
				newPosX -= 3.0f;
				newSizeX += 3.0f;
			}
			if (familyPresentRight) {
				newSizeX += 3.0f;
			}
			if (panelBorder->box.pos.x != newPosX || panelBorder->box.size.x != newSizeX) {
				panelBorder->box.pos.x = newPosX;
				panelBorder->box.size.x = newSizeX;
				svgPanel->fb->dirty = true;
			}
			
			// Logos
			bool rightIsBlank = (module->rightExpander.module && (module->rightExpander.module->model == modelPatchMasterBlank));
			bool newLogoVisible = !familyPresentRight || (rightIsBlank && !calcFamilyPresent(module->rightExpander.module->rightExpander.module, false));
			//
			bool leftIsBlank = (module->leftExpander.module && (module->leftExpander.module->model == modelPatchMasterBlank));
			bool newOmriVisible = !newLogoVisible && (!familyPresentLeft || (leftIsBlank && !calcFamilyPresent(module->leftExpander.module->leftExpander.module, false)));
			//
			if (logoSvg->visible != newLogoVisible || omriLogoSvg->visible != newOmriVisible) {
				logoSvg->visible = newLogoVisible;
				omriLogoSvg->visible = newOmriVisible;
				svgPanel->fb->dirty = true;
			}
			
			
			// Update param and port tooltips and message bus at 1Hz (and filter lights also)
			time_t currentTime = time(0);
			if (currentTime != oldTime) {
				oldTime = currentTime;
				char strBuf[32];

				// Fader
				snprintf(strBuf, 32, "%s level", module->master.masterLabel.c_str());
				module->paramQuantities[McGlobalInfo::MAIN_FADER_PARAM]->name = strBuf;
				// Mute/fade
				if (module->master.isFadeMode()) {
					snprintf(strBuf, 32, "%s fade", module->master.masterLabel.c_str());
				}
				else {
					snprintf(strBuf, 32, "%s mute", module->master.masterLabel.c_str());
				}
				module->paramQuantities[McGlobalInfo::MAIN_MUTE_PARAM]->name = strBuf;
				// Dim
				snprintf(strBuf, 32, "%s dim", module->master.masterLabel.c_str());
				module->paramQuantities[McGlobalInfo::MAIN_DIM_PARAM]->name = strBuf;
				// Mono
				snprintf(strBuf, 32, "%s mono", module->master.masterLabel.c_str());
				module->paramQuantities[McGlobalInfo::MAIN_MONO_PARAM]->name = strBuf;

				// module->outputInfos[TMixMaster::MAIN_OUTPUTS + 0]->name = "Main left";
				// module->outputInfos[TMixMaster::MAIN_OUTPUTS + 1]->name = "Main right";
			}
		}
		
		ModuleWidget::step();
	}// step()
	
};


Model *modelMasterChannel = createModel<MasterChannel, MasterChannelWidget>("MasterChannel");
