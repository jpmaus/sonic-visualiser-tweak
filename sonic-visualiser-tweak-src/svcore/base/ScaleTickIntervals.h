/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006-2017 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_SCALE_TICK_INTERVALS_H
#define SV_SCALE_TICK_INTERVALS_H

#include <string>
#include <vector>
#include <cmath>

#include "LogRange.h"
#include "Debug.h"

// Can't have this on by default, as we're called on every refresh
//#define DEBUG_SCALE_TICK_INTERVALS 1

class ScaleTickIntervals
{
public:
    struct Range {
        double min;        // start of value range
        double max;        // end of value range
        int n;             // number of divisions (approximate only)
    };

    struct Tick {
        double value;      // value this tick represents
        std::string label; // value as written 
    };

    typedef std::vector<Tick> Ticks;

    /**
     * Return a set of ticks that divide the range r linearly into
     * roughly r.n equal divisions, in such a way as to yield
     * reasonably human-readable labels.
     */
    static Ticks linear(Range r) {
        return linearTicks(r);
    }

    /**
     * Return a set of ticks that divide the range r into roughly r.n
     * logarithmic divisions, in such a way as to yield reasonably
     * human-readable labels.
     */
    static Ticks logarithmic(Range r) {
        LogRange::mapRange(r.min, r.max);
        return logarithmicAlready(r);
    }

    /**
     * Return a set of ticks that divide the range r into roughly r.n
     * logarithmic divisions, on the asssumption that r.min and r.max
     * already represent the logarithms of the boundary values rather
     * than the values themselves.
     */
    static Ticks logarithmicAlready(Range r) {
        return logTicks(r);
    }
    
private:
    enum Display {
        Fixed,
        Scientific,
        Auto
    };
    
    struct Instruction {
        double initial;    // value of first tick
        double limit;      // max from original range
        double spacing;    // increment between ticks
        double roundTo;    // what all displayed values should be rounded to
                           // (if 0.0, then calculate based on precision)
        Display display;   // whether to use fixed precision (%e, %f, or %g)
        int precision;     // number of dp (%f) or sf (%e)
        bool logUnmap;     // true if values represent logs of display values
    };
    
    static Instruction linearInstruction(Range r)
    {
        Display display = Auto;

        if (r.max < r.min) {
            return linearInstruction({ r.max, r.min, r.n });
        }
        if (r.n < 1 || r.max == r.min) {
            return { r.min, r.min, 1.0, r.min, display, 1, false };
        }
        
        double inc = (r.max - r.min) / r.n;

        double digInc = log10(inc);
        double digMax = log10(fabs(r.max));
        double digMin = log10(fabs(r.min));

        int precInc = int(floor(digInc));
        double roundTo = pow(10.0, precInc);

        if (precInc > -4 && precInc < 4) {
            display = Fixed;
        } else if ((digMax >= -2.0 && digMax <= 3.0) &&
                   (digMin >= -3.0 && digMin <= 3.0)) {
            display = Fixed;
        } else {
            display = Scientific;
        }
        
        int precRange = int(ceil(digMax - digInc));

        int prec = 1;
        
        if (display == Fixed) {
            if (digInc < 0) {
                prec = -precInc;
            } else {
                prec = 0;
            }
        } else {
            prec = precRange;
        }

#ifdef DEBUG_SCALE_TICK_INTERVALS
        SVDEBUG << "ScaleTickIntervals: calculating linearInstruction" << endl
                << "ScaleTickIntervals: min = " << r.min << ", max = " << r.max
                << ", n = " << r.n << ", inc = " << inc << endl;
        SVDEBUG << "ScaleTickIntervals: digMax = " << digMax
                << ", digInc = " << digInc << endl;
        SVDEBUG << "ScaleTickIntervals: display = " << display
                << ", inc = " << inc << ", precInc = " << precInc
                << ", precRange = " << precRange
                << ", prec = " << prec << ", roundTo = " << roundTo
                << endl;
#endif

        double min = r.min;
        
        if (roundTo != 0.0) {
            // Round inc to the nearest multiple of roundTo, and min
            // to the next multiple of roundTo up. The small offset of
            // eps is included to avoid inc of 2.49999999999 rounding
            // to 2 or a min of -0.9999999999 rounding to 0, both of
            // which would prevent some of our test cases from getting
            // the most natural results.
            double eps = 1e-7;
            inc = round(inc / roundTo + eps) * roundTo;
            if (inc < roundTo) inc = roundTo;
            min = ceil(min / roundTo - eps) * roundTo;
            if (min > r.max) min = r.max;
            if (min == -0.0) min = 0.0;
#ifdef DEBUG_SCALE_TICK_INTERVALS
            SVDEBUG << "ScaleTickIntervals: rounded inc to " << inc
                    << " and min to " << min << endl;
#endif
        }

        if (display == Scientific && min != 0.0) {
            double digNewMin = log10(fabs(min));
            if (digNewMin < digInc) {
                prec = int(ceil(digMax - digNewMin));
#ifdef DEBUG_SCALE_TICK_INTERVALS
                SVDEBUG << "ScaleTickIntervals: min is smaller than increment, adjusting prec to " << prec << endl;
#endif
            }
        }
        
        return { min, r.max, inc, roundTo, display, prec, false };
    }
    
