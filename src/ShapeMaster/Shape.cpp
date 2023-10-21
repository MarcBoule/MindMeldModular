//***********************************************************************************************
//Mind Meld Modular: Modules for VCV Rack by Steve Baker and Marc Boulé
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "Shape.hpp"
#include "Bjorklund.hpp"


float Shape::applyScalingToCtrl(float ctrl, float exponent) {
	bool mirror = false;
	if (ctrl > 0.5f) {
		ctrl = 1.0f - ctrl;
		mirror = true;
	}
	ctrl *= 2.0f;
	ctrl = std::pow(ctrl, exponent);
	ctrl /= 2.0f;
	if (mirror) {
		ctrl = 1.0f - ctrl;
	}
	return ctrl;
}


void Shape::onReset() {
	if (lock_shape) {// test needed here since we can construct with a NULL lock_shape (from channelDirtyCache) which will call the reset automatically, but all good after that
		lockShapeBlocking();
	}
	points[0].x = 0.0f;// must be 0.0f
	points[0].y = 0.0f;
	points[1].x = 0.5f;
	points[1].y = 1.0f;
	points[2].x = 1.0f;// must be 1.0f
	points[2].y = 0.0f;
	for (int p = 0; p < 3; p++) {
		ctrl[p] = 0.5f;// default is 50% which is linear
		type[p] = 0;
	}
	numPts = 3;
	pc = 0;// not saved
	pcDelta = 0;// not saved
	if (lock_shape) {// see comment above
		unlockShape();
	}
	evalShapeForProcessRet = 0.0f;
}


void Shape::initMinPts() {
	lockShapeBlocking();
	for (int p = 0; p < 2; p++) {
		points[p].y = 0.0f;
		ctrl[p] = 0.5f;// default is 50% which is linear
		type[p] = 0;
	}
	points[1].x = 1.0f;
	numPts = 2;
	pc = 0;
	pcDelta = 0;
	unlockShape();
}


void Shape::setPointWithSafety(int p, Vec newPt, int xQuant, int yQuant, bool decoupledFirstLast) {
	// newPt is in normalized space and pre clamped
	// method assumes point p already has enough space between neighbors, i.e. :
	//   points[p - 1].x + SAFETY  <  points[p].x  <  points[p + 1].x - SAFETY
	// quantize y to range if wanted
	newPt.y = normalizedQuantize(newPt.y, yQuant);
	if (p == 0 || p == (numPts - 1)) {
		if (!decoupledFirstLast) {
			points[0].y = newPt.y;
			points[numPts - 1].y = newPt.y;
		}
		else {
			points[p].y = newPt.y;
		}
	}
	else {
		// here we are guaranteed to have a left and right neighbour point
		// quantize x to snap if wanted
		newPt.x = normalizedQuantize(newPt.x, xQuant);
		// write actual point with safety
		points[p].x = clamp(newPt.x, points[p - 1].x + SAFETY, points[p + 1].x - SAFETY);
		points[p].y = newPt.y;
	}
}


void Shape::insertPoint(int p, Vec newPt, float newCtrl, int8_t newType) {
	// called must take care of getting lock
	// caller must make sure there is enough room, i.e. method does not check MAX_PTS
	for (int j = numPts; j > p; j--) {
		copyPoint(j, j - 1);
	}
	writePoint(p, newPt, newCtrl, newType);
	numPts++;
}


int Shape::insertPointWithSafetyAndBlock(Vec newPt, bool withHistory, bool withSafety, float newCtrl, int8_t newType) {
	// newPt is in normalized space
	// returns index of new point or -1 when not inserted
	int p = -1;
	float safety = withSafety ? SAFETY : (SAFETY * 0.9f);// must still have a form of safety when no safety, but this is useful for undoing a point deletion, because of float precision, using normal safety does not always guarantee successful reinsertion
	if (newPt.x >= points[0].x && numPts < MAX_PTS) {
		for (int i = 1; i < numPts; i++) {
			if (newPt.x < points[i].x) {
				// here i points to the location where the new point should be placed 
				//   but before the insertion, it's the location of the point that has its x to the right of the new point's x
				// test safety
				if (newPt.x > (points[i - 1].x + safety) && newPt.x < (points[i].x - safety)) {
					// here we have found the location of new point and safety is good
					lockShapeBlocking();
					insertPoint(i, newPt, newCtrl, newType);
					if (pc > i) pc++;
					p = i;
					unlockShape();
					if (withHistory) {
						// Push InsertPointChange history action
						InsertPointChange* h = new InsertPointChange;
						h->shapeSrc = this;
						h->newPointVec = newPt;	
						h->newPt = p;	
						APP->history->push(h);
					}
				}
				break;
			}
		}
	}
	return p;
}


