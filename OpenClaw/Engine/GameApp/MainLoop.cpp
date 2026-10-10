#include "BaseGameApp.h"
#include "MainLoop.h"
#include <fstream>

#if !(defined(__ANDROID__) || defined(__WINDOWS__))
#include <pwd.h>
#include <unistd.h>
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

int RunGameEngine(int argc, char** argv)
{
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
