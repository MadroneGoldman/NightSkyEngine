#pragma once

#include "Templates/SharedPointer.h"
//#include "Styling/SlateStyle.h"

//class FSlateStyleSet;

class NIGHTSKYENGINE_API NSEStyleManager
{
public:
	static void Initialize();
	static void Shutdown();
private:
	static TSharedPtr<class FSlateStyleSet > StyleSet;
};
