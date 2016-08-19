#include "VSTGUIController.h"

namespace VSTGUI
{
enum {LOG, VOLTOCTAVE, LINEAR};

#ifdef AUPLUGIN
// --- this is called when presets change for us to sync to GUI
void EventListenerDispatch(void *inRefCon, void *inObject, const AudioUnitEvent *inEvent, UInt64 inHostTime, Float32 inValue)
{
    CVSTGUIController* pController = (CVSTGUIController*)inObject;
    if(pController)
        pController->initControls();
}
#endif

CVSTGUIController::CVSTGUIController()
{
	// --- initalize your variables here


	// create a timer used for idle update: will call notify method
	timer = new CVSTGUITimer (dynamic_cast<CBaseObject*>(this));
}

CVSTGUIController::~CVSTGUIController()
{
	// --- stop timer
	if(timer)
		timer->forget();
}

// --- timer handler
CMessageResult CVSTGUIController::notify(CBaseObject* /*sender*/, const char* message)
{
    if(message == CVSTGUITimer::kMsgTimer)
    {
		// --- call idle() to repaint the GUI
        if(frame)
            idle();

        return kMessageNotified;
    }
    return kMessageUnknown;
}

// --- open/frame creation
bool CVSTGUIController::open(void* window, CPlugIn* pPlugIn, int& nWidth, int& nHeight, void* hPlugInInstance)
{
	if(!window) return false;

	m_pPlugIn = pPlugIn;

#if MAC && AUPLUGIN
	m_AUInstance = (AudioUnit)hPlugInInstance; // this is the AU AudioComponentInstance
#else
	m_hPlugInInstance = hPlugInInstance; // this is needed to ensure the resources exist
#endif

	// --- set the return variables (you may want to store them too)
	nWidth = 600;
	nHeight = 300;

	// --- create the frame rect: it dictates the size in pixels
	CRect frameSize(0, 0, nWidth, nHeight);

	// --- construct the frame
	frame = new CFrame(frameSize, this);

	// --- open it
#if defined _WINDOWS || defined WINDOWS || defined _WINDLL
    frame->open(window, kHWND);		// for WinOS, window = HWND
#else
    frame->open(window, kNSView);	// for MacOS, window = NSView*
#endif

	// --- set the frame background color and/or image
	//
	// COLORS: use either built-in colors, or construct your own from r,g,b,a
	/* --- Built In Constants:
	const CColor kTransparentCColor	= CColor (255, 255, 255,   0);
	const CColor kBlackCColor		= CColor (  0,   0,   0, 255);
	const CColor kWhiteCColor		= CColor (255, 255, 255, 255);
	const CColor kGreyCColor		= CColor (127, 127, 127, 255);
	const CColor kRedCColor			= CColor (255,   0,   0, 255);
	const CColor kGreenCColor		= CColor (  0, 255,   0, 255);
	const CColor kBlueCColor		= CColor (  0,   0, 255, 255);
	const CColor kYellowCColor		= CColor (255, 255,   0, 255);
	const CColor kMagentaCColor		= CColor (255,   0, 255, 255);
	const CColor kCyanCColor		= CColor (  0, 255, 255, 255);*/

	// --- example with built-in color
	frame->setBackgroundColor(kBlackCColor);

	// --- example of tiled bitmap - optional
	CBitmap* pTiledBitmap = getBitmap("greymetal.png", 0, 0, 0, 0);

	// --- always check pointer!
	if(pTiledBitmap)
	{
		// --- set it -- this paints over the black background above
		frame->setBackground(pTiledBitmap);

		// --- forget after adding this pointer to the frame, VSTGUI uses reference counting
		pTiledBitmap->forget();
	}

	// --- now that the frame has a background, continue with controls
	//     I made this a separate function because it is usually very long
	createControls();

	// --- the main control inits
	initControls(true); // true = setup AU Listener (only once)

    // --- set/start the timer
	if(timer)
	{
        timer->setFireTime(METER_UPDATE_INTERVAL_MSEC);
        timer->start();
    }

	return true;
}

void CVSTGUIController::close()
{
	if(!frame) return;

	// --- stop timer
	if(timer)
		timer->stop();

	//-- on close we need to delete the frame object.
	//-- once again we make sure that the member frame variable is set to zero before we delete it
	//-- so that calls to setParameter won't crash.
	CFrame* oldFrame = frame;
	frame = 0;
	oldFrame->forget(); // this will remove/destroy controls
}

void CVSTGUIController::idle()
{
	if(!m_pPlugIn) return;

	// --- do any custom specific stuff here
	//
	// --- VU Meter objects need to have their value set here so they animate properly


	// --- handle sendUpdateGUI() from the plug-in
	//     check the UPDATE_GUI flag
	if(m_pPlugIn->m_uPlugInEx[UPDATE_GUI] == 1)
	{
		// --- update the controls
		initControls();

		// --- reset flag
		m_pPlugIn->m_uPlugInEx[UPDATE_GUI] = 0;
	}
	// --- then, update frame - important; this updates edit boxes, sliders, etc...
	if(frame)
		frame->idle();
}

void CVSTGUIController::createControls()
{
	if(!frame)
		return;

	if(!m_pPlugIn)
		return frame->onActivate(true);

	// NOTES ON CREATING CONTROLS
	/*
		TAGS:
			A tag is an integer that identifies the variable that the control is connected to
		    You have many options on how to code your tags; in RackAFX I use a control map due
			to the exotic controls like the LCD and Joystick/XYPad.

			However, since each RAFX variable has a unique ID value, you can use that. It is the
			same ID as listed in the comment block above userInterfaceChange()

		CControlListeners
			Almost all controls are derived from CControl; the control listener is the object that will
			get notified when a value changes - that control listener is this object (see declaration).
			Many controls require that you give them the listner in the constructor.

		TEXT FONTS:
		     NOTE: with VSTGUI4, it is up to you to ensure the font is installed on the
		           target computer; is using exotic fonts, I suggest creating a frame backgroud with
		           the text in place -  less issues

				   You can also use built in fonts that are platform independent
					kSystemFont
					kNormalFontVeryBig
					kNormalFontBig
					kNormalFont
					kNormalFontSmall
					kNormalFontSmaller
					kNormalFontVerySmall
					kSymbolFont

		TEXT STYLES:
			Styles are wire-OR-ed using the following UINTS
			kShadowText
			k3DIn
			k3DOut
			kNoTextStyle
			kNoDrawStyle
			kRoundRectStyle


		FONT STYLES:
			Styles are wire-OR-ed using the following UINTS
			kNormalFace
			kBoldFace
			kItalicFace
			kUnderlineFace
			kStrikethroughFace

	    POSITIONING:
			VSTGUI4 objects are positioned with a CRect object that sets the origin (top, left)
			and size (width, height) variables. The coordinates are based on the view they are being inserted
			into.

		Plugin Control Objects
		Your plugin carries around a list of CUICtrl pointers. You can query the plugin for a CUICtrl pointer
		with the control ID value.
	*/

	// --- add your code here to instantiate and add each view to its parent view
}

void CVSTGUIController::initControls(bool bSetListener)
{
	if(!frame)
		return;

	if(!m_pPlugIn)
		return frame->onActivate(true);

#ifdef AUPLUGIN
	if(m_AUInstance && bSetListener)
    {
        // --- create the event listener and tell it the name of our Dispatcher function
        //     EventListenerDispatcher
		verify_noerr(AUEventListenerCreate(EventListenerDispatch, this,
                                           CFRunLoopGetCurrent(), kCFRunLoopDefaultMode, 0.05, 0.05,
                                           &m_AUEventListener));

        // --- start with first control 0
 		AudioUnitEvent auEvent;

		// --- parameter 0
		AudioUnitParameter parameter = {m_AUInstance, 0, kAudioUnitScope_Global, 0};

		// --- set param & add it
        auEvent.mArgument.mParameter = parameter;
       	auEvent.mEventType = kAudioUnitEvent_ParameterValueChange;
        verify_noerr(AUEventListenerAddEventType(m_AUEventListener, this, &auEvent));

        // --- notice the way additional params are added using mParameterID
        for(int i=1; i<m_pPlugIn->m_UIControlList.count(); i++)
        {
    		auEvent.mArgument.mParameter.mParameterID = i;
            verify_noerr(AUEventListenerAddEventType(m_AUEventListener, this, &auEvent));
        }
	}
#endif

	// --- initialize the location of the controls based on the plugin variables
	//     this is also used for sendUpdateGUI()
	//
	// --- All VSTGUI controls accept a normalized value; use built in helper functions to make that easy
	//
	// --- add your code here to initialize the GUI, see example code





	// --- lastly, call the repaint() function on frame
	frame->invalid();
}

int32_t CVSTGUIController::getKnobMode() const
{
	/* choices are: kLinearMode;
				    kRelativCircularMode;
					kCircularMode; */

	return kLinearMode;
}

void CVSTGUIController::valueChanged(VSTGUI::CControl* pControl)
{
	if(!m_pPlugIn) return;

	// --- get the RAFX ID for this control
	int32_t nTag = pControl->getTag();

	// --- get the control for re-broadcast of some types
	CUICtrl* pUICtrl = m_pPlugIn->getUICtrlByControlID(nTag);
	if(!pUICtrl) return;

	// --- Normalized control value
	float fControlValue = 0.0;

	// --- edit controls are handled differently than all others since they are text based
	//
	// Use dynamic casting to see if this is an edit control
	CTextEdit* control = dynamic_cast<CTextEdit*>(pControl);
	if(control)
		fControlValue = updateEditControl(pControl, pUICtrl);
	else
		fControlValue = pControl->getValue();

	// --- this function handles the case of Option Menus, which are a bit different
	//     as they store actual, not normalized, index values
	float fPluginValue = getPluginParameterValue(fControlValue, pControl);

	// --- deal with log/volt-octave controls
	if(pUICtrl->bLogSlider)
		fPluginValue = calcLogPluginValue(fPluginValue); //log10(9.0*fPluginValue + 1.0);
	else if(pUICtrl->bExpSlider)
		fPluginValue = calcVoltOctavePluginValue(fPluginValue, pUICtrl);

	// --- fPluginValue is now final normalized value for plugin
	//
	// --- this helper function also calls userInterfaceChange() on the plugin
	setPlugInParameterNormalized(pUICtrl, fPluginValue);

	// --- now broadcast control change to all other controls with same tag, but not this control
	switch(nTag)
	{
		case 1:
		{
			// --- add your code for broadcast to first control, see example code

			break;
		}
		// --- add a case statement for each controlID


		default:
			break;

	}

}

CBitmap* CVSTGUIController::getBitmap(const CResourceDescription& desc, CCoord left, CCoord top, CCoord right, CCoord bottom)
{
	// --- if coords are all >= 0 then this is a nine-part tiled, else normal
	if(left >= 0 && top >= 0 && right >= 0 && bottom >= 0)
		return new CNinePartTiledBitmap(desc, (CNinePartTiledBitmap::PartOffsets(left, top, right, bottom)));
	else
		return new CBitmap(desc);

	return NULL; // should never happen
}

float CVSTGUIController::getNormalizedValue(CUICtrl* pUICtrl)
{
	float fRawValue = 0;
	switch(pUICtrl->uUserDataType)
	{
		case intData:
		{
			fRawValue = calcSliderVariable(pUICtrl->fUserDisplayDataLoLimit, pUICtrl->fUserDisplayDataHiLimit, *(pUICtrl->m_pUserCookedIntData));
			break;
		}

		case floatData:
		{
			fRawValue = calcSliderVariable(pUICtrl->fUserDisplayDataLoLimit, pUICtrl->fUserDisplayDataHiLimit, *(pUICtrl->m_pUserCookedFloatData));
			break;
		}

		case doubleData:
		{
			fRawValue = calcSliderVariable(pUICtrl->fUserDisplayDataLoLimit, pUICtrl->fUserDisplayDataHiLimit, *(pUICtrl->m_pUserCookedDoubleData));
			break;
		}

		case UINTData:
		{
			fRawValue = calcSliderVariable(pUICtrl->fUserDisplayDataLoLimit, pUICtrl->fUserDisplayDataHiLimit, *(pUICtrl->m_pUserCookedUINTData));
			break;
		}

		default:
			break;
	}

	if(pUICtrl->bLogSlider && pUICtrl->uUserDataType != UINTData)
	{
		fRawValue = calcLogParameter(fRawValue);
	}
	else if(pUICtrl->bExpSlider && pUICtrl->uUserDataType != UINTData)
	{
		if(pUICtrl->fUserDisplayDataLoLimit > 0)
		{
			fRawValue = calcVoltOctaveParameter(pUICtrl);
		}
	}
	return fRawValue;
}

}