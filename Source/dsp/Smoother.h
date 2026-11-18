#pragma once

#include <cmath>

namespace bleep::dsp
{

/** One-pole parameter smoother (~ time constant in seconds). Real-time safe. */
class Smoother
{
public:
    void prepare (double sampleRate, float timeSeconds) noexcept
    {
        const double t = timeSeconds > 0.0f ? timeSeconds : 0.01;
        coefficient = static_cast<float> (std::exp (-1.0 / (t * sampleRate)));
    }

    void setTarget (float newTarget) noexcept { target = newTarget; }
    void snapTo (float value) noexcept        { target = current = value; }

    float next() noexcept
    {
        current = target + (current - target) * coefficient;
        return current;
    }

    float getCurrent() const noexcept { return current; }

private:
    float coefficient = 0.0f;
    float current = 0.0f;
    float target = 0.0f;
};

} // namespace bleep::dsp
