#pragma once
#ifndef PANEBIND_BROWSER_SINK_TESTS
#error This interface is compiled only into the synthetic sink test executable.
#endif
#include "platform/windows/explorer/explorer_shell_inventory.h"
#include <oaidl.h>
namespace panebind::platform::windows::explorer::testing {
// No HWND, capability or Explorer provisioning. The caller owns its fake COM
// source; only the existing production sink's Invoke/receipt logic is exercised.
IDispatch* create_browser_sink(IUnknown* expected);
BrowserReadinessFacts browser_sink_facts(IDispatch* sink);
void drain_browser_sink(IDispatch* sink);
void retire_browser_sink(IDispatch* sink);
}
