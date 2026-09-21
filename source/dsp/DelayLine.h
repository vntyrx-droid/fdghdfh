#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace chorus
{

/** Fractional-delay circular buffer.

    The capacity is rounded up to a power of two so that wrapping is a single
    mask operation, and reads use 4-point Catmull-Rom interpolation. Cubic
    interpolation matters here: a chorus sweeps its read pointer continuously,
    and linear interpolation applies a fluctuating low-pass whose cutoff tracks
    the fractional part, which is audible as a dull, warbling high end.
*/
class DelayLine
{
public:
    /** Allocates capacity for at least maxDelaySamples of delay.
        Must be called before any read/write. Not real-time safe.
    */
    void prepare (int maxDelaySamples)
    {
        const std::size_t required = static_cast<std::size_t> (std::max (16, maxDelaySamples)) + 4;

        std::size_t capacity = 16;
        while (capacity < required)
            capacity <<= 1;

        buffer.assign (capacity, 0.0f);
        mask     = capacity - 1;
        writePos = 0;
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
    }

    /** Largest delay this line can produce, in samples. */
    float maxDelaySamples() const noexcept
    {
        return static_cast<float> (buffer.size()) - 3.0f;
    }

    void write (float sample) noexcept
    {
        buffer[writePos] = sample;
        writePos         = (writePos + 1) & mask;
    }

    /** Reads delaySamples behind the write head, clamped into the usable range. */
    float read (float delaySamples) const noexcept
    {
        delaySamples = std::clamp (delaySamples, 1.0f, maxDelaySamples());

        const float readPos = static_cast<float> (writePos) - delaySamples;
        const int   base    = static_cast<int> (std::floor (readPos));
        const float frac    = readPos - static_cast<float> (base);

        const float ym1 = at (base - 1);
        const float y0  = at (base);
        const float y1  = at (base + 1);
        const float y2  = at (base + 2);

        // Catmull-Rom, Horner form.
        const float c0 = y0;
        const float c1 = 0.5f * (y1 - ym1);
        const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
        const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);

        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

private:
    /** Index may be negative; the offset keeps the conversion to size_t defined. */
    float at (int index) const noexcept
    {
        const int wrapped = index + static_cast<int> (buffer.size());
        return buffer[static_cast<std::size_t> (wrapped) & mask];
    }

    std::vector<float> buffer;
    std::size_t        mask     = 0;
    std::size_t        writePos = 0;
};

} // namespace chorus
