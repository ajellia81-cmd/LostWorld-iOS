// Screen-space heuristics applied to a built scene: geometry the game draws
// over the 3D image that a widescreen window treats differently.
#pragma once
#include "r3d_scene.h"

// Collapses the black cinematic bars the game draws over the scene
void r3d_strip_letterbox(R3DScene& out);
// Marks the 3D HUD elements at the sides of the screen (ammo) so the
// renderer can move them to the edges of a widescreen window
void r3d_anchor_hud(R3DScene& out);
// Collapses a gun sight the game draws at (x, y) (fractions of the 4:3
// screen): small near geometry of an overlay holding that point, unless it
// also holds (keep_x, keep_y), another player's sight
void r3d_hide_sight(R3DScene& out, float x, float y, float keep_x, float keep_y);
