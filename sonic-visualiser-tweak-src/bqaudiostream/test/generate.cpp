/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#include "AudioStreamTestData.h"

#include <iostream>

#include <cstdlib>

using namespace Turbot;
using namespace std;

int main(int argc, char **argv)
{
    cerr << endl;

    if (argc != 4) {
	cerr << "Usage: " << argv[0] << " <rate> <channels> <outfile.wav>" << endl;
	cerr << "Supported outfile extensions: ";
	QStringList exts = AudioWriteStreamFactory::getSupportedFileExtensions();
	foreach (QString e, exts) cerr << e << " ";
	cerr << endl;
	return 2;
    }

    float rate = atof(argv[1]);
    int channels = atoi(argv[2]);
    QString filename = argv[3];

    cerr << "Sample rate: " << rate << endl;
    cerr << "Channel count: " << channels << endl;
    cerr << "Output filename: " << filename << endl;
    
    if (rate < 1 || rate > 1e6) {
	cerr << "ERROR: Crazy rate " << rate << " (try somewhere between 1 and a million)" << endl;
	return 2;
    }

    if (channels < 1 || channels > 20) {
	cerr << "ERROR: Crazy channel count " << channels << " (try somewhere between 1 and 20)" << endl;
	return 2;
    }

    AudioStreamTestData td(rate, channels);
    try {
	td.writeToFile(filename);
    } catch (std::exception &e) {
	std::cerr << "Failed to write test data to output file \"" << filename << "\": " << e.what() << std::endl;
	return 1;
    }

    return 0;
}