void Shape::deletePoint(int p) {
	if (p > 0 && p < (numPts - 1)) {
		for (int i = p; i < (numPts - 1); i++) {
			points[i] = points[i + 1];
			ctrl[i] = ctrl[i + 1];
			type[i] = type[i + 1];
		}
		numPts--;
		if (pc > p) pc--;
	}		
}
void Shape::deletePointWithBlock(int p, bool withHistory) {
	if ((p != -1) && withHistory) {
		// Push DeletePointChange history action
		DeletePointChange* h = new DeletePointChange;
		h->shapeSrc = this;
		h->oldPointVec = points[p];	
		h->oldCtrl = ctrl[p];	
		h->oldType = type[p];	
		h->oldPt = p;	
		APP->history->push(h);
	}
	lockShapeBlocking();
	deletePoint(p);
	unlockShape();
}


void Shape::makeStep(int p, Vec vecStepPt, int xQuant, int yQuant) {
	// assumes: xQuant must be valid (i.e. not -1)
	// will only make the step if there is enough room for 4 new points (which is the worst case)
	if (vecStepPt.x > 0.0f && vecStepPt.x < 1.0f && numPts <= (MAX_PTS - 4)) {// must exclude 0.0 and 1.0 since vecStepPt.x was clamped to 0.0:1.0 and don't want values < 0.0 and > 1.0 to be detected here as 0.0 and 1.0;
		// must calc both bounds below in the same manner, to ensure consistent bounds when moving through grid,
		//   so they must absolutely be calculated as a fraction of integer valued floats
		// the std::min() calls are there for extra safeness for float precision in multiplications
		float xLeftQuant = std::fmin((float)(xQuant - 1), std::floor(vecStepPt.x * (float)xQuant)) / (float)xQuant;
		float xRightQuant = std::fmin((float)(xQuant), std::floor(vecStepPt.x * (float)xQuant) + 1.0f) / (float)xQuant;
		
		float yStep = normalizedQuantize(vecStepPt.y, yQuant);// quantizes if yQuant != -1
		
		// right
		int pRightClip = p;// pRightClip, when finished, should point to the first point to the right of the step (including xRightQuant boundary)
		while (points[pRightClip].x < xRightQuant) {
			pRightClip++;
		}
		// here pRightClip is first point that has its x >= xRightQuant
					
		// see if this point is outside of [xRightQuant ; xRightQuant + SAFETY], if so, clip is needed
		float yRightClip = -1.0f;
		if (points[pRightClip].x > (xRightQuant + SAFETY)) {
			// clip needed (this will never happen when pRightClip is the last point)
			// clip calc needs to be done with pRightClip - 1
			yRightClip = calcY<float>(pRightClip - 1, xRightQuant - points[pRightClip - 1].x);
			// new clip point needs to be inserted at "pRightClip (xRightQuant, yRightClip)", but don't do it yet since we need to calc the left clip first, and remove any point within the step to make sure we have the safety room needed to add the new clip point
		}
		
		
		lockShapeBlocking();
		
		
		// left
		if (xLeftQuant != 0.0) {
			int pLeftClip = p;// pLeftClip, when finished, should point to the first point to the right of "xLeftQuant - 2*SAFETY" (including "xLeftQuant - 2*SAFETY" boundary)
			float xLeftClip = xLeftQuant - SAFETY;
			float xLeftClip2 = xLeftQuant - 2.0f * SAFETY;
			
			while ((pLeftClip - 1) >= 0 && points[pLeftClip - 1].x >= xLeftClip2) {
				pLeftClip--;
			}
			while (points[pLeftClip].x < xLeftClip2) {
				pLeftClip++;
			}
			// here pLeftClip is first point that has its x >= xLeftClip2	
			
			// see if this point is outside of [xLeftQuant - 2*SAFETY ; xLeftQuant - SAFETY], if so, clip is needed
			if (points[pLeftClip].x > xLeftClip) {
				// clip needed
				float yLeftClip = calcY<float>(pLeftClip - 1, xLeftClip - points[pLeftClip - 1].x);
				insertPoint(pLeftClip, Vec(xLeftClip, yLeftClip));
				pRightClip++;
			}
			pLeftClip++;
			
			// create new right clip point if needed
			if (yRightClip != -1.0f) {
				insertPoint(pRightClip, Vec(xRightQuant, yRightClip));
			}
			
			int numPtsInStepInit = pRightClip - pLeftClip;
			if (numPtsInStepInit > 2) {
				// delete extraneous points within step if there are more than two of them
				for (int i = 0; i < (numPtsInStepInit - 2); i++) {
					deletePoint(pLeftClip);
				}
			}
			else {
				// add necessary points if less than two
				for (int i = numPtsInStepInit; i < 2; i++) {
					insertPoint(pLeftClip, Vec(0.0f, 0.0f));// dummy values
				}
			}	
			
			// create top right of step
			writePoint(pLeftClip + 1, Vec(xRightQuant - SAFETY, yStep));
			
			// create top left of step 
			writePoint(pLeftClip    , Vec(xLeftQuant, yStep));
		}
		else {// the step is the left-most section in the grid
			// create new right clip point if needed
			if (yRightClip != -1.0f) {
				insertPoint(pRightClip, Vec(xRightQuant, yRightClip));
			}
			
			// create top right of step
			if (pRightClip < 2) {
				// need new point for top right of step
				insertPoint(1, Vec(xRightQuant - SAFETY, yStep));
			}
			else {// pRightClip >= 2, so there is at least one extra point, which we will re-use for top-right of step, others will be deleted
				while (pRightClip > 2) {
					pRightClip--;
					deletePoint(pRightClip);
				}
				// use existing point to make top right of step
				writePoint(1, Vec(xRightQuant - SAFETY, yStep));
			}
			
			// set extreme points
			points[0].y = yStep;
			points[numPts - 1].y = yStep;
		}
		
		unlockShape();
	}
}


