#include "LCDController.h"
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/base/futils.h"
#include "pluginterfaces/base/fstrdefs.h"
#include "base/source/fstring.h"
#include "pluginconstants.h"

namespace VSTGUI {

LCDController::LCDController(IController* baseController, Steinberg::Vst::ParameterContainer* pLCDparameters, 
									 Steinberg::Vst::Parameter* alphaParameter,
									 Steinberg::Vst::Parameter* LCDParameter,
									 Steinberg::Vst::EditController* pEditController) //Steinberg::Vst::Parameter* param)
: DelegationController(baseController)
{
	ASSERT(pLCDparameters);

	// --- container of LCD Params
	m_pLCDparameters = pLCDparameters;
	int count = m_pLCDparameters->getParameterCount();
	m_sTotalParams = intToString(count);
	m_sIndexAndCount = addStrings("1/", m_sTotalParams);

	// --- the fixed alpha (0 -> 1) and LCD (0 -> 1)controls.
	m_pAlphaParameter = alphaParameter; // to use when alpha changes
	
	// --- save edit ctrl
	m_pEditController = pEditController;

	// --- get the current param
	Steinberg::Vst::ParamValue f = m_pAlphaParameter->getNormalized();
	Steinberg::Vst::ParamValue plain = m_pAlphaParameter->toPlain(f);
	m_pCurrentParameter = m_pLCDparameters->getParameterByIndex((int)plain);
	
	m_pLCDValueKnob = NULL;
	m_pControlEdit = NULL;
	m_pControlLabel = NULL;
	m_pIndexAndCountLabel = NULL;
}

void LCDController::alphaWheelValueChanged()
{
	if(this->refCount < 0 || !m_pAlphaParameter)
		return;

	Steinberg::Vst::ParamValue f = m_pAlphaParameter->getNormalized();
	Steinberg::Vst::ParamValue plain = m_pAlphaParameter->toPlain(f);
	Steinberg::Vst::Parameter* pCurrentParameter = m_pLCDparameters->getParameterByIndex((int)plain);
	
	if(pCurrentParameter != m_pCurrentParameter)
	{
		m_pCurrentParameter->removeDependent(this);
		m_pCurrentParameter = pCurrentParameter;
		m_pCurrentParameter->addDependent(this);

		if(strlen(m_sIndexAndCount) > 0)
		{
			if(m_pIndexAndCountLabel)
			{		
				// --- this frees the previous text
				//     need this or crashes when deleting below
				UTF8StringPtr txt(" ");
				m_pIndexAndCountLabel->setText(txt); 
			}
			// --- bye
			delete [] m_sIndexAndCount;
		}

		char* current = intToString((int)plain + 1);
		char* p = addStrings(current, "/");
		m_sIndexAndCount = addStrings(p, m_sTotalParams);

		delete [] p;
		delete [] current;

		// --- this is to set the initial location; must change tag or will not update
		//     during the update call below
		if(m_pLCDValueKnob)
		{
			// nothing so far
		}

		if(m_pControlEdit)
		{
			// int32 stepCount;	//< number of discrete steps (0: continuous, 1: toggle, discrete value otherwise 
								//< (corresponding to max - min, for example: 127 for a min = 0 and a max = 127) - see \ref vst3parameterIntro)

			m_pControlEdit->setTag(m_pCurrentParameter->getInfo().id);
			if(m_pCurrentParameter->getInfo().stepCount > 0)
				m_pControlEdit->setMax(m_pCurrentParameter->getInfo().stepCount);
			else
				m_pControlEdit->setMax(1.0);

		}

		// --- this is our message, but will be calle externally
		//     we can pass a message that the m_pLCDValueKnob tag needs to be reset
		update(m_pCurrentParameter, kChanged);
	}

}

LCDController::~LCDController()
{
	m_pCurrentParameter->removeDependent(this);
}


// --- this will get called for every view object that has this object as a sub-controller
//     Because of the UI editor for VSTGUI, we can't have shared controller objects (only a GUI editor
//     issue, you can share these as long as you remember to call addRef())
CView* LCDController::verifyView (CView* view, const UIAttributes& attributes, IUIDescription* description)
{
	// --- (1) the value knob
	CAnimKnob* knob = dynamic_cast<CAnimKnob*>(view);
	if(knob)
	{
		// --- save
		m_pLCDValueKnob = knob;

		// --- NOTE: do not change the tag on knob, it always transmits 0->1

		// --- this will allow the knob to trigger valueChanged() below
		m_pLCDValueKnob->setListener(this);

		// --- update
		update(m_pCurrentParameter, kChanged);
	}

	// --- (2) the edit control that shows the variable value
	CTextEdit* edit = dynamic_cast<CTextEdit*>(view);
	if(edit)
	{
		// --- save
		m_pControlEdit = edit;

		// --- set the knob tag
		m_pControlEdit->setTag(m_pCurrentParameter->getInfo().id);
		if(m_pCurrentParameter->getInfo().stepCount > 0)
			m_pControlEdit->setMax(m_pCurrentParameter->getInfo().stepCount);
		else
			m_pControlEdit->setMax(1.0);

		update(m_pCurrentParameter, kChanged);
	}
	
	// --- the control name (and maybe units?)
	CTextLabel* label = dynamic_cast<CTextLabel*>(view);
	if(label)
	{
		// --- decode the label; can have more
		UTF8StringPtr p = label->getText();

		if(strcmp(p, "LCD Control Name") == 0)
		{
			// --- just save it
			m_pControlLabel = label;
			update(m_pCurrentParameter, kChanged);
		}
		else if(strcmp(p, "1/N") == 0)
		{
			// --- just save it
			m_pIndexAndCountLabel = label;
			update(m_pCurrentParameter, kChanged);
		}
	}

	return view;
}

// --- this is the update that gets called when the alpha wheel changes (or the view is revealed after being hidden, etc...)
void PLUGIN_API LCDController::update(Steinberg::FUnknown* changedUnknown, Steinberg::int32 message)
{	
	// --- parse the param
	Steinberg::Vst::Parameter* p = Steinberg::FCast<Steinberg::Vst::Parameter>(changedUnknown);
	if(!p) return;

	// --- get normalized value
	Steinberg::Vst::ParamValue fNormalizedValue = p->getNormalized();

	// --- update the controls
	if(m_pLCDValueKnob)
	{
		// --- set and update
		m_pLCDValueKnob->setValue(fNormalizedValue);
		m_pLCDValueKnob->invalid();
	}
	if(m_pControlEdit)
	{
		// --- set and update
		if(m_pControlEdit->getMax() > 1.0)
			m_pControlEdit->setValue(p->toPlain(fNormalizedValue));	
		else
			m_pControlEdit->setValue(fNormalizedValue);	

		m_pControlEdit->invalid();
	}
	if(m_pControlLabel)
	{		
		// --- convert title to char*				
		Steinberg::String controlName(m_pCurrentParameter->getInfo().title);
		controlName.toMultiByte(Steinberg::kCP_Utf8);
		
		// --- update textlabel
		m_pControlLabel->setText(controlName);
		m_pControlLabel->invalid();
	}
	if(m_pIndexAndCountLabel)
	{		
		// --- update textlabel
		UTF8StringPtr txt(m_sIndexAndCount);
		m_pIndexAndCountLabel->setText(txt); 
		m_pIndexAndCountLabel->invalid();
	}
}
//------------------------------------------------------------------------
void LCDController::controlBeginEdit(CControl* pControl)
{
	if(pControl == m_pLCDValueKnob)
	{
		m_pEditController->startGroupEdit();
		m_pEditController->beginEdit(m_pCurrentParameter->getInfo().id);
	}
	else
	{
		DelegationController::controlBeginEdit(pControl);
	}
}

//------------------------------------------------------------------------
void LCDController::controlEndEdit(CControl* pControl)
{
	if(pControl == m_pLCDValueKnob)
	{
		m_pEditController->endEdit(m_pCurrentParameter->getInfo().id);
		m_pEditController->finishGroupEdit();
	}
	else
	{
		DelegationController::controlEndEdit(pControl);
	}
}

// --- all the tie-in work is here
void LCDController::valueChanged(CControl* pControl)
{
	//if(pControl == m_pLCDValueKnob || pControl == m_pControlEdit)

	// --- the trick: must set us as the LISTENER for these 
	//     so that when they change, we can update the current PARAMETER (not control!)	
	if(pControl == m_pLCDValueKnob)
	{
		// --- tell the GUI to perform a backdoor edit on the PARAMETER (not control) using the Control's value
		//     The LCD Value is controlling a dummy parameter that we never see
		//     Here is where we pick up it's value
		m_pEditController->performEdit(m_pCurrentParameter->getInfo().id, pControl->getValue());
		m_pEditController->setParamNormalized(m_pCurrentParameter->getInfo().id, pControl->getValue());
	}
	else
	{
		DelegationController::valueChanged(pControl);
	}
}

}