#include "MovieArchive.h"
#include "SmackerDecoder.h"

#include <stdio.h>
#include <string.h>

// Gap between two movies in CLAW.FEC is less than 1 KB, search a lot further to be safe
static const size_t SEARCH_WINDOW_SIZE = 1024 * 1024;

static bool SeekFile(FILE* pFile, uint64_t position)
{
#ifdef _WIN32
    return _fseeki64(pFile, (__int64)position, SEEK_SET) == 0;
#else
    return fseeko(pFile, (off_t)position, SEEK_SET) == 0;
#endif
}

static uint64_t GetFileSize(FILE* pFile)
{
#ifdef _WIN32
    _fseeki64(pFile, 0, SEEK_END);
    return (uint64_t)_ftelli64(pFile);
#else
    fseeko(pFile, 0, SEEK_END);
    return (uint64_t)ftello(pFile);
#endif
}

// Returns offset of first Smacker signature at or after searchStart within search window, or false
static bool FindSmackerSignature(FILE* pFile, uint64_t searchStart, uint64_t fileSize, uint64_t& outOffset)
{
    if (searchStart >= fileSize)
    {
        return false;
    }

    size_t windowSize = (size_t)(fileSize - searchStart < SEARCH_WINDOW_SIZE ? fileSize - searchStart : SEARCH_WINDOW_SIZE);
    std::vector<uint8_t> window(windowSize);
    if (!SeekFile(pFile, searchStart) || fread(window.data(), 1, windowSize, pFile) != windowSize)
    {
        return false;
    }

    for (size_t i = 0; i + 4 <= windowSize; i++)
    {
        if (window[i] == 'S' && window[i + 1] == 'M' && window[i + 2] == 'K' &&
            (window[i + 3] == '2' || window[i + 3] == '4'))
        {
            outOffset = searchStart + i;
            return true;
        }
    }

    return false;
}

bool MovieArchive::Open(const std::string& filePath)
{
    m_FilePath = filePath;
    m_MovieOffsets.clear();

    FILE* pFile = fopen(filePath.c_str(), "rb");
    if (pFile == NULL)
    {
        return false;
    }

    uint64_t fileSize = GetFileSize(pFile);
    uint64_t searchStart = 0;
    uint64_t movieOffset;
    while (FindSmackerSignature(pFile, searchStart, fileSize, movieOffset))
    {
        uint64_t movieSize;
        if (SmackerDecoder::GetStreamSize(pFile, movieOffset, movieSize) &&
            movieOffset + movieSize <= fileSize)
        {
            m_MovieOffsets.push_back(movieOffset);
            searchStart = movieOffset + movieSize;
        }
        else
        {
            // False positive
            searchStart = movieOffset + 1;
        }
    }

    fclose(pFile);

    return !m_MovieOffsets.empty();
}

bool MovieArchive::OpenMovie(int movieIdx, SmackerDecoder& decoder) const
{
    if (movieIdx < 0 || movieIdx >= (int)m_MovieOffsets.size())
    {
        return false;
    }

    return decoder.Open(m_FilePath, m_MovieOffsets[movieIdx]);
}
