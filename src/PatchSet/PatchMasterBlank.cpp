//***********************************************************************************************
//Configurable multi-controller with parameter mapping
//For VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************

#include "../MindMeldModular.hpp"
#include "PatchSetWidgets.hpp"


struct PatchMasterBlank : Module {
	
	// Need to save, no reset
	int facePlate;
	
	PatchMasterBlank() {
		config(0, 0, 0, 0);
		facePlate = 0;
	}
	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// facePlate
		json_object_set_new(rootJ, "facePlate", json_integer(facePlate));
		
		return rootJ;
	}


	void dataFromJson(json_t *rootJ) override {
		// facePlate
		json_t *facePlateJ = json_object_get(rootJ, "facePlate");
		if (facePlateJ)
			facePlate = json_integer_value(facePlateJ);

		// resetNonJson(true);	
	}	
};


struct PatchMasterBlankWidget : ModuleWidget {
	SvgPanel* svgPanel;
	PanelBorder* panelBorder;
	std::shared_ptr<window::Svg> svgs[2];
	int lastFacePlate;

	void appendContextMenu(Menu *menu) override {
		PatchMasterBlank *module = static_cast<PatchMasterBlank*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator());
		
		menu->addChild(createCheckMenuItem("Show vertical separator", "",
			[=]() {return module->facePlate == 0;},
			[=]() {module->facePlate ^= 0x1;}
		));	
	}	



	PatchMasterBlankWidget(PatchMasterBlank *module) {
		setModule(module);
		svgPanel = static_cast<SvgPanel*>(getPanel());
		panelBorder = findBorder(svgPanel->fb);
        svgs[0] = APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/patchset/pm-vsep-bg.svg"));
		setPanel(svgs[0]);
		svgs[1] = nullptr;
		lastFacePlate = 0;
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
			const PatchMasterBlank* pmbModule = static_cast<PatchMasterBlank*>(module);
			
			// Faceplate
			int facePlate = pmbModule->facePlate;
			if (facePlate != lastFacePlate) {
				lastFacePlate = facePlate;
				
				if (svgs[1] == nullptr) {
					svgs[1] = APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/patchset/pm-vsep-bg-noline.svg"));
				}
				SvgPanel* panel = static_cast<SvgPanel*>(getPanel());
				panel->setBackground(svgs[facePlate]);
				panel->fb->dirty = true;
			}		
		
			// Borders	
			bool familyPresentLeft  = calcFamilyPresent(module->leftExpander.module);
			bool familyPresentRight = calcFamilyPresent(module->rightExpander.module);
			float newPosX = svgPanel->box.pos.x;
			float newSizeX = svgPanel->box.size.x;
			
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
		}
		
		ModuleWidget::step();
	}// step()
};


Model *modelPatchMasterBlank = createModel<PatchMasterBlank, PatchMasterBlankWidget>("PatchMasterBlank");
