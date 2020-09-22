/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "LogRange.h"
#include "system/System.h"

#include <algorithm>
#include <iostream>
#include <cmath>

void
LogRange::mapRange(double &min, double &max, double logthresh)
{
    static double eps = 1e-10;
    
    // ensure that max > min:
    if (min > max) std::swap(min, max);
    if (max == min) max = min + 1;

    if (min >= 0.0) {

        // and max > min, so we know min >= 0 and max > 0
        
        max = log10(max);

        if (min == 0.0) min = std::min(logthresh, max);
        else min = log10(min);

    } else if (max <= 0.0) {

        // and max > min, so we know min < 0 and max <= 0
        
        min = log10(-min);

        if (max == 0.0) max = std::min(logthresh, min);
        else max = log10(-max);
        
        std::swap(min, max);

    } else {
        
        // min < 0 and max > 0
        
        max = log10(std::max(max, -min));
        min = std::min(logthresh, max);
    }

    if (fabs(max - min) < eps) min = max - 1;
}        

double
LogRange::map(double value, double thresh)
{
    if (value == 0.0) return thresh;
    return log10(fabs(value));
}

double
LogRange::unmap(double value)
{
    return pow(10.0, value);
}

static double
sd(const std::vector<double> &values, int start, int n)
{
    double sum = 0.0, mean = 0.0, variance = 0.0;
    for (int i = 0; i < n; ++i) {
        sum += values[start + i];
    }
    mean = sum / n;
    for (int i = 0; i < n; ++i) {
        double diff = values[start + i] - mean;
        variance += diff * diff;
    }
    variance = variance / n;
    return sqrt(variance);
}

bool
LogRange::shouldUseLogScale(std::vector<double> values)
{
    // Principle: Partition the data into two sets around the median;
    // calculate the standard deviation of each set; if the two SDs
    // are very different, it's likely that a log scale would be good.

    int n = int(values.size());
    if (n < 4) return false;
    std::sort(values.begin(), values.end());
    int mi = n / 2;

    double sd0 = sd(values, 0, mi);
    double sd1 = sd(values, mi, n - mi);

    SVDEBUG << "LogRange::useLogScale: sd0 = "
              << sd0 << ", sd1 = " << sd1 << endl;

    if (sd0 == 0 || sd1 == 0) return false;

    // I wonder what method of determining "one sd much bigger than
    // the other" would be appropriate here...
    if (std::max(sd0, sd1) / std::min(sd0, sd1) > 10.) return true;
    else return false;
}
    
