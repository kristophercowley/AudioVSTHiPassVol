#include "RafxPluginFactory.h"

CPlugIn* CRafxPluginFactory::getRafxPlugIn()
{
	return new CSimpleHPF;
}