void Shape::makeLinear(int p) {
	// Push TypeAndCtrlChange history action
	TypeAndCtrlChange* h = new TypeAndCtrlChange;
	h->shapeSrc = this;
	h->pt = p;
	h->oldCtrl = ctrl[p];
	h->oldType = type[p];
	
	ctrl[p] = 0.5f;
	type[p] = 0;

	h->newCtrl = ctrl[p];
	h->newType = type[p];
	h->name = "reset control point";
	APP->history->push(h);
}



json_t* Shape::dataToJsonShape() {
	json_t* shapeJ = json_object();
	
	// points and isCtrl
	json_t* pointsXJ = json_array();
	json_t* pointsYJ = json_array();
	json_t* ctrlJ    = json_array();
	json_t* typeJ    = json_array();
	for (int p = 0; p < numPts; p++) {
		json_array_insert_new(pointsXJ, p , json_real(points[p].x));
		json_array_insert_new(pointsYJ, p , json_real(points[p].y));
		json_array_insert_new(ctrlJ,    p , json_real(ctrl[p]));
		json_array_insert_new(typeJ,    p , json_integer(type[p]));
	}
	json_object_set_new(shapeJ, "pointsX", pointsXJ);
	json_object_set_new(shapeJ, "pointsY", pointsYJ);
	json_object_set_new(shapeJ, "ctrl",    ctrlJ);
	json_object_set_new(shapeJ, "type",    typeJ);

	// numPts
	json_object_set_new(shapeJ, "numPts", json_integer(numPts));

	return shapeJ;
}


