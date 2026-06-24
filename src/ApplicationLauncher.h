#pragma once

#include "ToolRegistry.h"

#include <QString>

struct LaunchResult {
    bool ok = false;
    QString error;
    unsigned long processId = 0;
};

class ApplicationLauncher final {
public:
    static LaunchResult launchDetached(const ToolEntry& tool);
    static QString win32ErrorMessage(unsigned long errorCode);
};
