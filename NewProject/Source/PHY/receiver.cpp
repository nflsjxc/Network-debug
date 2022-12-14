#include "receiver.h"
#include "Settings.h"
#include <vector>

//Receiver::Receiver() {
//    syncPower_localMax = 0;
//    isRecording = false;
//    data_state = -1;
//    state = SYNC;
//    recordedSampleNum = 0;
//
//    sampleRate = 48000;
//
//    GenerateCarrierWave();
//    GenerateHeader();
//}

Receiver::~Receiver()
{
    ;
}

/**number_bits_per_frame, number_bits_per_sample
*/
Receiver::Receiver(int packlen, int bitlen)
{
    bitLen = bitlen;
    packLen = packlen;

    syncPower_localMax = 0;
    isRecording = false;
    data_state = -1;
    state = SYNC;
    recordedSampleNum = 0;

    sampleRate = 48000;
    //headerLength = 120;
    headerLength = HEADER_LEN;

    GenerateCarrierWave();
    GenerateHeader();

    //fout.open("sound.txt");
}

void
Receiver::GenerateCarrierWave()
{
    int carrierAmp = 1;
    int carrierFreq = 5000;

    for (int j = 0; j < bitLen; j++)
    {
        carrierWave.add(carrierAmp * sin(2 * PI * carrierFreq * j / sampleRate));
    }

}

void
Receiver::GenerateHeader() {
    int start_freq = 2000;
    int end_freq = 10000;
    float freq_step = (end_freq - start_freq) / (headerLength / 2);
    float time_gap = (float)1 / (float)sampleRate;
    std::vector<float> fp;
    std::vector<float> omega;
    for (int i = 0; i < headerLength; i++)
    {
        fp.push_back(0);
        omega.push_back(0);
    }
    float* header_stack = new float[headerLength];
    fp[0] = start_freq;
    fp[headerLength / 2] = end_freq;
    for (int i = 1; i < headerLength / 2; i++)
        fp[i] = fp[i - 1] + freq_step;
    for (int i = headerLength / 2 + 1; i < headerLength; i++)
        fp[i] = fp[i - 1] - freq_step;
    for (int i = 1; i < headerLength; i++)
        omega[i] = omega[i - 1] + (fp[i] + fp[i - 1]) / 2.0 * time_gap;
    for (int i = 0; i < headerLength; i++)
        header_stack[i] = sin(2 * PI * omega[i]);
    syncHeader = Array<float>(header_stack, headerLength);
    delete[] header_stack;
}


void Receiver::audioDeviceAboutToStart(AudioIODevice* device)
{

}

void
Receiver::audioDeviceIOCallback(const float** inputChannelData, int numInputChannels,
    float** outputChannelData, int numOutputChannels, int numSamples)
{
    //const ScopedLock sl(lock); 
    if (isRecording)
    {
        for (int i = 0; i < numSamples; i++)
        {
            auto inputSamp = 0.0f;
            for (auto j = numInputChannels; --j >= 0;) {
                if (inputChannelData[j] != nullptr)
                {
                    inputSamp += inputChannelData[j][i];
                    //std::cout << inputChannelData[j][i] << "\n";
                }
            }
            {
                const ScopedLock sl1(lock);
                recordedSound.add(inputSamp);
                //fout << "sound callback" << inputSamp << '\n';
            }
            power_ = power_ * (1 - 1.0 / 64.0) + inputSamp * inputSamp / 64.0;
            //std::cout << power_ << '\n';
        }
        {
            for (int i = 0; i < numOutputChannels; ++i)
                if (outputChannelData[i] != nullptr)
                    zeromem(outputChannelData[i], numSamples * sizeof(float));
        }
    }

    
}


