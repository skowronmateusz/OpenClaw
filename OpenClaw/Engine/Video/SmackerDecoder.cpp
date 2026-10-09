#include "SmackerDecoder.h"

#include <string.h>

// Smacker format description: https://wiki.multimedia.cx/index.php/Smacker

static const uint32_t HUFF_NODE = 0x80000000;
static const int MAX_TREE_DEPTH = 32;
static const uint32_t SMK_HEADER_SIZE = 104;
static const int SMK_MAX_AUDIO_TRACKS = 7;

enum SmackerBlockType
{
    SmackerBlock_Mono = 0,
    SmackerBlock_Full = 1,
    SmackerBlock_Skip = 2,
    SmackerBlock_Fill = 3
};

static const int g_BlockRuns[64] =
{
     1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16,
    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
    33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
    49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 128, 256, 512, 1024, 2048
};

// 6-bit palette component -> 8-bit
static const uint8_t g_PaletteMap[64] =
{
    0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24, 0x28, 0x2C, 0x30, 0x34, 0x38, 0x3C,
    0x41, 0x45, 0x49, 0x4D, 0x51, 0x55, 0x59, 0x5D, 0x61, 0x65, 0x69, 0x6D, 0x71, 0x75, 0x79, 0x7D,
    0x82, 0x86, 0x8A, 0x8E, 0x92, 0x96, 0x9A, 0x9E, 0xA2, 0xA6, 0xAA, 0xAE, 0xB2, 0xB6, 0xBA, 0xBE,
    0xC3, 0xC7, 0xCB, 0xCF, 0xD3, 0xD7, 0xDB, 0xDF, 0xE3, 0xE7, 0xEB, 0xEF, 0xF3, 0xF7, 0xFB, 0xFF
};

