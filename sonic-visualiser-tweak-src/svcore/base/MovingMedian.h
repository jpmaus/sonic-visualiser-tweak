/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2007-2015 Particular Programs Ltd, 
    copyright 2018 Queen Mary University of London.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_MOVING_MEDIAN_H
#define SV_MOVING_MEDIAN_H

#include <bqvec/Allocators.h>
#include <bqvec/VectorOps.h>

#include <algorithm>
#include <iostream>
#include <stdexcept>

/**
 * Obtain the median (or other percentile) of a moving window across a
 * time series. Construct the MovingMedian object, then push() each
 * new value in the time series and get() the median of the most
 * recent window. The size of the window, and the percentile
 * calculated, can both be changed after construction.
 *
 * Note that for even-sized windows, the "median" is taken to be the
 * value at the start of the second half when sorted, e.g. for size 4,
 * the element at index 2 (zero-based) in the sorted window.
 *
 * Not thread-safe.
 */
template <typename T>
class MovingMedian
{
public:
    MovingMedian(int size, double percentile = 50.f) :
        m_size(size),
        m_percentile(percentile) {
        if (size < 1) throw std::logic_error("size must be >= 1");
        m_frame = breakfastquay::allocate_and_zero<T>(size);
	m_sorted = breakfastquay::allocate_and_zero<T>(size);
        calculateIndex();
    }

    ~MovingMedian() { 
        breakfastquay::deallocate(m_frame);
        breakfastquay::deallocate(m_sorted);
    }

    MovingMedian(const MovingMedian &) =delete;
    MovingMedian &operator=(const MovingMedian &) =delete;

    void setPercentile(double p) {
        m_percentile = p;
        calculateIndex();
    }

    void push(T value) {
        if (value != value) {
            std::cerr << "WARNING: MovingMedian: NaN encountered" << std::endl;
            value = T();
        }
	drop(m_frame[0]);
        breakfastquay::v_move(m_frame, m_frame+1, m_size-1);
	m_frame[m_size-1] = value;
	put(value);
    }

    T get() const {
	return m_sorted[m_index];
    }

    int size() const {
        return m_size;
    }

    void reset() {
        breakfastquay::v_zero(m_frame, m_size);
        breakfastquay::v_zero(m_sorted, m_size);
    }

    void resize(int target) {
        if (target == m_size) return;
        int diff = std::abs(target - m_size);
        if (target > m_size) { // grow
            // we don't want to change the median, so fill spaces with it
            T fillValue = get();
            m_frame = breakfastquay::reallocate(m_frame, m_size, target);
            breakfastquay::v_move(m_frame + diff, m_frame, m_size);
            breakfastquay::v_set(m_frame, fillValue, diff);
            m_sorted = breakfastquay::reallocate(m_sorted, m_size, target);
            for (int sz = m_size + 1; sz <= target; ++sz) {
                put(m_sorted, sz, fillValue);
            }
        } else { // shrink
            for (int i = 0; i < diff; ++i) {
                drop(m_sorted, m_size - i, m_frame[i]);
            }
            m_sorted = breakfastquay::reallocate(m_sorted, m_size, target);
            breakfastquay::v_move(m_frame, m_frame + diff, target);
            m_frame = breakfastquay::reallocate(m_frame, m_size, target);
        }

        m_size = target;
        calculateIndex();
    }

    void checkIntegrity() const {
        check();
    }

private:
    int m_size;
    double m_percentile;
    int m_index;
    T *m_frame;
    T *m_sorted;

    void calculateIndex() {
        m_index = int((m_size * m_percentile) / 100.f);
        if (m_index >= m_size) m_index = m_size-1;
        if (m_index < 0) m_index = 0;
    }
    
    void put(T value) {
        put(m_sorted, m_size, value);
    }

    static void put(T *const sorted, int size, T value) {

        // precondition: sorted points to size-1 sorted values,
        // followed by an unused slot (i.e. only the first size-1
        // values of sorted are actually sorted)
        // 
        // postcondition: sorted points to size sorted values
        
	T *ptr = std::lower_bound(sorted, sorted + size - 1, value);
        breakfastquay::v_move(ptr + 1, ptr, int(sorted + size - ptr) - 1);
	*ptr = value;
    }

    void drop(T value) {
        drop(m_sorted, m_size, value);
    }

    static void drop(T *const sorted, int size, T value) {

        // precondition: sorted points to size sorted values, one of
        // which is value
        //
        // postcondition: sorted points to size-1 sorted values,
        // followed by a slot that has been reset to default value
        // (i.e. only the first size-1 values of sorted are actually
        // sorted)

	T *ptr = std::lower_bound(sorted, sorted + size, value);
	if (*ptr != value) {
            throw std::logic_error
                ("MovingMedian::drop: value being dropped is not in array");
        }
        breakfastquay::v_move(ptr, ptr + 1, int(sorted + size - ptr) - 1);
        sorted[size-1] = T();
    }

    void check() const {
        bool good = true;
        for (int i = 1; i < m_size; ++i) {
            if (m_sorted[i] < m_sorted[i-1]) {
                std::cerr << "ERROR: MovingMedian::checkIntegrity: "
                          << "mis-ordered elements in sorted array starting "
                          << "at index " << i << std::endl;
                good = false;
                break;
            }
        }
        for (int i = 0; i < m_size; ++i) {
            bool found = false;
            for (int j = 0; j < m_size; ++j) {
                if (m_sorted[j] == m_frame[i]) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                std::cerr << "ERROR: MovingMedian::checkIntegrity: "
                          << "element in frame at index " << i
                          << " not found in sorted array" << std::endl;
                good = false;
                break;
            }
        }
        for (int i = 0; i < m_size; ++i) {
            bool found = false;
            for (int j = 0; j < m_size; ++j) {
                if (m_sorted[i] == m_frame[j]) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                std::cerr << "ERROR: MovingMedian::checkIntegrity: "
                          << "element in sorted array at index " << i
                          << " not found in source frame" << std::endl;
                good = false;
                break;
            }
        }
        if (!good) {
            std::cerr << "Frame contains:" << std::endl;
            std::cerr << "[ ";
            for (int j = 0; j < m_size; ++j) {
                std::cerr << m_frame[j] << " ";
            }
            std::cerr << "]" << std::endl;
            std::cerr << "Sorted array contains:" << std::endl;
            std::cerr << "[ ";
            for (int j = 0; j < m_size; ++j) {
                std::cerr << m_sorted[j] << " ";
            }
            std::cerr << "]" << std::endl;
            throw std::logic_error("MovingMedian failed integrity check");
        }
    }
};

#endif

