
#ifndef __CVSTGUIController__
#define __CVSTGUIController__

#include "../vstgui4/vstgui/vstgui.h"
#include "plugin.h"
#include <cstdio>
#include <string>
#include <vector>
#include <map>

#ifdef AUPLUGIN
    #include <AudioToolbox/AudioToolbox.h>
#endif

using namespace std;

namespace VSTGUI {

class CVSTGUIController : public VSTGUIEditorInterface, public CControlListener, public CBaseObject
{
public:
	CVSTGUIController();
	virtual ~CVSTGUIController();

#ifdef AUPLUGIN
	// --- the AU for preset change notification
    AudioUnit m_AUInstance;
    AUEventListenerRef m_AUEventListener;
#endif

	// --- timer norification callback
   	CMessageResult notify(CBaseObject* sender, const char* message);

	// --- open function:
	// window: WinOS = HWND*
	//		   MacOS = NSView*
	//
	// hPlugInInstance: WinOS = instance handle passed during creation of DLL
	//					MacOS = the AU AudioComponentInstance (which is actually a ComponentInstanceRecord*) used for notifications
	//
	//     main job is to create the frame and populate it with controls
	//     the return variables are the window width/height in pixels
	bool open(void* window, CPlugIn* pPlugIn, int& nWidth, int& nHeight, void* hPlugInInstance = NULL);

	// --- close function; destroy frame and forget timer
	void close();

	// --- do idle processing
	void idle();

	// --- function to create/initalize/destory the controls
	void createControls();
	void initControls(bool bSetListener = false);	// bSetListener is AU only

	// --- VSTGUIEditorInterface override
	virtual int32_t getKnobMode() const;

	// --- CControlListener override (pure abstract, so must)
	virtual void valueChanged(VSTGUI::CControl* pControl);

	// --- get a bitmap
	CBitmap* getBitmap(const CResourceDescription& desc, CCoord left = -1, CCoord top = -1, CCoord right = -1, CCoord bottom = -1);

	// --- plugin helpers
	float getNormalizedValue(CUICtrl* pUICtrl);

	// --- GUI SPECIFIC CONTROL POINTERS: ADD YOURS HERE




	// --- END GUI SPECIFIC CONTROL POINTERS

protected:
	void* m_hPlugInInstance;	// HINSTANCE of this DLL (WinOS only) may NOT be NULL

	// --- our plugin
	CPlugIn* m_pPlugIn;

	// --- timer for GUI updates
    CVSTGUITimer* timer;

// --- miscellaneous functions
public:
	// --- helpers for log/volt-octave controls
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

	inline float calcLogParameter(float fNormalizedParam)
	{
		return (pow(10.f, fNormalizedParam) - 1.0)/9.0;
	}

	inline float calcLogPluginValue(float fPluginValue)
	{
		return log10(9.0*fPluginValue + 1.0);
	}

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

	inline float calcVoltOctavePluginValue(float fPluginValue, CUICtrl* pCtrl)
	{
		if(pCtrl->uUserDataType == UINTData)
			return *(pCtrl->m_pUserCookedUINTData);

		double dOctaves = fastlog2(pCtrl->fUserDisplayDataHiLimit/pCtrl->fUserDisplayDataLoLimit);
		float fDisplay = pCtrl->fUserDisplayDataLoLimit*fastpow2(fPluginValue*dOctaves); //(m_fDisplayMax - m_fDisplayMin)*value + m_fDisplayMin; //m_fDisplayMin*fastpow2(value*dOctaves);
		float fDiff = pCtrl->fUserDisplayDataHiLimit - pCtrl->fUserDisplayDataLoLimit;
		return (fDisplay - pCtrl->fUserDisplayDataLoLimit)/fDiff;

	}

	inline int getEnumStringIndex(char* enumString, const char* testString)
	{
		string sEnumStr(enumString);
		string sTestStr(testString);
		int index = 0;
		bool bWorking = true;
		while(bWorking)
		{
			int nComma = sEnumStr.find_first_of(',');
			if(nComma <= 0)
			{
				if(sEnumStr == sTestStr)
					return index;

				bWorking = false;
			}
			else
			{
				string sL = sEnumStr.substr(0, nComma);
				sEnumStr = sEnumStr.substr(nComma+1);

				if(sL == sTestStr)
					return index;

				index++;
			}
		}

		return -1;
	}

