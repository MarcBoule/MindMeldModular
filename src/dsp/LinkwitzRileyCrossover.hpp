//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//See ./LICENSE.md for all licenses
//***********************************************************************************************



class LinkwitzRileyCrossover {	
	dsp::IIRFilter<2 + 1, 2 + 1, float> iirs[8];// Left low 1, low 2, Right low 1, low 2, Left high 1, high 2, Right high 1, high 2
	
	public: 
	
	void init() {

	}
	
	void reset() {
		for (int i = 0; i < 8; i++) {
			iirs[i].reset();
		}			
	}
	
	float process(float vin) {
		return iirs[0].process(vin);
	}
};

