// UI 截图工具：离线渲染 MoonVoc 编辑器为 PNG，用于视觉验证
#include <JuceHeader.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

int main()
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    MoonVocProcessor processor;
    processor.prepareToPlay(48000.0, 512);

    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    editor->setSize(1280, 660);
    editor->resized(); // 离线组件需手动布局

    if (auto* me = dynamic_cast<MoonVocEditor*>(editor.get()))
        me->dumpLayout();
    else
        std::printf("not a MoonVocEditor\n");
    return 0;
}
