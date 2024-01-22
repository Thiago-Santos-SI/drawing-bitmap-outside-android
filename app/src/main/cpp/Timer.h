#pragma once

#include <chrono>

struct Timer
{
    using clock_t = std::chrono::high_resolution_clock;
    using time_point_t = std::chrono::time_point<clock_t>;

    time_point_t mStartTime;
    time_point_t mStopTime;
    double mostRecentElapsed;

    void Start()
    {
        mostRecentElapsed = 0;
        mStartTime = clock_t::now();
    }

    void Stop()
    {
        mStopTime = clock_t::now();
        std::chrono::duration<double, std::milli> elapsedTime = mStopTime - mStartTime;
        mostRecentElapsed = elapsedTime.count();
    }
};