#pragma once
#include "vstgui/plugin-bindings/vst3editor.h"
#include <vector>

namespace Steinberg {
namespace Vst {
namespace Sock2VST3 {

#define GUI_IDLE_UPDATE_INTERVAL_MSEC 50
const unsigned int kHostChoice = 3;

class VST3EditorWP : public VST3Editor
{
public:
	VST3EditorWP(Steinberg::Vst::EditController* controller, UTF8StringPtr templateName, UTF8StringPtr xmlFile);
	CMessageResult notify(CBaseObject* sender, IdStringPtr message) VSTGUI_OVERRIDE_VMETHOD;

	// --- RAFX v6.6 adds log/voltoctave controls	
	virtual void valueChanged(CControl* pControl);

	// --- RAFX v6.6 adds knob mode
	VSTGUI_INT32 getKnobMode() const;
	void setKnobMode(VSTGUI_INT32 n){m_uKnobMode = n;}
	
	// --- RAFX v6.6 adds GUI customization 
	CBitmap* getBitmap(UTF8StringPtr name);
	UTF8StringPtr lookupBitmapName(const CBitmap* bitmap);

protected:
	VSTGUI_INT32 m_uKnobMode;
};
}}}