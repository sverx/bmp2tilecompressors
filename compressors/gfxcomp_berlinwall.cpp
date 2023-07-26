#include <cstdint>
#include <iterator>
#include <vector>
#include <ranges>

#include "utils.h"

extern "C" __declspec(dllexport) const char* getName()
{
    // A pretty name for this compression type
    // Generally, the name of the game it was REd from
    return "Berlin Wall LZ";
}

extern "C" __declspec(dllexport) const char* getExt()
{
    // A string suitable for use as a file extension
    return "berlinwallcompr";
}

class Bitstream
{
    std::vector<uint8_t> _buffer;
    int _bitCount = 8;
    std::vector<uint8_t>::reverse_iterator _it;

public:
    void addBit(int bit)
    {
        // If we have run out of space then emit a new byte to work on
        if (_bitCount == 8)
        {
            _buffer.push_back(0);
            _it = _buffer.rbegin();
            _bitCount = 0;
        }
        // Or into the end of the current byte, left to right
        auto b = *_it;
        b |= (bit << (7 - _bitCount));
        *_it = b;
        ++_bitCount;
    }

    void addBits(int byte, int bitCount)
    {
        for (auto i = 0; i < bitCount; ++i)
        {
            addBit((byte >> (bitCount - i - 1)) & 1);
        }
    }

    const std::vector<uint8_t>& getBuffer()
    {
        return _buffer;
    }
};

// The actual compressor function
int32_t compress(
    const uint8_t* pSource,
    const size_t sourceLength,
    uint8_t* pDestination,
    const size_t destinationLength)
{
    const auto source = Utils::toVector(pSource, sourceLength);
    Bitstream result;

    for (auto offset = 0; offset < static_cast<int>(source.size());) // increment in loop
    {
        // Look for the longest run in data before the current offset which matches the current data
        auto bestLzOffset = -1;
        auto bestLzLength = -1;

        // Look for an LZ run. This must be in the last 256 bytes but can run over the current point.
        const auto minLzOffset = std::max(0, offset - 256);
        const auto maxLzOffset = std::max(0, offset - 1);
        for (int i = minLzOffset; i <= maxLzOffset; ++i)
        {
            // Check for the match length at i compared to offset
            const auto maxMatchLength = std::min(17, static_cast<int>(source.size()) - offset);

            for (auto matchLength = 0; matchLength < maxMatchLength; ++matchLength)
            {
                if (source[i + matchLength] != source[offset + matchLength])
                {
                    // Mismatch; stop looking here
                    break;
                }
                // Else see if it's a new record
                // matchLength will be n here if we have matched n+1 bytes so far
                if (matchLength + 1 > bestLzLength)
                {
                    bestLzLength = matchLength + 1;
                    bestLzOffset = i;
                }
            }
            if (bestLzLength == maxMatchLength)
            {
                // No point searching for anything better, we already found the max
                break;
            }
        }

        // Did we find anything?
        if (bestLzLength >= 2)
        {
            // Yes: emit an LZ reference
            // A 0 bit in the bitstream
            result.addBit(0);
            // Then the absolute offset of the match in the buffer. This happens to just be the offset modulo 256. TODO test of course...
            result.addBits(bestLzOffset % 256, 8);
            // Then the length - 2 as 4 bits
            result.addBits(bestLzLength - 2, 4);
            // And move on
            offset += bestLzLength;
        }
        else
        {
            // No: emit a raw byte
            // A 0 bit in the bitstream
            result.addBit(0);
            // Then the raw byte
            result.addBits(source[offset], 8);
            // And move on
            ++offset;
        }
    }

    return Utils::copyToDestination(result.getBuffer(), pDestination, destinationLength);
}

extern "C" __declspec(dllexport) int32_t compressTiles(
    const uint8_t* pSource,
    const uint32_t numTiles,
    uint8_t* pDestination,
    const uint32_t destinationLength)
{
    return compress(pSource, numTiles * 32, pDestination, destinationLength);
}

extern "C" __declspec(dllexport) int32_t compressTilemap(
    const uint8_t* pSource,
    const uint32_t width,
    const uint32_t height,
    uint8_t* pDestination,
    const uint32_t destinationLength)
{
    // Compress tilemap
    return compress(pSource, width * height * 2, pDestination, destinationLength);
}
