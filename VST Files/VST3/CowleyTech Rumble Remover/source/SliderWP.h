
#ifndef __csliderwp__
#define __csliderwp__

#include "vstgui/lib/controls/cslider.h"

namespace VSTGUI {

//-----------------------------------------------------------------------------
// CSlider Declaration
//! @brief a slider control
/// @ingroup controls
//-----------------------------------------------------------------------------
class CVerticalSliderWP : public CVerticalSlider
{
public:
	CVerticalSliderWP (const CRect& size, CControlListener* listener, int32_t tag, int32_t iMinPos, int32_t iMaxPos, CBitmap* handle, CBitmap* background, const CPoint& offset = CPoint (0, 0), const int32_t style = kBottom);

	// overrides
	virtual void draw (CDrawContext*) VSTGUI_OVERRIDE_VMETHOD;
	
	CLASS_METHODS(CVerticalSliderWP, CControl)

protected:
	~CVerticalSliderWP ();

};

//-----------------------------------------------------------------------------
// CHorizontalSlider Declaration
//! @brief a horizontal slider control
/// @ingroup controls
//-----------------------------------------------------------------------------
class CHorizontalSliderWP : public CHorizontalSlider
{
public:
	CHorizontalSliderWP (const CRect& size, CControlListener* listener, int32_t tag, int32_t iMinPos, int32_t iMaxPos, CBitmap* handle, CBitmap* background, const CPoint& offset = CPoint (0, 0), const int32_t style = kRight);

	// overrides
	virtual void draw (CDrawContext*) VSTGUI_OVERRIDE_VMETHOD;

	CLASS_METHODS(CHorizontalSliderWP, CControl)

protected:
	~CHorizontalSliderWP ();
};

} // namespace

#endif