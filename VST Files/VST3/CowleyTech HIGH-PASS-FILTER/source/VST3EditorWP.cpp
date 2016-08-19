#include "VST3EditorWP.h"

namespace VSTGUI {

//-----------------------------------------------------------------------------
class ParameterChangeListener : public Steinberg::FObject
{
public:
	ParameterChangeListener (Steinberg::Vst::EditController* editController, Steinberg::Vst::Parameter* parameter, CControl* control)
	: editController (editController)
	, parameter (parameter)
	{
		if (parameter)
		{
			parameter->addRef ();
			parameter->addDependent (this);
		}
		addControl (control);
		if (parameter)
			parameter->changed ();
	}

	~ParameterChangeListener ()
	{
		if (parameter)
		{
			parameter->removeDependent (this);
			parameter->release ();
		}
		VSTGUI_RANGE_BASED_FOR_LOOP(ControlList, controls, CControl*, c)
			c->forget ();
		VSTGUI_RANGE_BASED_FOR_LOOP_END
	}

	void addControl (CControl* control)
	{
		control->remember ();
		controls.push_back (control);
		Steinberg::Vst::ParamValue value = 0.;
		if (parameter)
		{
			value = editController->getParamNormalized (getParameterID ());
		}
		else
		{
			CControl* control = controls.front ();
			if (control)
				value = control->getValueNormalized ();
		}
		CParamDisplay* display = dynamic_cast<CParamDisplay*> (control);
		if (display)
			display->setValueToStringProc (valueToString, this);

		if (parameter)
			parameter->deferUpdate ();
		else
			updateControlValue (value);
	}
	
	void removeControl (CControl* control)
	{
		VSTGUI_RANGE_BASED_FOR_LOOP(ControlList, controls, CControl*, c)
			if (c == control)
			{
				controls.remove (control);
				control->forget ();
				return;
			}
		VSTGUI_RANGE_BASED_FOR_LOOP_END
	}
	
	bool containsControl (CControl* control)
	{
		return std::find (controls.begin (), controls.end (), control) != controls.end ();
	}
	
	void PLUGIN_API update (FUnknown* changedUnknown, Steinberg::int32 message)
	{
		if (message == IDependent::kChanged && parameter)
		{
			updateControlValue (editController->getParamNormalized (getParameterID ()));
		}
	}

	Steinberg::Vst::ParamID getParameterID () 
	{
		if (parameter)
			return parameter->getInfo ().id;
		CControl* control = controls.front ();
		if (control)
			return control->getTag ();
		return 0xFFFFFFFF;
	}
	
	void beginEdit ()
	{
		if (parameter)
			editController->beginEdit (getParameterID ());
	}
	
	void endEdit ()
	{
		if (parameter)
			editController->endEdit (getParameterID ());
	}
	
	void performEdit (Steinberg::Vst::ParamValue value)
	{
		if (parameter)
		{
			editController->setParamNormalized (getParameterID (), value);
			editController->performEdit (getParameterID (), value);
		}
		else
		{
			updateControlValue (value);
		}
	}
	Steinberg::Vst::Parameter* getParameter () const { return parameter; }

protected:
	bool convertValueToString (float value, char utf8String[256])
	{
		if (parameter)
		{
			Steinberg::Vst::String128 utf16Str;
			if (parameter && parameter->getInfo ().stepCount)
			{
				// convert back to normalized value
				value = (float)editController->plainParamToNormalized (getParameterID (), (Steinberg::Vst::ParamValue)value);
			}
			editController->getParamStringByValue (getParameterID (), value, utf16Str);
			Steinberg::String utf8Str (utf16Str);
			utf8Str.toMultiByte (Steinberg::kCP_Utf8);
			utf8Str.copyTo8 (utf8String, 0, 256);
			return true;
		}
		return false;
	}

	static bool valueToString (float value, char utf8String[256], void* userData)
	{
		ParameterChangeListener* This = (ParameterChangeListener*)userData;
		return This->convertValueToString (value, utf8String);
	}

