#pragma once
#include "vstgui/lib/controls/cxypad.h"

namespace VSTGUI {

class CXYPadWP : public CXYPad
{
public:
	CXYPadWP(const CRect& size = CRect (0, 0, 0, 0));
//	CXYPadWP(void);
//	~CXYPadWP(void);

protected:
	int32_t tagX;
	int32_t tagY;
	bool m_bDraggingPuck;

	// --- for easy trackpad and XY stuff
public:
	virtual void setValue(float val);

	void setTagX(int32_t tag){tagX = tag;}
	int32_t getTagX(){return tagX;}

	void setTagY(int32_t tag){tagY = tag;}
	int32_t getTagY(){return tagY;}

	bool m_bIsJoystickPad;

	// --- overrides
	void draw(CDrawContext* context) VSTGUI_OVERRIDE_VMETHOD;
	CMouseEventResult onMouseMoved(CPoint& where, const CButtonState& buttons) VSTGUI_OVERRIDE_VMETHOD;
	CMouseEventResult onMouseUp (CPoint& where, const CButtonState& buttons) VSTGUI_OVERRIDE_VMETHOD;
	CMouseEventResult onMouseDown (CPoint& where, const CButtonState& buttons) VSTGUI_OVERRIDE_VMETHOD;

	inline int pointInPolygon(int nvert, float *vertx, float *verty, float testx, float testy)
	{
		int i, j, c = 0;
		for (i = 0, j = nvert-1; i < nvert; j = i++) 
		{
			if ( ((verty[i]>testy) != (verty[j]>testy)) &&
				(testx < (vertx[j]-vertx[i]) * (testy-verty[i]) / (verty[j]-verty[i]) + vertx[i]) )
			c = !c;
		}
	 return c;
	}

protected:
	float m_fVertX[4];
	float m_fVertY[4];

	float m_fLastX;
	float m_fLastY;

};
}
