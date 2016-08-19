//-----------------------------------------------------------------------------
// LICENSE
// (c) 2013, Steinberg Media Technologies GmbH, All Rights Reserved
//-----------------------------------------------------------------------------
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of the Steinberg Media Technologies nor the names of its
//     contributors may be used to endorse or promote products derived from this
//     software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.
//-----------------------------------------------------------------------------

#ifndef __vst_synth_processor__
#define __vst_synth_processor__

#include "public.sdk/source/vst/vstsinglecomponenteffect.h"

// NOTE: the wrapper include here MUST:
//			be AFTER the #include vstsinglecomponenteffect AND
//		    PRECEDE any #include that refernces vsteditcontroller,
//          which is #include "vstgui/plugin-bindings/vst3editor.h" below!
#include "public.sdk/source/vst/vst2wrapper/vst2wrapper.h"

// --- MIDI EVENTS
#include "pluginterfaces/vst/ivstevents.h"

// --- WString Support
#include "pluginterfaces/base/ustring.h"

// --- VST3EditorDelegate
#include "vstgui/plugin-bindings/vst3editor.h"

// --- RackAFX Specific stuff
#include "synthfunctions.h"
#include "plugin.h"
#include "KnobWP.h"
#include <vector>

const UINT LCD_VISIBLE			= 25;
const UINT JS_VISIBLE			= 28;
const UINT ASSIGNBUTTON_1		= 32768;
const UINT ASSIGNBUTTON_2		= 32769;
const UINT ASSIGNBUTTON_3		= 32770;
const UINT ALPHA_WHEEL			= 32771;
const UINT LCD_KNOB				= 32772;
const UINT JOYSTICK_X_PARAM		= 32773;
const UINT JOYSTICK_Y_PARAM		= 32774;
const UINT JOYSTICK				= 32775; // used in RAFX only!
const UINT TRACKPAD 			= 32776;
const UINT LCD_TITLE 			= 32777;
const UINT LCD_COUNT 			= 32778;

// --- v6.6 custom
const UINT RAFX_VERSION = 0;
const UINT VSTGUI_VERSION = 1;
const UINT KNOB_MODE = 2;
const UINT LATENCY_IN_SAMPLES = 0;

#define MAX_VOICES 16
#define OUTPUT_CHANNELS 2 // stereo only!
#define INPUT_CHANNELS 2 // stereo only!

namespace Steinberg {
namespace Vst {
namespace Sock2VST3 {
/*
	The Processor object here ALSO contains the Edit Controller component since these
	are combined as SingleComponentEffect; see documentation
*/
class VST3EditorWP;
class CRafxCustomView;

class Processor : public SingleComponentEffect, public IMidiMapping, public IUnitInfo, public VST3EditorDelegate
{
public:
	// --- constructor
	Processor();

	// --- destructor
	~Processor();

	/*** IAudioProcessor Interface ***/
	// --- One time init to define our I/O and vsteditcontroller parameters
	tresult PLUGIN_API initialize(FUnknown* context);

	// --- Define the audio I/O we support
	tresult PLUGIN_API setBusArrangements(SpeakerArrangement* inputs, int32 numIns, SpeakerArrangement* outputs, int32 numOuts);

	// --- Define our word-length capabilities (currently 32 bit only)
	tresult PLUGIN_API canProcessSampleSize(int32 symbolicSampleSize);

	// --- you can access info about the processing via ProcessSetup; see ivstaudioprocessor.h
	tresult PLUGIN_API setupProcessing(ProcessSetup& newSetup);

	// --- Turn on/off; this is equivalent to prepareForPlay() in RAFX
	tresult PLUGIN_API setActive(TBool state);

	// --- Serialization: Save and load presets from a file stream
	//					  These get/set the RackAFX variables
	tresult PLUGIN_API setState(IBStream* fileStream);
	tresult PLUGIN_API getState(IBStream* fileStream);

	// --- functions to reduce size of process()
	//     Update the GUI control variables
	bool doControlUpdate(ProcessData& data);

	// --- for MIDI note-on/off, aftertouch
	bool doProcessEvent(Event& vstEvent);

	// --- The all important process method where the audio is rendered/effected
	tresult PLUGIN_API process(ProcessData& data);


	/*** EditController Functions ***/
	//
	// --- IMidiMapping
	virtual tresult PLUGIN_API getMidiControllerAssignment(int32 busIndex, int16 channel, CtrlNumber midiControllerNumber, ParamID& id/*out*/);