	// old VST2 function for safe strncpy()
	inline char* vst_strncpy(char* dst, const char* src, size_t maxLen)
	{
		char* result = strncpy(dst, src, maxLen);
		dst[maxLen] = 0;
		return result;
	}

	inline char* getEnumString(char* string, int index)
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

	inline float getPluginParameterValue(float fControlValue, CControl* pControl = NULL)
	{
		if(pControl)
		{
			COptionMenu* OMControl = dynamic_cast<COptionMenu*>(pControl);
			CVerticalSwitch* VSControl = dynamic_cast<CVerticalSwitch*>(pControl);
			CHorizontalSwitch* HSControl = dynamic_cast<CHorizontalSwitch*>(pControl);

			if(OMControl)
				return pControl->getValue()/((float)((COptionMenu*)pControl)->getNbEntries() - 1);
			else if(VSControl || HSControl)
				return pControl->getValue()/pControl->getMax();
			else
				return pControl->getValue();
		}

		return fControlValue;
	}

	inline void setEditControlValue(CTextEdit* pControl, CUICtrl* pCtrl)
	{
		switch(pCtrl->uUserDataType)
		{
			case floatData:
			{
				pControl->setText(floatToString(*pCtrl->m_pUserCookedFloatData,2));
				break;
			}
			case doubleData:
			{
				pControl->setText(doubleToString(*pCtrl->m_pUserCookedDoubleData,2));
				break;
			}
			case intData:
			{
				pControl->setText(intToString(*pCtrl->m_pUserCookedIntData));
				break;
			}
			case UINTData:
			{
				char* pEnum;
				pEnum = getEnumString(pCtrl->cEnumeratedList, (int)(*(pCtrl->m_pUserCookedUINTData)));

				if(pEnum)
					pControl->setText(pEnum);
				break;
			}
			default:
				break;
		}
	}

