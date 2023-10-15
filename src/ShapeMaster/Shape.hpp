//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#pragma once

#include "../MindMeldModular.hpp"
#include "Util.hpp"
#include "RandomSettings.hpp"


static const int MAX_PTS = 270;

class Shape {	
	// Constants
	public:
	// static const int8_t decoupledFirstLast = 0x1;

	private:
	static constexpr float SAFETY = 1e-5f;
	static constexpr float SAFETYx5 = SAFETY * 5.0f;
	static constexpr float MIN_CTRL = 7.5e-8f;
	
	// The following are invariants in the points:
	//   * numPts >= 2;
	//   * points[0].x = 0.0f
	//   * points[numPts-1].x = 1.0f
	//   * points[0].y = points[numPts-1].y (only when first/last are coupled)
	//   * ctrl[numPts-1] is unused and always 0.5f
	//   * type[numPts-1] is unused and always 0
	//   * x values of all points are always sorted
	Vec points[MAX_PTS];
	float ctrl[MAX_PTS];// from MIN_CTRL to 1-MIN_CTRL, positive only, this is a percentage of the abs(dy) span
	int8_t type[MAX_PTS];// 0 is smooth, 1 is s-shape
	int numPts;
	int pc;// point cache, index into points, has to be managed in lockstep with numPts such that 0 <= pc < (numPts - 1)
	int pcDelta;
	
	std::atomic_flag* lock_shape;// blocking and mandatory for all modifications that can temporarily change invariants, non-blocking test for process() (should leave output unchanged when can't acquire lock)
	float evalShapeForProcessRet = 0.0f;
	
	
	public:
	
	static float applyScalingToCtrl(float ctrl, float exponent);
	static float calcRndCtrl(float _ctrlMax) {
		// with some pow scaling
		float rndVal = random::uniform();
		rndVal = applyScalingToCtrl(rndVal, 2.0f);
		return (rndVal - 0.5f) * _ctrlMax * 0.01f + 0.5f;
	}	
	
	bool lockShapeNonBlocking() {// returns true if we got the lock successfully
		return !lock_shape->test_and_set();//std::memory_order_acquire);
	}
	
	void lockShapeBlocking() {
		while (lock_shape->test_and_set()) {};//std::memory_order_acquire)) {}
	}
	
	void unlockShape() {// only call this after having called lockShapeBlocking() or after a successful lockShapeNonBlocking()
		lock_shape->clear();//std::memory_order_release);
	}
	
	
	Shape(std::atomic_flag* _lock_shape) {
		lock_shape = _lock_shape;
		onReset(); 
	}
		
	void onReset();
	
	void initMinPts();
	
	int getPc() {
		return pc;
	}
	int getPcDelta() {
		return pcDelta;
	}
		

	// Smooth function:
	// y(x) := a*x*e^(b*x)
	// solve( y(1/2) = c and y(1) = 1, {a, b} )
	//   a = 4*c^2 and b = 2*ln(1/(2*c))
	// substitution and re-arranging yields
	// y(x) := x*(2c)^(2(1-x))
	// where:
	//   c is within [MIN_CTRL to 1-MIN_CTRL], positive only
	//   x, y are within [0:1] 
	template<typename T>
	T _y(T _x, T dx, T dy, T c) {
		// _y and _x are relative to the left neighbour
		// dx and dy are the horizontal and vertical deltas between left and right neighbour nodes
		// assumes but not critical: 0 <= _x <= dx
		// returns: 0 <= _y <= dy
		if (dx < (T)1e-6) {
			return (T)0.0;
		}
		if (_x > dx) {
			return dy;
		}
		_x /= dx;// 0 <= _x <= 1  for rest of function
		
		bool mirror = c > (T)0.5;
		if (mirror) {
			c = (T)1.0 - c;
			_x = (T)1.0 - _x;
		}
		
		c *= (T)2.0;
		T _y = _x * std::pow(c, (T)2.0 * ((T)1.0 - _x));
	
		if (mirror) {
			_y = (T)1.0 - _y;
		}
				
		return _y * dy;
	}

