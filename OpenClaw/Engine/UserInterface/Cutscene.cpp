#include "Cutscene.h"
#include "../GameApp/BaseGameApp.h"
#include "../Audio/Audio.h"
#include "../Events/Events.h"
#include "../Video/MovieArchive.h"

#include <algorithm>
#include <cstring>

// Original game was laid out for 640x480, movies are scaled to fit into that area
static const double MOVIE_AREA_WIDTH = 640.0;
static const double MOVIE_AREA_HEIGHT = 480.0;

// Do not stall the game if decoding falls behind
static const int MAX_FRAMES_DECODED_PER_UPDATE = 8;

//=====================================================================================================================
// Cutscenes
//=====================================================================================================================

static MovieArchive* GetMovieArchive()
{
    static MovieArchive* s_pArchive = NULL;
    static bool s_bTriedToOpen = false;

    if (!s_bTriedToOpen)
    {
        s_bTriedToOpen = true;

        const GameOptions* pOptions = g_pApp->GetGameConfig();
        std::string moviesPath = pOptions->moviesFile;
        bool isAbsolutePath = !moviesPath.empty() &&
            (moviesPath[0] == '/' || moviesPath[0] == '\\' || moviesPath.find(':') != std::string::npos);
        if (!isAbsolutePath)
        {
            moviesPath = pOptions->assetsFolder + moviesPath;
        }

        MovieArchive* pArchive = new MovieArchive();
        if (pArchive->Open(moviesPath))
        {
            LOG("Movies file \"" + moviesPath + "\" loaded, movies: " + ToStr(pArchive->GetMovieCount()));
            s_pArchive = pArchive;
        }
        else
        {
            LOG_WARNING("Movies file \"" + moviesPath + "\" not found or contains no movies. Movies are disabled.");
            delete pArchive;
        }
    }

    return s_pArchive;
}

bool Cutscenes::AreAvailable()
{
    return g_pApp->GetGameConfig()->playMovies && GetMovieArchive() != NULL;
}

std::vector<int> Cutscenes::GetStartupMovies()
{
    std::vector<int> movies;
    if (!AreAvailable())
    {
        return movies;
    }

    if (!g_pApp->GetGameConfig()->skipLogoMovies)
    {
        movies.push_back(Movie_Logo);
    }
    movies.push_back(Movie_Intro);

    return movies;
}

int Cutscenes::GetMovieAfterLevel(int levelNumber)
{
    // Movies are played after each boss level
    switch (levelNumber)
    {
        case 2: return Movie_Escape;
        case 4: return Movie_OutOfTheWoods;
        case 6: return Movie_Tavern;
        case 8: return Movie_Chase;
        case 10: return Movie_Reunited;
        case 12: return Movie_TigerIsland;
        case 13: return Movie_WeMeetAgain;
        case 14: return Movie_Ceremony;
        default: return -1;
    }
}

//=====================================================================================================================
// ScreenElementCutscene
//=====================================================================================================================

ScreenElementCutscene::ScreenElementCutscene(SDL_Renderer* pRenderer, const std::vector<int>& movies, IEventDataPtr pFinishedEvent)
    :
    m_pRenderer(pRenderer),
    m_Movies(movies),
    m_NextMovieListIdx(0),
    m_pFinishedEvent(pFinishedEvent),
    m_bFinished(false),
    m_bSkipCurrent(false),
    m_bSkipAll(false),
    m_pTexture(NULL),
    m_MovieStartTime(0),
    m_bAudioHooked(false),
    m_pAudioMutex(NULL),
    m_MixerFormat(AUDIO_S16SYS),
    m_MixerBytesPerSecond(0),
    m_NextAudioFrame(0),
    m_AudioQueueReadPos(0),
    m_AudioVolume(0)
{
    m_pAudioMutex = SDL_CreateMutex();

    if (!StartNextMovie())
    {
        Finish();
    }
}

