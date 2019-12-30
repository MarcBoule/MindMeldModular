// http://www.earlevel.com/main/2012/12/15/a-one-pole-filter/
// A one-pole filter
// Posted on December 15, 2012 by Nigel Redmon
// Adapted by Marc Boulé


struct OnePoleFilter {
    float b1 = 0.0f;
	float lowout = 0.0f;
	
    void reset() {
		lowout = 0.0f;
	}
	
	void setCutoff(float Fc) {// normalized freq
		b1 = std::exp(-2.0f * M_PI * Fc);
	}
    float processLP(float in) {
		return lowout = in * (1.0f - b1) + lowout * b1;
	}
    float processHP(float in) {
		return in - processLP(in);
	}

};
