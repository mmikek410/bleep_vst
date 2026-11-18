#pragma once

#include "seq/Pattern.h"

#include <juce_core/juce_core.h>

#include <atomic>
#include <cstdint>

/** The sequencer pattern shared between the UI/state code and the audio thread.

    Writers (editor, setStateInformation, later RANDOM/MUTATE) take a short
    spin lock. The audio thread only ever *tries* the lock, so it can never
    block: if a write is in progress it keeps playing its previous copy and
    picks the new one up on the next block. */
class PatternStore
{
public:
    PatternStore() : pattern (bleep::seq::Pattern::makeDefault()) {}

    bleep::seq::Pattern get() const
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        return pattern;
    }

    void set (const bleep::seq::Pattern& p)
    {
        {
            const juce::SpinLock::ScopedLockType sl (lock);
            pattern = p;
        }
        version.fetch_add (1, std::memory_order_release);
    }

    template <typename Fn>
    void modify (Fn&& fn)
    {
        {
            const juce::SpinLock::ScopedLockType sl (lock);
            fn (pattern);
        }
        version.fetch_add (1, std::memory_order_release);
    }

    std::uint32_t getVersion() const noexcept { return version.load (std::memory_order_acquire); }

    /** Audio thread: copies the pattern if it changed since `seenVersion`. Never blocks. */
    bool tryCopyIfChanged (bleep::seq::Pattern& dest, std::uint32_t& seenVersion) const noexcept
    {
        const auto v = getVersion();
        if (v == seenVersion)
            return false;

        const juce::SpinLock::ScopedTryLockType sl (lock);
        if (! sl.isLocked())
            return false;

        dest = pattern;
        seenVersion = v;
        return true;
    }

private:
    mutable juce::SpinLock lock;
    bleep::seq::Pattern pattern;
    std::atomic<std::uint32_t> version { 1 };
};
