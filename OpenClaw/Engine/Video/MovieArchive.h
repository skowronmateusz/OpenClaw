#ifndef __MOVIE_ARCHIVE_H__
#define __MOVIE_ARCHIVE_H__

#include <stdint.h>
#include <string>
#include <vector>

class SmackerDecoder;

// Original game stores all of its movies in MOVIES/CLAW.FEC. It is a container with encrypted directory
// and plain Smacker streams, so the movies are found by scanning for Smacker headers.
// Movies are indexed in order of their appearance in the file:
//  0 - Monolith logo, 1 - intro, 2..9 - cutscenes between chapters (last one is the ending)
// A plain .SMK file is treated as an archive with a single movie.
class MovieArchive
{
public:
    bool Open(const std::string& filePath);
    bool IsOpen() const { return !m_MovieOffsets.empty(); }

    int GetMovieCount() const { return (int)m_MovieOffsets.size(); }
    bool OpenMovie(int movieIdx, SmackerDecoder& decoder) const;

private:
    std::string m_FilePath;
    std::vector<uint64_t> m_MovieOffsets;
};

#endif
