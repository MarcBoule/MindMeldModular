// downloaded from https://gist.github.com/unohee/d4f32b3222b42de84a5f
// modified by Marc Boulé 2021-02-14

#include <cmath>
#include <algorithm>
#include <vector>


class Bjorklund{   
	int lengthOfSeq;
	int pulseAmt;
	std::vector<int>remainder;
	std::vector<int>count;
	std::vector<bool>sequence; //accessing sequence directly is discouraged. use LoadSequence()

	public:

	Bjorklund() {
		lengthOfSeq = 0;
		pulseAmt = 0;
	};
	
	void init(int step, int pulse);
	
	void print();
	
	int getSequence(int index) {
		return sequence.at(index);
	}
	
	int nextOne(int onePos) {
		// ignores current position (will start by incrementing)
		// will automatically wrap around end point
		do {
			onePos++;
			onePos %= size();
		} while (getSequence(onePos) == 0);
		return onePos;
	}
	
	int randomOne() {
		// find a random "1"
		int onePos = std::rand() % size();
		return nextOne(onePos);
	}
	
	void randomRotate() {
		// random rotate such that a "1" is in index 0
		std::rotate(sequence.begin(), sequence.begin() + randomOne(), sequence.end());
	}
	
	int size() {
		return (int)sequence.size();
	}

	private:

	void iter();
	void buildSeq(int slot);
};