	void updateControlValue (Steinberg::Vst::ParamValue value)
	{
		bool mouseEnabled = true;
		bool isStepCount = false;
		Steinberg::Vst::ParamValue defaultValue = 0.5;
		float minValue = 0.f;
		float maxValue = 1.f;
		if (parameter)
		{
			defaultValue = parameter->getInfo ().defaultNormalizedValue;
			if (parameter->getInfo ().flags & Steinberg::Vst::ParameterInfo::kIsReadOnly)
				mouseEnabled = false;
			if (parameter->getInfo ().stepCount)
			{
				isStepCount = true;
				value = parameter->toPlain (value);
				defaultValue = parameter->toPlain (defaultValue);
				minValue = (float)parameter->toPlain ((Steinberg::Vst::ParamValue)minValue);
				maxValue = (float)parameter->toPlain ((Steinberg::Vst::ParamValue)maxValue);
			}
		}
		VSTGUI_RANGE_BASED_FOR_LOOP(ControlList, controls, CControl*, c)
			c->setMouseEnabled (mouseEnabled);
			c->setDefaultValue ((float)defaultValue);
			CTextLabel* label = dynamic_cast<CTextLabel*>(c);
			if (label)
			{
				Steinberg::Vst::ParamValue normValue = value;
				if (isStepCount)
				{
					normValue = parameter->toNormalized (value);
				}
				Steinberg::Vst::String128 utf16Str;
				editController->getParamStringByValue (getParameterID (), normValue, utf16Str);
				Steinberg::String utf8Str (utf16Str);
				utf8Str.toMultiByte (Steinberg::kCP_Utf8);
				label->setText (utf8Str);
			}
			else
			{
				if (isStepCount)
				{
					c->setMin (minValue);
					c->setMax (maxValue);
					COptionMenu* optMenu = dynamic_cast<COptionMenu*>(c);
					if (optMenu)
					{
						optMenu->removeAllEntry ();
						for (Steinberg::int32 i = 0; i <= parameter->getInfo ().stepCount; i++)
						{
							Steinberg::Vst::String128 utf16Str;
							editController->getParamStringByValue (getParameterID (), (Steinberg::Vst::ParamValue)i / (Steinberg::Vst::ParamValue)parameter->getInfo ().stepCount, utf16Str);
							Steinberg::String utf8Str (utf16Str);
							utf8Str.toMultiByte (Steinberg::kCP_Utf8);
							optMenu->addEntry (utf8Str);
						}
						c->setValue ((float)value - minValue);
					}
					else
						c->setValue ((float)value);
				}
				else
					c->setValueNormalized ((float)value);
			}
			c->invalid ();
		VSTGUI_RANGE_BASED_FOR_LOOP_END
	}
	Steinberg::Vst::EditController* editController;
	Steinberg::Vst::Parameter* parameter;
	
	typedef std::list<CControl*> ControlList;
	ControlList controls;
};}

namespace Steinberg {
namespace Vst {
namespace Sock2VST3 {

VST3EditorWP::VST3EditorWP(Steinberg::Vst::EditController* controller, UTF8StringPtr templateName, UTF8StringPtr xmlFile)
: VST3Editor(controller, templateName, xmlFile)
{
	setIdleRate(GUI_IDLE_UPDATE_INTERVAL_MSEC);
	m_uKnobMode = kHostChoice; // --- v6.6
}

// --- v6.6 addition
VSTGUI_INT32 VST3EditorWP::getKnobMode() const
{
	switch(m_uKnobMode)
	{
		case kHostChoice:
		{
			switch(EditController::getHostKnobMode())
			{
				case kRelativCircularMode: return VSTGUI::kRelativCircularMode;
				case kLinearMode: return VSTGUI::kLinearMode;
				case kCircularMode: return VSTGUI::kCircularMode;
			}
			break;
		}

		case kRelativCircularMode: return VSTGUI::kRelativCircularMode;
		case kLinearMode: return VSTGUI::kLinearMode;
		case kCircularMode: return VSTGUI::kCircularMode;
	}

	return VSTGUI::kLinearMode; // RAFX Default!
}

// --- this is overridden for future revisions; currently identical to base class
void VST3EditorWP::valueChanged(CControl* pControl)
{
	ParameterChangeListener* pcl = getParameterChangeListener (pControl->getTag ());
	if(pcl)
	{
		Steinberg::Vst::ParamValue value = pControl->getValueNormalized ();
		CTextEdit* textEdit = dynamic_cast<CTextEdit*> (pControl);
		if (textEdit && pcl->getParameter ())
		{
			Steinberg::String str (textEdit->getText ());
			str.toWideString (Steinberg::kCP_Utf8);
			if (getController ()->getParamValueByString (pcl->getParameterID (), (Steinberg::Vst::TChar*)str.text16 (), value) != Steinberg::kResultTrue)
			{
				pcl->update (0, kChanged);
				return;
			}
		}

		// --- this does the value change edit
		pcl->performEdit(value);
	}
}
// --- the only reason for existence is for timed notifications
CMessageResult VST3EditorWP::notify(CBaseObject* sender, IdStringPtr message)
{
	if(message == CVSTGUITimer::kMsgTimer && getController())
		getController()->receiveText("VSTGUITimerPing");
	
	bool bRecreateView = doCreateView;

	CMessageResult result = VST3Editor::notify(sender, message);

	if(bRecreateView && getController())
		getController()->receiveText("RecreateView");
 	
	return result;
}

// --- RAFX v6.6 --- for GUI control from plugin 
CBitmap* VST3EditorWP::getBitmap(UTF8StringPtr name)
{
	CBitmap* bitmap = description->getBitmap(name);
	return bitmap;
}

UTF8StringPtr VST3EditorWP::lookupBitmapName(const CBitmap* bitmap)
{
	UTF8StringPtr name =  description->lookupBitmapName(bitmap);
	return name;
}


}}}