ScreenElementCutscene::~ScreenElementCutscene()
{
    StopMovie();

    if (m_pAudioMutex)
    {
        SDL_DestroyMutex(m_pAudioMutex);
    }
}

bool ScreenElementCutscene::StartNextMovie()
{
    while (m_NextMovieListIdx < (int)m_Movies.size())
    {
        int movieIdx = m_Movies[m_NextMovieListIdx++];
        if (StartMovie(movieIdx))
        {
            return true;
        }
    }

    return false;
}

bool ScreenElementCutscene::StartMovie(int movieIdx)
{
    StopMovie();

    if (!Cutscenes::AreAvailable())
    {
        return false;
    }

    if (!GetMovieArchive()->OpenMovie(movieIdx, m_Decoder))
    {
        LOG_ERROR("Failed to open movie: " + ToStr(movieIdx));
        return false;
    }

    // Scale movie smoothly, restore the hint afterwards since other textures expect it
    const char* pPrevScaleQuality = SDL_GetHint(SDL_HINT_RENDER_SCALE_QUALITY);
    std::string prevScaleQuality = pPrevScaleQuality ? pPrevScaleQuality : "";
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
    m_pTexture = SDL_CreateTexture(m_pRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
        m_Decoder.GetWidth(), m_Decoder.GetHeight());
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, prevScaleQuality.c_str());

    if (m_pTexture == NULL)
    {
        LOG_ERROR("Failed to create movie texture: " + std::string(SDL_GetError()));
        m_Decoder.Close();
        return false;
    }

    LOG("Playing movie: " + ToStr(movieIdx) + " (" + ToStr(m_Decoder.GetWidth()) + "x" +
        ToStr(m_Decoder.GetHeight()) + ", " + ToStr(m_Decoder.GetFrameCount()) + " frames)");

    StartAudio();

    m_bSkipCurrent = false;
    m_MovieStartTime = SDL_GetTicks();
    if (m_Decoder.DecodeNextFrame())
    {
        UpdateTexture();
    }

    return true;
}

void ScreenElementCutscene::StopMovie()
{
    StopAudio();

    if (m_pTexture)
    {
        SDL_DestroyTexture(m_pTexture);
        m_pTexture = NULL;
    }

    m_Decoder.Close();
}

void ScreenElementCutscene::StartAudio()
{
#ifndef __EMSCRIPTEN__
    if (!m_Decoder.HasAudio() || m_pAudioMutex == NULL)
    {
        return;
    }

    int mixerFrequency, mixerChannels;
    if (Mix_QuerySpec(&mixerFrequency, &m_MixerFormat, &mixerChannels) == 0)
    {
        return;
    }

    SDL_AudioFormat movieFormat = (m_Decoder.GetAudioBitsPerSample() == 16) ? AUDIO_S16LSB : AUDIO_U8;
    if (SDL_BuildAudioCVT(&m_AudioCvt, movieFormat, (Uint8)m_Decoder.GetAudioChannels(), m_Decoder.GetAudioRate(),
        m_MixerFormat, (Uint8)mixerChannels, mixerFrequency) < 0)
    {
        LOG_WARNING("Unsupported movie audio conversion: " + std::string(SDL_GetError()));
        return;
    }

    m_MixerBytesPerSecond = mixerFrequency * mixerChannels * (SDL_AUDIO_BITSIZE(m_MixerFormat) / 8);
    m_NextAudioFrame = 0;
    m_AudioQueue.clear();
    m_AudioQueueReadPos = 0;
    m_AudioVolume = 0;

    FeedAudio();

    Mix_HookMusic(&ScreenElementCutscene::AudioCallback, this);
    m_bAudioHooked = true;
#endif
}