int
Receiver::Demodulate(float sample)
{
    /*of << sample << "\n";*/
    //std::cout << sample << "\n";
    //powerf << power_ << "\n";
    //max_power = (max_power >= power_) ? max_power : power_;
    if (state == SYNC)// sync process
    {
        if (processingHeader.size() < headerLength)
        {
            processingHeader.add(sample);
            return NO_HEADER;
        }
        else
        {
            processingHeader.add(sample);
            if (processingHeader.size() > headerLength)
            {
                processingHeader.removeRange(0, 1);
            }
            float syncPower = 0;
            for (int j = 0; j < headerLength; j++)
            {
                syncPower += syncHeader[j] * processingHeader[j];
            }
            //std::cout << syncPower << "\n";
            if (syncPower > power_ && syncPower > syncPower_localMax && syncPower > POWER_THRES)
            {
                syncPower_localMax = syncPower;
                tempBuffer.clear();
                _ifheadercheck = true;
                //std::cout << "header found.\n";
            }
            else if (_ifheadercheck)
            {
                tempBuffer.add(sample);

                //recordeddebug << s[i] << "\n";
                if (tempBuffer.size() >= 150)
                {
                    //std::cout << "header found.\n";
                    syncPower_localMax = 0;
                    state = DATA_PROCESS;
                    processingData = tempBuffer;
                    for (int j = 0; j < FRAME_OFFSET + 8; j++)
                    {
                        float sum = 0;
                        for (int k = 0; k < bitLen; k++)
                        {
                            sum += processingData[j * bitLen + k] * carrierWave[k];
                            //std::cout << j << ": " << processingData[j * bitLen + k] << std::endl;
                            //sum +=  carrierWave[j];
                        }
                        if (sum > 0)
                            int_data.add(1);
                        else if (sum < 0)
                            int_data.add(0);
                        if (j + 1 == FRAME_OFFSET + 8)
                        {
                            frame_data = Int2Byte(int_data);
                            if (frame_data[0] == TYPE_ACK || frame_data[0] == TYPE_MACPING_REPLY || frame_data[0] == TYPE_MACPING_REQUEST)
                            {
                                is_short_packet = true;
                            }
                        }
                    }
                    int_data.clear();
                }
            }
            return NO_HEADER;
        }
    }
    else if (state == DATA_PROCESS) //data process
    {
        if (is_short_packet)
        {
            processingData.clear();
            processingHeader.clear();
            state = SYNC;
            _ifheadercheck = false;
            int_data.clear();
            tempBuffer.clear();
            is_short_packet = false;
            return DATA_RECEIVED;
        }

        processingData.add(sample);
        if (processingData.size() == bitLen * packLen)
        {
            //std::cout << "Data_process state"<<packLen<<"\n";
            for (int j = 0; j < packLen; j++)
            {
                float sum = 0;
                for (int k = 0; k < bitLen; k++)
                {
                    //std::cout <<"Receiver "<<j << " : " << processingData[j * bitLen + k] << '\n';
                    sum += processingData[j * bitLen + k] * carrierWave[k];
                    //sum +=  carrierWave[j];
                }
                if (sum > 0)
                    int_data.add(1);
                else if (sum < 0)
                    int_data.add(0);
            }
            processingData.clear();
            processingHeader.clear();
            state = SYNC;
            _ifheadercheck = false;

          /*  std::cout << "intdata:\n";
            for (int j = 0; j < int_data.size(); j++) {
                std::cout << int_data[j] << ' ';
            }
            std::cout << "-------------------------\n";*/

            frame_data = Int2Byte(int_data);
            int_data.clear();
            tempBuffer.clear();
            return DATA_RECEIVED;
        }
        else
        {
            return DATA_PROCESS;
        }
    }
}

void
Receiver::hiResTimerCallback()
{
    if (!recordedSound.isEmpty())
    {
        const ScopedLock sl(lock);
        demodulate_buffer.addArray(recordedSound);
        //std::cout << recordedSound.size() << '\n';
        recordedSound.clear();
    }
    
  /*  for (int i = 0; i < demodulate_buffer.size(); i++)
    {
        std::cout << "sound timer" << demodulate_buffer[i] << '\n';
    }*/

    while (!demodulate_buffer.isEmpty())
    {
        data_state = Demodulate(demodulate_buffer[0]);
        demodulate_buffer.remove(0);
    }
}

Array<int8_t>
Receiver::getData()
{
    //const ScopedLock sl(lock);
    data_state = SYNC;
    std::chrono::system_clock::time_point start_time = std::chrono::system_clock::now();
    timeout_flag = false;
    while (data_state != DATA_RECEIVED)
    {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start_time).count();
        if (duration >= MAX_WAITING_TIME)
        {
            timeout_flag = true;
            break;
        }
    }
    return frame_data;
}


void
Receiver::startRecording()
{
    const ScopedLock sl(lock);
    recordedSampleNum = 0;
    isRecording = true;
    startTimer(10);
}

void
Receiver::stopRecording()
{
    if (isRecording)
    {
        isRecording = false;
    }
}

Array<int8_t>
Receiver::Int2Byte(Array<int>& int_data)
{
    std::vector<int8_t> vec;
    for (auto i : int_data)
        vec.push_back(i);
    if (int_data.size() / 8 * 8 != int_data.size())
    {
        //std::cout << "data length wrong!\n";
    }
    Array<int8_t> byte_data;

    for (int i = 0; i < int_data.size(); i += 8)
    {
        int8_t temp = 0;
        for (int j = 0; j < 8; j++)
        {
            temp = temp << 1;
            if (int_data[i + j] == 1)
                temp = temp | 0x01;
        }
        byte_data.add(temp);
    }
    return byte_data;
}