#include "pch.h"
#include "Global.h"

namespace AFS_Core {

#ifdef _WXWIDGETS
    Global& Global::instance()
    {
        static Global instance;
        return instance;
    }
#endif

} // namespace AFS_Core
