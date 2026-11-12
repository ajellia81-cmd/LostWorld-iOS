// iOS port: no raw input. One pointer (the touches, or a mouse on an iPad)
// is player 1's gun; player 2 plays with a game controller.
#include "raw_mice.h"

namespace raw_mice {

void init(HWND) {}
void on_input(HRAWINPUT) {}
void update(HWND, bool, bool) {}
bool two_mice() { return false; }
void reset() {}
void set_saved(const std::string&, const std::string&) {}
Gun gun(int, HWND) { return Gun{{0, 0}, false, false, false}; }
std::vector<Device> devices(HWND) { return {}; }

}  // namespace raw_mice
