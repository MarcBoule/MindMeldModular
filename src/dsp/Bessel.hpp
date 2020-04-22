//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//See ./LICENSE.md for all licenses
//***********************************************************************************************

// Butterworth N=2, fc=1kHz, fs=44.1kHz
// float b[3] = {0.004604f, 2.0f * 0.004604f, 0.004604f};
// float a[2] = {-1.7990964095f, +0.8175124034f};

// Bessel N=2, fc=1kHz, fs=44.1kHz, u=2 (1 sample delay, mesured ?)
// float b[3] = {0.0155982532f, 2.0f * 0.0155982532f, 0.0155982532f};
// float a[2] = {-1.500428132f, +0.562821145f};


class Bessel3 {
	static const int N = 3;
	
	// Bessel LPF, N=3, fc=1kHz, fs=44.1kHz, u=3 (1.5 sample delay)
	// Measured: fc=1594Hz, delay = 8
	// static constexpr float acst = 0.0054594483186731f;
	// float b[N + 1] = {acst, 3.0f * acst, 3.0f * acst, acst};
	// float a[N] = {-1.9435048603831f, 1.2590703807778f, -0.27188993384517f};
	
	// Bessel HPF, N=3, fc=100Hz, fs=44.1kHz, u=3 (1.5 sample delay)
	// Measured: fc=519Hz, delay = not easy to identify
	static constexpr float acst = 0.93853170649674f;
	float b[N + 1] = {acst, -3.0f * acst, 3.0f * acst, -1.0f * acst};
	float a[N] = {-2.8744548023742f, 2.7541634702975f, -0.8796353793022f};

	dsp::IIRFilter<N + 1, N + 1, float> iir;
	
	public: 
	
	void init() {
		iir.setCoefficients(b, a);		
	}
	void reset() {
		iir.reset();		
	}
	float process(float vin) {
		return iir.process(vin);
	}
};



class Bessel4 {
	static const int N = 4;
	
	// Bessel LPF, N=4, fc=1kHz, fs=44.1kHz, u=4 (2 sample delay)
	// Measured: fc=1810Hz, delay = 8
	// static constexpr float acst = 0.002418454189f;
	// float b[N + 1] = {acst, 4.0f * acst, 6.0f * acst, 4.0f * acst, acst};
	// float a[N] = {-2.2259148799371f, 1.8580113947728f, -0.68929586845301f, 0.095894620641851f};

	// Bessel HPF, N=4, fc=100Hz, fs=44.1kHz, u=4 (2 sample delay)
	// Measured: fc=, delay = 
	static constexpr float acst = 0.89369838164541f;
	float b[N + 1] = {acst, -4.0f * acst, 6.0f * acst, -4.0f * acst, acst};
	float a[N] = {-3.7783544560884f, 5.3534858984408f, -3.3712278833302f, 0.79610586846683f};	
	
	dsp::IIRFilter<N + 1, N + 1, float> iir;
	
	public: 
	
	void init() {
		iir.setCoefficients(b, a);		
	}
	void reset() {
		iir.reset();		
	}
	float process(float vin) {
		return iir.process(vin);
	}
};