	// --- IPlugView: create our custom GUI
	IPlugView* PLUGIN_API createView(const char* _name);

	// --- VST3EditorDelegate
	IController* createSubController(UTF8StringPtr name, IUIDescription* description, VST3Editor* editor) VSTGUI_OVERRIDE_VMETHOD;
	virtual CView* createCustomView (UTF8StringPtr name, const UIAttributes& attributes, IUIDescription* description, VST3Editor* editor);
	virtual void didOpen(VST3Editor* editor);		///< called after the editor was opened
	virtual void willClose(VST3Editor* editor);	///< called before the editor will close

	// --- oridinarily not needed; see documentation on Automation for using these
	virtual ParamValue PLUGIN_API normalizedParamToPlain(ParamID id, ParamValue valueNormalized);
	virtual ParamValue PLUGIN_API plainParamToNormalized(ParamID id, ParamValue plainValue);

	// --- custom editor that createView() returns
	VST3EditorWP* m_pVST3Editor;

	// --- end. this destroys the RackAFX core
	tresult PLUGIN_API terminate();

	// --- for sendUpdateGUI()
	void updatePluginParams();
	virtual tresult receiveText(const char8* text);

	// --- helper function for serialization
	tresult PLUGIN_API setParamNormalizedFromFile(ParamID tag, ParamValue value);

	// --- serialize-read from file to setup the GUI parameters
	tresult PLUGIN_API setComponentState(IBStream* fileStream);

	// --- for RAFX Wrapper
	char* getEnumString(char* string, int index);

	// --- for meters
	void updateMeters(ProcessData& data, bool bForceOff = false);

	// --- our COM creation method
	static FUnknown* createInstance(void* context) {return (IAudioProcessor*)new Processor(); }

	// --- our Globally Unique ID
	static FUID cid;

	// --- IUnitInfo
	bool addUnit (Unit* unit);

	// --- for future compat; not curently supporting program lists; only have/need Factory Presets!
	bool addProgramList (ProgramList* list);
	ProgramList* getProgramList(ProgramListID listId) const;
	tresult notifyPogramListChange(ProgramListID listId, int32 programIndex = kAllProgramInvalid);

	// --- receives program changes
	tresult PLUGIN_API setParamNormalized (ParamID tag, ParamValue value);

	// --- program list stuff
	virtual int32 PLUGIN_API getProgramListCount();
	virtual tresult PLUGIN_API getProgramListInfo(int32 listIndex, ProgramListInfo& info /*out*/);
	virtual tresult PLUGIN_API getProgramName(ProgramListID listId, int32 programIndex, String128 name /*out*/);
	virtual tresult PLUGIN_API getProgramInfo(ProgramListID listId, int32 programIndex, CString attributeId /*in*/, String128 attributeValue /*out*/);
	virtual tresult PLUGIN_API hasProgramPitchNames(ProgramListID listId, int32 programIndex);
	virtual tresult PLUGIN_API getProgramPitchName(ProgramListID listId, int32 programIndex, int16 midiPitch, String128 name /*out*/);
	virtual tresult setProgramName(ProgramListID listId, int32 programIndex, const String128 name /*in*/);

	// units selection --------------------
	virtual int32 PLUGIN_API getUnitCount(){return units.total();}
	virtual tresult PLUGIN_API getUnitInfo(int32 unitIndex, UnitInfo& info /*out*/);
	virtual UnitID PLUGIN_API getSelectedUnit() {return selectedUnit;}
	virtual tresult PLUGIN_API selectUnit(UnitID unitId) {selectedUnit = unitId; return kResultTrue;}
	virtual tresult PLUGIN_API getUnitByBus(MediaType /*type*/, BusDirection /*dir*/, int32 /*busIndex*/, int32 /*channel*/, UnitID& /*unitId*/ /*out*/) {return kResultFalse;}
	virtual tresult PLUGIN_API setUnitProgramData(int32 /*listOrUnitId*/, int32 /*programIndex*/, IBStream* /*data*/) {return kResultFalse;}
	virtual tresult notifyUnitSelection ();
	virtual void PLUGIN_API update (FUnknown* changedUnknown, int32 message);

	// --- latency support
	uint32 m_uLatencyInSamples; // set in constructor with plugin
	virtual uint32 PLUGIN_API getLatencySamples() {
		return m_uLatencyInSamples; } 

