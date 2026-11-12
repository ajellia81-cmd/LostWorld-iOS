// iOS port of crosshair_file.cpp: the pictures live in the app's Documents
// folder (the working folder, shown by the Files app), where the player puts
// crosshair.png / crosshair_p2.png. There is no file dialog to pick one from
// the menu, so the menu's choice only reloads what is there.
#include "crosshair_file.h"

#include "image_io.h"

std::string crosshair_path(int player) { return player ? "crosshair_p2.png" : "crosshair.png"; }

bool crosshair_load(int player, int& w, int& h, std::vector<uint8_t>& rgba) {
    return png_load(crosshair_path(player), w, h, rgba) || (player && png_load(crosshair_path(0), w, h, rgba));
}

bool crosshair_pick(int player) {
    int w, h;
    std::vector<uint8_t> rgba;
    return crosshair_load(player, w, h, rgba);
}
