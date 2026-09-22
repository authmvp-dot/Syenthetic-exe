#pragma once
#include <string>
#include "imgui.h"

namespace AuthGui {
    void Init();
    void Render();
    bool IsAuthenticated();
}