void Shape::dataFromJsonShape(json_t *shapeJ) {
	lockShapeBlocking();
	
	// points
	json_t* pointsXJ = json_object_get(shapeJ, "pointsX");
	json_t* pointsYJ = json_object_get(shapeJ, "pointsY");
	json_t* ctrlJ    = json_object_get(shapeJ, "ctrl");
	json_t* typeJ    = json_object_get(shapeJ, "type");
	if (pointsXJ && pointsYJ && ctrlJ && typeJ && 
		json_is_array(pointsXJ) && json_is_array(pointsYJ) && json_is_array(ctrlJ) && json_is_array(typeJ)) {
		for (int p = 0; p < std::min(MAX_PTS, (int)json_array_size(pointsXJ)); p++) {
			json_t* pointXarrayJ = json_array_get(pointsXJ, p);
			json_t* pointYarrayJ = json_array_get(pointsYJ, p);
			json_t* ctrlArrayJ   = json_array_get(ctrlJ, p);
			json_t* typeArrayJ   = json_array_get(typeJ, p);
			if (pointXarrayJ && pointYarrayJ && ctrlArrayJ && typeArrayJ) {
				points[p].x = json_number_value(pointXarrayJ);
				points[p].y = json_number_value(pointYarrayJ);
				ctrl[p]     = json_number_value(ctrlArrayJ);
				type[p]     = json_integer_value(typeArrayJ);
			}
		}
	}

	// numPts
	json_t *numPtsJ = json_object_get(shapeJ, "numPts");
	if (numPtsJ) {
		numPts = json_integer_value(numPtsJ);
		pc = 0;
		pcDelta = 0;
	}
	
	unlockShape();
}


// other
// ----------------

void Shape::copyShapeTo(Shape* destShape) {
	destShape->lockShapeBlocking();
	memcpy(destShape->points, points, sizeof(Vec) * numPts);
	memcpy(destShape->ctrl, ctrl, sizeof(float) * numPts);
	memcpy(destShape->type, type, sizeof(int8_t) * numPts);
	destShape->numPts = numPts;
	destShape->pc = 0;
	destShape->pcDelta = 0;
	destShape->unlockShape();
}


void Shape::pasteShapeFrom(const Shape* srcShape) {
	lockShapeBlocking();
	memcpy(points, srcShape->points, sizeof(Vec) * srcShape->numPts);
	memcpy(ctrl, srcShape->ctrl, sizeof(float) * srcShape->numPts);
	memcpy(type, srcShape->type, sizeof(int8_t) * srcShape->numPts);
	numPts = srcShape->numPts;
	pc = 0;
	pcDelta = 0;
	unlockShape();
}


void Shape::reverseShape() {	
	lockShapeBlocking();
	
	// do first and last if ever decoupledFirstLast is on
	float tmpY = points[0].y;
	points[0].y = points[numPts - 1].y;
	points[numPts - 1].y = tmpY;
	
	int p = 1;// skip first/last reversal since done above 
	for (; p < (numPts >> 1); p++) {			
		// copy 2nd into tmp while reversing
		Vec tmpPt = getPointVectFlipX(numPts - 1 - p);
		
		// paste 1st into 2nd while reversing
		points[numPts - 1 - p] = getPointVectFlipX(p);
		
		// paste tmp into 1st
		points[p] = tmpPt;
	}
	if ((numPts & 0x1) != 0) {// if odd number of points, must reverse middle node
		points[p].x = 1.0f - points[p].x;
	}
	
	// reverse ctrl and type (all but last have to be adjusted)
	int numPtsC = numPts - 1;
	for (p = 0; p < (numPtsC >> 1); p++) {		
		// copy 2nd into tmp while adjusting		
		float tmpCt = ctrl[numPtsC - 1 - p];
		int8_t tmpTy = type[numPtsC - 1 - p];
		if (tmpTy == 0) {
			tmpCt = 1.0f - tmpCt;
		}
		
		// paste 1st into 2nd while adjusting
		ctrl[numPtsC - 1 - p] = ctrl[p];
		type[numPtsC - 1 - p] = type[p];
		if (type[numPtsC - 1 - p] == 0) {
			ctrl[numPtsC - 1 - p] = 1.0f - ctrl[numPtsC - 1 - p];
		}
		
		// paste tmp into 1st
		ctrl[p] = tmpCt;
		type[p] = tmpTy;
	}
	if ((numPtsC & 0x1) != 0) {// if odd number of points, must adjust middle node
		ctrl[p] = 1.0f - ctrl[p];
	}
	
	pc = (numPts - 1) >> 1;
	unlockShape();
}


