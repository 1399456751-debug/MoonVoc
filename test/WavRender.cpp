// 离线渲染工具：输入 WAV → MoonVoc 处理 → 输出 WAV
// 用法：
//   MoonVocRender.exe 输入.wav [输出.wav]    处理用户音频
//   MoonVocRender.exe --test [输出.wav]      生成内置测试信号（人声模拟）并处理
// 无输出文件名时输出 "输入名_processed.wav" / "demo_processed.wav"
#include <JuceHeader.h>
#include "../Source/PluginProcessor.h"

static void fillTestSignal(juce::AudioBuffer<float>& buf, int sampleRate, int blockIndex)
{
    // 模拟人声：基频 180Hz 动态变化 + 谐波 + 每 64 块一个齿音爆发（4-6k）+ 每 32 块起音脉冲
    const int start = blockIndex * buf.getNumSamples();
    for (int c = 0; c < buf.getNumChannels(); ++c)
    {
        auto* d = buf.getWritePointer(c);
        for (int n = 0; n < buf.getNumSamples(); ++n)
        {
            const int t = start + n;
            const double sec = (double) t / sampleRate;
            // 基频带轻微颤音
            const double f0 = 180.0 + 8.0 * std::sin(2.0 * 3.14159 * 5.0 * sec);
            const double vib = 0.03 * std::sin(2.0 * 3.14159 * 6.0 * sec);
            float s = (float) (0.45 * (1.0 + vib) * std::sin(2.0 * 3.14159 * f0 * sec)
                               + 0.20 * std::sin(2.0 * 3.14159 * f0 * 2.0 * sec)
                               + 0.08 * std::sin(2.0 * 3.14159 * f0 * 3.0 * sec));

            // 起音脉冲（每 32 块开头）
            if (t % (32 * 512) < 4 && t % (32 * 512) >= 0)
                s += 0.25f;

            // 齿音爆发（每 64 块，3-6k 噪声 + 高频瞬态）
            if ((t / (64 * 512)) % 2 == 0 && t % (64 * 512) < 1200)
            {
                const double p = (double) (t % (64 * 512)) / 1200.0;
                const double env = std::exp(-p * 6.0);
                s += (float) (env * (0.10 * std::sin(2.0 * 3.14159 * 4500.0 * sec)
                                     + 0.06 * std::sin(2.0 * 3.14159 * 6500.0 * sec)));
            }

            d[n] = s;
        }
    }
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();

    juce::String inPath, outPath;
    bool useTest = false;

    if (argc >= 2 && juce::String(argv[1]) == "--test")
    {
        useTest = true;
        inPath = "test signal (built-in)";
        outPath = argc >= 3 ? argv[2] : "demo_processed.wav";
    }
    else if (argc >= 2)
    {
        inPath = argv[1];
        juce::File inFile(inPath);
        outPath = argc >= 3 ? argv[2] : inFile.getParentDirectory().getChildFile(inFile.getFileNameWithoutExtension() + "_processed.wav").getFullPathName();
    }
    else
    {
        std::printf("用法: MoonVocRender.exe 输入.wav [输出.wav]\n"
                    "      MoonVocRender.exe --test [输出.wav]  (内置测试信号)\n");
        return 1;
    }

    std::unique_ptr<juce::AudioFormatReader> reader;
    double sampleRate = 48000.0;
    int numCh = 2;

    if (! useTest)
    {
        reader.reset(fm.createReaderFor(juce::File(inPath)));
        if (reader == nullptr)
        {
            std::printf("无法读取输入文件: %s\n", inPath.toRawUTF8());
            return 1;
        }
        sampleRate = reader->sampleRate;
        numCh = jmin(2, (int) reader->numChannels);
        std::printf("输入: %s  (%.1f kHz, %d ch, %.1f s)\n",
                    inPath.toRawUTF8(), sampleRate / 1000.0, numCh,
                    (double) reader->lengthInSamples / sampleRate);
    }
    else
    {
        std::printf("使用内置测试信号 (模拟人声: 基频+齿音+起音脉冲)\n");
    }

    MoonVocProcessor processor;
    processor.prepareToPlay(sampleRate, 512);

    const int blockSize = 512;
    juce::AudioBuffer<float> buf(numCh, blockSize);
    juce::MidiBuffer midi;

    juce::WavAudioFormat wavFormat;
    auto outStream = std::make_unique<juce::FileOutputStream>(juce::File(outPath));
    if (outStream->failedToOpen())
    {
        std::printf("无法创建输出文件: %s\n", outPath.toRawUTF8());
        return 1;
    }
    std::unique_ptr<juce::OutputStream> stream(std::move(outStream)); // 所有权移交给 writer
    std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(
        stream, juce::AudioFormatWriter::Options{}.withSampleRate(sampleRate)
                                                  .withNumChannels(numCh)
                                                  .withBitsPerSample(24)));
    if (writer == nullptr)
    {
        std::printf("无法创建 WAV writer\n");
        return 1;
    }

    const int64 total = useTest ? (int64) (8.0 * sampleRate) : reader->lengthInSamples;
    int64 pos = 0;
    int blocks = 0;
    while (pos < total)
    {
        const int n = (int) jmin<int64>(blockSize, total - pos);
        if (useTest)
        {
            fillTestSignal(buf, (int) sampleRate, blocks);
        }
        else
        {
            buf.clear();
            reader->read(&buf, 0, n, pos, true, true);
        }
        processor.processBlock(buf, midi);
        writer->writeFromAudioSampleBuffer(buf, 0, n);
        pos += n;
        ++blocks;
        if (blocks % 500 == 0)
            std::printf("  渲染进度: %lld / %lld\n", (long long) pos, (long long) total);
    }

    std::printf("完成: %s\n", outPath.toRawUTF8());
    return 0;
}
