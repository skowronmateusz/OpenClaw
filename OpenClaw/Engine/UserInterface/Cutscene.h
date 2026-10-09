#ifndef __CUTSCENE_H__
#define __CUTSCENE_H__

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include "../SharedDefines.h"
#include "../Events/EventMgr.h"
#include "../Video/SmackerDecoder.h"

// Movies in original game's CLAW.FEC, in order of appearance
enum Movie
{
    Movie_Logo = 0,
    Movie_Intro,
    Movie_Escape,           // After level 2
    Movie_OutOfTheWoods,    // After level 4
    Movie_Tavern,           // After level 6
    Movie_Chase,            // After level 8
    Movie_Reunited,         // After level 10
    Movie_TigerIsland,      // After level 12
    Movie_WeMeetAgain,      // After level 13
    Movie_Ceremony          // After level 14 (ending)
};

namespace Cutscenes
{
    // Movies are enabled in config and the movies file was found
    bool AreAvailable();
    // Logo and intro which are played when the game starts
    std::vector<int> GetStartupMovies();
    // Movie which is played after finishing given level, -1 if there is none
    int GetMovieAfterLevel(int levelNumber);
}

// Plays list of movies fullscreen. Any key / click skips current movie, Escape skips all of them.
// When the movies are finished, the finished event is queued.
class ScreenElementCutscene : public IScreenElement
{
public:
    ScreenElementCutscene(SDL_Renderer* pRenderer, const std::vector<int>& movies, IEventDataPtr pFinishedEvent);
    virtual ~ScreenElementCutscene();

    // IScreenElement implementation
    virtual void VOnLostDevice() override { }
    virtual void VOnUpdate(uint32 msDiff) override;
    virtual void VOnRender(uint32 msDiff) override;

    virtual int32 VGetZOrder() const override { return 30000; }
    virtual void VSetZOrder(int32 const zOrder) override { }
    virtual bool VIsVisible() override { return true; }
    virtual void VSetVisible(bool visible) override { }

    virtual bool VOnEvent(SDL_Event& evt) override;

private:
    // Starts next playable movie from the list, returns false when there are none left
    bool StartNextMovie();
    bool StartMovie(int movieIdx);
    void StopMovie();
    void StartAudio();
    void StopAudio();
    // Keeps converted audio of upcoming frames queued for the audio callback
    void FeedAudio();
    // Called from audio thread
    static void AudioCallback(void* pUserData, Uint8* pStream, int length);
    void UpdateTexture();
    void Finish();

    SDL_Renderer* m_pRenderer;
    std::vector<int> m_Movies;
    int m_NextMovieListIdx;
    IEventDataPtr m_pFinishedEvent;
    bool m_bFinished;
    bool m_bSkipCurrent;
    bool m_bSkipAll;

    SmackerDecoder m_Decoder;
    SDL_Texture* m_pTexture;
    uint32 m_MovieStartTime;

    // Audio is streamed through SDL_mixer's music hook (no music is played during cutscenes)
    bool m_bAudioHooked;
    SDL_mutex* m_pAudioMutex;
    SDL_AudioCVT m_AudioCvt;
    Uint16 m_MixerFormat;
    int m_MixerBytesPerSecond;
    int m_NextAudioFrame;
    std::vector<uint8_t> m_DecodedAudio;
    std::vector<uint8_t> m_ConvertedAudio;
    // Shared with audio thread
    std::vector<uint8_t> m_AudioQueue;
    size_t m_AudioQueueReadPos;
    int m_AudioVolume;
};

#endif
