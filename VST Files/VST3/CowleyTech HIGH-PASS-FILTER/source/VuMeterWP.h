#pragma once
//#include "vstgui/vstgui_win32.cpp"
#include "vstgui/lib/controls/cvumeter.h"
#include "vstgui/lib/cbitmap.h"
#include "RackAFXMeterDetector.h"

namespace VSTGUI {

class CVuMeterWP : public CVuMeter
{
public:
	CVuMeterWP(const CRect& size, CBitmap* onBitmap, CBitmap* offBitmap, int32_t nbLed, bool bInverted, bool bAnalogVU, int32_t style = kVertical);
	
	CVuMeterWP(void);
	~CVuMeterWP(void);
	
	// --- overrides!
	virtual void draw(CDrawContext* pContext) VSTGUI_OVERRIDE_VMETHOD;
	virtual void setViewSize (const CRect& newSize, bool invalid = true) VSTGUI_OVERRIDE_VMETHOD;

	inline void initDetector(float samplerate, float attack_in_ms, float release_in_ms, bool bAnalogTC, UINT uDetect, bool bLogDetector)
	{
		if(m_bAnalogVU)
			bLogDetector = true;

		m_Detector.init(samplerate, attack_in_ms, release_in_ms, bAnalogTC, uDetect, bLogDetector);
		m_Detector.prepareForPlay();
	}

	void setHtOneImage(double d){heightOfOneImage = d;}
	void setImageCount(double d){subPixMaps = d;}
	void setZero_dB_Frame(double d){m_dZero_dB_Frame = d;}

protected:
	bool m_bInverted;
	bool m_bAnalogVU;
	double m_dZero_dB_Frame;
	double heightOfOneImage;
	double subPixMaps;

	CRackAFXMeterDetector m_Detector;
};
}