//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//See ./LICENSE.md for all licenses
//***********************************************************************************************



class Bessel2 {
	static const int N = 2;
	
	dsp::IIRFilter<N + 1, N + 1, float> iir;
	
	public: 
	
	void init(bool isLPF) {
		static constexpr float wc = 2 * M_PI * 100.0f;
		static constexpr float t = 1.0f / 44100.0f;
		if (isLPF) {
			// MZT.tns p.2, linear phase confirmed, group delay is function of wc, and number of samples delay is thus function of wc and fs
			// static constexpr float g = 4.85f *1.43f / (98.31f * 98.78f);
			// float b[N + 1] = {1.0f * g, 0.0f, 0.0f};
			// float a[N] = {-2.0f * std::exp(-1.5f * wc * t) * std::cos(wc * t * std::sqrt(3.0f) / 2.0f), std::exp(-3.0f * wc * t)};
			
			// MZT.tns p3, linear phase confirmed, group delay is function of wc, and number of samples delay is thus function of wc and fs
			static constexpr float g = 0.1f / 3.0f;
			float b[N + 1] = {1.0f * g, 0.0f, 0.0f};
			float a[N] = {-1.7023421214676f, 0.73047970302298f};
			
			iir.setCoefficients(b, a);
		}
		else {
			// MZT.tns p.1, NOT linear phase!
			static constexpr float g = 1.0f;
			float b[N + 1] = {1.0f * g, -2.0f * g, 1.0f * g};
			float a[N] = {-2.0f * std::exp(-0.5f * wc * t) * std::cos(wc * t * std::sqrt(3.0f) / 6.0f), std::exp(-1.0f * wc * t)};
			iir.setCoefficients(b, a);
			
			// MZT.tns p.4, NOT linear phase but no need to test since get almost same coeffs as https://www-users.cs.york.ac.uk/~fisher/cgi-bin/mkfscript
			// and his graphs at bottom show not linear phase
		}
	}
	void reset() {
		iir.reset();		
	}
	float process(float vin) {
		return iir.process(vin);
	}
};




class Bessel3 {
	static const int N = 3;
	
	dsp::IIRFilter<N + 1, N + 1, float> iir;
	
	public: 
	
	void init(bool isLPF) {
		if (isLPF) {
			// Bessel LPF, N=3, fc=1kHz, fs=44.1kHz, u=3 (1.5 sample delay)
			// Measured: fc = 1594 Hz, delay = 8 samples
			// static constexpr float acst = 0.0054594483186731f;
			// float b[N + 1] = {acst, 3.0f * acst, 3.0f * acst, acst};
			// float a[N] = {-1.9435048603831f, 1.2590703807778f, -0.27188993384517f};
			
			// Bessel LPF, N=3, fc=1kHz, fs=44.1kHz, u=4 (2 sample delay)
			// Measured: fc = 1695 Hz slight unstable, delay = 8 samples
			static constexpr float acst = 0.0044489387671785f;
			float b[N + 1] = {acst, 3.0f * acst, 3.0f * acst, acst};
			float a[N] = {-2.0879166523604f, 1.4790278763047f, -0.35551971380702f};
			
			iir.setCoefficients(b, a);
		}
		else {
			// Bessel HPF, N=3, fc=100Hz, fs=44.1kHz, u=3 (1.5 sample delay)
			// Measured: fc = 519.6 Hz, delay = ? samples
			// static constexpr float acst = 0.93853170649674f;// with high pass proto
			// float b[N + 1] = {acst, -3.0f * acst, 3.0f * acst, -1.0f * acst};
			// float a[N] = {-2.8744548023742f, 2.7541634702975f, -0.8796353793022f};
			// static constexpr float acst = 0.99290990599652f;// with 1/p transformation
			// float b[N + 1] = {acst, -3.0f * acst, 3.0f * acst, -1.0f * acst};
			// float a[N] = {-2.9857861664049f, 2.9716396771649f, -0.98585340440209f};
			
			// Bessel HPF, N=3, fc=100Hz, fs=44.1kHz, u=4 (2 sample delay)
			// Measured: fc = ? Hz, delay = ? samples
			// static constexpr float acst = 0.94996559952778f;// with high pass proto
			// float b[N + 1] = {acst, -3.0f * acst, 3.0f * acst, -1.0f * acst};
			// float a[N] = {-2.897975354123f, 2.7998898727326f, -0.90185956936666f};

			// Bessel HPF, N=3, fc=2000Hz, fs=44.1kHz, u=4 (2 sample delay)
			// Measured: fc = ? Hz, delay = ? samples
			static constexpr float acst = 0.40051574149395f;// with high pass proto
			float b[N + 1] = {acst, -3.0f * acst, 3.0f * acst, -1.0f * acst};
			float a[N] = {-1.3831540220771f, 0.69473173915223f, -0.12624017072227f};

			
			iir.setCoefficients(b, a);		
		}
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