void ScreenElementCutscene::StopAudio()
{
#ifndef __EMSCRIPTEN__
    if (m_bAudioHooked)
    {
        // Waits until the callback is not running
        Mix_HookMusic(NULL, NULL);
        m_bAudioHooked = false;
    }
#endif

    m_AudioQueue.clear();
    m_AudioQueueReadPos = 0;
    m_MixerBytesPerSecond = 0;
}

void ScreenElementCutscene::FeedAudio()
{
    if (m_MixerBytesPerSecond == 0)
    {
        return;
    }

    // Movie audio follows sound settings
    int volume = g_pApp->GetAudio()->IsSoundActive() ? Mix_Volume(-1, -1) : 0;

    SDL_LockMutex(m_pAudioMutex);
    m_AudioVolume = volume;
    if (m_AudioQueueReadPos > 0)
    {
        m_AudioQueue.erase(m_AudioQueue.begin(), m_AudioQueue.begin() + m_AudioQueueReadPos);
        m_AudioQueueReadPos = 0;
    }
    size_t queuedBytes = m_AudioQueue.size();
    SDL_UnlockMutex(m_pAudioMutex);

    // Keep half a second of audio ready
    while (queuedBytes < (size_t)m_MixerBytesPerSecond / 2 && m_NextAudioFrame < m_Decoder.GetFrameCount())
    {
        m_DecodedAudio.clear();
        if (!m_Decoder.DecodeFrameAudio(m_NextAudioFrame++, m_DecodedAudio) || m_DecodedAudio.empty())
        {
            continue;
        }

        m_ConvertedAudio.resize(m_DecodedAudio.size() * m_AudioCvt.len_mult);
        memcpy(m_ConvertedAudio.data(), m_DecodedAudio.data(), m_DecodedAudio.size());
        int convertedSize = (int)m_DecodedAudio.size();
        if (m_AudioCvt.needed)
        {
            m_AudioCvt.buf = m_ConvertedAudio.data();
            m_AudioCvt.len = (int)m_DecodedAudio.size();
            if (SDL_ConvertAudio(&m_AudioCvt) != 0)
            {
                continue;
            }
            convertedSize = m_AudioCvt.len_cvt;
        }

        SDL_LockMutex(m_pAudioMutex);
        m_AudioQueue.insert(m_AudioQueue.end(), m_ConvertedAudio.begin(), m_ConvertedAudio.begin() + convertedSize);
        queuedBytes = m_AudioQueue.size() - m_AudioQueueReadPos;
        SDL_UnlockMutex(m_pAudioMutex);
    }
}

void ScreenElementCutscene::AudioCallback(void* pUserData, Uint8* pStream, int length)
{
    ScreenElementCutscene* pThis = (ScreenElementCutscene*)pUserData;

    SDL_memset(pStream, (pThis->m_MixerFormat == AUDIO_U8) ? 0x80 : 0, length);

    SDL_LockMutex(pThis->m_pAudioMutex);
    size_t available = pThis->m_AudioQueue.size() - pThis->m_AudioQueueReadPos;
    size_t mixedBytes = std::min(available, (size_t)length);
    if (mixedBytes > 0 && pThis->m_AudioVolume > 0)
    {
        SDL_MixAudioFormat(pStream, &pThis->m_AudioQueue[pThis->m_AudioQueueReadPos], pThis->m_MixerFormat,
            (Uint32)mixedBytes, pThis->m_AudioVolume);
    }
    pThis->m_AudioQueueReadPos += mixedBytes;
    SDL_UnlockMutex(pThis->m_pAudioMutex);
}