    static Instruction logInstruction(Range r)
    {
        Display display = Auto;

#ifdef DEBUG_SCALE_TICK_INTERVALS
        SVDEBUG << "ScaleTickIntervals::logInstruction: Range is "
                << r.min << " to " << r.max << endl;
#endif
        
        if (r.n < 1) {
            return {};
        }
        if (r.max < r.min) {
            return logInstruction({ r.max, r.min, r.n });
        }
        if (r.max == r.min) {
            return { r.min, r.max, 1.0, r.min, display, 1, true };
        }
        
        double inc = (r.max - r.min) / r.n;

#ifdef DEBUG_SCALE_TICK_INTERVALS
        SVDEBUG << "ScaleTickIntervals::logInstruction: "
                << "Naive increment is " << inc << endl;
#endif

        int precision = 1;

        if (inc < 1.0) {
            precision = int(ceil(1.0 - inc)) + 1;
        }

        double digInc = log10(inc);
        int precInc = int(floor(digInc));
        double roundIncTo = pow(10.0, precInc);

        inc = round(inc / roundIncTo) * roundIncTo;
        if (inc < roundIncTo) inc = roundIncTo;

#ifdef DEBUG_SCALE_TICK_INTERVALS
        SVDEBUG << "ScaleTickIntervals::logInstruction: "
                << "Rounded increment to " << inc << endl;
#endif

        // if inc is close to giving us powers of two, nudge it
        if (fabs(inc - 0.301) < 0.01) {
            inc = log10(2.0);

#ifdef DEBUG_SCALE_TICK_INTERVALS
            SVDEBUG << "ScaleTickIntervals::logInstruction: "
                    << "Nudged increment to " << inc << " to get powers of two"
                    << endl;
#endif
        }

        double min = r.min;
        if (inc != 0.0) {
            min = ceil(r.min / inc) * inc;
            if (min > r.max) min = r.max;
        }

        return { min, r.max, inc, 0.0, display, precision, true };
    }

    static Ticks linearTicks(Range r) {
        Instruction instruction = linearInstruction(r);
        Ticks ticks = explode(instruction);
        return ticks;
    }

    static Ticks logTicks(Range r) {
        Instruction instruction = logInstruction(r);
        Ticks ticks = explode(instruction);
        return ticks;
    }
    
    static Tick makeTick(Display display, int precision, double value) {

        if (value == -0.0) {
            value = 0.0;
        }
        
        const int buflen = 40;
        char buffer[buflen];

        if (display == Auto) {

            double eps = 1e-7;
            
            int digits = (value != 0.0 ?
                          1 + int(floor(eps + log10(fabs(value)))) :
                          0);

#ifdef DEBUG_SCALE_TICK_INTERVALS
            SVDEBUG << "makeTick: display = Auto, precision = "
                    << precision << ", value = " << value
                    << ", resulting digits = " << digits << endl;
#endif
            
            // This is not the same logic as %g uses for determining
            // whether to delegate to use scientific or fixed notation

            if (digits < -3 || digits > 4) {

                display = Auto; // delegate planning to %g

            } else {

                display = Fixed;
                
                // in %.*f, the * indicates decimal places, not sig figs
                if (precision >= digits) {
                    precision -= digits;
                } else {
                    precision = 0;
                }
            }
        }

        const char *spec = (display == Auto ? "%.*g" :
                            display == Scientific ? "%.*e" :
                            "%.*f");

#pragma GCC diagnostic ignored "-Wformat-nonliteral"
        
        snprintf(buffer, buflen, spec, precision, value);

#ifdef DEBUG_SCALE_TICK_INTERVALS
        SVDEBUG << "makeTick: spec = \"" << spec
                << "\", prec = " << precision << ", value = " << value
                << ", label = \"" << buffer << "\"" << endl;
#endif
        
        return Tick({ value, std::string(buffer) });
    }
    
    static Ticks explode(Instruction instruction) {

#ifdef DEBUG_SCALE_TICK_INTERVALS
        SVDEBUG << "ScaleTickIntervals::explode:" << endl
                << "initial = " << instruction.initial
                << ", limit = " << instruction.limit
                << ", spacing = " << instruction.spacing
                << ", roundTo = " << instruction.roundTo
                << ", display = " << instruction.display
                << ", precision = " << instruction.precision
                << ", logUnmap = " << instruction.logUnmap
                << endl;
#endif

        if (instruction.spacing == 0.0) {
            return {};
        }

        double eps = 1e-7;
        if (instruction.spacing < eps * 10.0) {
            eps = instruction.spacing / 10.0;
        }

        double max = instruction.limit;
        int n = 0;

        Ticks ticks;
        
        while (true) {

            double value = instruction.initial + n * instruction.spacing;

            if (value >= max + eps) {
                break;
            }

            if (instruction.logUnmap) {
                value = pow(10.0, value);
            }

            double roundTo = instruction.roundTo;

            if (roundTo == 0.0 && value != 0.0) {
                // We don't want the internal value secretly not
                // matching the displayed one
                roundTo =
                    pow(10, ceil(log10(fabs(value))) - instruction.precision);
            }
                                           
            if (roundTo != 0.0) {
                value = roundTo * round(value / roundTo);
            }

            if (fabs(value) < eps) {
                value = 0.0;
            }
            
            ticks.push_back(makeTick(instruction.display,
                                     instruction.precision,
                                     value));
            ++n;
        }

        return ticks;
    }
};

#endif