void Shape::invertShape() {
	lockShapeBlocking();
	
	for (int i = 0; i < numPts; i++) {
		points[i].y = 1.0f - points[i].y;
	}
	
	unlockShape();	
}


float calcRandCv(const RandomSettings* randomSettings, float restCv, int rangeValue) {
	// return restCv when zeroV's random decides it
	float zeroOrMaxTestRnd = random::uniform() * 100.0f;
	if (zeroOrMaxTestRnd < randomSettings->zeroV) {
		return restCv;
	}
	if (zeroOrMaxTestRnd >= (100.0f - randomSettings->maxV)) {
		return 1.0f;
	}
	
	if (!randomSettings->quantized) {
		return random::uniform();
	}
	
	// here we are quantized 
	// the top-most note in the vertical range will never be selected so that C can be equiprobable with other notes in the scale
	
	uint16_t scale = randomSettings->scale;
	if (scale == 0x000) {
		scale = 0xFFF;
	}
	
	int8_t packedScaleNotes[12] = {};// contains numbers from 0 to 11, but only those that are part of the chosen scale
	int numPackedScaleNotes = 0;
	for (int i = 0; i < 12; i++) {
		if (((scale >> i) & 0x1) != 0) {
			packedScaleNotes[numPackedScaleNotes] = i;
			numPackedScaleNotes++;
		}
	}
	
	int baseNote = packedScaleNotes[random::u32() % numPackedScaleNotes];// [0:11], a random base note from the scale
	int numOct = (rangeValue > 0 ? rangeValue : rangeValue * -2);// number of octaves over which to span
	int baseOct = (random::u32() % numOct);// a random octave [0:numOct-1]
	int intNote = baseOct * 12 + baseNote;
	
	return ((float)intNote) / ((float)(numOct * 12));
}


struct SegmentPair {
	int16_t pt;
	int16_t isStep;
};

