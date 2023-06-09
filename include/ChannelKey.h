/* vim:set noexpandtab tabstop=4 wrap */
#ifndef CHANNELKEY_H
#define CHANNELKEY_H

#include <iostream>

using namespace std;

enum class subdetector : uint8_t { TDC, ADC, LAPPD};

class ChannelKey{
	
	
	public:
  ChannelKey() : SubDetectorType(subdetector::ADC), DetectorElementIndex(0){}
	//ChannelKey(uint32_t subdetin, uint32_t detelin) : SubDetectorType(subdetin), DetectorElementIndex(detelin){};
  ChannelKey(subdetector subdetin, uint32_t detelin) : SubDetectorType(subdetin), DetectorElementIndex(detelin){}
	
	//inline uint32_t GetSubDetectorType() {return SubDetectorType;}
	inline subdetector GetSubDetectorType() const{return SubDetectorType;}
	inline uint32_t GetDetectorElementIndex() const {return DetectorElementIndex;}
	//inline void SetSubDetectorType(uint32_t subdetin){SubDetectorType=subdetin;}
	inline void SetSubDetectorType(subdetector subdetin){SubDetectorType=subdetin;}
	inline void SetDetectorElementIndex(uint32_t detelin){DetectorElementIndex=detelin;}
	
	bool Print() {
		cout<<"SubDetectorType : "<<uint8_t(SubDetectorType)<<endl;
		cout<<"DetectorElementIndex : "<<DetectorElementIndex<<endl;
		
		return true;
	}
	
	bool operator <( const ChannelKey rhs) const{
		subdetector rhstype = rhs.GetSubDetectorType();
		uint32_t rhsindex = rhs.GetDetectorElementIndex();
		if(SubDetectorType == rhstype){
			return (DetectorElementIndex < rhsindex);
		} else {
			return (SubDetectorType < rhstype);
		}
	}
	
	private:
	//uint32_t SubDetectorType;
	subdetector SubDetectorType;
	uint32_t DetectorElementIndex;
	
};

#endif
