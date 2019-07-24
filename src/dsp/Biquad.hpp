//
//  Biquad.h
//
//  Created by Nigel Redmon on 11/24/12
//  EarLevel Engineering: earlevel.com
//  Copyright 2012 Nigel Redmon
//
//  For a complete explanation of the Biquad code:
//  http://www.earlevel.com/main/2012/11/26/biquad-c-source-code/
// 
//  License:
//
//  This source code is provided as is, without warranty.
//  You may copy and distribute verbatim copies of this document.
//  You may modify and use this source code to create binary code
//  for your own purposes, free or commercial.
//

#ifndef Biquad_h
#define Biquad_h

#include "rack.hpp"
using namespace rack;

enum {
    BQ_TYPE_LOWPASS = 0,
    BQ_TYPE_HIGHPASS,
    BQ_TYPE_BANDPASS,
    BQ_TYPE_NOTCH,
    BQ_TYPE_PEAK,
    BQ_TYPE_LOWSHELF,
    BQ_TYPE_HIGHSHELF
};

class Biquad {
public:
    Biquad();
    Biquad(int type, double Fc, double Q, double peakGainDB);
    ~Biquad();
    void setType(int type);
    void setQ(double Q);
    void setFc(double Fc);
    void setPeakGain(double peakGainDB);
    void setBiquad(int type, double Fc, double Q, double peakGain);
    float process(float in);
    float processHPFopt(float in);
    
protected:
    void calcBiquad(void);

    int type;
    double a0, a1, a2, b1, b2;
    double Fc, Q, peakGain;
    double z1, z2;
};

// transposed direct form II
inline float Biquad::process(float in) {
    double out = 	in * a0 			+ z1;
    z1 = 			in * a1 - b1 * out 	+ z2;
    z2 = 			in * a2 - b2 * out		;
    return out;
}
inline float Biquad::processHPFopt(float in) {
	// if (a0 >= 0.998f) {
		// z1 = z2 = 0.0f;
		// a0 = 1.0f;
		// a1 = b1 = -2.0f; 
		// a2 = b2 = 1.0f; 
		
		// return in;// 0.998f minimal a0 coefficient for 20Hz HPF at 44100 kHz
	// }
	double out = 	in * a0 			+ z1;
    z1 = 			in * a1 - b1 * out 	+ z2;
    z2 = 			in * a2 - b2 * out		;
	// printf("a0 = %.4f, a1 = %.4f, b1 = %.4f, a2 = %.4f, b2 = %.4f\n    in = %f, out = %f, z1 = %.4f, z2 = %.4f\n", a0, a1, b1, a2, b2,   in, out, z1, z2);
	
    return out;
}

#endif // Biquad_h