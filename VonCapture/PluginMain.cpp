#include <InfinityPlugin.h>
#include "VonCapture.h"

using namespace Infinity;
using namespace Infinity::Logging;

// Infinity calls this exported entry during the engine Module-Init phase.
// The VoN relay code is already resident in the server image at this point,
// so attaching the detour here is safe (no players are speaking yet).
__declspec(dllexport) void OnPluginLoad()
{
    Println("[VonCapture] loading VoN capture plugin...");

    if (VonCapture::Init())
        Println("[VonCapture] loaded.");
    else
        Errorln("[VonCapture] failed to initialize — VoN capture is DISABLED.");
}
