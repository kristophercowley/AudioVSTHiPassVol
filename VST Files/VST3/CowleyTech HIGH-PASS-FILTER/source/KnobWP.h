#ifndef __cknobwp__
#define __cknobwp__
//#include "LCDController.h"

#include "vstgui/lib/controls/cknob.h"
//#include "vstgui/lib/cbitmap.h"
#include <vector>

namespace VSTGUI {

class CKnobWP : public CAnimKnob
{
public:
	CKnobWP(const CRect& size, CControlListener* listener, int32_t tag, int32_t subPixmaps, 
			CCoord heightOfOneImage, CBitmap* background, const CPoint &offset, 
			bool bSwitchKnob = false, bool bAlphaWheelKnob = false, bool bLCDValueKnob = false);
	
	virtual void draw (CDrawContext* pContext) VSTGUI_OVERRIDE_VMETHOD;
	virtual CMouseEventResult onMouseMoved (CPoint& where, const CButtonState& buttons) VSTGUI_OVERRIDE_VMETHOD;
	virtual void valueChanged();

	void addLCDController(void* controller);

protected:
	bool m_bSwitchKnob;
	bool m_bAlphaWheelKnob;
	bool m_bLCDValueKnob;
	typedef std::vector<void*> LCDKnobControlList;
	LCDKnobControlList m_LCDControllers;

	float m_fMaxValue;

	virtual ~CKnobWP(void);
};
}

#endif