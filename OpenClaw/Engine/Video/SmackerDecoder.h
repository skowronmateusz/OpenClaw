#ifndef __SMACKER_DECODER_H__
#define __SMACKER_DECODER_H__

#include <stdint.h>
#include <stdio.h>
#include <string>
#include <vector>

// Decoder for Smacker (SMK2 / SMK4) videos which were used for cutscenes in the original game.
// Video is decoded into 8-bit paletted frames, audio (track 0 only) into raw PCM.
//
// The video can be embedded inside another file (e.g. CLAW.FEC), so it is opened with an offset.
class SmackerDecoder
{
public:
    SmackerDecoder();
    ~SmackerDecoder();

    bool Open(const std::string& filePath, uint64_t offset = 0);
    void Close();
    bool IsOpen() const { return m_pFile != NULL; }

    // Decodes next video frame. Returns false when there are no more frames or on error
    bool DecodeNextFrame();

    // Decodes audio track 0 of given frame and appends it as PCM samples (format described by GetAudio* methods).
    // Audio is usually stored ahead of video, so it is decoded independently of video decoding state.
    bool DecodeFrameAudio(int frameIdx, std::vector<uint8_t>& outPcm);

    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }
    int GetFrameCount() const { return m_FrameCount; }
    // Index of last decoded frame, -1 if no frame was decoded yet
    int GetCurrentFrame() const { return m_CurrentFrame; }
    double GetFramesPerSecond() const { return m_Fps; }

    // Width * Height palette indices
    const uint8_t* GetFrameData() const { return m_Frame.data(); }
    // 256 RGB triplets
    const uint8_t* GetPalette() const { return m_Palette; }

    bool HasAudio() const { return m_AudioRate != 0; }
    int GetAudioRate() const { return m_AudioRate; }
    int GetAudioChannels() const { return m_AudioChannels; }
    int GetAudioBitsPerSample() const { return m_AudioBits; }

    // Total size of the Smacker stream (header + all frames) in bytes
    static bool GetStreamSize(FILE* pFile, uint64_t offset, uint64_t& outSize);

private:
    // Huffman tree stored in a flat array. Node entries have NODE flag set and contain
    // size of their left subtree, leaf entries contain decoded value.
    typedef std::vector<uint32_t> HuffTree;

    struct BigTree
    {
        HuffTree tree;
        int last[3];
    };

    class BitReader;

    bool ReadHeaderTrees(const std::vector<uint8_t>& treeData);
    static bool ReadByteTree(BitReader& reader, HuffTree& tree);
    static bool ReadByteTreeRecursive(BitReader& reader, HuffTree& tree, int depth);
    static bool ReadBigTree(BitReader& reader, BigTree& bigTree);
    static bool ReadBigTreeRecursive(BitReader& reader, BigTree& bigTree, const HuffTree& lowTree,
        const HuffTree& highTree, const int escapes[3], int depth);
    static uint32_t DecodeValue(BitReader& reader, const HuffTree& tree);
    static uint32_t DecodeBigValue(BitReader& reader, BigTree& bigTree);
    static void ResetBigTreeCache(BigTree& bigTree);

    void DecodePalette(const uint8_t* pData, size_t size);
    void DecodeVideo(const uint8_t* pData, size_t size);
    bool DecodeAudioChunk(const uint8_t* pData, size_t size, std::vector<uint8_t>& outPcm);

    bool ReadAt(uint64_t position, void* pDest, size_t size);

    FILE* m_pFile;
    uint64_t m_Offset;

    bool m_bIsVersion4;
    int m_Width;
    int m_Height;
    int m_FrameCount;
    double m_Fps;
    uint32_t m_Flags;

    uint32_t m_AudioFlags;
    int m_AudioRate;
    int m_AudioChannels;
    int m_AudioBits;

    std::vector<uint32_t> m_FrameSizes;
    std::vector<uint8_t> m_FrameTypes;
    std::vector<uint64_t> m_FrameOffsets;

    BigTree m_MMapTree;
    BigTree m_MClrTree;
    BigTree m_FullTree;
    BigTree m_TypeTree;

    int m_CurrentFrame;
    std::vector<uint8_t> m_Frame;
    uint8_t m_Palette[256 * 3];
    std::vector<uint8_t> m_FrameBuffer;
};

#endif
