#pragma once
#include<vector>
#include<iostream>
#include<fstream>
#include<JuceHeader.h>
//#include"ringbuffer.hpp"
using namespace juce;

class Sender : public AudioIODeviceCallback, private HighResolutionTimer {
public:

    Sender(int nbpf, int nspb);
    ~Sender();

    void setHeaderLen(int len);

    void setCarrierFreq(int freq);

    //int** getBitStream();

    void generateHeader();

    void Modulation(Array<int8_t> cur_frame_data, int frame_len);

    void audioDeviceIOCallback(const float** inputChannelData, int numInputChannels,
        float** outputChannelData, int numOutputChannels, int numSamples);

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override {}

    void audioDeviceStopped() {}

    void GenerateCarrierWave();

    //void send();

    //void printOutput_buffer();

    void sendOnePacket(int frame_len, Array<int8_t> cur_frame_data);

    int startSend();

    void hiResTimerCallback() override;

    void gendebug(int frame_len, Array<int8_t> cur_frame_data);

private:
    int header_len;
    int sample_rate;
    int carrier_freq;
    int carrier_amp;
    int carrier_phase;
    int playingSampleNum;
    int num_frame;
    int len_zeros;
    int len_warm_up;
    int len_frame;
    int output_buffer_idx;
    int num_bits_per_frame;
    int num_samples_per_bit;
    CriticalSection lock;
    //RingBuffer<float> output_buffer;
    //Buffer buffer;
    Array<Array<float> >buffer;
    Array<float> carrier_wave;
    float* header_wave;
    float* frame_wave;
    float* zeros;
    float* startup_wave;
    bool isPlaying;
    std::atomic<int> finish_prev_send{ 0 };
    //    AudioDeviceManager audioDeviceManager;
    //    unique_ptr<audioDevice> device;
};