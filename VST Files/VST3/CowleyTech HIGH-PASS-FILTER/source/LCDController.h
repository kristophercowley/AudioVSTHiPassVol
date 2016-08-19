#ifndef __wpswitchcontroller__
#define __wpswitchcontroller__

#include "vstgui/vstgui.h"
#include "vstgui/vstgui_uidescription.h"
#include "base/source/fobject.h"
#include "public.sdk/source/vst/vstparameters.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
//#include "public.sdk/source/vst/vsteditcontroller.h"
#include "KnobWP.h"

namespace VSTGUI {

//------------------------------------------------------------------------
class LCDController : public Steinberg::FObject, public DelegationController
{
public:
	LCDController(IController* baseController, Steinberg::Vst::ParameterContainer* pLCDparameters, 
						Steinberg::Vst::Parameter* alphaParameter, 
						Steinberg::Vst::Parameter* LCDParameter,
						Steinberg::Vst::EditController* pEditController);
	~LCDController();
	
	// --- triggered when LCD Value Knob moves
	void valueChanged (CControl* pControl) VSTGUI_OVERRIDE_VMETHOD;
		
	// --- called once per object that has this as a controller
	CView* verifyView (CView* view, const UIAttributes& attributes, IUIDescription* description) VSTGUI_OVERRIDE_VMETHOD;

	// --- standard overrides (not needed but implemented in case they need tweaking later
	void controlBeginEdit (CControl* pControl) VSTGUI_OVERRIDE_VMETHOD;
	void controlEndEdit (CControl* pControl) VSTGUI_OVERRIDE_VMETHOD;

	// --- funciton that the alpha wheel object calls to send messages to us
	void alphaWheelValueChanged();

	//-----------------------------------------------------------------------------
	OBJ_METHODS(LCDController, FObject)

protected:

	// --- override for update function, called from us when control changes
	void PLUGIN_API update (Steinberg::FUnknown* changedUnknown, Steinberg::int32 message) VSTGUI_OVERRIDE_VMETHOD;

	Steinberg::Vst::Parameter* m_pAlphaParameter;		// param for AW knob; need it to look up index
	Steinberg::Vst::EditController* m_pEditController;	// the edit controller
	Steinberg::Vst::ParameterContainer* m_pLCDparameters;	// the contianer of parmaeters indexed with alpha wheel
	Steinberg::Vst::Parameter* m_pCurrentParameter;		// the currently selected param

	char* m_sIndexAndCount;
	char* m_sTotalParams;

	// -- controls
	CAnimKnob* m_pLCDValueKnob;
	CTextLabel* m_pControlLabel;
	CTextLabel* m_pIndexAndCountLabel; 
	CTextEdit* m_pControlEdit;

	//SharedPointer<UIDescription> uiDescription;
};

} // namespace

#endif // __vst3padcontroller__
