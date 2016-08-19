#include "KnobWP.h"
#include "vstgui/lib/cbitmap.h"
#include <cmath>
#include "LCDController.h"

namespace VSTGUI {

CKnobWP::CKnobWP (const CRect& size, CControlListener* listener, int32_t tag, int32_t subPixmaps, CCoord heightOfOneImage, CBitmap* background, const CPoint &offset, bool bSwitchKnob, bool bAlphaWheelKnob, bool bLCDValueKnob)
: CAnimKnob (size, listener, tag, subPixmaps, heightOfOneImage, background, offset)
, m_bSwitchKnob(bSwitchKnob)
, m_bAlphaWheelKnob(bAlphaWheelKnob)
, m_bLCDValueKnob(bLCDValueKnob)
{
	m_fMaxValue = -1.0;
}
CKnobWP::~CKnobWP(void)
{
	while (!m_LCDControllers.empty())
	{
		m_LCDControllers.pop_back();
	}
}

void CKnobWP::addLCDController(void* controller)
{
	m_LCDControllers.push_back(controller);
}

void CKnobWP::draw(CDrawContext* pContext)
{
	//if(getMax() > m_fMaxValue)
	//{
	//	m_fMaxValue = getMax();
	//}
	//setMax(1.0);

	if(getDrawBackground())
	{
		CPoint where (0, 0);

		// --- alpha wheel knob has vmax = 2 so we can use the 1.0 crossing point to 
		//     reset the knob location
		if(m_bAlphaWheelKnob)
		{
			if(value > 1.0)
				value -= 1.0;
			if(value < 0.0)
				value = 1.0 + value;
		}

		float savedValue = value;

		// --- mormalize to the switch for clicky behaviour
		if(m_bSwitchKnob)
			value /= vmax;

		if(value >= 0.f && heightOfOneImage > 0.) 
		{
			CCoord tmp = heightOfOneImage * (getNumSubPixmaps () - 1);
			if (bInverseBitmap)
				where.v = floor ((1. - value) * tmp);
			else
				where.v = floor (value * tmp);
			where.v -= (int32_t)where.v % (int32_t)heightOfOneImage;
		}

		value = savedValue;

		// --- draw it
		getDrawBackground()->draw(pContext, getViewSize(), where);
	
		//if(m_bSwitchKnob)
		//	value *= vmax;

		// --- broadcast to all LCD (element) Controllers
		if(m_bAlphaWheelKnob)// && bUpdateAlpha)
		{
			VSTGUI_RANGE_BASED_FOR_LOOP (LCDKnobControlList, m_LCDControllers, void*, p)
				((LCDController*)p)->alphaWheelValueChanged();
			VSTGUI_RANGE_BASED_FOR_LOOP_END

	/*		for(std::vector<void*>::iterator it = m_LCDControllers.begin(); it != m_LCDControllers.end(); ++it) 
			{
				((LCDController*)it)->alphaWheelValueChanged();
			}*/

		}
		//((LCDController*)m_pLCDController)->alphaWheelValueChanged();
		//if(m_pLCDController && m_bLCDValueKnob)
		//	((LCDController*)m_pLCDController)->LCDValueKnobChanged();
	}

	setDirty (false);
}


CMouseEventResult CKnobWP::onMouseMoved(CPoint& where, const CButtonState& buttons)
{
	CMouseEventResult res = CKnob::onMouseMoved(where, buttons);

	return res;
}

void CKnobWP::valueChanged()
{
	CControl::valueChanged();

	//float savedValue = value;

	//if(listener)
	//	listener->valueChanged(this);

	// --- doesn't work ;(
	//if(m_bSwitchKnob)
	//{
	//	if(savedValue >= 1.0)
	//		value = savedValue/vmax;
	//	else
	//		value = savedValue;
	//}

	//changed(kMessageValueChanged);
}


}