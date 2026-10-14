#include "BaseGameApp.h"
#include "MainLoop.h"
#include <fstream>

#if !(defined(__ANDROID__) || defined(__WINDOWS__))
#include <pwd.h>
#include <unistd.h>
#endif

#ifdef __APPLE__
#include <fcntl.h>
#include <sys/stat.h>
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// Default files shipped with the game. config.xml and SAVES.XML are created
// from them on the first run, so the user's own copies are never versioned.
static const char* CONFIG_TEMPLATE_FILE = "config.template.xml";
static const char* SAVES_TEMPLATE_FILE = "SAVES.template.XML";

static bool FileExists(const std::string& path)
{
    std::ifstream f(path.c_str());
    return f.good();
}

static bool CopyFileFromTemplate(const std::string& templatePath, const std::string& targetPath)
{
    std::ifstream src(templatePath.c_str(), std::ios::binary);
    if (!src.good())
    {
        return false;
    }

    std::ofstream dst(targetPath.c_str(), std::ios::binary);
    if (!dst.good())
    {
        return false;
    }

    dst << src.rdbuf();
    return dst.good();
}

#ifdef __APPLE__
// Files shipped inside OpenClaw.app. The game opens them relative to the working directory,
// so they are copied next to the user's game data.
static const char* BUNDLED_FILES[] = {
    "ASSETS.ZIP", "clacon.ttf", "console02.tga", CONFIG_TEMPLATE_FILE, SAVES_TEMPLATE_FILE
};

static bool IsSameFile(const struct stat& a, const struct stat& b)
{
    return a.st_size == b.st_size && a.st_mtimespec.tv_sec == b.st_mtimespec.tv_sec;
}

// When the game runs from OpenClaw.app, the app bundle is read-only and the game data from the
// original disc can't be shipped with it. The game runs in ~/Library/Application Support/OpenClaw,
// where the user copies the disc files and where config.xml and SAVES.XML are created.
// Returns false when the game can't start.
static bool PrepareAppBundleDirectory()
{
    char* basePath = SDL_GetBasePath();
    if (basePath == NULL)
    {
        return true;
    }
    std::string resourceDirectory = basePath;
    SDL_free(basePath);

    const std::string bundleSuffix = ".app/Contents/Resources/";
    if (resourceDirectory.size() < bundleSuffix.size() ||
        resourceDirectory.compare(resourceDirectory.size() - bundleSuffix.size(), bundleSuffix.size(), bundleSuffix) != 0)
    {
        // Not started from an app bundle, e.g. ./openclaw in Build_Release
        return true;
    }

    const char* homedir = getenv("HOME");
    if (homedir == NULL)
    {
        homedir = getpwuid(getuid())->pw_dir;
    }

    std::string dataDirectory = std::string(homedir) + "/Library/Application Support/OpenClaw/";
    std::string command = "mkdir -p \"" + dataDirectory + "\"";
    if (system(command.c_str()) != 0 || chdir(dataDirectory.c_str()) != 0)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "OpenClaw",
            ("Could not create the game data folder:\n" + dataDirectory).c_str(), NULL);
        return false;
    }

    for (const char* fileName : BUNDLED_FILES)
    {
        std::string sourcePath = resourceDirectory + fileName;
        struct stat source, target;
        if (stat(sourcePath.c_str(), &source) != 0)
        {
            LOG_WARNING("Missing in the app bundle: " + sourcePath);
            continue;
        }
        if (stat(fileName, &target) == 0 && IsSameFile(source, target))
        {
            continue;
        }

        if (CopyFileFromTemplate(sourcePath, fileName))
        {
            // Keep the modification time of the bundled file, so the copy is only refreshed by a new release
            struct timespec times[2] = { source.st_atimespec, source.st_mtimespec };
            utimensat(AT_FDCWD, fileName, times, 0);
        }
        else
        {
            LOG_WARNING("Could not copy " + sourcePath + " to " + dataDirectory);
        }
    }

    if (!FileExists("CLAW.REZ"))
    {
        std::string message =
            "OpenClaw needs the data of the original game (Captain Claw or Kapitan Pazur).\n\n"
            "Copy these files from your game disc into the folder that is now open in Finder:\n"
            "    DATA/CLAW.REZ (required)\n"
            "    MUSIC/CLAW4.SF2 and MUSIC/CLAW.SF2 (music)\n"
            "    MOVIES/claw.fec, renamed to CLAW.FEC (movies)\n\n"
            "Folder: " + dataDirectory + "\n\n"
            "Then start OpenClaw again.";
        LOG_ERROR("CLAW.REZ not found in " + dataDirectory);

        if (getenv("OPENCLAW_NO_DIALOG") == NULL)
        {
            command = "open \"" + dataDirectory + "\"";
            system(command.c_str());
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "OpenClaw", message.c_str(), NULL);
        }
        return false;
    }

    return true;
}
#endif

