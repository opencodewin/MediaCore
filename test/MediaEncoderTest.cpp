#include <iostream>
#include <string>
#include "MediaParser.h"
#include "MediaReader.h"
#include "MediaEncoder.h"
#include "Logger.h"

using namespace std;
using namespace Logger;
using namespace MediaCore;

struct MediaHandlers
{
    MediaReader::Holder hVidReader{nullptr};
    MediaReader::Holder hAudReader{nullptr};
    MediaEncoder::Holder hEncoder;
};

int main(int argc, const char* argv[])
{
    if (argc < 3)
    {
        Log(Error) << "Wrong arguments!" << endl;
        return -1;
    }
    // GetMediaReaderLogger()->SetShowLevels(DEBUG);
    GetDefaultLogger()->SetShowLevels(DEBUG);
    MediaEncoder::GetLogger()->SetShowLevels(DEBUG);

    // string codecHint = "hevc";
    // vector<MediaEncoder::EncoderDescription> encoderDescList;
    // MediaEncoder::FindEncoder(codecHint, encoderDescList);
    // Log(DEBUG) << "Results for searching codec hint '" << codecHint << "':" << endl;
    // if (encoderDescList.empty())
    //     Log(DEBUG) << "NO ENCODER IS FOUND." << endl;
    // else
    // {
    //     for (int i = 0; i < encoderDescList.size(); i++)
    //     {
    //         auto& encdesc = encoderDescList[i];
    //         Log(DEBUG) << "[" << i << "] " << encdesc << endl;
    //     }
    // }

    string vidEncCodec = "h264";
    uint32_t outWidth{1920}, outHeight{1080};
    Ratio outFrameRate = { 25, 1 };
    uint64_t outVidBitRate = 10*1000*1000;
    string audEncCodec = "aac";
    uint32_t outAudChannels = 2;
    uint32_t outSampleRate = 44100;
    uint64_t outAudBitRate = 128*1000;
    int64_t maxEncodeDuration = 60*1000;
    bool videoOnly{false}, audioOnly{false};

    MediaParser::Holder hParser = MediaParser::CreateInstance();
    if (!hParser->Open(argv[1]))
    {
        Log(Error) << "FAILED to open MediaParser by file '" << argv[1] << "'! Error is '" << hParser->GetError() << "'." << endl;
        return -1;
    }
    MediaInfo::Holder hInfo = hParser->GetMediaInfo();

    MediaHandlers mhandlers;
    MediaReader::Holder hVidReader, hAudReader;
    string audioPcmFormat;
    uint32_t audioBlockAlign{0};
    if (hParser->GetBestVideoStreamIndex() >= 0 && !audioOnly)
    {
        mhandlers.hVidReader = hVidReader = MediaReader::CreateInstance();
        if (!hVidReader->Open(hParser))
        {
            Log(Error) << "FAILED to open video MediaReader! Error is '" << hVidReader->GetError() << "'." << endl;
            return -2;
        }
        if (!hVidReader->ConfigVideoReader(outWidth, outHeight))
        {
            Log(Error) << "FAILED to configure video MediaReader! Error is '" << hVidReader->GetError() << "'." << endl;
            return -3;
        }
        hVidReader->Start();
    }
    if (hParser->GetBestAudioStreamIndex() >= 0 && !videoOnly)
    {
        mhandlers.hAudReader = hAudReader = MediaReader::CreateInstance();
        if (!hAudReader->Open(hParser))
        {
            Log(Error) << "FAILED to open audio MediaReader! Error is '" << hAudReader->GetError() << "'." << endl;
            return -4;
        }
        if (!hAudReader->ConfigAudioReader(outAudChannels, outSampleRate, "flt"))
        {
            Log(Error) << "FAILED to configure audio MediaReader! Error is '" << hAudReader->GetError() << "'." << endl;
            return -5;
        }
        audioPcmFormat = hAudReader->GetAudioOutPcmFormat();
        audioBlockAlign = hAudReader->GetAudioOutFrameSize();
        hAudReader->Start();
    }

    auto hEncoder = mhandlers.hEncoder = MediaEncoder::CreateInstance();
    if (!hEncoder->Open(argv[2]))
    {
        Log(Error) << "FAILED to open MediaEncoder by '" << argv[2] << "'! Error is '" << hEncoder->GetError() << "'." << endl;
        return -6;
    }

    vector<MediaEncoder::Option> extraOpts = {
        { "profile",                { MediaEncoder::Option::OPVT_STRING, {}, "high" } },
        // { "aspect",                 { MediaEncoder::Option::OPVT_STRING, {}, "1:1" } },
    };
    string vidEncImgFormat;
    if (hVidReader && !hEncoder->ConfigureVideoStream(vidEncCodec, vidEncImgFormat, outWidth, outHeight, outFrameRate, outVidBitRate, &extraOpts))
    {
        Log(Error) << "FAILED to configure video encoder! Error is '" << hEncoder->GetError() << "'." << endl;
        return -7;
    }
    string audEncSmpFormat;
    if (hAudReader && !hEncoder->ConfigureAudioStream(audEncCodec, audEncSmpFormat, outAudChannels, outSampleRate, outAudBitRate))
    {
        Log(Error) << "FAILED to configure audio encoder! Error is '" << hEncoder->GetError() << "'." << endl;
        return -8;
    }
    hEncoder->Start();

    bool vidInputEof = hVidReader ? false : true;
    bool audInputEof = hAudReader ? false : true;
    int64_t audpos = 0, vidpos = 0;
    uint32_t vidFrameCount = 0;
    ImGui::ImMat vmat, amat;
    while (!vidInputEof || !audInputEof)
    {
        if ((!vidInputEof && vidpos <= audpos) || audInputEof)
        {
            bool eof;
            vidpos = (int64_t)((double)vidFrameCount*outFrameRate.den/outFrameRate.num*1000);
            if (!hVidReader->ReadVideoFrame(vidpos, vmat, eof) && !eof)
            {
                Log(Error) << "FAILED to read video frame! Error is '" << hVidReader->GetError() << "'." << endl;
                break;
            }
            vidFrameCount++;
            if (maxEncodeDuration > 0 && vidpos >= maxEncodeDuration)
                eof = true;
            if (!eof)
            {
                vmat.time_stamp = (double)vidpos/1000;
                if (!hEncoder->EncodeVideoFrame(vmat))
                {
                    Log(Error) << "FAILED to encode video frame! Error is '" << hEncoder->GetError() << "'." << endl;
                    break;
                }
            }
            else
            {
                vmat.release();
                if (!hEncoder->EncodeVideoFrame(vmat))
                {
                    Log(Error) << "FAILED to encode video EOF! Error is '" << hEncoder->GetError() << "'." << endl;
                    break;
                }
                vidInputEof = true;
            }
        }
        else
        {
            bool eof;
            uint32_t readSamples = 0;
            if (!hAudReader->ReadAudioSamples(amat, readSamples, audpos, eof) && !eof)
            {
                Log(Error) << "FAILED to read audio samples! Error is '" << hAudReader->GetError() << "'." << endl;
                break;
            }
            if (maxEncodeDuration > 0 && audpos >= maxEncodeDuration)
                eof = true;
            if (!eof)
            {
                if (!hEncoder->EncodeAudioSamples(amat))
                {
                    Log(Error) << "FAILED to encode audio samples! Error is '" << hEncoder->GetError() << "'." << endl;
                    break;
                }
            }
            else
            {
                amat.release();
                if (!hEncoder->EncodeAudioSamples(nullptr, 0))
                {
                    Log(Error) << "FAILED to encode audio EOF! Error is '" << hEncoder->GetError() << "'." << endl;
                    break;
                }
                audInputEof = true;
            }
        }
    }
    hEncoder->FinishEncoding();
    hEncoder->Close();

    return 0;
}