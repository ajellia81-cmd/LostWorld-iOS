#include "platform_files.h"

#include <SDL3/SDL.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <string>

#ifdef __APPLE__
#include <objc/message.h>
#include <objc/runtime.h>
#endif

// The bundle (read-only) keeps what ships with the app under game/: res/,
// textures/, third_party/fonts and, when build.sh was given it, lostwsga.zip.
// The game reads and writes paths relative to its working folder (save/,
// out/, settings.ini, records), so that is the app's Documents folder, which
// the Files app shows (Info.plist: UIFileSharingEnabled): a lostwsga.zip,
// crosshair.png or texture pack put there is found as next to LostWorld.exe.
void platform_prepare_files() {
    const char* base = SDL_GetBasePath();   // the app bundle (with a trailing /)
    std::string game = std::string(base ? base : "./") + "game";
    struct stat st;
    if (stat(game.c_str(), &st) != 0) {   // no bundle (the Linux check build): the current folder as it is
        printf("[files] no %s: using the current folder\n", game.c_str());
        return;
    }
    const char* docs = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
    std::string work = docs ? docs : "";
    if (work.empty()) {
        char* pref = SDL_GetPrefPath("snicolast", "LostWorld");
        if (pref) work = pref, SDL_free(pref);
    }
    if (work.empty() || chdir(work.c_str()) != 0) {
        fprintf(stderr, "[files] no writable folder; running from the bundle\n");
        chdir(game.c_str());
        return;
    }
    // What the game prints goes to lostworld.log there (a pipe nobody reads
    // would hold the game up once full); the previous run's is kept
    rename("lostworld.log", "lostworld_previous.log");
    if (freopen("lostworld.log", "w", stdout)) {
        setvbuf(stdout, nullptr, _IOLBF, 0);
        dup2(fileno(stdout), fileno(stderr));
    }
    for (const char* d : {"save", "out", "data", "third_party"}) mkdir(d, 0755);
    // The GPU time of each pass, every 300 images, in the log (the renderer's
    // LW_GPUTIME): with the [timing] line it shows what grows as the game slows
    setenv("LW_GPUTIME", "1", 0);
    // The bundle's folders, linked again at every launch: the bundle moves when
    // the app is reinstalled or updated. A real folder of the player's (their
    // own textures/) is left alone.
    auto link_to = [&](const std::string& from, const std::string& to) {
        struct stat ls;
        if (lstat(to.c_str(), &ls) == 0 && !S_ISLNK(ls.st_mode)) return;
        unlink(to.c_str());
        if (symlink(from.c_str(), to.c_str()) != 0) perror(("[files] link " + to).c_str());
    };
    link_to(game + "/res", "res");
    link_to(game + "/textures", "textures");
    link_to(game + "/third_party/fonts", "third_party/fonts");
    // A lostwsga.zip in Documents wins (app.cpp looks in the working folder);
    // otherwise the one packed into the app
    std::string zip = game + "/lostwsga.zip";
    if (stat("lostwsga.zip", &st) != 0 && stat(zip.c_str(), &st) == 0) setenv("LW_ROM_ZIP", zip.c_str(), 0);
    printf("[files] working folder %s, game files %s\n", work.c_str(), game.c_str());
}

int platform_thermal_state() {
#ifdef __APPLE__
    // [[NSProcessInfo processInfo] thermalState], without Objective-C sources
    Class cls = objc_getClass("NSProcessInfo");
    if (!cls) return -1;
    id info = reinterpret_cast<id (*)(Class, SEL)>(objc_msgSend)(cls, sel_registerName("processInfo"));
    if (!info) return -1;
    return int(reinterpret_cast<long (*)(id, SEL)>(objc_msgSend)(info, sel_registerName("thermalState")));
#else
    return -1;
#endif
}
