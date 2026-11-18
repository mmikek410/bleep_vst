#include "Pattern.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace bleep::seq
{

Pattern Pattern::makeDefault() noexcept
{
    Pattern p;
    // 16th grid: hits on 1, the "e" of 1, 2-and, 3, 4-e ... a classic rolling bleep.
    const int hits[][2] = { { 0, 0 }, { 3, 0 }, { 6, 7 }, { 8, 0 }, { 11, 12 }, { 14, 3 } };
    for (const auto& h : hits)
    {
        p.steps[(size_t) h[0]].active = true;
        p.steps[(size_t) h[0]].pitch  = h[1];
    }
    return p;
}

// Format: "P1;" then one record per step, separated by ';'
//   active,pitch,velocity(0-127),probability(0-100),gate(0-100),ratchet,locked
std::string Pattern::encode() const
{
    std::ostringstream os;
    os << "P1";
    for (const auto& s : steps)
    {
        os << ';' << (s.active ? 1 : 0) << ',' << s.pitch << ','
           << (int) std::lround (s.velocity * 127.0f) << ','
           << (int) std::lround (s.probability * 100.0f) << ','
           << (int) std::lround (s.gate * 100.0f) << ','
           << s.ratchet << ',' << (s.locked ? 1 : 0);
    }
    return os.str();
}

bool Pattern::decode (const std::string& text)
{
    if (text.rfind ("P1", 0) != 0)
        return false;

    Pattern result;  // missing trailing steps keep defaults (inactive)
    std::istringstream is (text.substr (2));
    std::string record;
    size_t index = 0;

    while (std::getline (is, record, ';'))
    {
        if (record.empty())
            continue;
        if (index >= result.steps.size())
            break;

        int a = 0, pitch = 0, vel = 102, prob = 100, gate = 100, ratchet = 1, lock = 0;
        const int n = std::sscanf (record.c_str(), "%d,%d,%d,%d,%d,%d,%d", &a, &pitch, &vel, &prob, &gate, &ratchet, &lock);
        if (n < 2)
            return false;

        auto& s = result.steps[index++];
        s.active      = a != 0;
        s.pitch       = std::clamp (pitch, kMinPitch, kMaxPitch);
        s.velocity    = std::clamp (vel, 0, 127) / 127.0f;
        s.probability = std::clamp (prob, 0, 100) / 100.0f;
        s.gate        = std::clamp (gate, 0, 100) / 100.0f;
        s.ratchet     = std::clamp (ratchet, 1, 4);
        s.locked      = lock != 0;
    }

    *this = result;
    return true;
}

} // namespace bleep::seq