	// S-Curve function:
	// formula adapted from https://dhemery.github.io/DHE-Modules/technical/sigmoid/
	// y2(x) := ((x-0.5)*(1-k)) / (k-4*k*|x-0.5|+1) + 0.5
	// where:
	//   k is within [-1 : 1]
	//   x, y are within [0:1] 
	template<typename T>
	T _y2(T _x, T dx, T dy, T c) {
		// _y and _x are relative to the left neighbour
		// dx and dy are the horizontal and vertical deltas between left and right neighbour nodes
		// assumes but not critical: 0 <= _x <= dx
		// assumes c is within [MIN_CTRL to 1-MIN_CTRL], positive only
		// returns: 0 <= _y <= dy
		if (dx < (T)1e-6) {
			return (T)0.0;
		}
		if (_x > dx) {
			return dy;
		}
		_x /= dx;// 0 <= _x <= 1  for rest of function
		
		// T k = (T)2.0 * c - (T)1.0;
		T k = (T)1.98 * c - (T)0.99;// map the [MIN_CTRL to 1-MIN_CTRL] ctrl range to appropriate k
		
		_x = _x - (T)0.5;
		
		T _y = (_x * ((T)1.0 - k)) / (k - (T)4.0 * k * std::abs(_x) + (T)1.0) + (T)0.5;
	
		return _y * dy;
	}

	template<typename T>
	T calcY(int p, T _x) {
		// _x is relative to points[p].x
		// do not call on last point
		T dx = std::abs<T>((T)points[p + 1].x - (T)points[p].x);
		T dy = (T)points[p + 1].y - (T)points[p].y;
		T eval = (T)points[p].y;
		if (type[p] == 0) {
			eval += _y<T>(_x, dx, dy, (T)ctrl[p]);
		}
		else {
			eval += _y2<T>(_x, dx, dy, (T)ctrl[p]);
		}
		return eval;	
	}
	
	
	
	// points
	// ----------------
	
	int getNumPts() {
		return numPts;
	}
	
	Vec getPointVect(int p) {
		return points[p];
	}
	
	Vec getPointVectFlipY(int p) {
		return Vec(points[p].x, 1.0f - points[p].y);
	}
	
	Vec getPointVectFlipY(int p, float _x) {// _x is relative to point p
		float yVal = calcY<float>(p, _x);
		return Vec(points[p].x + _x, 1.0f - yVal);
	}
	
	Vec getPointVectFlipX(int p) {
		return Vec(1.0f - points[p].x, points[p].y);
	}
	
	float getPointX(int p) {
		return points[p].x;
	}
	
	float getPointY(int p) {
		return points[p].y;
	}
		
	template<typename T>
	int calcPointFromXBisectRecurse(T x, int low, int high) {// must have acquired lock before calling
		// method must scan [low:high] (inclusive on bounds)
		// assumes 0.0 < x < 1.0
		// assumes: 0 <= low,high < (numPts - 1)
		// returns a point
		int retp;
		int span = high - low + 1;
		if (span > 2) {
			// test mid point
			int mid = low + (span >> 1);
			if (x >= points[mid].x) {
				retp = calcPointFromXBisectRecurse(x, mid, high);
			}
			else {
				mid--;
				if (mid != low) {
					retp = calcPointFromXBisectRecurse(x, low, mid);
				}
				else {
					retp = low;
				}
			}
		}
		else if (span == 2) {
			if (x < points[high].x) {
				retp = low;
			}
			else {
				retp = high;
			}
		}
		else {// span == 1
			retp = low;// same as high
		}
		return retp;
	}
	
	template<typename T>
	int calcPointFromX(T x, int gp) {
		// before recursing with bisection, check the guess point (gp) and its left and right neighbours, 
		//   this is a speed heuristic since play head will typically not jump and most of the time the neighbour is 
		//   the good next point (right when moving forward, left when playing shape in reverse)
		// assumes: 0.0 < x < 1.0
		// assumes: 0 <= gp < (numPts - 1)
		if (x >= points[gp].x) {
			if (x >= points[gp + 1].x) {
				gp++;
				if (x >= points[gp + 1].x) {
					gp = calcPointFromXBisectRecurse<T>(x, gp + 1, numPts - 2);
				}
				// else gp is now spot on
			}
			// else gp was spot on 
		}
		else if (gp > 0) {// the if statement here is just to avoid a warning
			// here we know for sure that gp > 0
			gp--;
			if (x < points[gp].x) {
				gp = calcPointFromXBisectRecurse<T>(x, 0, gp - 1);
			}
			// else gp is now spot on
		}
		return gp;// no longer a guess point, but the real point
	}	
	
