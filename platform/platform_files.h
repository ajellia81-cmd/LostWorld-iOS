// iOS port: where the game's files are. The app bundle holds what ships with
// it (res/, textures/, third_party/fonts, the ROM) under game/; the game reads
// and writes relative paths (save/, out/, data/), so the working folder is
// the app's Documents folder, with links to the bundle's read-only folders.
#pragma once
void platform_prepare_files();

// The phone's heat as iOS sees it (NSProcessInfo.thermalState): 0 nominal,
// 1 fair, 2 serious (it slows the GPU and CPU down), 3 critical; -1 unknown
int platform_thermal_state();

// Whether the app is in front and active (UIApplication.applicationState).
// While it is not (the home screen, the app switcher, Control Center, a
// call), iOS lets no GPU work through: a frame submitted then loses the
// Vulkan device. Asked each frame, after the events, since the moment it
// changes comes from UIKit while the events are pumped. Always true off iOS.
bool platform_app_active();
