#pragma once

#include "SqTime.h"

// #define _TIME_DRAWING
class DrawTimer
{
public:
    void start()
    {
        thisTime = SqTime::seconds();
    }
    void stop()
    {
        double elapsed = SqTime::seconds() - thisTime;
        hits[index++] = elapsed;
        if (index >= frames) {
            dump();
        }
    }
    DrawTimer(const char* n)
    {
        name = n;
    }
private:
    static const int frames = 400;
    double hits[frames];
    int index = 0;
    std::string name;

    double thisTime;

    void dump()
    {
        index = 0;
        double sum = 0;
        for (double x : hits) {
            sum += x;
        }
        const double avg = sum / frames;

        double sumDevSq = 0;
         for (double x : hits) {
             double dev = (x - avg);
             sumDevSq += dev * dev;
        }
        double stdDev = std::sqrt( sumDevSq / double(frames));

        // Our "quota" is 1% of the frame draw time
        double quotaSeconds = (1.0 / 60.0) * .01;
        double million = 1000000.0;
        printf("%s: avg = %f, stddev = %f (us) Quota frac=%f\n",
            name.c_str(),
            avg * million,
            stdDev * million,
            avg / quotaSeconds); 
        fflush(stdout);
    }
};


class DrawLocker
{
public:
    DrawLocker() = delete;
    DrawLocker(DrawTimer& t) : dt(t)
    {
        dt.start();
    }
    ~DrawLocker()
    {
        dt.stop();
    }
private:
    DrawTimer& dt;
};
