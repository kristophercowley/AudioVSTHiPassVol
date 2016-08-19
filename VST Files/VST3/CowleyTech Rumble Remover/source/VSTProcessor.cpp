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

// --- first
#include "vstprocessor.h"

#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/base/ftypes.h"
#include "pluginterfaces/base/futils.h"

#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"

#include "base/source/fstreamer.h"

#include "version.h"	// for versioning
#include "PeakParameter.h"
#include "LogParameter.h"	// WP Custom paremeter for RAFX
#include "VoltOctaveParameter.h"	// WP Custom paremeter for RAFX

// --- RackAFX Core
#include "synthfunctions.h"
#include "synthparamlimits.h" // param limits file
#include "SynthParamLimits.h"
#include "SimpleHPF.h"
#include "RafxPluginFactory.h"

// --- for RackAFX GUI support
#include "vstgui/plugin-bindings/vst3padcontroller.h"
#include "vstgui/plugin-bindings/vst3groupcontroller.h"
#include "vstgui/uidescription/xmlparser.h"

// --- v6.6
#include "vstgui/uidescription/uiviewswitchcontainer.h"

// --- RackAFX custom VSTGUI4 derived classes
#include "VST3EditorWP.h"
#include "LCDController.h"
#include "XYPadWP.h"
#include "VuMeterWP.h"
#include "SliderWP.h"
#include "XYPadWP.h"
#include "PadControllerWP.h"
#include "RafxPluginFactory.h"
#include "KickButtonWP.h"

// --- Synth Stuff
#define SYNTH_PROC_BLOCKSIZE 32 // 32 samples per processing block = 0.7 mSec = OK for tactile response WP
float dummyInputL[SYNTH_PROC_BLOCKSIZE];
float dummyInputR[SYNTH_PROC_BLOCKSIZE];
float* dummyInputPtr[2];
extern void* moduleHandle;

enum {
	kPresetParam = 'prst',
};
// --- VST2 Wrapper built-in
::AudioEffect* createEffectInstance(audioMasterCallback audioMaster)
{
    return Steinberg::Vst::Vst2Wrapper::create(GetPluginFactory(),	/* calls factory.cpp macro */
											   Steinberg::Vst::Sock2VST3::Processor::cid,		/* proc CID */
											   'S2v3',	/* 10 dig code for Sock2VST3 */
											   audioMaster);
}