void Shape::randomizeShape(const RandomSettings* randomSettings, uint8_t gridX, int8_t rangeIndex, bool decoupledFirstLast) {
	if (randomSettings->deltaMode != 0) {
		// delta mode randomization (aka vertical randomization)
		std::vector<SegmentPair> ptSeg;
		int ranges[24];// for quantization
		
		if (randomSettings->quantized != 0) {
			// This method is by Andrew Belt and is from the Fundamental Quantizer, modified by Marc Boulé
			// Find closest notes for each range
			for (int i = 0; i < 24; i++) {
				int closestNote = 0;
				int closestDist = INT_MAX;
				for (int note = -12; note <= 24; note++) {
					int dist = std::abs((i + 1) / 2 - note);
					// Ignore enabled state if no notes are enabled
					if (randomSettings->scale != 0 && ( (randomSettings->scale & (0x1 << eucMod(note, 12))) == 0 )) {   // !enabledNotes[eucMod(note, 12)]) {
						continue;
					}
					if (dist < closestDist) {
						closestNote = note;
						closestDist = dist;
					}
					else if (dist == closestDist) {
						// coin-flip for equidistant quantization, to ensure no drift towards bottom when continual random
						if (random::uniform() >= 0.5f) { 
							closestNote = note;
							closestDist = dist;
						}
					}
					else {
						// If dist increases, we won't find a better one.
						break;
					}
				}
				ranges[i] = closestNote;
			}	
		}
		
		lockShapeBlocking();
		
		int numTruePts = numPts - (decoupledFirstLast ? 0 : 1);
				
		// find all segments
		for (int pt = 0; pt < numTruePts ; pt++) {
			SegmentPair nseg;
			nseg.pt = pt;
			nseg.isStep = 0;
			// now check for step
			int nextPt = pt + 1;
			if (nextPt < numTruePts && points[pt].y == points[nextPt].y) {// TODO check gridx
				float p1xg = points[pt].x * (float)gridX;
				float p2xg = points[nextPt].x * (float)gridX;
				float p1xgr = std::round(p1xg);
				float p2xgr = std::round(p2xg);
				if (p2xgr - p1xgr == 1) {
					float d1xg = std::fabs(p1xg - p1xgr); 
					float d2xg = std::fabs(p2xg - p2xgr); 
					float safg = SAFETYx5 * (float)gridX;
					if (d1xg < safg && d2xg < safg) {
						nseg.isStep = 1;
						pt++;
					}
				}
			}
			ptSeg.push_back(nseg);
		}
			
		// keep only deltaNodes % of the segments (randomly chosen)
		int numToKeep = std::round(randomSettings->deltaNodes * 0.01f * ptSeg.size());	
		if (numToKeep > 0) {
			if (numToKeep < (int)ptSeg.size()) {
				std::random_shuffle(ptSeg.begin(), ptSeg.end());
				ptSeg.resize(numToKeep);
			}
			
			for (int s = 0; s < numToKeep; s++) {
				int ptToMove = ptSeg[s].pt;
				float newOffset = (random::uniform() - 0.5f) * randomSettings->deltaChange * 0.02f;
				float newVert = points[ptToMove].y + newOffset;
				// begin fold
				if (newVert > 1.0f) {
					newVert = 1.0f - (newVert - 1.0f) * 0.5f; 
				}
				if (newVert < 0.0f) {
					newVert = newVert * -0.5f;
				}
				// end fold
				newVert = clamp(newVert, 0.0f, 1.0f);// always keep this even when folding
				// start quantization 
				if (randomSettings->quantized != 0) {
					int rangeValue = rangeValues[rangeIndex];
					int numOct = (rangeValue > 0 ? rangeValue : rangeValue * -2);// number of octaves over which to span
					newVert *= (float)numOct;
					int range = std::floor(newVert * 24);
					int octave = eucDiv(range, 24);
					range -= octave * 24;
					int note = ranges[range] + octave * 12;
					newVert = float(note) / 12;
					newVert /= (float)numOct;
				}
				// end quantization
				points[ptToMove].y = newVert;		
				if (ptSeg[s].isStep != 0) {
					points[ptToMove + 1].y = points[ptToMove].y;
				}
			}
			if (!decoupledFirstLast) {
				points[numPts - 1].y = points[0].y;
			}
		}
		unlockShape();
	}
	else {
		// non delta mode randomization
		Bjorklund bjorklund;
		initMinPts();
		
		int numPtsMin = (int)(randomSettings->numNodesMin + 0.5f);
		int numPtsMax = std::max(numPtsMin, (int)(randomSettings->numNodesMax + 0.5f));// safety
		int numPtsRnd = random::u32() % (numPtsMax - numPtsMin + 1) + numPtsMin;

		float restCv = rangeValues[rangeIndex] < 0 ? 0.5f : 0.0f;
		
		// prepare euclidean grid if needed
		if (randomSettings->grid) {
			while (numPtsRnd > (int)gridX) {
				gridX <<= 1;
			}
			// here gridX <= 128, numPtsRnd <= gridX
			bjorklund.init(gridX, numPtsRnd);// gridX is size of seqeunce, numPtsRnd is numPulses which are <= gridX
			bjorklund.randomRotate();
			// bjorklund.print();// only shows in terminal when Rack quits
		}
		
		if (randomSettings->stepped) {
			lockShapeBlocking();
			if (!randomSettings->grid) {
				for (int rp = numPtsRnd - 1; rp >= 0; rp--) {
					float rndCv = calcRandCv(randomSettings, restCv, rangeValues[rangeIndex]);
					float xStepL = (float)rp / (float)numPtsRnd;
					bool slide = random::uniform() < (randomSettings->ctrlMax * 0.01f);
					// right node
					if (!slide) {
						float xStepR = (float)(rp + 1) / (float)numPtsRnd - SAFETY;
						insertPoint(1, Vec(xStepR, rndCv));
					}
					// left node
					if (rp > 0) { 
						insertPoint(1, Vec(xStepL, rndCv));
					}
					else {
						// do end points manually
						points[0].y = rndCv;
						ctrl[0] = 0.5f;
						type[0] = 0;
						points[numPts - 1].y = rndCv;
					}	
					if (slide) {
						setCtrlWithSafety(rp > 0 ? 1 : 0, calcRndCtrl(90.0f));// max ctrl is 90% in this case, don't want too extreme curves
					}
				}
			}
			else {// is grid
				int onePos = 0;
				int nextInsPt = 1;
				for (int rp = 0; rp < numPtsRnd; rp++) {
					float rndCv = calcRandCv(randomSettings, restCv, rangeValues[rangeIndex]);
					float xStepL = (float)onePos / (float)gridX;
					bool slide = random::uniform() < (randomSettings->ctrlMax * 0.01f);
					// left node
					if (rp > 0) { 
						insertPoint(nextInsPt, Vec(xStepL, rndCv));
						if (slide) {
							setCtrlWithSafety(nextInsPt, calcRndCtrl(90.0f));// max ctrl is 90% in this case, don't want too extreme curves
						}
						nextInsPt++;
					}
					else {
						// do end points manually
						points[0].y = rndCv;
						ctrl[0] = 0.5f;
						type[0] = 0;
						points[numPts - 1].y = rndCv;
						if (slide) {
							setCtrlWithSafety(0, calcRndCtrl(90.0f));// max ctrl is 90% in this case, don't want too extreme curves
						}
					}	
					onePos = bjorklund.nextOne(onePos);
					// right node
					if (!slide) {
						float xStepR;
						if (rp >= numPtsRnd - 1) {
							xStepR = 1.0 - SAFETY;
						}
						else {
							xStepR = (float)onePos / (float)gridX;
						}
						insertPoint(nextInsPt, Vec(xStepR, rndCv));
						nextInsPt++;
					}
				}			
			}
			if (decoupledFirstLast && numPts > 2) {
				points[numPts - 2].x = 1.0f;
				numPts--;
			}
			unlockShape();
		}
		else {// not stepped
			if (!randomSettings->grid) {
				for (int rp = 0; rp < numPtsRnd - 1; rp++) {
					float rndX = random::uniform();
					float rndCv = calcRandCv(randomSettings, restCv, rangeValues[rangeIndex]);
					int retPt = insertPointWithSafetyAndBlock(Vec(rndX, rndCv), false);// without history

					setCtrlWithSafety(retPt, calcRndCtrl(randomSettings->ctrlMax));			
					type[retPt] = 0;
				}
			}
			else {// is grid
				int onePos = 0;
				for (int rp = 0; rp < numPtsRnd; rp++) {
					onePos = bjorklund.nextOne(onePos);
					float rndX = (float)onePos / (float)gridX;
					float rndCv = calcRandCv(randomSettings, restCv, rangeValues[rangeIndex]);
					int retPt = insertPointWithSafetyAndBlock(Vec(rndX, rndCv), false);// without history

					setCtrlWithSafety(retPt, calcRndCtrl(randomSettings->ctrlMax));			
					type[retPt] = 0;
				}
			}
			setCtrlWithSafety(0, calcRndCtrl(randomSettings->ctrlMax));
			points[0].y = restCv;
			if (decoupledFirstLast) {
				points[numPts - 1].y = calcRandCv(randomSettings, restCv, rangeValues[rangeIndex]);
			}
			else {
				points[numPts - 1].y = restCv;
			}
		}
	}
}


bool Shape::isDirty(const Shape* refShape) {
	if (numPts != refShape->numPts) {
		return true;
	}
	for (int p = 0; p < numPts; p++) {
		if (std::fabs(points[p].x - refShape->points[p].x) > 0.004f) {
			return true;
		}
		if (std::fabs(points[p].y - refShape->points[p].y) > 0.008f) {
			return true;
		}
		if (std::fabs(ctrl[p] - refShape->ctrl[p]) > 0.004f) {
			return true;
		}
		if (type[p] != refShape->type[p]) {
			return true;
		}
	}
	return false;
}