	// --- define the IMidiMapping interface
	OBJ_METHODS(Processor, SingleComponentEffect)
	DEFINE_INTERFACES
		DEF_INTERFACE(IMidiMapping)
		DEF_INTERFACE(IUnitInfo)
	END_DEFINE_INTERFACES(SingleComponentEffect)
	REFCOUNT_METHODS(SingleComponentEffect)

private:
	CPlugIn* m_pRAFXPlugIn;
	double m_dJoystickX;
	double m_dJoystickY;
	bool m_bPlugInSideBypass;
	std::vector<int> meters;
	CRafxCustomView* m_pRafxCustomView;
	bool m_bHasSidechain;

protected:
	// --- IUnitInfo
	TArray<IPtr<Unit> > units;
	TDictionary<ProgramListID, IPtr<ProgramList> > programLists; // for future compat; not curently supporting program lists; only have/need Factory Presets!
	UnitID selectedUnit;

	// --- container of LCD params
	ParameterContainer LCDparameters;
	CKnobWP* m_pAlphaWheelKnob;

	// --- for new RAFX GUI Customization API
	VSTGUI_VIEW_INFO guiInfoStruct;

	// --- v6.6 for log/exp controls
	static inline float fastpow2 (float p)
	{
	  float offset = (p < 0) ? 1.0f : 0.0f;
	  float clipp = (p < -126) ? -126.0f : p;
	  int w = clipp;
	  float z = clipp - w + offset;
	  union { unsigned int i; float f; } v = { static_cast<unsigned int> ( (1 << 23) * (clipp + 121.2740575f + 27.7280233f / (4.84252568f - z) - 1.49012907f * z) ) };

	  return v.f;
	}

	static inline float fastlog2 (float x)
	{
	  union { float f; unsigned int i; } vx = { x };
	  union { unsigned int i; float f; } mx = { (vx.i & 0x007FFFFF) | 0x3f000000 };
	  float y = vx.i;
	  y *= 1.1920928955078125e-7f;

	  return y - 124.22551499f
			   - 1.498030302f * mx.f
			   - 1.72587999f / (0.3520887068f + mx.f);
	}

	// fNormalizedParam = 0->1
	// returns log scaled version 0->1
	inline float calcLogParameter(float fNormalizedParam)
	{
		return (pow(10.f, fNormalizedParam) - 1.0)/9.0;
	}

	// fPluginValue = log scaled version 0->1
	// returns normal 0->1
	inline float calcLogPluginValue(float fPluginValue)
	{
		return log10(9.0*fPluginValue + 1.0);
	}

	// uses cooked variable in UICTRL to set VA Scaled 0->1 values
	// not currently used
	inline float calcVoltOctaveParameter(CUICtrl* pCtrl)
	{
		float fRawValue = pCtrl->fUserDisplayDataLoLimit;
		if(fRawValue > 0)
		{
			double dOctaves = fastlog2(pCtrl->fUserDisplayDataHiLimit/pCtrl->fUserDisplayDataLoLimit);
			if(pCtrl->uUserDataType == intData)
				fRawValue = fastlog2(*(pCtrl->m_pUserCookedIntData)/pCtrl->fUserDisplayDataLoLimit)/dOctaves;
			else if(pCtrl->uUserDataType == floatData)
				fRawValue = fastlog2(*(pCtrl->m_pUserCookedFloatData)/pCtrl->fUserDisplayDataLoLimit)/dOctaves;
			else if(pCtrl->uUserDataType == doubleData)
				fRawValue = fastlog2(*(pCtrl->m_pUserCookedDoubleData)/pCtrl->fUserDisplayDataLoLimit)/dOctaves;
			else if(pCtrl->uUserDataType == UINTData)
				fRawValue = *(pCtrl->m_pUserCookedUINTData);
		}

		return fRawValue;
	}

	// cooked to VA Scaled 0->1 param
	inline float calcVoltOctaveParameter(float fCookedParam, CUICtrl* pCtrl)
	{
		double dOctaves = fastlog2(pCtrl->fUserDisplayDataHiLimit/pCtrl->fUserDisplayDataLoLimit);
		return fastlog2(fCookedParam/pCtrl->fUserDisplayDataLoLimit)/dOctaves;
	}