static uint32_t ReadLE32(const uint8_t* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool SeekFile(FILE* pFile, uint64_t position)
{
#ifdef _WIN32
    return _fseeki64(pFile, (__int64)position, SEEK_SET) == 0;
#else
    return fseeko(pFile, (off_t)position, SEEK_SET) == 0;
#endif
}

//=====================================================================================================================
// BitReader - Smacker bitstreams are read from least significant bit of each byte
//=====================================================================================================================

class SmackerDecoder::BitReader
{
public:
    BitReader(const uint8_t* pData, size_t size) : m_pData(pData), m_SizeInBits(size * 8), m_Position(0) { }

    uint32_t ReadBit()
    {
        if (m_Position >= m_SizeInBits)
        {
            m_Position++;
            return 0;
        }

        uint32_t bit = (m_pData[m_Position >> 3] >> (m_Position & 7)) & 1;
        m_Position++;
        return bit;
    }

    uint32_t ReadBits(int count)
    {
        uint32_t value = 0;
        for (int i = 0; i < count; i++)
        {
            value |= ReadBit() << i;
        }
        return value;
    }

    bool IsOverrun() const { return m_Position > m_SizeInBits; }

private:
    const uint8_t* m_pData;
    size_t m_SizeInBits;
    size_t m_Position;
};

//=====================================================================================================================
// SmackerDecoder
//=====================================================================================================================

SmackerDecoder::SmackerDecoder()
    :
    m_pFile(NULL),
    m_Offset(0),
    m_bIsVersion4(false),
    m_Width(0),
    m_Height(0),
    m_FrameCount(0),
    m_Fps(10.0),
    m_Flags(0),
    m_AudioFlags(0),
    m_AudioRate(0),
    m_AudioChannels(0),
    m_AudioBits(0),
    m_CurrentFrame(-1)
{
    memset(m_Palette, 0, sizeof(m_Palette));
}

SmackerDecoder::~SmackerDecoder()
{
    Close();
}

void SmackerDecoder::Close()
{
    if (m_pFile)
    {
        fclose(m_pFile);
        m_pFile = NULL;
    }

    m_FrameSizes.clear();
    m_FrameTypes.clear();
    m_FrameOffsets.clear();
    m_Frame.clear();
    m_FrameBuffer.clear();
    m_CurrentFrame = -1;
}

bool SmackerDecoder::ReadAt(uint64_t position, void* pDest, size_t size)
{
    if (!SeekFile(m_pFile, m_Offset + position))
    {
        return false;
    }

    return fread(pDest, 1, size, m_pFile) == size;
}

bool SmackerDecoder::GetStreamSize(FILE* pFile, uint64_t offset, uint64_t& outSize)
{
    uint8_t header[SMK_HEADER_SIZE];
    if (!SeekFile(pFile, offset) || fread(header, 1, sizeof(header), pFile) != sizeof(header))
    {
        return false;
    }

    if (memcmp(header, "SMK2", 4) != 0 && memcmp(header, "SMK4", 4) != 0)
    {
        return false;
    }

    uint32_t frameCount = ReadLE32(header + 12);
    uint32_t flags = ReadLE32(header + 20);
    uint32_t treesSize = ReadLE32(header + 52);
    if (flags & 1)
    {
        frameCount++;
    }

    if (frameCount == 0 || frameCount > 1000000)
    {
        return false;
    }

    std::vector<uint8_t> sizes(frameCount * 4);
    if (fread(sizes.data(), 1, sizes.size(), pFile) != sizes.size())
    {
        return false;
    }

    uint64_t totalSize = SMK_HEADER_SIZE + (uint64_t)frameCount * 5 + treesSize;
    for (uint32_t i = 0; i < frameCount; i++)
    {
        totalSize += ReadLE32(&sizes[i * 4]) & ~3u;
    }

    outSize = totalSize;
    return true;
}

bool SmackerDecoder::Open(const std::string& filePath, uint64_t offset)
{
    Close();

    m_pFile = fopen(filePath.c_str(), "rb");
    if (m_pFile == NULL)
    {
        return false;
    }
    m_Offset = offset;

    uint8_t header[SMK_HEADER_SIZE];
    if (!ReadAt(0, header, sizeof(header)))
    {
        Close();
        return false;
    }

    if (memcmp(header, "SMK2", 4) == 0)
    {
        m_bIsVersion4 = false;
    }
    else if (memcmp(header, "SMK4", 4) == 0)
    {
        m_bIsVersion4 = true;
    }
    else
    {
        Close();
        return false;
    }

    m_Width = (int)ReadLE32(header + 4);
    m_Height = (int)ReadLE32(header + 8);
    m_FrameCount = (int)ReadLE32(header + 12);
    int32_t frameRate = (int32_t)ReadLE32(header + 16);
    m_Flags = ReadLE32(header + 20);
    uint32_t treesSize = ReadLE32(header + 52);
    m_AudioFlags = ReadLE32(header + 72);

    if (m_Width <= 0 || m_Height <= 0 || m_Width > 4096 || m_Height > 4096 ||
        m_FrameCount <= 0 || m_FrameCount > 1000000)
    {
        Close();
        return false;
    }

    // Positive frame rate is in milliseconds per frame, negative in 10s of microseconds
    if (frameRate > 0)
    {
        m_Fps = 1000.0 / frameRate;
    }
    else if (frameRate < 0)
    {
        m_Fps = 100000.0 / -frameRate;
    }
    else
    {
        m_Fps = 10.0;
    }

    // Audio track 0: bit 30 - has data, bit 29 - 16 bit, bit 28 - stereo, bits 0-23 - rate
    if ((m_AudioFlags & (1u << 30)) && (m_AudioFlags & 0xFFFFFF))
    {
        // Bits 26-27 specify Bink audio compression which is not supported
        if ((m_AudioFlags >> 26) & 3)
        {
            m_AudioRate = 0;
        }
        else
        {
            m_AudioRate = (int)(m_AudioFlags & 0xFFFFFF);
            m_AudioBits = (m_AudioFlags & (1u << 29)) ? 16 : 8;
            m_AudioChannels = (m_AudioFlags & (1u << 28)) ? 2 : 1;
        }
    }

    // Ring frame is stored as additional frame at the end
    int storedFrameCount = m_FrameCount + ((m_Flags & 1) ? 1 : 0);

    std::vector<uint8_t> frameSizes(storedFrameCount * 4);
    m_FrameTypes.resize(storedFrameCount);
    std::vector<uint8_t> treeData(treesSize);
    if (!ReadAt(SMK_HEADER_SIZE, frameSizes.data(), frameSizes.size()) ||
        !ReadAt(SMK_HEADER_SIZE + frameSizes.size(), m_FrameTypes.data(), m_FrameTypes.size()) ||
        !ReadAt(SMK_HEADER_SIZE + frameSizes.size() + m_FrameTypes.size(), treeData.data(), treeData.size()))
    {
        Close();
        return false;
    }

    uint64_t frameOffset = SMK_HEADER_SIZE + frameSizes.size() + m_FrameTypes.size() + treeData.size();
    m_FrameSizes.resize(storedFrameCount);
    m_FrameOffsets.resize(storedFrameCount);
    for (int i = 0; i < storedFrameCount; i++)
    {
        // Lowest 2 bits are flags (keyframe)
        m_FrameSizes[i] = ReadLE32(&frameSizes[i * 4]) & ~3u;
        m_FrameOffsets[i] = frameOffset;
        frameOffset += m_FrameSizes[i];
    }

    if (!ReadHeaderTrees(treeData))
    {
        Close();
        return false;
    }

    m_Frame.assign(m_Width * m_Height, 0);
    memset(m_Palette, 0, sizeof(m_Palette));
    m_CurrentFrame = -1;

    return true;
}

//---------------------------------------------------------------------------------------------------------------------
// Huffman trees
//---------------------------------------------------------------------------------------------------------------------

bool SmackerDecoder::ReadByteTreeRecursive(BitReader& reader, HuffTree& tree, int depth)
{
    if (depth > MAX_TREE_DEPTH || reader.IsOverrun())
    {
        return false;
    }

    if (!reader.ReadBit())
    {
        tree.push_back(reader.ReadBits(8));
        return true;
    }

    size_t nodeIdx = tree.size();
    tree.push_back(HUFF_NODE);
    if (!ReadByteTreeRecursive(reader, tree, depth + 1))
    {
        return false;
    }
    tree[nodeIdx] = HUFF_NODE | (uint32_t)(tree.size() - nodeIdx - 1);

    return ReadByteTreeRecursive(reader, tree, depth + 1);
}

bool SmackerDecoder::ReadByteTree(BitReader& reader, HuffTree& tree)
{
    tree.clear();

    // Tree is not present - every decoded value is 0
    if (!reader.ReadBit())
    {
        tree.push_back(0);
        return true;
    }

    if (!ReadByteTreeRecursive(reader, tree, 0))
    {
        return false;
    }

    // Tree is terminated by a 0 bit
    reader.ReadBit();
    return true;
}

bool SmackerDecoder::ReadBigTreeRecursive(BitReader& reader, BigTree& bigTree, const HuffTree& lowTree,
    const HuffTree& highTree, const int escapes[3], int depth)
{
    if (depth > MAX_TREE_DEPTH || reader.IsOverrun())
    {
        return false;
    }

    HuffTree& tree = bigTree.tree;
    if (!reader.ReadBit())
    {
        uint32_t value = DecodeValue(reader, lowTree);
        value |= DecodeValue(reader, highTree) << 8;

        // Leaves with escape values become cache slots for recently decoded values
        int leafIdx = (int)tree.size();
        for (int i = 0; i < 3; i++)
        {
            if ((int)value == escapes[i])
            {
                bigTree.last[i] = leafIdx;
                value = 0;
                break;
            }
        }

        tree.push_back(value);
        return true;
    }

    size_t nodeIdx = tree.size();
    tree.push_back(HUFF_NODE);
    if (!ReadBigTreeRecursive(reader, bigTree, lowTree, highTree, escapes, depth + 1))
    {
        return false;
    }
    tree[nodeIdx] = HUFF_NODE | (uint32_t)(tree.size() - nodeIdx - 1);

    return ReadBigTreeRecursive(reader, bigTree, lowTree, highTree, escapes, depth + 1);
}

bool SmackerDecoder::ReadBigTree(BitReader& reader, BigTree& bigTree)
{
    bigTree.tree.clear();

    // Tree is not present
    if (!reader.ReadBit())
    {
        bigTree.tree.push_back(0);
        bigTree.last[0] = bigTree.last[1] = bigTree.last[2] = 0;
        return true;
    }

    HuffTree lowTree, highTree;
    if (!ReadByteTree(reader, lowTree) || !ReadByteTree(reader, highTree))
    {
        return false;
    }

    int escapes[3];
    for (int i = 0; i < 3; i++)
    {
        escapes[i] = (int)reader.ReadBits(16);
        bigTree.last[i] = -1;
    }

    if (!ReadBigTreeRecursive(reader, bigTree, lowTree, highTree, escapes, 0))
    {
        return false;
    }

    // Tree is terminated by a 0 bit
    reader.ReadBit();

    // Escape values which did not occur in the tree still need their cache slots
    for (int i = 0; i < 3; i++)
    {
        if (bigTree.last[i] == -1)
        {
            bigTree.last[i] = (int)bigTree.tree.size();
            bigTree.tree.push_back(0);
        }
    }

    return !reader.IsOverrun();
}

bool SmackerDecoder::ReadHeaderTrees(const std::vector<uint8_t>& treeData)
{
    BitReader reader(treeData.data(), treeData.size());

    return ReadBigTree(reader, m_MMapTree) &&
        ReadBigTree(reader, m_MClrTree) &&
        ReadBigTree(reader, m_FullTree) &&
        ReadBigTree(reader, m_TypeTree);
}

uint32_t SmackerDecoder::DecodeValue(BitReader& reader, const HuffTree& tree)
{
    size_t idx = 0;
    while (tree[idx] & HUFF_NODE)
    {
        if (reader.ReadBit())
        {
            // Skip left subtree
            idx += tree[idx] & ~HUFF_NODE;
        }
        idx++;
    }

    return tree[idx];
}

uint32_t SmackerDecoder::DecodeBigValue(BitReader& reader, BigTree& bigTree)
{
    HuffTree& tree = bigTree.tree;
    uint32_t value = DecodeValue(reader, tree);

    // Maintain cache of 3 recently decoded values
    if (value != tree[bigTree.last[0]])
    {
        tree[bigTree.last[2]] = tree[bigTree.last[1]];
        tree[bigTree.last[1]] = tree[bigTree.last[0]];
        tree[bigTree.last[0]] = value;
    }

    return value;
}

void SmackerDecoder::ResetBigTreeCache(BigTree& bigTree)
{
    for (int i = 0; i < 3; i++)
    {
        bigTree.tree[bigTree.last[i]] = 0;
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Frame decoding
//---------------------------------------------------------------------------------------------------------------------

bool SmackerDecoder::DecodeNextFrame()
{
    if (!IsOpen() || m_CurrentFrame + 1 >= m_FrameCount)
    {
        return false;
    }

    int frameIdx = m_CurrentFrame + 1;
    uint32_t frameSize = m_FrameSizes[frameIdx];
    m_FrameBuffer.resize(frameSize);
    if (frameSize > 0 && !ReadAt(m_FrameOffsets[frameIdx], m_FrameBuffer.data(), frameSize))
    {
        return false;
    }

    m_CurrentFrame = frameIdx;

    const uint8_t* pData = m_FrameBuffer.data();
    size_t remaining = frameSize;
    uint8_t frameType = m_FrameTypes[frameIdx];

    if ((frameType & 1) && remaining > 0)
    {
        size_t paletteSize = (size_t)pData[0] * 4;
        if (paletteSize == 0 || paletteSize > remaining)
        {
            return false;
        }

        DecodePalette(pData + 1, paletteSize - 1);
        pData += paletteSize;
        remaining -= paletteSize;
    }

    // Audio is decoded separately, skip it
    for (int track = 0; track < SMK_MAX_AUDIO_TRACKS; track++)
    {
        if (frameType & (2 << track))
        {
            if (remaining < 4)
            {
                return false;
            }

            uint32_t chunkSize = ReadLE32(pData);
            if (chunkSize < 4 || chunkSize > remaining)
            {
                return false;
            }

            pData += chunkSize;
            remaining -= chunkSize;
        }
    }

    DecodeVideo(pData, remaining);

    return true;
}

void SmackerDecoder::DecodePalette(const uint8_t* pData, size_t size)
{
    uint8_t oldPalette[256 * 3];
    memcpy(oldPalette, m_Palette, sizeof(oldPalette));

    const uint8_t* pEnd = pData + size;
    int colorIdx = 0;
    while (colorIdx < 256 && pData < pEnd)
    {
        uint8_t cmd = *pData++;
        if (cmd & 0x80)
        {
            // Skip (keep) colors
            colorIdx += (cmd & 0x7F) + 1;
        }
        else if (cmd & 0x40)
        {
            // Copy colors from old palette
            if (pData >= pEnd)
            {
                break;
            }

            int srcIdx = *pData++;
            int count = (cmd & 0x3F) + 1;
            if (srcIdx + count > 256)
            {
                break;
            }

            while (count-- > 0 && colorIdx < 256)
            {
                memcpy(&m_Palette[colorIdx * 3], &oldPalette[srcIdx * 3], 3);
                colorIdx++;
                srcIdx++;
            }
        }
        else
        {
            // New color
            if (pData + 2 > pEnd)
            {
                break;
            }

            m_Palette[colorIdx * 3 + 0] = g_PaletteMap[cmd & 0x3F];
            m_Palette[colorIdx * 3 + 1] = g_PaletteMap[*pData++ & 0x3F];
            m_Palette[colorIdx * 3 + 2] = g_PaletteMap[*pData++ & 0x3F];
            colorIdx++;
        }
    }
}

void SmackerDecoder::DecodeVideo(const uint8_t* pData, size_t size)
{
    BitReader reader(pData, size);

    ResetBigTreeCache(m_MMapTree);
    ResetBigTreeCache(m_MClrTree);
    ResetBigTreeCache(m_FullTree);
    ResetBigTreeCache(m_TypeTree);

    const int stride = m_Width;
    const int blocksPerRow = m_Width / 4;
    const int blockCount = blocksPerRow * (m_Height / 4);
    uint8_t* pFrame = m_Frame.data();

    int block = 0;
    while (block < blockCount && !reader.IsOverrun())
    {
        uint32_t type = DecodeBigValue(reader, m_TypeTree);
        int run = g_BlockRuns[(type >> 2) & 0x3F];

        switch (type & 3)
        {
            case SmackerBlock_Mono:
            {
                for (; run > 0 && block < blockCount; run--, block++)
                {
                    uint32_t colors = DecodeBigValue(reader, m_MClrTree);
                    uint32_t map = DecodeBigValue(reader, m_MMapTree);
                    uint8_t high = (uint8_t)(colors >> 8);
                    uint8_t low = (uint8_t)(colors & 0xFF);

                    uint8_t* pOut = pFrame + (block / blocksPerRow) * stride * 4 + (block % blocksPerRow) * 4;
                    for (int y = 0; y < 4; y++)
                    {
                        pOut[0] = (map & 1) ? high : low;
                        pOut[1] = (map & 2) ? high : low;
                        pOut[2] = (map & 4) ? high : low;
                        pOut[3] = (map & 8) ? high : low;
                        map >>= 4;
                        pOut += stride;
                    }
                }
                break;
            }

            case SmackerBlock_Full:
            {
                int mode = 0;
                if (m_bIsVersion4)
                {
                    if (reader.ReadBit())
                    {
                        mode = 1;
                    }
                    else if (reader.ReadBit())
                    {
                        mode = 2;
                    }
                }

                for (; run > 0 && block < blockCount; run--, block++)
                {
                    uint8_t* pOut = pFrame + (block / blocksPerRow) * stride * 4 + (block % blocksPerRow) * 4;
                    if (mode == 0)
                    {
                        for (int y = 0; y < 4; y++)
                        {
                            uint32_t pixels = DecodeBigValue(reader, m_FullTree);
                            pOut[2] = (uint8_t)(pixels & 0xFF);
                            pOut[3] = (uint8_t)(pixels >> 8);
                            pixels = DecodeBigValue(reader, m_FullTree);
                            pOut[0] = (uint8_t)(pixels & 0xFF);
                            pOut[1] = (uint8_t)(pixels >> 8);
                            pOut += stride;
                        }
                    }
                    else if (mode == 1)
                    {
                        // Double width and height
                        for (int half = 0; half < 2; half++)
                        {
                            uint32_t pixels = DecodeBigValue(reader, m_FullTree);
                            for (int y = 0; y < 2; y++)
                            {
                                pOut[0] = pOut[1] = (uint8_t)(pixels & 0xFF);
                                pOut[2] = pOut[3] = (uint8_t)(pixels >> 8);
                                pOut += stride;
                            }
                        }
                    }
                    else
                    {
                        // Double height
                        for (int half = 0; half < 2; half++)
                        {
                            uint32_t pixels2 = DecodeBigValue(reader, m_FullTree);
                            uint32_t pixels1 = DecodeBigValue(reader, m_FullTree);
                            for (int y = 0; y < 2; y++)
                            {
                                pOut[0] = (uint8_t)(pixels1 & 0xFF);
                                pOut[1] = (uint8_t)(pixels1 >> 8);
                                pOut[2] = (uint8_t)(pixels2 & 0xFF);
                                pOut[3] = (uint8_t)(pixels2 >> 8);
                                pOut += stride;
                            }
                        }
                    }
                }
                break;
            }

            case SmackerBlock_Skip:
            {
                block += run;
                break;
            }

            case SmackerBlock_Fill:
            {
                uint8_t color = (uint8_t)(type >> 8);
                for (; run > 0 && block < blockCount; run--, block++)
                {
                    uint8_t* pOut = pFrame + (block / blocksPerRow) * stride * 4 + (block % blocksPerRow) * 4;
                    for (int y = 0; y < 4; y++)
                    {
                        memset(pOut, color, 4);
                        pOut += stride;
                    }
                }
                break;
            }
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
// Audio decoding
//---------------------------------------------------------------------------------------------------------------------

bool SmackerDecoder::DecodeFrameAudio(int frameIdx, std::vector<uint8_t>& outPcm)
{
    if (!IsOpen() || !HasAudio() || frameIdx < 0 || frameIdx >= m_FrameCount)
    {
        return false;
    }

    uint8_t frameType = m_FrameTypes[frameIdx];
    if (!(frameType & 2))
    {
        return true;
    }

    uint64_t position = m_FrameOffsets[frameIdx];
    uint32_t frameSize = m_FrameSizes[frameIdx];
    uint32_t consumed = 0;

    if (frameType & 1)
    {
        uint8_t paletteSize;
        if (!ReadAt(position, &paletteSize, 1))
        {
            return false;
        }
        consumed += (uint32_t)paletteSize * 4;
    }

    uint8_t chunkSizeData[4];
    if (consumed + 4 > frameSize || !ReadAt(position + consumed, chunkSizeData, 4))
    {
        return false;
    }

    uint32_t chunkSize = ReadLE32(chunkSizeData);
    if (chunkSize < 4 || consumed + chunkSize > frameSize)
    {
        return false;
    }

    std::vector<uint8_t> chunk(chunkSize - 4);
    if (!chunk.empty() && !ReadAt(position + consumed + 4, chunk.data(), chunk.size()))
    {
        return false;
    }

    return DecodeAudioChunk(chunk.data(), chunk.size(), outPcm);
}

bool SmackerDecoder::DecodeAudioChunk(const uint8_t* pData, size_t size, std::vector<uint8_t>& outPcm)
{
    // Uncompressed audio
    if (!(m_AudioFlags & (1u << 31)))
    {
        outPcm.insert(outPcm.end(), pData, pData + size);
        return true;
    }

    if (size < 4)
    {
        return false;
    }

    uint32_t unpackedSize = ReadLE32(pData);
    BitReader reader(pData + 4, size - 4);

    // No data in this chunk
    if (!reader.ReadBit())
    {
        return true;
    }

    int stereo = (int)reader.ReadBit();
    int is16Bit = (int)reader.ReadBit();
    if ((stereo + 1) != m_AudioChannels || (is16Bit ? 16 : 8) != m_AudioBits)
    {
        return false;
    }

    // 8-bit: 1 tree per channel, 16-bit: 2 trees (low and high byte) per channel
    HuffTree trees[4];
    int treeCount = 1 << (is16Bit + stereo);
    for (int i = 0; i < treeCount; i++)
    {
        if (!ReadByteTree(reader, trees[i]))
        {
            return false;
        }
    }

    size_t outStart = outPcm.size();
    outPcm.resize(outStart + unpackedSize);
    uint8_t* pOut = outPcm.data() + outStart;

    if (is16Bit)
    {
        uint32_t sampleCount = unpackedSize / 2;
        uint16_t predictors[2] = { 0, 0 };
        for (int ch = stereo; ch >= 0; ch--)
        {
            uint16_t high = (uint16_t)reader.ReadBits(8);
            uint16_t low = (uint16_t)reader.ReadBits(8);
            predictors[ch] = (uint16_t)((high << 8) | low);
        }

        uint32_t sampleIdx = 0;
        for (int ch = 0; ch <= stereo && sampleIdx < sampleCount; ch++, sampleIdx++)
        {
            pOut[sampleIdx * 2] = (uint8_t)(predictors[ch] & 0xFF);
            pOut[sampleIdx * 2 + 1] = (uint8_t)(predictors[ch] >> 8);
        }

        for (; sampleIdx < sampleCount; sampleIdx++)
        {
            int ch = (sampleIdx & stereo) ? 1 : 0;
            uint16_t delta = (uint16_t)DecodeValue(reader, trees[ch * 2]);
            delta |= (uint16_t)(DecodeValue(reader, trees[ch * 2 + 1]) << 8);

            // Wraparound is intended
            predictors[ch] = (uint16_t)(predictors[ch] + delta);
            pOut[sampleIdx * 2] = (uint8_t)(predictors[ch] & 0xFF);
            pOut[sampleIdx * 2 + 1] = (uint8_t)(predictors[ch] >> 8);
        }
    }
    else
    {
        uint8_t predictors[2] = { 0, 0 };
        for (int ch = stereo; ch >= 0; ch--)
        {
            predictors[ch] = (uint8_t)reader.ReadBits(8);
        }

        uint32_t sampleIdx = 0;
        for (int ch = 0; ch <= stereo && sampleIdx < unpackedSize; ch++, sampleIdx++)
        {
            pOut[sampleIdx] = predictors[ch];
        }

        for (; sampleIdx < unpackedSize; sampleIdx++)
        {
            int ch = (sampleIdx & stereo) ? 1 : 0;
            predictors[ch] = (uint8_t)(predictors[ch] + DecodeValue(reader, trees[ch]));
            pOut[sampleIdx] = predictors[ch];
        }
    }

    return true;
}