int RunGameEngine(int argc, char** argv)
{
#ifdef __APPLE__
    if (!PrepareAppBundleDirectory())
    {
        return 1;
    }
#endif

    if (SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH) != 0)
    {
        LOG_WARNING("Failed to set high priority class to this process");
    }

    std::string userDirectory = "";

#if defined(__ANDROID__)
    userDirectory = "/sdcard/claw/";
#elif defined(__WINDOWS__)
    userDirectory = "";
#else
    const char* homedir;

    if ((homedir = getenv("HOME")) == NULL) 
    {
        homedir = getpwuid(getuid())->pw_dir;
    }
    assert(homedir != NULL);

    userDirectory = std::string(homedir) + "/.config/openclaw/";

#endif

    LOG("Looking for: " + userDirectory + "config.xml");

    // Temporary hack - always prefer config in the same folder as binary to default config
    if (TryToFindFile("config.xml"))
    {
        // File was found in a working directory
        userDirectory = "";
    }
    else if (!FileExists(userDirectory + "config.xml") &&
             CopyFileFromTemplate(CONFIG_TEMPLATE_FILE, "config.xml"))
    {
        // First run - create config in a working directory from the template
        LOG("Created config.xml from " + std::string(CONFIG_TEMPLATE_FILE));
        userDirectory = "";
    }

    LOG("Expecting config.xml in path: " + userDirectory + "config.xml");

    // Load options
    if (!g_pApp->LoadGameOptions(std::string(userDirectory + "config.xml").c_str()))
    {
        LOG_ERROR("Could not load game options. Exiting.");
        return -1;
    }

    g_pApp->GetGameConfig()->userDirectory = userDirectory;

    std::string savesFilePath = g_pApp->GetGameConfig()->userDirectory + g_pApp->GetGameConfig()->savesFile;
    if (!FileExists(savesFilePath))
    {
        if (CopyFileFromTemplate(SAVES_TEMPLATE_FILE, savesFilePath))
        {
            LOG("Created " + savesFilePath + " from " + std::string(SAVES_TEMPLATE_FILE));
        }
        else
        {
            LOG_WARNING("Could not create " + savesFilePath + " from " + std::string(SAVES_TEMPLATE_FILE));
        }
    }

    LOG("Loaded with:\n\tConfig File: " + userDirectory + "config.xml" + "\n\tSaves File: " + savesFilePath);

    // Initialize game instance
    if (!g_pApp->Initialize(argc, argv))
    {
        LOG_ERROR("Failed to initialize. Exiting.");
        return -1;
    }

    // Run the game
    return g_pApp->Run();
}

bool TryToFindFile(const char *path) {
    {
        std::ifstream f(path);
        if (f.good()) {
            return true;
        }
    }
#ifdef __EMSCRIPTEN__
    // File does not exist. Try to download
    emscripten_wget(path, path);
    {
        // Check is file downloaded
        std::ifstream f(path);
        if (f.good()) {
            return true;
        }
    }
#endif
    return false;
}
