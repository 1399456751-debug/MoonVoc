cmake_minimum_required(VERSION 3.22)
project(MoonVoc VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(C:/JUCE JUCE)

if(MSVC)
    add_compile_options(/utf-8)
endif()

juce_add_plugin(MoonVoc
    COMPANY_NAME "TUJZMIXING"
    PLUGIN_MANUFACTURER_CODE Tujm
    PLUGIN_CODE MoVc
    FORMATS VST3 Standalone
    PRODUCT_NAME "MoonVoc"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES Fx
)

juce_generate_juce_header(MoonVoc)

# 嵌入字体（Montserrat，OFL 开源协议）+ 月亮背景图
juce_add_binary_data(MoonVocFonts
    SOURCES
        fonts/Montserrat-Bold.ttf
        fonts/Montserrat-SemiBold.ttf
        fonts/Montserrat-Medium.ttf
        assets/moon.jpg
)

target_sources(MoonVoc PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UI/MoonVocLookAndFeel.cpp
    Source/dsp/VoiceDeEsser.cpp
    Source/dsp/VoiceEq.cpp
    Source/dsp/VoiceComp.cpp
    Source/dsp/VoiceSat.cpp
    Source/dsp/VoiceEdge.cpp
)

target_link_libraries(MoonVoc PRIVATE MoonVocFonts)

target_compile_definitions(MoonVoc PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(MoonVoc PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# Headless 回归测试（不装插件，直接压测音频线程路径）
add_executable(MoonVocHeadlessTest
    test/HeadlessTest.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UI/MoonVocLookAndFeel.cpp
    Source/dsp/VoiceDeEsser.cpp
    Source/dsp/VoiceEq.cpp
    Source/dsp/VoiceComp.cpp
    Source/dsp/VoiceSat.cpp
    Source/dsp/VoiceEdge.cpp
)

# 复用插件的 JuceLibraryCode 生成目录（juce_generate_juce_header 仅支持 juce_add_* target）
target_include_directories(MoonVocHeadlessTest PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/MoonVoc_artefacts/JuceLibraryCode)

target_compile_definitions(MoonVocHeadlessTest PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JucePlugin_Name="MoonVoc"
    JucePlugin_Manufacturer="TUJZMIXING"
    JucePlugin_VersionString="0.1.0"
    JucePlugin_VersionCode=0x100
)

target_link_libraries(MoonVocHeadlessTest PRIVATE
    MoonVocFonts
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

# 离线渲染工具：WAV 进 → MoonVoc 处理 → WAV 出（试听/A-B 对比）
add_executable(MoonVocRender
    test/WavRender.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UI/MoonVocLookAndFeel.cpp
    Source/dsp/VoiceDeEsser.cpp
    Source/dsp/VoiceEq.cpp
    Source/dsp/VoiceComp.cpp
    Source/dsp/VoiceSat.cpp
    Source/dsp/VoiceEdge.cpp
)

target_include_directories(MoonVocRender PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/MoonVoc_artefacts/JuceLibraryCode)

target_compile_definitions(MoonVocRender PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JucePlugin_Name="MoonVoc"
    JucePlugin_Manufacturer="TUJZMIXING"
    JucePlugin_VersionString="0.1.0"
    JucePlugin_VersionCode=0x100
)

target_link_libraries(MoonVocRender PRIVATE
    MoonVocFonts
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_audio_formats
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

# UI 截图工具（视觉验证）
add_executable(MoonVocUiSnapshot
    test/UiSnapshot.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UI/MoonVocLookAndFeel.cpp
    Source/dsp/VoiceDeEsser.cpp
    Source/dsp/VoiceEq.cpp
    Source/dsp/VoiceComp.cpp
    Source/dsp/VoiceSat.cpp
    Source/dsp/VoiceEdge.cpp
)

target_include_directories(MoonVocUiSnapshot PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/MoonVoc_artefacts/JuceLibraryCode)

target_compile_definitions(MoonVocUiSnapshot PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JucePlugin_Name="MoonVoc"
    JucePlugin_Manufacturer="TUJZMIXING"
    JucePlugin_VersionString="0.1.0"
    JucePlugin_VersionCode=0x100
)

target_link_libraries(MoonVocUiSnapshot PRIVATE
    MoonVocFonts
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
