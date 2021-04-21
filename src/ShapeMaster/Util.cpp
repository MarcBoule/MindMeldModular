//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "Util.hpp"


// Mutl/div ratios for sync'ed length
// --------

int numRatiosPerSection[NUM_SECTIONS] = {5, 5, 5, 5, 4, 8};// sum should equal NUM_RATIOS
std::string sectionNames[NUM_SECTIONS] = {"Fast", "Common", "Slow", "Triplet", "Dotted", "Other"};

MutlDivRatio multDivTable[NUM_RATIOS] = { // should calc for even number of shapes because of swing
	// Fast
	{"1/128", "1/128", 4.0 * 1.0 / 128.0},
	{"1/64", "1/64", 4.0 * 1.0 / 64.0},
	{"1/32", "1/32", 4.0 * 1.0 / 32.0},
	{"1/16", "1/16", 4.0 * 1.0 / 16.0},
	{"1/8", "1/8", 4.0 * 1.0 / 8.0},
	// Common
	{"1/4", "1/4", 4.0 * 1.0 / 4.0},
	{"1/2", "1/2", 4.0 * 1.0 / 2.0},
	{"1 bar", "1 bar (4/4)", 4.0 * 1.0},
	{"2 bars", "2 bars (8/4)", 4.0 * 2.0},
	{"4 bars", "4 bars", 4.0 * 4.0},
	// Slow
	{"8 bars", "8 bars", 4.0 * 8.0},
	{"16 bars", "16 bars", 4.0 * 16.0},
	{"32 bars", "32 bars", 4.0 * 32.0},
	{"64 bars", "64 bars", 4.0 * 64.0},
	{"128 bar", "128 bars", 4.0 * 128.0},
	// Triplet
	{"1/16T", "1/16T (1/24)", 4.0 * 1.0 / 24.0}, 
	{"1/8T", "1/8T (1/12)", 4.0 * 1.0 / 12.0},
	{"1/4T", "1/4T (1/6)", 4.0 * 1.0 / 6.0},
	{"1/2T", "1/2T (1/3)", 4.0 * 1.0 / 3.0},
	{"1barT", "1barT (2/3)", 4.0 * 2.0 / 3.0},
	// Dotted
	{"1/16d", "1/16d (3/32)", 4.0 * 3.0 / 32.0},
	{"1/8d", "1/8d (3/16)", 4.0 * 3.0 / 16.0},
	{"1/4d", "1/4d (3/8)", 4.0 * 3.0 / 8.0},
	{"1/2d", "1/2d (3/4)", 4.0 * 3.0 / 4.0},
	// Other
	{"5/16", "5/16", 4.0 * 5.0 / 16.0},
	{"5/8", "5/8", 4.0 * 5.0 / 8.0},
	{"7/8", "7/8", 4.0 * 7.0 / 8.0},
	{"1.5 bars", "1.5 bars (6/4)", 4.0 * 1.5},
	{"3 bars", "3 bars", 4.0 * 3.0},
	{"6 bars", "6 bars", 4.0 * 6.0},
	{"12 bars", "12 bars", 4.0 * 12.0},
	{"24 bars", "24 bars", 4.0 * 24.0},
};



// Trig mode and play mode
// --------

std::string trigModeNames[NUM_TRIG_MODES] = {"AUTO", "T/G", "CTRL", "SC", "CV"};
std::string trigModeNamesLong[NUM_TRIG_MODES] = {"Automatic", "Trigger/Gate", "Gate control", "Sidechain", "CV playhead (uses T/G in)"};

std::string playModeNames[NUM_PLAY_MODES] = {"FWD", "REV", "PNG"};
std::string playModeNamesLong[NUM_PLAY_MODES] = {"Forward", "Reverse", "PingPong"};



// Grid
// --------

int snapValues[NUM_SNAP_OPTIONS] = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 24, 32, 64, 128};

int rangeValues[NUM_RANGE_OPTIONS] = {10, 5, 3, 2, 1, -5, -3, -2, -1};// neg means bipolar



// Ppqn
// --------

int ppqnValues[NUM_PPQN_CHOICES] = {12, 24, 48, 96, 192};



// Poly sum
// --------

std::string polyModeNames[NUM_POLY_MODES] = {"None (default)", "Sum to stereo", "Sum to mono"};