namespace Steinberg {
namespace Vst {
namespace Sock2VST3 {

using namespace std;

const UINT FILTER_CONTROL_USER_VARIABLE				= 105; // user variables 8/7/14
const UINT FILTER_CONTROL_USER_VSTGUI_VARIABLE		= 106; // user variables 8/7/14

// --- for versioning in serialization
static uint64 CowleyTech Rumble RemoverVersion = 0;

// --- the unique identifier (use guidgen.exe to generate)
FUID Processor::cid(712195627, 4294967233, 4294967226, 1279993116);

/*
	Processor::Processor()
	construction
*/
Processor::Processor()
{
	// --- just to be a good programmer
	m_pRAFXPlugIn = NULL;
	m_pRafxCustomView = NULL;
	m_pVST3Editor = NULL;
	m_bHasSidechain = false;

	// ---  now set plugin buddy
	m_pRAFXPlugIn = CRafxPluginFactory::getRafxPlugIn();
	// --- set latency, sidechain
	if(m_pRAFXPlugIn)
	{
		m_uLatencyInSamples = (uint32)(m_pRAFXPlugIn->m_fPlugInEx[LATENCY_IN_SAMPLES]);
		m_bHasSidechain = m_pRAFXPlugIn->m_uPlugInEx[ENABLE_SIDECHAIN_VSTAU];
	}

	m_dJoystickX = 0.5;
	m_dJoystickY = 0.5;
	m_bPlugInSideBypass = false;
}

/*
	Processor::~Processor()
	destruction
*/
Processor::~Processor()
{
	if(m_pRAFXPlugIn)
		delete m_pRAFXPlugIn;
	m_pRAFXPlugIn = NULL;
}
/*
	Processor::initialize()
	Call the base class
	Add a Stereo Audio Output
	Add a MIDI event inputs (16: one for each channel)
	Add GUI parameters (EditController part)
*/
tresult PLUGIN_API Processor::initialize(FUnknown* context)
{
	tresult result = SingleComponentEffect::initialize(context);
	if(result == kResultTrue)
	{
		// stereo output bus (SYNTH and FX)
		addAudioOutput(STR16("Audio Output"), SpeakerArr::kStereo);

		if(!m_pRAFXPlugIn->m_bOutputOnlyPlugIn)
		{
			// stereo input bus (FX ONLY)
			addAudioInput(STR16("AudioInput"), SpeakerArr::kStereo);

			if(m_bHasSidechain)
				addAudioInput(STR16("AuxInput"), SpeakerArr::kStereo, kAux);
		}

		// SYNTH/FX: MIDI event input bus, 16 channels
		addEventInput(STR16("Event Input"), 16);

		// --- our buddy plugin
		if(!m_pRAFXPlugIn)
			m_pRAFXPlugIn = CRafxPluginFactory::getRafxPlugIn();

		m_pAlphaWheelKnob = NULL;
		m_pVST3Editor = NULL;

		// --- Init parameters
		Parameter* param;

		if(m_pRAFXPlugIn)
		{
			int nParams = m_pRAFXPlugIn->m_UIControlList.count();

			// iterate
			for(int i = 0; i < nParams; i++)
			{
				// they are in VST proper order in the ControlList - do NOT reference them with RackAFX ID values any more!
				CUICtrl* pUICtrl = m_pRAFXPlugIn->m_UIControlList.getAt(i);

				if(pUICtrl)
				{
					float fDefault = 0.0;
					if(pUICtrl->uUserDataType == intData)
						fDefault = pUICtrl->fInitUserIntValue;
					else if(pUICtrl->uUserDataType == floatData)
						fDefault = pUICtrl->fInitUserFloatValue;
					else if(pUICtrl->uUserDataType == doubleData)
						fDefault = pUICtrl->fInitUserDoubleValue;
					else if(pUICtrl->uUserDataType == UINTData)
						fDefault = pUICtrl->fInitUserUINTValue;

					if(pUICtrl->uControlType == FILTER_CONTROL_LED_METER)
					{
						PeakParameter* peakParam = new PeakParameter(ParameterInfo::kIsReadOnly, i, USTRING(pUICtrl->cControlName));
						parameters.addParameter(peakParam);
						meters.push_back(i); // save tag
					}
					else if(pUICtrl->uControlType == FILTER_CONTROL_CONTINUOUSLY_VARIABLE ||
							pUICtrl->uControlType == FILTER_CONTROL_RADIO_SWITCH_VARIABLE ||
							pUICtrl->uControlType == FILTER_CONTROL_USER_VSTGUI_VARIABLE ||
							pUICtrl->uControlType == FILTER_CONTROL_COMBO_VARIABLE)
					{
						if(pUICtrl->uUserDataType != UINTData)
						{
							char* pName = pUICtrl->cControlName;
							bool bHaveUnits = false;//trimString(pUICtrl->cControlUnits);

							// --- add () for LCS Control Units
							if(pUICtrl->uControlTheme[31] == 1 && bHaveUnits)
							{
								char* p1 = addStrings(" (", pUICtrl->cControlUnits);
								char* p2 = addStrings(p1, ")");
								pName = addStrings(pName, p2);
								delete [] p1;
								delete [] p2;
							}

							Parameter* param = NULL;

							if(pUICtrl->bLogSlider)
							{
								param = new LogParameter(USTRING(pName),
														   i, /* INDEX !! */
														   USTRING(pUICtrl->cControlUnits),
														   pUICtrl->fUserDisplayDataLoLimit,
														   pUICtrl->fUserDisplayDataHiLimit,
														   fDefault);
								param->setPrecision(pUICtrl->uUserDataType == intData ? 0 : 2); // fractional sig digits
							}
							else if(pUICtrl->bExpSlider)
							{
								param = new VoltOctaveParameter(USTRING(pName),
														   i, /* INDEX !! */
														   USTRING(pUICtrl->cControlUnits),
														   pUICtrl->fUserDisplayDataLoLimit,
														   pUICtrl->fUserDisplayDataHiLimit,
														   fDefault);
								param->setPrecision(pUICtrl->uUserDataType == intData ? 0 : 2); // fractional sig digits
							}
							else // linear
							{
								param = new RangeParameter(USTRING(pName),
														   i, /* INDEX !! */
														   USTRING(pUICtrl->cControlUnits),
														   pUICtrl->fUserDisplayDataLoLimit,
														   pUICtrl->fUserDisplayDataHiLimit,
														   fDefault);
								param->setPrecision(pUICtrl->uUserDataType == intData ? 0 : 2); // fractional sig digits
							}
							//
							// --- add it
							parameters.addParameter(param);

							if(pUICtrl->uControlTheme[31] == 1)
							{
								LCDparameters.addParameter(param);
								param->addRef(); // important!
							}
						}
						else
						{
							char* pName = pUICtrl->cControlName;
							bool bHaveUnits = false; //trimString(pUICtrl->cControlUnits);

							// --- add () for LCS Control Units
							if(pUICtrl->uControlTheme[31] == 1 && bHaveUnits)
							{
								char* p1 = addStrings(" (", pUICtrl->cControlUnits);
								char* p2 = addStrings(p1, ")");
								pName = addStrings(pName, p2);
								delete [] p1;
								delete [] p2;
							}

							StringListParameter* enumStringParam = new StringListParameter(USTRING(pName), i);
							int m = 0;
							char* pEnumString = NULL;

							pEnumString = getEnumString(pUICtrl->cEnumeratedList, m++);
							while(pEnumString)
							{
								enumStringParam->appendString(USTRING(pEnumString));
								pEnumString = getEnumString(pUICtrl->cEnumeratedList, m++);
							}
							parameters.addParameter(enumStringParam);

							if(pUICtrl->uControlTheme[31] == 1)
							{
								LCDparameters.addParameter(enumStringParam);
								enumStringParam->addRef(); // important!
							}
						}
					}
				}
			}

//			if(m_pRAFXPlugIn->m_uControlTheme[LCD_VISIBLE])
			if(true) // for off-screen view support
			{
				Parameter* param = new RangeParameter(USTRING("Alpha Wheel"),
										  ALPHA_WHEEL, /* INDEX !! */
										   USTRING(""),
										   0,
										   LCDparameters.getParameterCount()-1,
										   0);
				param->setPrecision(2); // fractional sig digits
				parameters.addParameter(param);

				PeakParameter* peakParam = new PeakParameter(ParameterInfo::kIsReadOnly, LCD_KNOB, USTRING("LCD Value"));
				parameters.addParameter(peakParam);
			}

//			if(m_pRAFXPlugIn->m_uControlTheme[JS_VISIBLE])
			if(true) // for off-screen view support
			{
				param = new RangeParameter(USTRING("VectorJoystick X"), JOYSTICK_X_PARAM, USTRING(""), 0, 1, 0.5);
				param->setPrecision(2); // fractional sig digits
				parameters.addParameter(param);

				param = new RangeParameter(USTRING("VectorJoystick Y"), JOYSTICK_Y_PARAM, USTRING(""), 0, 1, 0.5);
				param->setPrecision(2); // fractional sig digits
				parameters.addParameter(param);
			}

			char* p = m_pRAFXPlugIn->m_AssignButton1Name;
			int n = strlen(p);
			if(n > 0)
			{
				param = new RangeParameter(USTRING(m_pRAFXPlugIn->m_AssignButton1Name), ASSIGNBUTTON_1, USTRING(""),
										   MIN_UNIPOLAR, MAX_UNIPOLAR, DEFAULT_UNIPOLAR);
				param->setPrecision(0); // fractional sig digits
				parameters.addParameter(param);
			}

			p = m_pRAFXPlugIn->m_AssignButton2Name;
			n = strlen(p);
			if(n > 0)
			{
				param = new RangeParameter(USTRING(m_pRAFXPlugIn->m_AssignButton2Name), ASSIGNBUTTON_2, USTRING(""),
										   MIN_UNIPOLAR, MAX_UNIPOLAR, DEFAULT_UNIPOLAR);
				param->setPrecision(0); // fractional sig digits
				parameters.addParameter(param);
			}

			p = m_pRAFXPlugIn->m_AssignButton3Name;
			n = strlen(p);
			if(n > 0)
			{
				param = new RangeParameter(USTRING(m_pRAFXPlugIn->m_AssignButton3Name), ASSIGNBUTTON_3, USTRING(""),
										   MIN_UNIPOLAR, MAX_UNIPOLAR, DEFAULT_UNIPOLAR);
				param->setPrecision(0); // fractional sig digits
				parameters.addParameter(param);
			}
		}

		// --- one and only bypass parameter
		param = new RangeParameter(USTRING("Bypass"), PLUGIN_SIDE_BYPASS, USTRING(""),
								   0, 1, 0, 0, ParameterInfo::kCanAutomate|ParameterInfo::kIsBypass);
		parameters.addParameter(param);

		// MIDI Params - these have no knobs in main GUI but do have to appear in default
		// NOTE: this is for VST3 ONLY! Not needed in AU or RAFX
		param = new RangeParameter(USTRING("PitchBend"), MIDI_PITCHBEND, USTRING(""),
								   MIN_UNIPOLAR, MAX_UNIPOLAR, DEFAULT_UNIPOLAR);
		param->setPrecision(1); // fractional sig digits
		parameters.addParameter(param);

		param = new RangeParameter(USTRING("MIDI Vol"), MIDI_VOLUME_CC7, USTRING(""),
								   MIN_UNIPOLAR, MAX_UNIPOLAR, DEFAULT_UNIPOLAR);
		param->setPrecision(1); // fractional sig digits
		parameters.addParameter(param);

		param = new RangeParameter(USTRING("MIDI Pan"), MIDI_PAN_CC10, USTRING(""),
								   MIN_UNIPOLAR, MAX_UNIPOLAR, DEFAULT_UNIPOLAR);
		param->setPrecision(1); // fractional sig digits
		parameters.addParameter(param);

		param = new RangeParameter(USTRING("MIDI Mod Wheel"), MIDI_MODWHEEL, USTRING(""),
								   MIN_UNIPOLAR, MAX_UNIPOLAR, DEFAULT_UNIPOLAR);
		param->setPrecision(1); // fractional sig digits
		parameters.addParameter(param);

		param = new RangeParameter(USTRING("MIDI Expression"), MIDI_EXPRESSION_CC11, USTRING(""),
								   MIN_UNIPOLAR, MAX_UNIPOLAR, DEFAULT_UNIPOLAR);
		param->setPrecision(1); // fractional sig digits
		parameters.addParameter(param);

		param = new RangeParameter(USTRING("MIDI Channel Pressure"), MIDI_CHANNEL_PRESSURE, USTRING(""),
								   MIN_UNIPOLAR, MAX_UNIPOLAR, DEFAULT_UNIPOLAR);
		param->setPrecision(1); // fractional sig digits
		parameters.addParameter(param);

		param = new RangeParameter(USTRING("MIDI Sustain Pedal"), MIDI_SUSTAIN_PEDAL, USTRING(""),
								   MIN_UNIPOLAR, MAX_UNIPOLAR, DEFAULT_UNIPOLAR);
		param->setPrecision(1); // fractional sig digits
		parameters.addParameter(param);

		param = new RangeParameter(USTRING("All Notes Off"), MIDI_ALL_NOTES_OFF, USTRING(""),
								   MIN_UNIPOLAR, MAX_UNIPOLAR, DEFAULT_UNIPOLAR);
		param->setPrecision(1); // fractional sig digits
		parameters.addParameter(param);
	}

	// --- check for presets, avoid crashy
	int nPresets = 0;
	for(int i=0; i<PRESET_COUNT; i++)
	{
		char* p = m_pRAFXPlugIn->m_PresetNames[i];
		if(p)
		{
			if(strlen(p) > 0)
				nPresets++;
		}
	}

	// --- root
	UnitInfo uinfoRoot;
	uinfoRoot.id = 1;
	uinfoRoot.parentUnitId = kRootUnitId;
	uinfoRoot.programListId = kNoProgramListId;
	Steinberg::UString (uinfoRoot.name, USTRINGSIZE (uinfoRoot.name)).assign (USTRING ("RootUnit"));
	addUnit(new Unit (uinfoRoot));

	if(nPresets > 0)
	{
		// --- add presets
		UnitInfo uinfoPreset;
		uinfoPreset.id = kRootUnitId;
		uinfoPreset.parentUnitId = 1;
		uinfoPreset.programListId = kPresetParam;
		// --- this causes garbage to show up in VST2 plugins with Default GUI
		//     you can comment out the two lines below for a partial fix; it
		//     worked for me on Reaper and Cubase Win, but not on Ableton Live MacOS
		UString name(uinfoPreset.name, 128);
		name.fromAscii("PresetsUnit");

		// --- then add it
		addUnit(new Unit (uinfoPreset));

		// --- the PRESET parameter
		StringListParameter* presetParam = new StringListParameter(USTRING("Factory Presets"),
																   kPresetParam, USTRING(""),
																   ParameterInfo::kIsProgramChange | ParameterInfo::kIsList,
																   kRootUnitId);
		// --- enumerate names
		for(int i=0; i<PRESET_COUNT; i++)
		{
			char* p = m_pRAFXPlugIn->m_PresetNames[i];
			if(p)
			{
				if(strlen(p) > 0)
					presetParam->appendString(USTRING(p));
			}
		}

		// --- add eeet
		parameters.addParameter(presetParam);
	}

	return result;
}

/*
	Processor::setBusArrangements()
	Client queries us for our supported Busses; this is where you can modify to support mono, surround, etc...
*/
tresult PLUGIN_API Processor::setBusArrangements(SpeakerArrangement* inputs, int32 numIns,
												   SpeakerArrangement* outputs, int32 numOuts)
{
	if(m_pRAFXPlugIn->m_bOutputOnlyPlugIn)
	{
		// SYNTH: one stereo output bus
		if(numIns == 0 && numOuts == 1 && outputs[0] == SpeakerArr::kStereo)
		{
			return SingleComponentEffect::setBusArrangements (inputs, numIns, outputs, numOuts);
		}
	}
	else
	{
		// FX: one input bus and one output bus of same channel count
		if(numIns == 1 && numOuts == 1 && inputs[0] == outputs[0])
		{
			return SingleComponentEffect::setBusArrangements (inputs, numIns, outputs, numOuts);
		}
	}

	return kResultFalse;
}

/*
	Processor::canProcessSampleSize()
	Client queries us for our supported sample lengths
*/
tresult PLUGIN_API Processor::canProcessSampleSize(int32 symbolicSampleSize)
{
	// this is a challenge in the book; here is where you say you support it but
	// you will need to deal with different buffers in the process() method
//	if (symbolicSampleSize == kSample32 || symbolicSampleSize == kSample64)

	// --- currently 32 bit only
	if (symbolicSampleSize == kSample32)
	{
		return kResultTrue;
	}
	return kResultFalse;
}

/*
	Processor::setupProcessing()

	we get information about sample rate, bit-depth, etc...
*/
tresult PLUGIN_API Processor::setupProcessing(ProcessSetup& newSetup)
{
	if(!m_pRAFXPlugIn) // should never fail to have plugin
		m_pRAFXPlugIn = CRafxPluginFactory::getRafxPlugIn();

	m_pRAFXPlugIn->m_nSampleRate = (int)processSetup.sampleRate;
	m_pRAFXPlugIn->prepareForPlay();

	// --- base class
	return SingleComponentEffect::setupProcessing(newSetup);
}

/*
	Processor::setActive()
	This is the analog of prepareForPlay() in RAFX since the Sample Rate is now set.

	VST3 plugins may be turned on or off; you are supposed to dynamically delare stuff when activated
	then delete when de-activated.
*/
tresult PLUGIN_API Processor::setActive(TBool state)
{
	if(state)
	{
		// --- do ON stuff; dynamic allocations
		if(!m_pRAFXPlugIn) // should never fail to have plugin
			m_pRAFXPlugIn = CRafxPluginFactory::getRafxPlugIn();

		m_pRAFXPlugIn->initialize();
		m_pRAFXPlugIn->m_nSampleRate = (int)processSetup.sampleRate;
		m_pRAFXPlugIn->prepareForPlay();
	}
	else
	{
		// --- do OFF stuff; delete stuff allocated above
		// do not delete RAFX plugin here, use Controller::terminate()
	}

	// --- base class method call is last
	return SingleComponentEffect::setActive (state);
}

/*
	Processor::setState()
	This is the READ part of the serialization process. We get the stream interface and use it
	to read from the filestream.

	NOTE: The datatypes/read order must EXACTLY match the getState() version or crashes may happen or variables
	      not initialized properly.
*/
tresult PLUGIN_API Processor::setState(IBStream* fileStream)
{
	IBStreamer s(fileStream, kLittleEndian);
	uint64 version = 0;

	// --- needed to convert to our UINT reads
	uint32 udata = 0;
	int32 data = 0;

	// --- read the version
	if(!s.readInt64u(version)) return kResultFalse;

	int nParams = m_pRAFXPlugIn->m_UIControlList.count();

	// iterate
	for(int i = 0; i < nParams; i++)
	{
		// they are in VST proper order in the ControlList - do NOT reference them with RackAFX ID values any more!
		CUICtrl* pUICtrl = m_pRAFXPlugIn->m_UIControlList.getAt(i);

		// iterate
		if(pUICtrl)
		{
			if(pUICtrl->uUserDataType == intData){
				if(!s.readInt32(data)) return kResultFalse; else *pUICtrl->m_pUserCookedIntData = data;}
			else if(pUICtrl->uUserDataType == floatData){
				if(!s.readFloat(*pUICtrl->m_pUserCookedFloatData)) return kResultFalse;}
			else if(pUICtrl->uUserDataType == doubleData){
				if(!s.readDouble(*pUICtrl->m_pUserCookedDoubleData)) return kResultFalse;}
			else if(pUICtrl->uUserDataType == UINTData){
				if(!s.readInt32u(udata)) return kResultFalse; else *pUICtrl->m_pUserCookedUINTData = udata;}
		}
	}
	// --- add plugin side bypassing
	if(!s.readBool(m_bPlugInSideBypass)) return kResultFalse;

	// --- do next version...
	if (version >= 2)
	{
		// add v1 stuff here
	}
	return kResultTrue;
}

/*
	Processor::getState()
	This is the WRITE part of the serialization process. We get the stream interface and use it
	to write to the filestream. This is important because it is how the Factory Default is set
	at startup, as well as when writing presets.
*/
tresult PLUGIN_API Processor::getState(IBStream* fileStream)
{
	// --- get a stream I/F
	IBStreamer s(fileStream, kLittleEndian);

	// --- Sock2VST3Version - place this at top so versioning can be used during the READ operation
	if(!s.writeInt64u(CowleyTech Rumble RemoverVersion)) return kResultFalse;

	// --- write out all of the params
	int nParams = m_pRAFXPlugIn->m_UIControlList.count();

	// iterate
	for(int i = 0; i < nParams; i++)
	{
		// they are in VST proper order in the ControlList - do NOT reference them with RackAFX ID values any more!
		CUICtrl* pUICtrl = m_pRAFXPlugIn->m_UIControlList.getAt(i);

		// iterate
		if(pUICtrl)
		{
			if(pUICtrl->uUserDataType == intData){
				if(!s.writeInt32(*pUICtrl->m_pUserCookedIntData)) return kResultFalse;}

			else if(pUICtrl->uUserDataType == floatData){
				if(!s.writeFloat(*pUICtrl->m_pUserCookedFloatData)) return kResultFalse;}

			else if(pUICtrl->uUserDataType == doubleData){
				if(!s.writeDouble(*pUICtrl->m_pUserCookedDoubleData)) return kResultFalse;}

			else if(pUICtrl->uUserDataType == UINTData){
				if(!s.writeInt32u(*pUICtrl->m_pUserCookedUINTData)) return kResultFalse;}
		}
	}
	// --- add plugin side bypassing
	if(!s.writeBool(m_bPlugInSideBypass)) return kResultFalse;

	return kResultTrue;
}

/*
	Processor::doControlUpdate()
	Find and issue Control Changes (same as userInterfaceChange() in RAFX)
	returns true if a control was changed
*/
bool Processor::doControlUpdate(ProcessData& data)
{
	bool paramChange = false;

	// --- check
	if(!data.inputParameterChanges)
		return paramChange;

	// --- get the param count and setup a loop for processing queue data
	int32 count = data.inputParameterChanges->getParameterCount();

	// --- make sure there is something there
	if(count <= 0)
		return paramChange;

	bool bUpdateJS = false;

	// --- loop
	for(int32 i=0; i<count; i++)
	{
		// get the message queue for ith parameter
		IParamValueQueue* queue = data.inputParameterChanges->getParameterData(i);

		if(queue)
		{
			// --- check for control points
			if(queue->getPointCount() <= 0) return false;

			int32 sampleOffset = 0.0;
			ParamValue value = 0.0;
			ParamID pid = queue->getParameterId();

			// this is the same as userInterfaceChange(); these only are updated if a change has
			// occurred (a control got moved)
			//
			// NOTE: same as with VST sample synth, we are taking the last value in the queue: queue->getPointCount()-1
			//       the value parameter is [0..1] so MUST BE COOKED before using
			//
			// NOTE: These are NOT MIDI Events! You can't get the channel directly
			// TODO: maybe try to make this nearly sample accurate <- from Steinberg...

			// --- get the last point in queue
			if(queue->getPoint(queue->getPointCount()-1, /* last update point */
				sampleOffset,			/* sample offset */
				value) == kResultTrue)	/* value = [0..1] */
			{
				// --- at least one param changed
				paramChange = true;

				// NOTE: because of the strange way VST3 handles MIDI messages, the channel and note/velocity information is lost
				//       for all but the three messages in the doProcessEvent() method
				UINT uChannel = 0;
				UINT uNote = 0;
				UINT uVelocity = 0;

				// first, get the normal plugin parameters
				CUICtrl* pUICtrl = m_pRAFXPlugIn->m_UIControlList.getAt(pid);
				if(pUICtrl)
				{
					// --- v6.6
					if(pUICtrl->bLogSlider)
						value = calcLogPluginValue(value);
					else if(pUICtrl->bExpSlider)
						value = calcVoltOctavePluginValue(value, pUICtrl);

					// --- use the VST style parameter set function
					m_pRAFXPlugIn->setParameter(pid, value); // this will call userIntefaceChange()
				}
				// --- custom RAFX
				else if(pid == ASSIGNBUTTON_1) // && value > 0.5)
					m_pRAFXPlugIn->userInterfaceChange(50);
				else if(pid == ASSIGNBUTTON_2) // && value > 0.5)
					m_pRAFXPlugIn->userInterfaceChange(51);
				else if(pid == ASSIGNBUTTON_3) // && value > 0.5)
					m_pRAFXPlugIn->userInterfaceChange(52);
				else if(pid == JOYSTICK_X_PARAM)
				{
					m_dJoystickX = value;
					bUpdateJS = true;
				}
				else if(pid == JOYSTICK_Y_PARAM)
				{
					m_dJoystickY = value;
					bUpdateJS = true;
				}
				else if(pid == PLUGIN_SIDE_BYPASS) // want 0 to 1
				{
					if(value == 0)
						m_bPlugInSideBypass = false;
					else
						m_bPlugInSideBypass = true;
					break;
				}
				else // try the MIDI controls
				{
					switch(pid) // same as RAFX uControlID
					{
						// --- MIDI messages
						case MIDI_PITCHBEND: // want -1 to +1
						{
							double dMIDIPitchBend = unipolarToBipolar(value);
							int nPitchBend = dMIDIPitchBend == -1.0 ? -8192 : (int)(dMIDIPitchBend*8191.0);
							if(m_pRAFXPlugIn)
								m_pRAFXPlugIn->midiPitchBend(uChannel, nPitchBend, dMIDIPitchBend);

							break;
						}
						case MIDI_MODWHEEL: // want 0 to 127
						{
							if(m_pRAFXPlugIn)
								m_pRAFXPlugIn->midiModWheel(uChannel, unipolarToMIDI(value));

							break;
						}
						case MIDI_ALL_NOTES_OFF:
						{
							if(m_pRAFXPlugIn)
								m_pRAFXPlugIn->midiNoteOff(uChannel, uNote, uVelocity, true);
							break;
						}
						case MIDI_VOLUME_CC7: // want 0 to 127
						{
							if(m_pRAFXPlugIn)
								m_pRAFXPlugIn->midiMessage(uChannel, VOLUME_CC07, unipolarToMIDI(value), 0);
							break;
						}
						case MIDI_PAN_CC10: // want 0 to 127
						{
							if(m_pRAFXPlugIn)
								m_pRAFXPlugIn->midiMessage(uChannel, PAN_CC10, unipolarToMIDI(value), 0);
							break;
						}
						case MIDI_EXPRESSION_CC11: // want 0 to 127
						{
							if(m_pRAFXPlugIn)
								m_pRAFXPlugIn->midiMessage(uChannel, EXPRESSION_CC11, unipolarToMIDI(value), 0);
							break;
						}
						case MIDI_CHANNEL_PRESSURE:
						{
							if(m_pRAFXPlugIn)
								m_pRAFXPlugIn->midiMessage(uChannel, CHANNEL_PRESSURE, unipolarToMIDI(value), 0);
							break;
						}
						case MIDI_SUSTAIN_PEDAL: // want 0 to 1
						{
							UINT uSustain = value > 0.5 ? 127 : 0;
							if(m_pRAFXPlugIn)
								m_pRAFXPlugIn->midiMessage(uChannel, SUSTAIN_PEDAL, uSustain, 0);
							break;
						}
					}
				}
			}
		}
	}

	return paramChange;
}

bool Processor::doProcessEvent(Event& vstEvent)
{
	bool noteEvent = false;

	// --- process Note On or Note Off messages
	switch(vstEvent.type)
	{
		// --- NOTE ON
		case Event::kNoteOnEvent:
		{
			// --- get the channel/note/vel
			UINT uMIDIChannel = (UINT)vstEvent.noteOn.channel;
			UINT uMIDINote = (UINT)vstEvent.noteOn.pitch;
			UINT uMIDIVelocity = (UINT)(127.0*vstEvent.noteOn.velocity);
			noteEvent = true;

			if(m_pRAFXPlugIn)
				m_pRAFXPlugIn->midiNoteOn(uMIDIChannel, uMIDINote, uMIDIVelocity);

			break;
		}

		// --- NOTE OFF
		case Event::kNoteOffEvent:
		{
			// --- get the channel/note/vel
			UINT uMIDIChannel = (UINT)vstEvent.noteOff.channel;
			UINT uMIDINote = (UINT)vstEvent.noteOff.pitch;
			UINT uMIDIVelocity = (UINT)(127.0*vstEvent.noteOff.velocity);
			noteEvent = true;

			if(m_pRAFXPlugIn)
				m_pRAFXPlugIn->midiNoteOff(uMIDIChannel, uMIDINote, uMIDIVelocity, false);

			break;
		}

		// --- polyphonic aftertouch 0xAn
		case Event::kPolyPressureEvent:
		{
			// --- get the channel
			UINT uMIDIChannel = (UINT)vstEvent.polyPressure.channel;
			UINT uMIDINote = (UINT)vstEvent.polyPressure.pitch;
			UINT uMIDIPressure = (UINT)(127.0*vstEvent.polyPressure.pressure);

			if(m_pRAFXPlugIn)
				m_pRAFXPlugIn->midiMessage(uMIDIChannel, POLY_PRESSURE, uMIDINote, uMIDIPressure);

			break;
		}

		// --- WP HACK for other MIDI -- only works for VST2 plugins :((((( VST3 sucks for MIDI events
		case Event::kChordEvent:
		{
			// --- get the channel/note/vel
			UINT uMIDIChannel = (UINT)vstEvent.chord.root;
			UINT uStatus = (UINT)vstEvent.chord.bassNote;
			UINT uData1 = (UINT)vstEvent.chord.mask;
			UINT uData2 = (UINT)vstEvent.chord.textLen;

			if(m_pRAFXPlugIn && vstEvent.chord.text == USTRING("RAFXMIDI"))
			{
				if(uStatus == CONTROL_CHANGE && uData1 == MOD_WHEEL)
					m_pRAFXPlugIn->midiModWheel(uMIDIChannel, uData2);
				else if(uStatus == TIMING_CLOCK)
					m_pRAFXPlugIn->midiClock();
				else if(uStatus == CONTROL_CHANGE && uData1 == ALL_NOTES_OFF)
					m_pRAFXPlugIn->midiNoteOff(uMIDIChannel, 0, 0, true);
				else if(uStatus == PITCH_BEND)	// pitchbend
				{
					UINT uLSB7 = uData1 & 0x7f;
					UINT uMSB7 = uData2 & 0x7f;

					unsigned short shValue = (unsigned short)uLSB7;  // 0xxx xxxx
					unsigned short shMSPart = (unsigned short)uMSB7; // 0yyy yyyy
					unsigned short shMSPartShift = shMSPart<<7;

					shValue = shValue | shMSPartShift;
					int nPitchBend = (int)shValue - 8192.0;

					int nNormPB = nPitchBend;
					if(nNormPB == -8192)
						nNormPB = -8191;

					float fPitchBend = (float)nNormPB/8191.0; // -1.0 -> 1.0

					m_pRAFXPlugIn->midiPitchBend(uMIDIChannel, nPitchBend, fPitchBend);
				}
				else if(m_pRAFXPlugIn->m_bWantAllMIDIMessages)
					m_pRAFXPlugIn->midiMessage(uMIDIChannel, uStatus, uData1, uData2);
			}

			break;
		}
	}

	// -- note event occurred?
	return noteEvent;
}

/*
	Processor::process()
	The most important function handles:
		Control Changes (same as userInterfaceChange() in RAFX)
		Synth voice rendering
		Output GUI Changes (allows you to write back to the GUI, advanced.
*/
tresult PLUGIN_API Processor::process(ProcessData& data)
{
	if(!m_pRAFXPlugIn->m_bOutputOnlyPlugIn)
	{
		// v6.6 FIX
		if(!data.inputs || !data.outputs)
			return kResultTrue;

		// --- FX
		// --- check for conrol chages and update synth if needed
		doControlUpdate(data);

		// --- loop and process
		if(data.numSamples > 0)
		{
			SpeakerArrangement arr;
			getBusArrangement(kOutput, 0, arr);
			int32 numChannels = SpeakerArr::getChannelCount(arr);

			if(m_bPlugInSideBypass)
			{
				for(int32 sample = 0; sample < data.numSamples; sample++)
				{
					// --- output = input
					(data.outputs[0].channelBuffers32[0])[sample] = (data.inputs[0].channelBuffers32[0])[sample];
					if(numChannels == 2)
						(data.outputs[0].channelBuffers32[1])[sample] = (data.inputs[0].channelBuffers32[1])[sample];
				}

				// --- update the meters
				updateMeters(data, true);

				return kResultTrue;
			}

			// --- sidechain off
			audioProcessData auxInputProcessData;
			auxInputProcessData.uInputBus = 1;
			auxInputProcessData.bInputEnabled = false;
			auxInputProcessData.uNumInputChannels = 0;
			auxInputProcessData.uBufferSize = 0;
			auxInputProcessData.pFrameInputBuffer = NULL;
			auxInputProcessData.pRAFXInputBuffer = NULL;
			auxInputProcessData.ppVSTInputBuffer = NULL;

			// --- see if plugin will process natively
			if(m_pRAFXPlugIn->m_bWantVSTBuffers)
			{
				if(m_bHasSidechain)
				{
					BusList* busList = getBusList (kAudio, kInput);
					Bus* bus0 = busList ? (Bus*)busList->at (0) : 0;
					Bus* bus = busList ? (Bus*)busList->at (1) : 0;
					if (bus && bus->isActive ())
					{
						auxInputProcessData.bInputEnabled = true;
						auxInputProcessData.uNumInputChannels = data.inputs[1].numChannels;
						auxInputProcessData.ppVSTInputBuffer = &data.inputs[1].channelBuffers32[0]; //** to sidechain

						// --- process sidechain
						m_pRAFXPlugIn->processAuxInputBus(&auxInputProcessData);
					}
				}

				m_pRAFXPlugIn->processVSTAudioBuffer(&data.inputs[0].channelBuffers32[0], &data.outputs[0].channelBuffers32[0], numChannels, data.numSamples);

				// --- update the meters
				updateMeters(data);

				return kResultTrue;
			}
			// else processAudioFrame() in sub-arrays

			// --- static buffs for RAFX
			float fInputs[2]; fInputs[0] = 0.0; fInputs[1] = 0.0;
			float fOutputs[2]; fOutputs[0] = 0.0; fOutputs[1] = 0.0;
			float fAuxInput[2]; fAuxInput[0] = 0.0; fAuxInput[1] = 0.0;

			for(int32 sample = 0; sample < data.numSamples; sample++)
			{
				if(m_bHasSidechain)
				{
					BusList* busList = getBusList (kAudio, kInput);
					Bus* bus0 = busList ? (Bus*)busList->at (0) : 0;
					Bus* bus = busList ? (Bus*)busList->at (1) : 0;
					if (bus && bus->isActive ())
					{
						fAuxInput[0] = (data.inputs[1].channelBuffers32[0])[sample];
						if(data.inputs[1].numChannels == 2)
							fAuxInput[1] = (data.inputs[1].channelBuffers32[1])[sample];

						auxInputProcessData.bInputEnabled = true;
						auxInputProcessData.uNumInputChannels = data.inputs[1].numChannels;
						auxInputProcessData.pFrameInputBuffer = &fAuxInput[0];

						// --- process sidechain
						m_pRAFXPlugIn->processAuxInputBus(&auxInputProcessData);
					}
				}

				fInputs[0] = (data.inputs[0].channelBuffers32[0])[sample];
				if(numChannels == 2)
					fInputs[1] = (data.inputs[0].channelBuffers32[1])[sample];

				if(m_pRAFXPlugIn)
					m_pRAFXPlugIn->processAudioFrame(&fInputs[0], &fOutputs[0], numChannels, numChannels);

				// --- write outputs
				(data.outputs[0].channelBuffers32[0])[sample] = fOutputs[0];
				if(numChannels == 2)
					 (data.outputs[0].channelBuffers32[1])[sample] = fOutputs[1];
			}

			// --- update the meters
			updateMeters(data);
		}
	}
	else
	{
		// --- SYNTH
		// v6.6 FIX
		if(!data.outputs)
			return kResultTrue;

		// --- for synths, VST3 clients have NULL input buffers!;
		//	   RAFX wants non-null buffers even if it isn't going to use them
		//	   the dummy buffers are declared at the top of this file as globals
		dummyInputPtr[0] = &dummyInputL[0];
		dummyInputPtr[1] = &dummyInputR[0];
		doControlUpdate(data);

		// --- we process 32 samples at a time; MIDI events are then accurate to 0.7 mSec
		const int32 kBlockSize = SYNTH_PROC_BLOCKSIZE;

		// 32-bit is float
		// if doing a 64-bit version, you need to replace with double*
		// initialize audio output buffers
		float* buffers[OUTPUT_CHANNELS]; // Precision is float - need to change this do DOUBLE if supporting 64 bit

		// 32-bit is float
		// if doing a 64-bit version, you need to replace with double* here too
		for(int i = 0; i < OUTPUT_CHANNELS; i++)
		{
			// data.outputs[0] = BUS 0
			buffers[i] = (float*)data.outputs[0].channelBuffers32[i];
			memset (buffers[i], 0, data.numSamples * sizeof(float));
		}

		// --- total number of samples in the input Buffer
		int32 numSamples = data.numSamples;

		// --- this is used when we need to shove an event into the next block
		int32 samplesProcessed = 0;

		// --- get our list of events
		IEventList* inputEvents = data.inputEvents;
		Event e = {0};
		Event* eventPtr = 0;
		int32 eventIndex = 0;

		// --- count of events
		int32 numEvents = inputEvents ? inputEvents->getEventCount () : 0;

		// get the first event
		if(numEvents)
		{
			inputEvents->getEvent (0, e);
			eventPtr = &e;
		}

		while(numSamples > 0)
		{
			// bound the samples to process to BLOCK SIZE (32)
			int32 samplesToProcess = std::min<int32> (kBlockSize, numSamples);

			while(eventPtr != 0)
			{
				// --- if the event is not in the current processing block
				//     then adapt offset for next block
				if (e.sampleOffset > samplesToProcess)
				{
					e.sampleOffset -= samplesToProcess;
					break;
				}

				// --- find MIDI note-on/off and broadcast
				doProcessEvent(e);

				// --- get next event
				eventIndex++;
				if (eventIndex < numEvents)
				{
					if (inputEvents->getEvent (eventIndex, e) == kResultTrue)
					{
						e.sampleOffset -= samplesProcessed;
					}
					else
					{
						eventPtr = 0;
					}
				}
				else
				{
					eventPtr = 0;
				}
			}

			// --- see if plugin will process natively
			if(m_pRAFXPlugIn->m_bWantVSTBuffers)
			{
				m_pRAFXPlugIn->processVSTAudioBuffer(&dummyInputPtr[0],
													 &buffers[0],
													 2,
													 samplesToProcess);
			}
			else
			{
				float input[2]; input[0] = 0.0; input[1] = 0.0;
				float output[2]; output[0] = 0.0; output[1] = 0.0;

				// the loop - samplesToProcess is more like framesToProcess
				for(int32 j=0; j<samplesToProcess; j++)
				{
					// --- synth output only!
					output[0] = 0.0; output[1] = 0.0;

					if(m_pRAFXPlugIn)
						m_pRAFXPlugIn->processAudioFrame(&input[0], &output[0], INPUT_CHANNELS, OUTPUT_CHANNELS);

					// just clear buffers
					buffers[0][j] = output[0];	// left
					buffers[1][j] = output[1];	// right
				}
			}

			// --- update the counter
			for(int i = 0; i < OUTPUT_CHANNELS; i++)
				buffers[i] += samplesToProcess;

			// --- update the samples processed/to process
			numSamples -= samplesToProcess;
			samplesProcessed += samplesToProcess;

		} // end while (numSamples > 0)

		// --- update the meters
		updateMeters(data);
	}

	return kResultTrue;
}

/*
	Processor::updateMeters()
	updates the meter variables
*/

void Processor::updateMeters(ProcessData& data, bool bForceOff)
{
	if(!m_pRAFXPlugIn) return;

	if(data.outputParameterChanges)
	{
		int nCount = meters.size();
		for(int i=0; i<nCount; i++)
		{
			CUICtrl* pUICtrl = m_pRAFXPlugIn->m_UIControlList.getAt(meters[i]);
			if(pUICtrl)
			{
				int32 index;
				IParamValueQueue* queue = data.outputParameterChanges->addParameterData(meters[i], index);

				if(queue && pUICtrl->m_pCurrentMeterValue)
				{
					float fMeter = bForceOff ? 0.0 : *pUICtrl->m_pCurrentMeterValue;
					queue->addPoint(i, fMeter, index);
				}
			}
		}
	}
}

/*
	--- IMIDIMapping Interface
	Processor::getMidiControllerAssignment()

	The client queries this 129 times for 130 possible control messages, see ivstsmidicontrollers.h for
	the VST defines for kPitchBend, kCtrlModWheel, etc... for each MIDI Channel in our Event Bus

	We respond with our ControlID value that we'll use to process the MIDI Messages in Processor::process().

	On the default GUI, these controls will actually move with the MIDI messages, but we don't want that on
	the final UI so that we can have any Modulation Matrix mapping we want.
*/
tresult PLUGIN_API Processor::getMidiControllerAssignment(int32 busIndex, int16 channel, CtrlNumber midiControllerNumber, ParamID& id/*out*/)
{
	if(!m_pRAFXPlugIn) return kResultFalse;

	// NOTE: we only have one EventBus(0)
	//       but it has 16 channels on it
	if(busIndex == 0)
	{
		// v6.6 FIX
		id = -1;
		bool bFoundIt = false;
		switch(midiControllerNumber)
		{
			// these messages handled in the Processor::process() method
			case kPitchBend:
				id = MIDI_PITCHBEND;
				break;
			case kCtrlModWheel:
				id = MIDI_MODWHEEL;
				break;
			case kCtrlVolume:
				id = MIDI_VOLUME_CC7;
				break;
			case kCtrlPan:
				id = MIDI_PAN_CC10;
				break;
			case kCtrlExpression:
				id = MIDI_EXPRESSION_CC11;
				break;
			case kAfterTouch:
				id = MIDI_CHANNEL_PRESSURE;
				break;
			case kCtrlSustainOnOff:
				id = MIDI_SUSTAIN_PEDAL;
				break;
			case kCtrlAllNotesOff:
				id = MIDI_ALL_NOTES_OFF;
				break;
		}

		if(id == -1)
		{
			int nParams = m_pRAFXPlugIn->m_UIControlList.count();

			// iterate
			for(int i = 0; i < nParams; i++)
			{
				// they are in VST proper order in the ControlList - do NOT reference them with RackAFX ID values any more!
				CUICtrl* pUICtrl = m_pRAFXPlugIn->m_UIControlList.getAt(i);

				// for RAFX MIDI Control only
				if(pUICtrl)
				{
					if(pUICtrl->bMIDIControl &&
					   pUICtrl->uMIDIControlName == midiControllerNumber)
					{
						bFoundIt = true; // this is because ID = -1 is illegal
						id = i;
					}
				}
			}
		}
		else
			bFoundIt = true;

		// v6.6 FIX
		if(id == -1)
		{
			id = 0;
			return kResultFalse;
		}
		else
			return kResultTrue;

		// return id >= 0 ? kResultTrue : kResultFalse;

		return bFoundIt ? kResultTrue : kResultFalse;
	}

	return kResultFalse;
}

/*
	Processor::createView()
	create our custom view here
*/
IPlugView* PLUGIN_API Processor::createView(const char* _name)
{
	if(!m_pRAFXPlugIn) return NULL;

	ConstString name(_name);
	if(name == ViewType::kEditor)
	{
		// --- get from RAFX
		if(!m_pRAFXPlugIn->m_bUseCustomVSTGUI)
			return NULL;

		// --- see if there is a pure custom view
		VSTGUI_VIEW_INFO info;
		info.message = GUI_HAS_USER_CUSTOM;
		info.bHasUserCustomView = false; // this flag will be set if they do
		m_pRAFXPlugIn->showGUI((void*)&info);
		if(info.bHasUserCustomView)
		{
			// CRafxCustomView creates the frame, populates with plugin view, then resizes frame
			m_pRafxCustomView = new CRafxCustomView(this);
			m_pRafxCustomView->setPlugIn(m_pRAFXPlugIn);
			return m_pRafxCustomView;
		}

		// --- create the editor using the RackAFX.uidesc file (in THIS project's \resources folder)
		/*
				This file was originally created and edited with GUI Designer in RackAFX.
			   	You can now use the WYSIWYG editor in any VST3 client to add more controls, rearrange, etc...
			   	You will need to see the VSTGUI4 documentation for notes on using the VST3 client's editor.

		  NOTE: When using the VST3 client WYSIWG editor, you must use File->Save As the first time you
		  		save the file to set the path; after that you can use File->Save.

		  		Normally, you want to save this file to the current VST3 project as:

		  		<ThisProject>\resources\RackAFX.uidesc

				Then you can re-compile this project and use the new GUI.

				Optionally, you can use File->Save As to overwrite the original file in your
				RackAFX Project folder as:

				<RackAFXProject>\rackafx.uidesc

				If you choose this option, you may want to make a backup of your original .uidesc file
				prior to overwriting it. The new GUI can still be opened and edited in RackAFX, but
				any non-RackAFX specific controls (e.g. CAnimationControl) will appear as black rectangles
				in the RackAFX GUI Designer. They are NOT deleted and will appear in your future Make VST ports,
				as well as your VST3 client.
		*/

#if defined _WINDOWS || defined _WINDLL
		m_pVST3Editor = new VST3EditorWP(this, "Editor", "rafx.uidesc"); // For WIN uses old resource ID for back compatibility; "rafx.uidesc" is the ID name, NOT the filename
#else
		m_pVST3Editor = new VST3EditorWP(this, "Editor", "RackAFX.uidesc"); // For MAC, uses actual file name (string) "RackAFX.uidesc"
#endif
		if(m_pVST3Editor)
		{
			// --- v6.6 you can now control the knob mode; see RAFX GUIDesigner
			m_pVST3Editor->setKnobMode(m_pRAFXPlugIn->m_uPlugInEx[KNOB_MODE]);
			return m_pVST3Editor;
		}

		// else - blank UI
		return new VST3Editor(this, "Editor", "vstgui.uidesc");
	}
	return 0;
}
void Processor::didOpen(VST3Editor* editor)		///< called after the editor was opened
{
	if(!m_pRAFXPlugIn) return;

	if(editor == m_pVST3Editor)
	{
		CFrame* frame = m_pVST3Editor->getFrame();

		// --- fill in the struct
		guiInfoStruct.message = GUI_DID_OPEN;
		guiInfoStruct.customViewName = "";
		guiInfoStruct.subControllerName = "";
		guiInfoStruct.listener = NULL;
		guiInfoStruct.editor = (void*)m_pVST3Editor;
		guiInfoStruct.bHasUserCustomView = false;

		m_pRAFXPlugIn->showGUI((void*)(&guiInfoStruct));
	}
}

void Processor::willClose(VST3Editor* editor)	///< called before the editor will close
{
	if(!m_pRAFXPlugIn) return;

	// --- fill in the struct
	guiInfoStruct.message = GUI_WILL_CLOSE;
	guiInfoStruct.customViewName = "";
	guiInfoStruct.subControllerName = "";
	guiInfoStruct.editor = (void*)m_pVST3Editor;
	guiInfoStruct.listener = NULL;
	guiInfoStruct.bHasUserCustomView = false;

	m_pRAFXPlugIn->showGUI((void*)(&guiInfoStruct));
}


/*
	Processor::createSubController()
	create subcontrollers for customized objects
	see VSTGUI4.2 documentation
*/
IController* Processor::createSubController(UTF8StringPtr _name, IUIDescription* description, VST3Editor* editor)
{
	if(!m_pRAFXPlugIn) return NULL;

	ConstString name (_name);

	string strName(name);

	if(name == "LCDController" || name == "LCDControllerControlName" || name == "LCDControllerIndexCount")
	{
		// --- params for the LCD Control
		Parameter* pAlpha = getParameterObject(ALPHA_WHEEL);
		Parameter* pLCD = getParameterObject(LCD_KNOB);
		if(!pAlpha || !pLCD)return NULL;

		LCDController* pController = new LCDController(editor, &LCDparameters, pAlpha, pLCD, this);

		// --- the alpha wheel broadcasts to all LCD controller "elements"
		if(m_pAlphaWheelKnob)
			m_pAlphaWheelKnob->addLCDController((void*)pController);

		return pController;
	}

	if(name == "VectorJoystick")
	{
		Parameter* jsX = getParameterObject(JOYSTICK_X_PARAM);
		Parameter* jsY = getParameterObject(JOYSTICK_Y_PARAM);
		PadController* padController = new PadController(editor, this, jsX, jsY);
		return padController;
	}

	int nJS = strName.find("Joystick_");
	if(nJS >= 0)
	{
		// --- decoding code
		int nX = strName.find("_X");
		int nY = strName.find("_Y");
		int len = strName.length();

		if(nX < 0 || nY < 0 || len < 0)
			return NULL;

		if(nX < nY && nY < len)
		{
			string sX = strName.substr(nX + 2, nY - 2 - nX);
			string sY = strName.substr(nY + 2, len - 2 - nY);
			int nParamX = atoi(sX.c_str());
			int nParamY = atoi(sY.c_str());
			Parameter* jsX = getParameterObject(nParamX);
			Parameter* jsY = getParameterObject(nParamY);
			CPadControllerWP* padController = new CPadControllerWP(editor, this, jsX, jsY);

			return padController;
		}
	}

	int nTP = strName.find("TrackPad_");
	if(nTP >= 0)
	{
		// --- decoding code
		int nX = strName.find("_X");
		int nY = strName.find("_Y");
		int len = strName.length();

		if(nX < 0 || nY < 0 || len < 0)
			return NULL;

		if(nX < nY && nY < len)
		{
			string sX = strName.substr(nX + 2, nY - 2 - nX);
			string sY = strName.substr(nY + 2, len - 2 - nY);
			int nParamX = atoi(sX.c_str());
			int nParamY = atoi(sY.c_str());
			Parameter* jsX = getParameterObject(nParamX);
			Parameter* jsY = getParameterObject(nParamY);
			CPadControllerWP* padController = new CPadControllerWP(editor, this, jsX, jsY);
			return padController;
		}
	}

	return NULL;
}

/*
	Processor::createCustomView()
	create custom views for customized objects
	see VSTGUI4.2 documentation
*/
CView* Processor::createCustomView(UTF8StringPtr name,
									const UIAttributes& attributes,
									IUIDescription* description,
									VST3Editor* editor)
{
	if(!m_pRAFXPlugIn) return NULL;

	ConstString viewname(name);
	if(viewname == "RafxKickButton" ||
		viewname == "RafxKickButtonDU" ||
		viewname == "RafxKickButtonU" ||
		viewname == "RafxKickButtonD")
	{
		const std::string* sizeString = attributes.getAttributeValue("size");
		const std::string* originString = attributes.getAttributeValue("origin");
		const std::string* bitmapString = attributes.getAttributeValue("bitmap");
		const std::string* tagString = attributes.getAttributeValue("control-tag");
		const std::string* offsetString = attributes.getAttributeValue("background-offset");

		// --- rect
		CPoint origin;
		CPoint size;
		parseSize(*sizeString, size);
		parseSize(*originString, origin);

		const CRect rect(origin, size);

		// --- tag
		int32_t tag = description->getTagForName(tagString->c_str());

		// --- listener "hears" the control
		const char* controlTagName = tagString->c_str();
		CControlListener* listener = description->getControlListener(controlTagName);

		// --- bitmap
		std::string BMString = *bitmapString;
		BMString += ".png";
		UTF8StringPtr bmp = BMString.c_str();
		CResourceDescription bmpRes(bmp);
		CBitmap* pBMP = new CBitmap(bmpRes);

		// --- offset
		CPoint offset;
		parseSize(*offsetString, offset);
		const CPoint offsetPoint(offset);

		CKickButtonWP* p = new CKickButtonWP(rect, listener, tag, pBMP, offset);
		if(p)
		{
			if(viewname == "RafxKickButtonDU")
				p->setMouseMode(mouseUpAndDown);
			else if(viewname == "RafxKickButtonD")
				p->setMouseMode(mouseDown);
			else if(viewname == "RafxKickButtonU")
				p->setMouseMode(mouseUp);
			else
				p->setMouseMode(mouseUp); // old
		}

		return p;
	}

	// attributes - has XML atts
	// description - use IUIDescription to convert color strings to CColors or bitmap strings to CBitmap*s
	if(viewname == "TrackPad" || viewname == "Joystick")
	{
		const std::string* sizeString = attributes.getAttributeValue("size");
		const std::string* originString = attributes.getAttributeValue("origin");
		const std::string* backColorString = attributes.getAttributeValue("back-color"); // c0lor of background
		const std::string* frameColorString = attributes.getAttributeValue("frame-color"); // frame (may not use)
		const std::string* puckColorString = attributes.getAttributeValue("font-color");// color of puck
		const std::string* frameWidthString = attributes.getAttributeValue("frame-width");
		const std::string* rrrString = attributes.getAttributeValue("round-rect-radius");
		const std::string* styleRRRString = attributes.getAttributeValue("style-round-rect");
		const std::string* styleNoFrameString = attributes.getAttributeValue("style-no-frame");

		// --- rect
		CPoint origin;
		CPoint size;
		parseSize(*sizeString, size);
		parseSize(*originString, origin);

		const CRect rect(origin, size);

		// --- colors
		CColor backColor;
		description->getColor(backColorString->c_str(), backColor);

		CColor frameColor;
		description->getColor(frameColorString->c_str(), frameColor);

		CColor puckColor;
		description->getColor(puckColorString->c_str(), puckColor);

		// --- the pad
		CXYPadWP* p = new CXYPadWP(rect);
		if(viewname == "TrackPad")
			p->m_bIsJoystickPad = false;
		else
			p->m_bIsJoystickPad = true;

		p->setBackColor(backColor);
		p->setFrameColor(frameColor);
		p->setFontColor(puckColor);

		p->setFrameWidth(atoi(frameWidthString->c_str()));
		p->setRoundRectRadius(atoi(rrrString->c_str()));

		if(strcmp(styleRRRString->c_str(), "true") == 0)
			p->setStyle(p->getStyle() | kRoundRectStyle);
		else
			p->setStyle(p->getStyle() & ~kRoundRectStyle);

		if(strcmp(styleNoFrameString->c_str(), "true") == 0)
			p->setStyle(p->getStyle() | kNoFrame);
		else
			p->setStyle(p->getStyle() & ~kNoFrame);

		return p;
	}


	if(viewname == "KnobSwitchView" || viewname == "AlphaWheelKnobView" || viewname == "LCDValueKnobView")
	{
		const std::string* sizeString = attributes.getAttributeValue("size");
		const std::string* offsetString = attributes.getAttributeValue("background-offset");
		const std::string* bitmapString = attributes.getAttributeValue("bitmap");
		const std::string* tagString = attributes.getAttributeValue("control-tag");
		const std::string* heightOneImageString = attributes.getAttributeValue("height-of-one-image");
		const std::string* subPixmapsString = attributes.getAttributeValue("sub-pixmaps");
		const std::string* originString = attributes.getAttributeValue("origin");

		// --- rect
		CPoint origin;
		CPoint size;
		parseSize(*sizeString, size);
		parseSize(*originString, origin);

		const CRect rect(origin, size);

		// --- listener "hears" the control
		const char* controlTagName = tagString->c_str();
		CControlListener* listener = description->getControlListener(controlTagName);

		// --- tag
		int32_t tag = description->getTagForName(tagString->c_str());

		// --- subPixmaps
		int32_t subPixmaps = strtol(subPixmapsString->c_str(), 0, 10);

		// --- height of one image
		CCoord heightOfOneImage = strtod(heightOneImageString->c_str(), 0);

		// --- bitmap
		std::string BMString = *bitmapString;
		BMString += ".png";
		UTF8StringPtr bmp = BMString.c_str();
		CResourceDescription bmpRes(bmp);
		CBitmap* pBMP = new CBitmap(bmpRes);

		// --- offset
		CPoint offset;
		parseSize(*offsetString, offset);
		const CPoint offsetPoint(offset);

		if(viewname == "KnobSwitchView")
		{
			// --- the knobswitch
			CKnobWP* p = new CKnobWP(rect, listener, tag, subPixmaps, heightOfOneImage, pBMP, offsetPoint, true, false); // true, false = IS swtchknob, NOT alpha wheel
			return p;
		}
		else if(viewname == "AlphaWheelKnobView")
		{
			// --- the knobswitch
			m_pAlphaWheelKnob = new CKnobWP(rect, listener, tag, subPixmaps, heightOfOneImage, pBMP, offsetPoint, false, true); // true, false = NOT swtchknob, IS alpha wheel
			return m_pAlphaWheelKnob;
		}
	}

	if(viewname == "SliderSwitchView")
	{
		const std::string* sizeString = attributes.getAttributeValue("size");
		const std::string* offsetString = attributes.getAttributeValue("handle-offset");
		const std::string* bitmapString = attributes.getAttributeValue("bitmap");
		const std::string* handleBitmapString = attributes.getAttributeValue("handle-bitmap");
		const std::string* tagString = attributes.getAttributeValue("control-tag");
		const std::string* originString = attributes.getAttributeValue("origin");
		const std::string* styleString = attributes.getAttributeValue("orientation");

		// --- rect
		CPoint origin;
		CPoint size;
		parseSize(*sizeString, size);
		parseSize(*originString, origin);

		const CRect rect(origin, size);

		// --- listener
		const char* controlTagName = tagString->c_str();
		CControlListener* listener = description->getControlListener(controlTagName);

		// --- tag
		int32_t tag = description->getTagForName(tagString->c_str());

		// --- bitmap
		std::string BMString = *bitmapString;
		BMString += ".png";
		UTF8StringPtr bmp = BMString.c_str();
		CResourceDescription bmpRes(bmp);
		CBitmap* pBMP_back = new CBitmap(bmpRes);

		std::string BMStringH = *handleBitmapString;
		BMStringH += ".png";
		UTF8StringPtr bmpH = BMStringH.c_str();
		CResourceDescription bmpResH(bmpH);
		CBitmap* pBMP_hand = new CBitmap(bmpResH);

		// --- offset
		CPoint offset;
		parseSize(*offsetString, offset);
		const CPoint offsetPoint(offset);

		// --- the knobswitch
		if(strcmp(styleString->c_str(), "vertical") == 0)
		{
			CVerticalSliderWP* p = new CVerticalSliderWP(rect, listener, tag, 0, 1, pBMP_hand, pBMP_back, offsetPoint);
			return p;
		}
		else
		{
			CHorizontalSliderWP* p = new CHorizontalSliderWP(rect, listener, tag, 0, 1, pBMP_hand, pBMP_back, offsetPoint);
			return p;
		}

		return NULL;
	}

	string customView(viewname);
	string analogMeter("AnalogMeterView");
	string invAnalogMeter("InvertedAnalogMeterView");
	int nAnalogMeter = customView.find(analogMeter);
	int nInvertedAnalogMeter = customView.find(invAnalogMeter);

	if(nAnalogMeter >= 0)
	{
		const std::string* sizeString = attributes.getAttributeValue("size");
		const std::string* originString = attributes.getAttributeValue("origin");
		const std::string* ONbitmapString = attributes.getAttributeValue("bitmap");
		const std::string* OFFbitmapString = attributes.getAttributeValue("off-bitmap");
		const std::string* numLEDString = attributes.getAttributeValue("num-led");
		const std::string* tagString = attributes.getAttributeValue("control-tag");

		if(sizeString && originString && ONbitmapString && OFFbitmapString && numLEDString)
		{
			CPoint origin;
			CPoint size;
			parseSize(*sizeString, size);
			parseSize(*originString, origin);

			const CRect rect(origin, size);

			std::string onBMString = *ONbitmapString;
			onBMString += ".png";
			UTF8StringPtr onbmp = onBMString.c_str();
			CResourceDescription bmpRes(onbmp);
			CBitmap* onBMP = new CBitmap(bmpRes);

			std::string offBMString = *OFFbitmapString;
			offBMString += ".png";
			UTF8StringPtr offbmp = offBMString.c_str();
			CResourceDescription bmpRes2(offbmp);
			CBitmap* offBMP = new CBitmap(bmpRes2);

			int32_t nbLed = strtol(numLEDString->c_str(), 0, 10);

			CVuMeterWP* p = NULL;

			if(nInvertedAnalogMeter >= 0)
				p = new CVuMeterWP(rect, onBMP, offBMP, nbLed, true, true); // inverted, analog
			else
				p = new CVuMeterWP(rect, onBMP, offBMP, nbLed, false, true); // inverted, analog

			// --- decode our stashed variables
			// decode hieght one image and zero db frame
			int nX = customView.find("_H");
			int nY = customView.find("_Z");
			int len = customView.length();
			string sH = customView.substr(nX + 2, nY - 2 - nX);
			string sZ = customView.substr(nY + 2, len - 2 - nY);

			p->setHtOneImage(atof(sH.c_str()));
			p->setImageCount(atof(numLEDString->c_str()));
			p->setZero_dB_Frame(atof(sZ.c_str()));

			// --- connect meters/variables
			int nParams = m_pRAFXPlugIn->m_UIControlList.count();

			// iterate
			for(int i = 0; i < nParams; i++)
			{
				// they are in VST proper order in the ControlList - do NOT reference them with RackAFX ID values any more!
				CUICtrl* pUICtrl = m_pRAFXPlugIn->m_UIControlList.getAt(i);
				if(pUICtrl && m_pVST3Editor)
				{
					if(strcmp(pUICtrl->cControlName, tagString->c_str()) == 0 && pUICtrl->m_pCurrentMeterValue)
					{
						// --- identical to RAFX, meters should behave the same way
						float fSampleRate = 1.0/(METER_UPDATE_INTERVAL_MSEC*0.001);
						p->initDetector(fSampleRate, pUICtrl->fMeterAttack_ms,
										pUICtrl->fMeterRelease_ms, true,
										pUICtrl->uDetectorMode,
										pUICtrl->bLogMeter);
					}
				}
			}
			return p;
		}
	}

	if(viewname == "InvertedMeterView" || viewname == "MeterView")
	{
		const std::string* sizeString = attributes.getAttributeValue("size");
		const std::string* originString = attributes.getAttributeValue("origin");
		const std::string* ONbitmapString = attributes.getAttributeValue("bitmap");
		const std::string* OFFbitmapString = attributes.getAttributeValue("off-bitmap");
		const std::string* numLEDString = attributes.getAttributeValue("num-led");
		const std::string* tagString = attributes.getAttributeValue("control-tag");

		if(sizeString && originString && ONbitmapString && OFFbitmapString && numLEDString)
		{
			CPoint origin;
			CPoint size;
			parseSize(*sizeString, size);
			parseSize(*originString, origin);

			const CRect rect(origin, size);

			std::string onBMString = *ONbitmapString;
			onBMString += ".png";
			UTF8StringPtr onbmp = onBMString.c_str();
			CResourceDescription bmpRes(onbmp);
			CBitmap* onBMP = new CBitmap(bmpRes);

			std::string offBMString = *OFFbitmapString;
			offBMString += ".png";
			UTF8StringPtr offbmp = offBMString.c_str();
			CResourceDescription bmpRes2(offbmp);
			CBitmap* offBMP = new CBitmap(bmpRes2);

			int32_t nbLed = strtol(numLEDString->c_str(), 0, 10);

			bool bInverted = false;

			if(viewname == "InvertedMeterView")
				bInverted = true;

			CVuMeterWP* p = new CVuMeterWP(rect, onBMP, offBMP, nbLed, bInverted, false); // inverted, analog

			// --- connect meters/variables
			int nParams = m_pRAFXPlugIn->m_UIControlList.count();

			// iterate
			for(int i = 0; i < nParams; i++)
			{
				// they are in VST proper order in the ControlList - do NOT reference them with RackAFX ID values any more!
				CUICtrl* pUICtrl = m_pRAFXPlugIn->m_UIControlList.getAt(i);
				if(pUICtrl && m_pVST3Editor)
				{
					if(strcmp(pUICtrl->cControlName, tagString->c_str()) == 0)
					{
						// --- identical to RAFX, meters should behave the same way
						float fSampleRate = 1.0/(METER_UPDATE_INTERVAL_MSEC*0.001);
						p->initDetector(fSampleRate, pUICtrl->fMeterAttack_ms,
										pUICtrl->fMeterRelease_ms, true,
										pUICtrl->uDetectorMode,
										pUICtrl->bLogMeter);
					}
				}
			}
			return p;
		}
	}

	// --- try plugin
	if(strlen(name) > 0)
	{
		// --- try plugin
		guiInfoStruct.message = GUI_CUSTOMVIEW;
		guiInfoStruct.subControllerName = "";
		guiInfoStruct.listener = NULL;
		guiInfoStruct.window = NULL;
		guiInfoStruct.editor = (void*)m_pVST3Editor;
		guiInfoStruct.bHasUserCustomView = false;

		guiInfoStruct.hPlugInInstance = NULL;
		guiInfoStruct.hRAFXInstance = NULL;
		guiInfoStruct.size.width = 0; guiInfoStruct.size.height = 0;

		// --- custom view setup --- //
		guiInfoStruct.customViewName = (char*)name;

		guiInfoStruct.listener = NULL;

		const std::string* tagString = attributes.getAttributeValue("control-tag");

		// --- setup listener
		if(tagString)
		{
			if(tagString->size() > 0)
			{
				// --- set tag as int
				int32_t tag = description->getTagForName(tagString->c_str());
				guiInfoStruct.customViewTag = tag;

				CControlListener* listener = description->getControlListener(tagString->c_str());
				guiInfoStruct.listener = (void*)listener;
			}
		}

		// --- rect
		const std::string* sizeString = attributes.getAttributeValue("size");
		const std::string* originString = attributes.getAttributeValue("origin");

		// --- rect
		CPoint origin;
		CPoint size;
		if(sizeString)
			parseSize(*sizeString, size);
		if(originString)
			parseSize(*originString, origin);

		const CRect rect(origin, size);
		guiInfoStruct.customViewRect.top = rect.top;
		guiInfoStruct.customViewRect.bottom = rect.bottom;
		guiInfoStruct.customViewRect.left = rect.left;
		guiInfoStruct.customViewRect.right = rect.right;

		const std::string* offsetString = attributes.getAttributeValue("background-offset");
		CPoint offset;
		if(offsetString)
			parseSize(*offsetString, offset);

		const CPoint offsetPoint(offset);
		guiInfoStruct.customViewOffset.x = offsetPoint.x;
		guiInfoStruct.customViewOffset.y = offsetPoint.y;

		const std::string* bitmapString = attributes.getAttributeValue("bitmap");
		if(bitmapString && bitmapString->size() > 0)
		{
			guiInfoStruct.customViewBitmapName = addStrings((char*)bitmapString->c_str(), ".png");
		}
		else
			guiInfoStruct.customViewBitmapName = "";

		const std::string* bitmap2String = attributes.getAttributeValue("handle-bitmap");
		const std::string* bitmap3String = attributes.getAttributeValue("off-bitmap");
		guiInfoStruct.customViewHandleBitmapName = "";
		guiInfoStruct.customViewOffBitmapName = "";

		if(bitmap2String && bitmap2String->size() > 0)
		{
			guiInfoStruct.customViewHandleBitmapName = addStrings((char*)bitmap2String->c_str(), ".png");
		}
		else if(bitmap3String && bitmap3String->size() > 0)
		{
			guiInfoStruct.customViewOffBitmapName = addStrings((char*)bitmap3String->c_str(), ".png");
		}

		guiInfoStruct.customViewOrientation = "";
		const std::string* styleString = attributes.getAttributeValue("orientation");
		if(styleString)
			guiInfoStruct.customViewOrientation = (char*)styleString->c_str();

		guiInfoStruct.customViewBackColor = NULL;
		guiInfoStruct.customViewFrameColor = NULL;
		guiInfoStruct.customViewFontColor = NULL;

		const std::string* backColorString = attributes.getAttributeValue("back-color"); // c0lor of background
		if(backColorString && backColorString->size() > 0)
		{
			CColor backColor;
			description->getColor(backColorString->c_str(), backColor);
			guiInfoStruct.customViewBackColor = (void*)&backColor;
		}

		const std::string* frameColorString = attributes.getAttributeValue("frame-color"); // frame (may not use)
		if(frameColorString && frameColorString->size() > 0)
		{
			CColor frameColor;
			description->getColor(frameColorString->c_str(), frameColor);
			guiInfoStruct.customViewFrameColor = (void*)&frameColor;
		}

		const std::string* fontColorString = attributes.getAttributeValue("font-color");// color of puck
		if(fontColorString && fontColorString->size() > 0)
		{
			CColor fontColor;
			description->getColor(fontColorString->c_str(), fontColor);
			guiInfoStruct.customViewFontColor = (void*)&fontColor;
		}

		guiInfoStruct.customViewFrameWidth = 0;
		const std::string* frameWidthString = attributes.getAttributeValue("frame-width");
		if(frameWidthString && frameWidthString->size() > 0)
			guiInfoStruct.customViewFrameWidth = atoi(frameWidthString->c_str());


		guiInfoStruct.customViewRoundRectRadius = 0;
		const std::string* rrrString = attributes.getAttributeValue("round-rect-radius");
		if(rrrString && rrrString->size() > 0)
			guiInfoStruct.customViewRoundRectRadius = atoi(rrrString->c_str());

		guiInfoStruct.customViewStyleRoundRect = false;
		const std::string* styleRRRString = attributes.getAttributeValue("style-round-rect");
		if(styleRRRString && styleRRRString->size() > 0)
			if(strcmp(styleRRRString->c_str(), "true") == 0) guiInfoStruct.customViewStyleRoundRect = true;

		guiInfoStruct.customViewStyleNoFrame = false;
		const std::string* styleNoFrameString = attributes.getAttributeValue("style-no-frame");
		if(styleNoFrameString && styleNoFrameString->size() > 0)
			if(strcmp(styleNoFrameString->c_str(), "true") == 0) guiInfoStruct.customViewStyleNoFrame = true;

		guiInfoStruct.customViewHtOneImage = 0;
		const std::string* heightOneImageString = attributes.getAttributeValue("height-of-one-image");
		if(heightOneImageString && heightOneImageString->size() > 0)
			guiInfoStruct.customViewHtOneImage = atoi(heightOneImageString->c_str());

		guiInfoStruct.customViewSubPixmaps = 0;
		const std::string* subPixmapsString = attributes.getAttributeValue("sub-pixmaps");
		if(subPixmapsString && subPixmapsString->size() > 0)
			guiInfoStruct.customViewSubPixmaps = atoi(subPixmapsString->c_str());

		void* pluginview = m_pRAFXPlugIn->showGUI((void*)(&guiInfoStruct));
		if(pluginview)
			return (CView*)pluginview;

	}

	return NULL;
}

/* See Automation in the docs
	Non-linear Scaling
	If the DSP representation of a value does not scale in a linear way to the exported
	normalized representation (which can happen when a decibel scale is used for example),
	the edit controller must provide a conversion to a plain representation.
	This allows the host to move automation data (being in GUI representation)
	and keep the original value relations intact.
	(Steinberg::Vst::IEditController::normalizedParamToPlain / Steinberg::Vst::IEditController::plainParamToNormalized).

	*** NOTE ***
	We do not use these since our controls are linear or logscale-controlled.
	I am just leaving them here in case you need to implement them. See docs.
*/
ParamValue PLUGIN_API Processor::plainParamToNormalized(ParamID tag, ParamValue plainValue)
{
	return EditController::plainParamToNormalized(tag, plainValue);
}
ParamValue PLUGIN_API Processor::normalizedParamToPlain(ParamID tag, ParamValue valueNormalized)
{
	return EditController::normalizedParamToPlain(tag, valueNormalized);
}

/*
	Processor::setParamNormalizedFromFile()
	helper function for setComponentState()
*/
tresult PLUGIN_API Processor::setParamNormalizedFromFile(ParamID tag, ParamValue value)
{
	// --- get the parameter
	Parameter* pParam = SingleComponentEffect::getParameterObject(tag);

	// --- verify pointer
	if(!pParam) return kResultFalse;

	//  --- convert serialized value to normalized (raw)
	return SingleComponentEffect::setParamNormalized(tag, pParam->toNormalized(value));
}

/*
	Processor::terminate()
	the end - delete the plugin
*/
tresult PLUGIN_API Processor::terminate()
{
	LCDparameters.removeAll();

	// --- clear out the index values
	meters.clear();

	return SingleComponentEffect::terminate();
}

/*
	Processor::updatePluginParams()
	for RackAFX sendUpdateGUI() support
*/
void Processor::updatePluginParams()
{
	if(!m_pRAFXPlugIn) return;

	for(int i = 0; i < m_pRAFXPlugIn->m_UIControlList.count(); i++)
	{
		setParamNormalized(i, m_pRAFXPlugIn->getParameter(i));
	}
}

/*
	Processor::receiveText()
	for RackAFX sendUpdateGUI() support
	could also add more communication with VST3EditorWP here
*/
tresult Processor::receiveText(const char8* text)
{
	if(!m_pRAFXPlugIn) return kResultTrue;

	if(strcmp(text, "VSTGUITimerPing") == 0)
	{
		if(m_pRAFXPlugIn->m_uPlugInEx[UPDATE_GUI] == 1)
		{
			updatePluginParams();
			m_pRAFXPlugIn->m_uPlugInEx[UPDATE_GUI] = 0; // reset
		}

		// --- timer ping -> PlugIn
		guiInfoStruct.message = GUI_TIMER_PING;
		m_pRAFXPlugIn->showGUI((void*)&guiInfoStruct);
	}
	if(strcmp(text, "RecreateView") == 0)
	{
		// user is editing in VST3 editor, update
		CFrame* frame = m_pVST3Editor->getFrame();

		// --- fill in the struct
		guiInfoStruct.message = GUI_DID_OPEN;
		guiInfoStruct.customViewName = "";
		guiInfoStruct.subControllerName = "";
		guiInfoStruct.editor = (void*)m_pVST3Editor;
		guiInfoStruct.listener = NULL;
		guiInfoStruct.bHasUserCustomView = false;

		m_pRAFXPlugIn->showGUI((void*)(&guiInfoStruct));
	}

	return kResultTrue;
}

/*
	Processor::setComponentState()
	This is the serialization-read function so the GUI can
	be updated from a preset or startup.

	fileStream - the IBStream interface from the client
*/
tresult PLUGIN_API Processor::setComponentState(IBStream* fileStream)
{
	if(!m_pRAFXPlugIn) return kResultFalse;

	// --- make a streamer interface using the
	//     IBStream* fileStream; this is for PC so
	//     data is LittleEndian
	IBStreamer s(fileStream, kLittleEndian);

	// --- variables for reading
	uint64 version = 0;
	double dDoubleParam = 0;
	float fFloatParam = 0;

	// --- needed to convert to our UINT reads
	uint32 udata = 0;
	int32 data = 0;

	// --- read the version
	if(!s.readInt64u(version)) return kResultFalse;

	int nParams = m_pRAFXPlugIn->m_UIControlList.count();

	// iterate
	for(int i = 0; i < nParams; i++)
	{
		// they are in VST proper order in the ControlList - do NOT reference them with RackAFX ID values any more!
		CUICtrl* pUICtrl = m_pRAFXPlugIn->m_UIControlList.getAt(i);

		// iterate
		if(pUICtrl)
		{
			if(pUICtrl->uUserDataType == intData){
				if(!s.readInt32(data)) return kResultFalse; else setParamNormalizedFromFile(i, (ParamValue)data);}

			else if(pUICtrl->uUserDataType == floatData){
				if(!s.readFloat(fFloatParam)) return kResultFalse; else setParamNormalizedFromFile(i, (ParamValue)fFloatParam);}

			else if(pUICtrl->uUserDataType == doubleData){
				if(!s.readDouble(dDoubleParam)) return kResultFalse; else setParamNormalizedFromFile(i, dDoubleParam);}

			else if(pUICtrl->uUserDataType == UINTData){
				if(!s.readInt32u(udata)) return kResultFalse; else setParamNormalizedFromFile(i, (ParamValue)udata);}
		}
	}

	// --- add plugin side bypassing
	bool dummy = false;
	if(!s.readBool(dummy))
		return kResultFalse;
	else
		setParamNormalizedFromFile(PLUGIN_SIDE_BYPASS, dummy);

	// --- do next version...
	if (version >= 1)
	{
		// add v1 stuff here
	}

	return kResultTrue;
}

/*
	Processor::getEnumString()
	helper function for initializing parameters
*/
char* Processor::getEnumString(char* string, int index)
{
	int nLen = strlen(string);
	char* copyString = new char[nLen+1];

	vst_strncpy(copyString, string, strlen(string));

	for(int i=0; i<index+1; i++)
	{
		char * comma = ",";

		int j = strcspn (copyString,comma);

		if(i==index)
		{
			char* pType = new char[j+1];
			strncpy (pType, copyString, j);
			pType[j] = '\0';
			delete [] copyString;

			// special support for 2-state switches
			// (new in RAFX 5.4.14)
			if(strcmp(pType, "SWITCH_OFF") == 0)
			{
				delete [] pType;
				return "OFF";
			}
			else if(strcmp(pType, "SWITCH_ON") == 0)
			{
				delete [] pType;
				return "ON";
			}

			return pType;
		}
		else // remove it
		{
			char* pch = strchr(copyString,',');

			if(!pch)
			{
				delete [] copyString;
				return NULL;
			}

			int nLen = strlen(copyString);
			memcpy (copyString,copyString+j+1,nLen-j);
		}
	}

	delete [] copyString;
	return NULL;
}

/*
	Processor::setParamNormalized()
	This is overridden only for selecting a preset.
*/
tresult PLUGIN_API Processor::setParamNormalized(ParamID tag, ParamValue value)
{
	// --- base class call
	tresult res = SingleComponentEffect::setParamNormalized(tag, value);

	// --- handle preset changes
	if (res == kResultOk && tag == kPresetParam)
	{
		int32 program = parameters.getParameter(tag)->toPlain(value);

		if(m_pRAFXPlugIn)
		{
			int nParams = m_pRAFXPlugIn->m_UIControlList.count();

			// iterate
			for(int i = 0; i < nParams; i++)
			{
				float fGUIWarpedVariable = 0.0;
				CUICtrl* pUICtrl = m_pRAFXPlugIn->m_UIControlList.getAt(i);
				if(pUICtrl)
				{
					// --- we store COOKED data in presets
					double dPreset = pUICtrl->dPresetData[program];

					// --- normalize, apply log/volt-octave if needed for GUI Warped Variable
					double dNormalizedValue = getNormalizedRackAFXVariable(dPreset, pUICtrl, fGUIWarpedVariable);

					// --- set it on plugin
					m_pRAFXPlugIn->setParameter(i, dNormalizedValue); // this will call userIntefaceChange()

					// --- set it on GUI
					SingleComponentEffect::setParamNormalized(i, fGUIWarpedVariable);

				}
			}

			// --- restart
			componentHandler->restartComponent(kParamValuesChanged);
		}
	}
	return res;
}

/*
	Processor::getUnitInfo()
	Just returns our info struct; only needed for presets.
*/
tresult PLUGIN_API Processor::getUnitInfo(int32 unitIndex, UnitInfo& info /*out*/)
{
	Unit* unit = units.at(unitIndex);
	if(unit)
	{
		info = unit->getInfo();
		return kResultTrue;
	}
	return kResultFalse;
}

/*
	Processor::notifyUnitSelection()
	only needed for presets.
*/
tresult Processor::notifyUnitSelection()
{
	tresult result = kResultFalse;
	FUnknownPtr<IUnitHandler> unitHandler (componentHandler);
	if (unitHandler)
		result = unitHandler->notifyUnitSelection (selectedUnit);
	return result;
}

/*
	Processor::addProgramList()
	Adds a program list (aka preset list)
*/
bool Processor::addProgramList(ProgramList* list)
{
	if (programLists.addKeyAndObject(list->getID (), IPtr<ProgramList> (list, false)))
	{
		list->addDependent(this);
		return true;
	}
	return false;
}

/*
	Processor::getProgramList()
	Selects a program list (aka preset list)
*/
ProgramList* Processor::getProgramList(ProgramListID listId) const
{
	return programLists.lookupObject(listId);
}

/*
	Processor::notifyPogramListChange()
	If list changes; should not be called as we only have one program list
*/
tresult Processor::notifyPogramListChange (ProgramListID listId, int32 programIndex)
{
	tresult result = kResultFalse;
	FUnknownPtr<IUnitHandler> unitHandler(componentHandler);
	if (unitHandler)
		result = unitHandler->notifyProgramListChange (listId, programIndex);
	return result;
}

/*
	Processor::getProgramListCount()
	We have one list for our one set of presets
*/
int32 PLUGIN_API Processor::getProgramListCount ()
{
	if (parameters.getParameter(kPresetParam))
		return 1;
	return 0;
}

/*
	Processor::getProgramListInfo()
	Get information about our preset list.
*/
tresult PLUGIN_API Processor::getProgramListInfo(int32 listIndex, ProgramListInfo& info /*out*/)
{
	Parameter* param = parameters.getParameter(kPresetParam);
	if(param && listIndex == 0)
	{
		info.id = kPresetParam;
		info.programCount = (int32)param->toPlain (1) + 1;
		UString name (info.name, 128);
		name.fromAscii("Presets");
		return kResultTrue;
	}
	return kResultFalse;

}

/*
	Processor::getProgramName()
	Get preset name
*/
tresult PLUGIN_API Processor::getProgramName(ProgramListID listId, int32 programIndex, String128 name /*out*/)
{
	if(listId == kPresetParam)
	{
		Parameter* param = parameters.getParameter(kPresetParam);
		if (param)
		{
			ParamValue normalized = param->toNormalized (programIndex);
			param->toString (normalized, name);
			return kResultTrue;
		}
	}
	return kResultFalse;
}

/*
	Processor::setProgramName()
	Set preset name
*/
tresult Processor::setProgramName(ProgramListID listId, int32 programIndex, const String128 name /*in*/)
{
	ProgramList* list = programLists.lookupObject(listId);
	if(list)
	{
		return list->setProgramName(programIndex, name);
	}
	return kResultFalse;
}

/*
	Processor::getProgramInfo()
	Only used for presets.
*/
tresult PLUGIN_API Processor::getProgramInfo(ProgramListID listId, int32 programIndex, CString attributeId /*in*/, String128 attributeValue /*out*/)
{
	ProgramList* list = programLists.lookupObject (listId);
	if (list)
	{
		return list->getProgramInfo(programIndex, attributeId, attributeValue);
	}
	return kResultFalse;
}

/*
	Processor::hasProgramPitchNames()
	Not Used.
*/
tresult PLUGIN_API Processor::hasProgramPitchNames(ProgramListID listId, int32 programIndex)
{
	ProgramList* list = programLists.lookupObject (listId);
	if (list)
	{
		return list->hasPitchNames(programIndex);
	}
	return kResultFalse;
}

/*
	Processor::getProgramPitchName()
	Not Used.
*/
tresult PLUGIN_API Processor::getProgramPitchName (ProgramListID listId, int32 programIndex, int16 midiPitch, String128 name /*out*/)
{
	ProgramList* list = programLists.lookupObject (listId);
	if (list)
	{
		return list->getPitchName(programIndex, midiPitch, name);
	}
	return kResultFalse;
}

/*
	Processor::update()
	Just for presets.
*/
void PLUGIN_API Processor::update(FUnknown* changedUnknown, int32 message)
{
	ProgramList* programList = FCast<ProgramList> (changedUnknown);
	if(programList)
	{
		FUnknownPtr<IUnitHandler> unitHandler(componentHandler);
		if (unitHandler)
			unitHandler->notifyProgramListChange(programList->getID (), kAllProgramInvalid);
	}
}

/*
	Processor::addUnit()
	Just for presets.
*/
bool Processor::addUnit(Unit* unit)
{
	return units.add(IPtr<Unit>(unit, false));
}

// --- custom view object for plugins that support it
CRafxCustomView::CRafxCustomView(void* controller, ViewRect* size)
: VSTGUIEditor(controller, size)
{

}

CRafxCustomView::~CRafxCustomView ()
{

}

bool PLUGIN_API CRafxCustomView::open(void* parent, const PlatformType& platformType)
{
	// --- create the frame
	frame = new CFrame(CRect (0, 0, 0, 0), this);
	frame->setTransparency(true);
	frame->enableTooltips(true);

	// --- populate the window
	if(m_pPlugIn)
	{
		systemWindow = parent;

		VSTGUI_VIEW_INFO info;
		info.message = GUI_USER_CUSTOM_OPEN;
		info.window = (void*)parent;
		info.hPlugInInstance = moduleHandle;
		info.hRAFXInstance = NULL;
		info.size.width = 0;
		info.size.height = 0;

		void* hasCustomGUI = m_pPlugIn->showGUI((void*)&info);
		if(hasCustomGUI)
		{
			rect.left = 0;
			rect.top = 0;
			rect.right = info.size.width;
			rect.bottom = info.size.height;

			// --- resize
			frame->setSize(info.size.width, info.size.height);
			return true;
		}
		else
			return false;
	}
	// --- should never happen
	return false;
}

/** Called when the editor will be closed. */
void PLUGIN_API CRafxCustomView::close()
{
	if(m_pPlugIn)
	{
		VSTGUI_VIEW_INFO info;
		info.message = GUI_USER_CUSTOM_CLOSE;
		info.window = (void*)systemWindow;
		info.hPlugInInstance = moduleHandle;
		info.hRAFXInstance = NULL;
		info.size.width = 0;
		info.size.height = 0;

		m_pPlugIn->showGUI((void*)&info);

		systemWindow = 0;
	}
	if(frame)
	{
		int32_t refCount = frame->getNbReference();
		if(refCount == 1)
		{
			frame->close();
			frame = 0;
		}
		else
		{
			frame->forget();
		}
	}
}

}}} // namespaces