	// fPluginValue = VA scaled version 0->1
	// returns normal 0->1
	inline float calcVoltOctavePluginValue(float fPluginValue, CUICtrl* pCtrl)
	{
		if(pCtrl->uUserDataType == UINTData)
			return *(pCtrl->m_pUserCookedUINTData);

		double dOctaves = fastlog2(pCtrl->fUserDisplayDataHiLimit/pCtrl->fUserDisplayDataLoLimit);
		float fDisplay = pCtrl->fUserDisplayDataLoLimit*fastpow2(fPluginValue*dOctaves); //(m_fDisplayMax - m_fDisplayMin)*value + m_fDisplayMin; //m_fDisplayMin*fastpow2(value*dOctaves);
		float fDiff = pCtrl->fUserDisplayDataHiLimit - pCtrl->fUserDisplayDataLoLimit;
		return (fDisplay - pCtrl->fUserDisplayDataLoLimit)/fDiff;

	}
	// --- helpers
	float getNormalizedRackAFXVariable(float fCookedVariable, CUICtrl* pUICtrl, float& fGUIWarpedVariable)
	{
		// --- calc normalized value
		float fRawValue = calcSliderVariable(pUICtrl->fUserDisplayDataLoLimit,
											 pUICtrl->fUserDisplayDataHiLimit,
											 fCookedVariable);
		// --- for linear
		fGUIWarpedVariable = fRawValue;

		// --- for nonlinear
		if(pUICtrl->bLogSlider)
		{
			fGUIWarpedVariable = calcLogParameter(fRawValue);
		}
		else if (pUICtrl->bExpSlider)
		{
			fGUIWarpedVariable = calcVoltOctaveParameter(fCookedVariable, pUICtrl);
		}

		return fRawValue;
	}

	inline static bool parseSize (const std::string& str, CPoint& point)
	{
		size_t sep = str.find (',', 0);
		if (sep != std::string::npos)
		{
			point.x = strtol (str.c_str (), 0, 10);
			point.y = strtol (str.c_str () + sep+1, 0, 10);
			return true;
		}
		return false;
	}

	inline bool trimString(char* str)
	{
	    while(*str == ' ' || *str == '\t' || *str == '\n')
            str++;

		int len = strlen(str);
		if(len <= 0) return false;

		while(len >= 0 && (str[len - 1] == ' ' || str[len - 1] == '\t' || *str == '\n'))
		{
            *(str + len - 1) = '\0';
            len--;
		}

		return true;
	}

	// old VST2 function for safe strncpy()
	inline char* vst_strncpy(char* dst, const char* src, size_t maxLen)
	{
		char* result = strncpy(dst, src, maxLen);
		dst[maxLen] = 0;
		return result;
	}

	#if defined _WINDOWS || defined _WINDLL
	char* getMyDLLDirectory(UString cPluginName)
	{
		HMODULE hmodule = GetModuleHandle(cPluginName);

		TCHAR dir[MAX_PATH];
		memset(&dir[0], 0, MAX_PATH*sizeof(TCHAR));
		dir[MAX_PATH-1] = '\0';

		if(hmodule)
			GetModuleFileName(hmodule, &dir[0], MAX_PATH);
		else
			return NULL;

		// convert to UString
		UString DLLPath(&dir[0], MAX_PATH);

		char* pFullPath = new char[MAX_PATH];
		char* pDLLRoot = new char[MAX_PATH];

		DLLPath.toAscii(pFullPath, MAX_PATH);

		int nLenDir = strlen(pFullPath);
		int nLenDLL = wcslen(cPluginName) + 1;	// +1 is for trailing backslash
		memcpy(pDLLRoot, pFullPath, nLenDir-nLenDLL);
		pDLLRoot[nLenDir-nLenDLL] = '\0';

		delete [] pFullPath;

		// caller must delete this after use
		return pDLLRoot;
	}
	#endif
};

// CRafxCustomView creates the frame, populates with plugin view, then resizes frame
class CRafxCustomView: public VSTGUIEditor
{
public:
    CRafxCustomView(void* controller, ViewRect* size = 0);
    virtual ~CRafxCustomView();

	// --- VST3Editor ---
	/** Called when the editor will be opened. */
	virtual bool PLUGIN_API open (void* parent, const PlatformType& platformType = kDefaultNative);

	/** Called when the editor will be closed. */
	virtual void PLUGIN_API close();

	// --- our buddy plugin
	void setPlugIn(CPlugIn* pPlugIn){m_pPlugIn = pPlugIn;}

protected:
	CPlugIn* m_pPlugIn;

};

}}} // namespaces

#endif