	// --- called when value changes in text edit; it needs to store the new stringindex value and update itself
	inline float updateEditControl(CControl* pControl, CUICtrl* pCtrl)
	{
		if(!pControl) return -1.0;
		if(!pCtrl) return -1.0;

		const char* p = ((CTextEdit*)pControl)->getText();

		float fValue = 0.0;
		switch(pCtrl->uUserDataType)
		{
			case floatData:
			{
				float f = atof(p);
				if(f > pCtrl->fUserDisplayDataHiLimit) f = pCtrl->fUserDisplayDataHiLimit;
				if(f < pCtrl->fUserDisplayDataLoLimit) f = pCtrl->fUserDisplayDataLoLimit;
				p = floatToString(f,2);

				float fDiff = pCtrl->fUserDisplayDataHiLimit - pCtrl->fUserDisplayDataLoLimit;
				float fCookedData = (f - pCtrl->fUserDisplayDataLoLimit)/fDiff;

				// --- v6.6
				if(pCtrl->bLogSlider)
					fValue = calcLogParameter(fCookedData); //(pow((float)10.0, fCookedData) - 1.0)/9.0;
				else if(pCtrl->bExpSlider)
				{
					fValue = calcVoltOctaveParameter(pCtrl);
				}
				else
					fValue = calcSliderVariable(pCtrl->fUserDisplayDataLoLimit, pCtrl->fUserDisplayDataHiLimit, f);

				pControl->setValue(fValue);
				((CTextEdit*)pControl)->setText(p);
				break;
			}
			case doubleData:
			{
				float f = atof(p);
				if(f > pCtrl->fUserDisplayDataHiLimit) f = pCtrl->fUserDisplayDataHiLimit;
				if(f < pCtrl->fUserDisplayDataLoLimit) f = pCtrl->fUserDisplayDataLoLimit;
				p = floatToString(f,2);

				float fDiff = pCtrl->fUserDisplayDataHiLimit - pCtrl->fUserDisplayDataLoLimit;
				float fCookedData = (f - pCtrl->fUserDisplayDataLoLimit)/fDiff;

				if(pCtrl->bLogSlider)
					fValue = calcLogParameter(fCookedData);
				else if(pCtrl->bExpSlider)
				{
					fValue = calcVoltOctaveParameter(pCtrl);
				}
				else
					fValue = calcSliderVariable(pCtrl->fUserDisplayDataLoLimit, pCtrl->fUserDisplayDataHiLimit, f);

				pControl->setValue(fValue);
				((CTextEdit*)pControl)->setText(p);
				break;
			}
			case intData:
			{
				int f = atoi(p);
				if(f > pCtrl->fUserDisplayDataHiLimit) f = pCtrl->fUserDisplayDataHiLimit;
				if(f < pCtrl->fUserDisplayDataLoLimit) f = pCtrl->fUserDisplayDataLoLimit;
				p = intToString(f);

				float fDiff = pCtrl->fUserDisplayDataHiLimit - pCtrl->fUserDisplayDataLoLimit;
				float fCookedData = (f - pCtrl->fUserDisplayDataLoLimit)/fDiff;

				if(pCtrl->bLogSlider)
					fValue = calcLogParameter(fCookedData);
				else if(pCtrl->bExpSlider)
				{
					fValue = calcVoltOctaveParameter(pCtrl);
				}
				else
					fValue = calcSliderVariable(pCtrl->fUserDisplayDataLoLimit, pCtrl->fUserDisplayDataHiLimit, f);

				pControl->setValue(fValue);
				((CTextEdit*)pControl)->setText(p);
				break;
			}
			case UINTData:
			{
				string str(p);
				string list(pCtrl->cEnumeratedList);
				if(list.find(str) == -1)
				{
					fValue = calcSliderVariable(pCtrl->fUserDisplayDataLoLimit, pCtrl->fUserDisplayDataHiLimit, *(pCtrl->m_pUserCookedUINTData));
					pControl->setValue(fValue);

					char* pEnum;
					pEnum = getEnumString(pCtrl->cEnumeratedList, (int)(*(pCtrl->m_pUserCookedUINTData)));
					if(pEnum)
						((CTextEdit*)pControl)->setText(pEnum);
				}
				else
				{
					int t = getEnumStringIndex(pCtrl->cEnumeratedList, p);
					if(t < 0)
					{
						// this should never happen...
						char* pEnum;
						pEnum = getEnumString(pCtrl->cEnumeratedList, 0);
						if(pEnum)
							((CTextEdit*)pControl)->setText(pEnum);
						fValue = 0.0;
					}
					else
					{
						fValue = calcSliderVariable(pCtrl->fUserDisplayDataLoLimit, pCtrl->fUserDisplayDataHiLimit, (float)t);
						pControl->setValue(fValue);
						((CTextEdit*)pControl)->setText(str.c_str());
					}
				}

				break;
			}
			default:
				break;
		}

		return fValue;
	}

	inline void setPlugInParameterNormalized(CUICtrl* pUICtrl, float value)
	{
		switch(pUICtrl->uUserDataType)
		{
			case intData:
				*(pUICtrl->m_pUserCookedIntData) = calcDisplayVariable(pUICtrl->fUserDisplayDataLoLimit, pUICtrl->fUserDisplayDataHiLimit, value);
				break;

			case floatData:
				*(pUICtrl->m_pUserCookedFloatData) = calcDisplayVariable(pUICtrl->fUserDisplayDataLoLimit, pUICtrl->fUserDisplayDataHiLimit, value);
				break;

			case doubleData:
				*(pUICtrl->m_pUserCookedDoubleData) = calcDisplayVariable(pUICtrl->fUserDisplayDataLoLimit, pUICtrl->fUserDisplayDataHiLimit, value);
				break;

			case UINTData:
				*(pUICtrl->m_pUserCookedUINTData) = calcDisplayVariable(pUICtrl->fUserDisplayDataLoLimit, pUICtrl->fUserDisplayDataHiLimit, value);
				break;

			default:
				break;
		}
		m_pPlugIn->userInterfaceChange(pUICtrl->uControlId);
	}
};
}

#endif // __CVSTGUIController__