void ScreenElementCutscene::UpdateTexture()
{
    void* pPixels;
    int pitch;
    if (SDL_LockTexture(m_pTexture, NULL, &pPixels, &pitch) != 0)
    {
        return;
    }

    const int width = m_Decoder.GetWidth();
    const int height = m_Decoder.GetHeight();
    const uint8_t* pFrame = m_Decoder.GetFrameData();
    const uint8_t* pPalette = m_Decoder.GetPalette();

    uint32_t colors[256];
    for (int i = 0; i < 256; i++)
    {
        colors[i] = 0xFF000000 | (pPalette[i * 3] << 16) | (pPalette[i * 3 + 1] << 8) | pPalette[i * 3 + 2];
    }

    for (int y = 0; y < height; y++)
    {
        uint32_t* pRow = (uint32_t*)((uint8_t*)pPixels + y * pitch);
        const uint8_t* pSrcRow = pFrame + y * width;
        for (int x = 0; x < width; x++)
        {
            pRow[x] = colors[pSrcRow[x]];
        }
    }

    SDL_UnlockTexture(m_pTexture);
}

void ScreenElementCutscene::Finish()
{
    if (m_bFinished)
    {
        return;
    }

    m_bFinished = true;
    StopMovie();

    if (m_pFinishedEvent)
    {
        IEventMgr::Get()->VQueueEvent(m_pFinishedEvent);
    }
}

void ScreenElementCutscene::VOnUpdate(uint32 msDiff)
{
    if (m_bFinished)
    {
        return;
    }

    if (m_bSkipAll)
    {
        Finish();
        return;
    }

    bool movieEnded = m_bSkipCurrent || !m_Decoder.IsOpen();
    if (!movieEnded)
    {
        // Video is synchronized to wall clock, the same as audio which plays independently
        uint32 elapsedMs = SDL_GetTicks() - m_MovieStartTime;
        double fps = m_Decoder.GetFramesPerSecond();
        int targetFrame = (int)(elapsedMs * fps / 1000.0);

        bool frameDecoded = false;
        for (int i = 0; i < MAX_FRAMES_DECODED_PER_UPDATE && m_Decoder.GetCurrentFrame() < targetFrame; i++)
        {
            if (!m_Decoder.DecodeNextFrame())
            {
                break;
            }
            frameDecoded = true;
        }

        if (frameDecoded)
        {
            UpdateTexture();
        }

        FeedAudio();

        // Last frame stays on screen for its duration
        movieEnded = targetFrame >= m_Decoder.GetFrameCount();
    }

    if (movieEnded && !StartNextMovie())
    {
        Finish();
    }
}

void ScreenElementCutscene::VOnRender(uint32 msDiff)
{
    if (m_pTexture == NULL)
    {
        return;
    }

    Point windowSize = g_pApp->GetWindowSizeScaled();
    double movieWidth = m_Decoder.GetWidth();
    double movieHeight = m_Decoder.GetHeight();

    // E.g. 320x240 movies are doubled, 640x240 logo keeps its size and is letterboxed
    double movieScale = std::min(MOVIE_AREA_WIDTH / movieWidth, MOVIE_AREA_HEIGHT / movieHeight);
    double areaScale = std::min(windowSize.x / MOVIE_AREA_WIDTH, windowSize.y / MOVIE_AREA_HEIGHT);
    double scale = movieScale * areaScale;

    SDL_Rect destRect;
    destRect.w = (int)(movieWidth * scale);
    destRect.h = (int)(movieHeight * scale);
    destRect.x = (int)((windowSize.x - destRect.w) / 2);
    destRect.y = (int)((windowSize.y - destRect.h) / 2);

    SDL_RenderCopy(m_pRenderer, m_pTexture, NULL, &destRect);
}

bool ScreenElementCutscene::VOnEvent(SDL_Event& evt)
{
    switch (evt.type)
    {
        case SDL_KEYDOWN:
        {
            if (evt.key.repeat == 0)
            {
                if (evt.key.keysym.sym == SDLK_ESCAPE)
                {
                    m_bSkipAll = true;
                }
                else
                {
                    m_bSkipCurrent = true;
                }
            }
            return true;
        }
        case SDL_MOUSEBUTTONDOWN:
        case SDL_FINGERDOWN:
        {
            m_bSkipCurrent = true;
            return true;
        }
        default:
            return false;
    }
}
