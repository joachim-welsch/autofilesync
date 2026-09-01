#include "pch.h"
#include "Global.h"

#ifdef _WXWIDGETS
    Global& Global::instance()
    {
        static Global instance;
        return instance;
    }
#endif