	float evalShapeForProcess(double x) {
		// should be used by process() only since it changes the local pc (if GUI uses this method, the pc will be changed)
		// returns previous value if couldn't get lock to calc an eval
		// x is in normalized space [0;1]
		if (x <= 0.0) {
			pcDelta = -pc;
			pc = 0;
			evalShapeForProcessRet = points[0].y;
		}
		else if (x >= 1.0) {
			int newpc = numPts - 2;// is sure to be >= 0, and pc must be < numPts-1
			pcDelta = newpc - pc;
			pc = newpc;
			evalShapeForProcessRet = points[numPts - 1].y;			
		}
		else {
			// here x is in ]0;1[
			if (lockShapeNonBlocking()) {
				int newpc = calcPointFromX<double>(x, pc);
				pcDelta = newpc - pc;
				pc = newpc;		
				evalShapeForProcessRet = calcY<double>(pc, x - (double)points[pc].x);
				unlockShape();
			}
			// else {
				// keep pc unchanged
				// keep evalShapeForProcessRet unchanged
			// }
		}	
		return evalShapeForProcessRet;
	}
	float evalShapeForDisplay(float x, int* epc) {
		// external point cache
		// x is in normalized space [0;1]
		if (x <= 0.0) {
			*epc = 0;
			return points[0].y;
		}
		else if (x >= 1.0) {
			*epc = numPts - 2;// is sure to be >= 0, and epc must be < numPts-1
			return points[numPts - 1].y;
		}
		// here x is in ]0;1[
		*epc = calcPointFromX<float>(x, *epc);			
		return calcY<float>(*epc, x - points[*epc].x);
	}
	
	float getPointYFlip(int p) {
		return 1.0f - points[p].y;
	}
	
	float normalizedQuantize(float val, int quant) {
		if (quant != -1) {
			return std::round(val * (float)quant) / (float)quant;
		}	
		return val;
	}
	
	
	void writePoint(int p, Vec newPt, float newCtrl = 0.5f, int newType = 0) {
		points[p] = newPt;
		ctrl[p] = newCtrl;
		type[p] = newType;
	}
	
	void copyPoint(int pDest, int pSrc) {
		points[pDest] = points[pSrc];
		ctrl[pDest] = ctrl[pSrc];
		type[pDest] = type[pSrc];
	}
	
	void setPoint(int p, Vec newPt) {
		points[p] = newPt;
	}
	void coupleFirstAndLast() {
		points[numPts - 1].y = points[0].y;
	}

	void setPointWithSafety(int p, Vec newPt, int xQuant, int yQuant, bool decoupledFirstLast);
	
	void insertPoint(int p, Vec newPt, float newCtrl = 0.5f, int8_t newType = 0);
	
	int insertPointWithSafetyAndBlock(Vec newPt, bool withHistory, bool withSafety = true, float newCtrl = 0.5f, int8_t newType = 0);
	
	void deletePoint(int p);
	
	void deletePointWithBlock(int p, bool withHistory);
	
	void makeStep(int p, Vec vecStepPt, int xQuant, int yQuant);
	
	
	// ctrl
	// ----------------
	
	bool isLinear(int p) {
		return ctrl[p] == 0.5f;
	}
	
	float getCtrl(int p) {
		return ctrl[p];
	}
	
	void makeLinear(int p);
	
	Vec getCtrlVect(int p) {// do not call on last point
		float xVal = points[p + 1].x - points[p].x;
		xVal *= (type[p] == 0 ? 0.5f : 0.25f); 
		float yVal = (ctrl[p] * (points[p + 1].y - points[p].y));
		if (type[p] != 0) {
			yVal *= 0.5f;
		}
		return Vec(points[p].x + xVal, points[p].y + yVal);
	}
	
	Vec getCtrlVectFlipY(int p) {// do not call on last point
		Vec ret = getCtrlVect(p);
		ret.y = 1.0f - ret.y;
		return ret;
	}
	
	void setCtrlWithSafety(int p, float newCtrl) {
		if (p < (numPts - 1)) {
			ctrl[p] = clamp(newCtrl, MIN_CTRL, 1.0f - MIN_CTRL);
		}		
	}

	bool isCtrlVisible(int pt) {
		return ( (std::fabs(points[pt + 1].y - points[pt].y) >= 0.005f) && 
				 (points[pt + 1].x - points[pt].x >= SAFETYx5) );
	}
	
	
	// type
	// ----------------
	int8_t getType(int pt) {
		return type[pt];
	}
	
	void setType(int pt, int8_t newType) {
		type[pt] = newType;
	}

	
	// json
	// ----------------
	json_t* dataToJsonShape();
	
	void dataFromJsonShape(json_t *shapeJ);


	// other
	// ----------------
	
	void copyShapeTo(Shape* destShape);

	void pasteShapeFrom(const Shape* srcShape);
	
	void reverseShape();
	
	void invertShape();
	
	void randomizeShape(const RandomSettings* randomSettings, uint8_t gridX, int8_t rangeIndex, bool decoupledFirstLast);
	
	bool isDirty(const Shape* refShape);
};


