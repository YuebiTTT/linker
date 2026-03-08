#pragma once
#include "resource.h"
#include "VertexUI/VertexUI.ClickArea.h"
#include "VertexUI/VertexUI.Panel.h"
#include "VertexUI/VertexUI.min.h"
#include "LegacyUtils.hpp"
#include "VinaWindow.hpp"

#include <Psapi.h>
#include <ShlObj.h>
#include <direct.h>
#include <io.h>
#include <regex>
#include <thread>
#include <atomic>
#include <tlhelp32.h>   // 进程快照
#include <shellapi.h>   // ShellExecute
#include <mutex>


