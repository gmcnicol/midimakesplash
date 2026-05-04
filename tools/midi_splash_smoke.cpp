#include "PluginProcessor.h"

#include <juce_audio_utils/juce_audio_utils.h>

#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace
{
struct CheckFailure : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

void require(bool condition, const juce::String& message)
{
    if (! condition)
        throw CheckFailure(message.toStdString());
}

juce::StringArray expectedScaleFiles()
{
    return {
        "aeolian.txt",
        "dorian.txt",
        "ionian.txt",
        "locrian.txt",
        "lydian.txt",
        "mixolydian.txt",
        "phrygian.txt",
    };
}

juce::String componentTypeName(juce::Component& component)
{
    if (dynamic_cast<juce::Viewport*>(&component) != nullptr)
        return "Viewport";

    if (dynamic_cast<juce::Slider*>(&component) != nullptr)
        return "Slider";

    if (dynamic_cast<juce::ComboBox*>(&component) != nullptr)
        return "ComboBox";

    if (dynamic_cast<juce::ScrollBar*>(&component) != nullptr)
        return "ScrollBar";

    if (dynamic_cast<juce::Label*>(&component) != nullptr)
        return "Label";

    return "Component";
}

void lintComponentBounds(juce::Component& component, const juce::String& path)
{
    for (int childIndex = 0; childIndex < component.getNumChildComponents(); ++childIndex)
    {
        auto* child = component.getChildComponent(childIndex);
        require(child != nullptr, "Null child component at " + path);

        auto childName = child->getName();
        if (childName.isEmpty())
            childName = componentTypeName(*child);

        const auto childPath = path + "/" + childName;
        if (dynamic_cast<juce::ScrollBar*>(child) != nullptr && ! child->isVisible())
            continue;

        const auto bounds = child->getBounds();
        const bool isLintedControl = dynamic_cast<juce::Label*>(child) != nullptr
            || dynamic_cast<juce::Slider*>(child) != nullptr
            || dynamic_cast<juce::ComboBox*>(child) != nullptr
            || dynamic_cast<juce::Viewport*>(child) != nullptr;

        require(child->isVisible(), "Hidden UI component: " + childPath);
        require(bounds.getWidth() > 0 && bounds.getHeight() > 0,
                "Collapsed UI component: " + childPath + " bounds=" + bounds.toString());
        require(bounds.getX() >= 0 && bounds.getY() >= 0,
                "UI component starts outside parent: " + childPath + " bounds=" + bounds.toString());

        if (auto* viewport = dynamic_cast<juce::Viewport*>(&component);
            viewport != nullptr && viewport->getViewedComponent() == child)
        {
            require(bounds.getWidth() <= component.getWidth(),
                    "Viewport content is horizontally clipped: " + childPath + " bounds=" + bounds.toString());
        }
        else
        {
            if (isLintedControl)
                require(bounds.getRight() <= component.getWidth() && bounds.getBottom() <= component.getHeight(),
                        "UI component is clipped by parent: " + childPath + " bounds=" + bounds.toString()
                            + " parent=" + component.getLocalBounds().toString());
        }

        if (dynamic_cast<juce::Slider*>(child) != nullptr)
            require(bounds.getWidth() >= 96 && bounds.getHeight() >= 72,
                    "Slider is too small to use: " + childPath + " bounds=" + bounds.toString());

        if (dynamic_cast<juce::ComboBox*>(child) != nullptr)
            require(bounds.getWidth() >= 96 && bounds.getHeight() >= 24,
                    "ComboBox is too small to use: " + childPath + " bounds=" + bounds.toString());

        if (auto* label = dynamic_cast<juce::Label*>(child))
            require(label->getText().trim().isNotEmpty(), "Visible label has no text: " + childPath);

        lintComponentBounds(*child, childPath);
    }
}

void lintEditor(juce::AudioProcessorEditor& editor, int width, int height)
{
    editor.setSize(width, height);
    editor.resized();
    lintComponentBounds(editor, "editor");
}

void renderEditorSnapshot(juce::AudioProcessorEditor& editor, const juce::File& outputFile)
{
    editor.setSize(960, 620);
    editor.resized();

    auto image = editor.createComponentSnapshot(editor.getLocalBounds(), false, 1.0f);
    require(image.isValid(), "Editor snapshot is invalid");

    int orangePixels = 0;
    int brightPixels = 0;
    int sampledPixels = 0;

    for (int y = 0; y < image.getHeight(); y += 2)
    {
        for (int x = 0; x < image.getWidth(); x += 2)
        {
            const auto pixel = image.getPixelAt(x, y);
            ++sampledPixels;

            if (pixel.getRed() > 160 && pixel.getGreen() > 80 && pixel.getBlue() < 80)
                ++orangePixels;

            if (pixel.getRed() > 180 && pixel.getGreen() > 180 && pixel.getBlue() > 180)
                ++brightPixels;
        }
    }

    require(sampledPixels > 0, "Editor snapshot has no sampled pixels");
    require(orangePixels > 20 || brightPixels > 80,
            "Editor snapshot looks blank: orange_pixels="
                + juce::String(orangePixels)
                + " bright_pixels="
                + juce::String(brightPixels));

    outputFile.getParentDirectory().createDirectory();
    juce::PNGImageFormat png;
    std::unique_ptr<juce::FileOutputStream> stream(outputFile.createOutputStream());
    require(stream != nullptr && stream->openedOk(), "Could not write editor snapshot: " + outputFile.getFullPathName());
    require(png.writeImageToStream(image, *stream), "Could not encode editor snapshot PNG");
}

juce::AudioParameterChoice& requireChoiceParameter(juce::AudioProcessor& processor, const juce::String& name)
{
    for (auto* parameter : processor.getParameters())
        if (parameter->getName(128) == name)
            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(parameter))
                return *choice;

    throw CheckFailure(("Missing choice parameter: " + name).toStdString());
}

juce::RangedAudioParameter& requireRangedParameter(juce::AudioProcessor& processor, const juce::String& name)
{
    for (auto* parameter : processor.getParameters())
        if (parameter->getName(128) == name)
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter))
                return *ranged;

    throw CheckFailure(("Missing ranged parameter: " + name).toStdString());
}

void setParameterRaw(juce::AudioProcessor& processor, const juce::String& name, float value)
{
    auto& parameter = requireRangedParameter(processor, name);
    parameter.setValueNotifyingHost(parameter.convertTo0to1(value));
}

void setChoiceParameter(juce::AudioProcessor& processor, const juce::String& name, const juce::String& choiceName)
{
    auto& choice = requireChoiceParameter(processor, name);
    const auto index = choice.choices.indexOf(choiceName);
    require(index >= 0, "Choice parameter " + name + " missing " + choiceName);
    choice.setValueNotifyingHost(choice.convertTo0to1(static_cast<float>(index)));
    require(choice.getCurrentChoiceName() == choiceName,
            "Choice parameter " + name + " did not select " + choiceName
                + ", got " + choice.getCurrentChoiceName());
}

bool isDAeolianNote(int note)
{
    static constexpr int intervals[] = { 0, 2, 3, 5, 7, 8, 10 };
    const auto relative = (note - 2 + 1200) % 12;
    return std::find(std::begin(intervals), std::end(intervals), relative) != std::end(intervals);
}

juce::String joinNotes(const std::vector<int>& notes)
{
    juce::StringArray values;
    for (const auto note : notes)
        values.add(juce::String(note));
    return values.joinIntoString(",");
}

void configureFastDAeolianSplash(MrgeeJsfxAudioProcessor& processor, int anchorPolicy)
{
    setChoiceParameter(processor, "Root", "D");
    setChoiceParameter(processor, "Scale File", "aeolian.txt");
    setChoiceParameter(processor,
                       "Anchor Policy",
                       anchorPolicy == 0 ? "NearestInScale" : "RequireInScale");
    setParameterRaw(processor, "Pitch Clamp Low", 50.0f);
    setParameterRaw(processor, "Pitch Clamp High", 86.0f);
    setParameterRaw(processor, "Base Droplets", 96.0f);
    setParameterRaw(processor, "Impact->Droplets (%)", 0.0f);
    setParameterRaw(processor, "Spray Radius (degrees)", 36.0f);
    setParameterRaw(processor, "Up/Down Bias", 0.0f);
    setParameterRaw(processor, "Channel Spread", 0.0f);
    setParameterRaw(processor, "Min Flight (ms)", 1.0f);
    setParameterRaw(processor, "Max Flight (ms)", 24.0f);
    setParameterRaw(processor, "Turbulence (ms)", 0.0f);
    setParameterRaw(processor, "Clumpiness", 0.0f);
    setParameterRaw(processor, "Min Velocity", 8.0f);
    setParameterRaw(processor, "Max Velocity", 127.0f);
    setParameterRaw(processor, "Pass Through Input", 0.0f);
    setParameterRaw(processor, "New Impact Cancels Tail", 1.0f);
    setParameterRaw(processor, "Feedback (%)", 0.0f);
    setParameterRaw(processor, "Seed", 12345.0f);
    setParameterRaw(processor, "Lock Seed", 1.0f);
}

std::vector<int> renderSplashNoteOns(MrgeeJsfxAudioProcessor& processor, int inputNote)
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 256;

    processor.prepareToPlay(sampleRate, blockSize);

    juce::AudioBuffer<float> buffer(2, blockSize);
    std::vector<int> noteOns;

    for (int block = 0; block < 24; ++block)
    {
        buffer.clear();
        juce::MidiBuffer midi;

        if (block == 0)
            midi.addEvent(juce::MidiMessage::noteOn(1, inputNote, juce::uint8(100)), 0);

        processor.processBlock(buffer, midi);

        if (block == 0)
        {
            require(std::abs(processor.getJsfxHost().getSlider(0) - 2.0f) < 0.01f,
                    "Root slider was not forwarded to JSFX as D; value="
                        + juce::String(processor.getJsfxHost().getSlider(0)));
            require(std::abs(processor.getJsfxHost().getRuntimeSlider(0) - 2.0f) < 0.01f,
                    "Root runtime slider is not D; value="
                        + juce::String(processor.getJsfxHost().getRuntimeSlider(0)));
            require(std::abs(processor.getJsfxHost().getSlider(2) - 0.0f) < 0.01f,
                    "Scale File slider was not forwarded to JSFX as aeolian.txt; value="
                        + juce::String(processor.getJsfxHost().getSlider(2)));
            require(std::abs(processor.getJsfxHost().getRuntimeSlider(2) - 0.0f) < 0.01f,
                    "Scale File runtime slider is not aeolian.txt; value="
                        + juce::String(processor.getJsfxHost().getRuntimeSlider(2)));
        }

        for (const auto metadata : midi)
        {
            const auto message = metadata.getMessage();
            if (message.isNoteOn())
                noteOns.push_back(message.getNoteNumber());
        }
    }

    processor.releaseResources();
    return noteOns;
}

void verifyScaleFilesAreBundled(MrgeeJsfxAudioProcessor& processor)
{
    const auto bundleRoot = processor.getJsfxHost().getMaterializedBundleRoot();
    const auto scaleRoot = bundleRoot.getChildFile("Data").getChildFile("scales");
    require(scaleRoot.isDirectory(), "Bundled scale directory is missing: " + scaleRoot.getFullPathName());

    for (const auto& filename : expectedScaleFiles())
    {
        const auto scaleFile = scaleRoot.getChildFile(filename);
        require(scaleFile.existsAsFile(), "Bundled scale file is missing: " + scaleFile.getFullPathName());
        require(scaleFile.loadFileAsString().trim().isNotEmpty(), "Bundled scale file is empty: " + filename);
    }
}

void verifyScaleSelector(MrgeeJsfxAudioProcessor& processor)
{
    const auto& descriptors = processor.getSliderDescriptors();
    auto scaleIt = std::find_if(descriptors.begin(),
                                descriptors.end(),
                                [](const auto& descriptor)
                                {
                                    return descriptor.name == "Scale File";
                                });

    require(scaleIt != descriptors.end(), "Missing JSFX Scale File descriptor");
    require(scaleIt->isEnum, "Scale File descriptor is not selectable");

    for (const auto& filename : expectedScaleFiles())
        require(scaleIt->enumNames.contains(filename), "Scale File selector missing " + filename);

    auto& scaleChoice = requireChoiceParameter(processor, "Scale File");
    require(scaleChoice.getCurrentChoiceName() == "ionian.txt",
            "Scale File default should be ionian.txt, got " + scaleChoice.getCurrentChoiceName());

    for (int index = 0; index < scaleChoice.choices.size(); ++index)
    {
        scaleChoice.setValueNotifyingHost(scaleChoice.convertTo0to1(static_cast<float>(index)));
        require(scaleChoice.getIndex() == index,
                "Scale File choice could not select index " + juce::String(index));
        require(expectedScaleFiles().contains(scaleChoice.getCurrentChoiceName()),
                "Scale File choice is not from bundled scales: " + scaleChoice.getCurrentChoiceName());
    }
}

void verifyDAeolianSnapping()
{
    for (const auto anchorPolicy : { 0, 1 })
    {
        MrgeeJsfxAudioProcessor processor;
        configureFastDAeolianSplash(processor, anchorPolicy);

        const auto noteOns = renderSplashNoteOns(processor, 62);
        require(! noteOns.empty(),
                "D aeolian generated no note-ons with anchor policy " + juce::String(anchorPolicy));

        for (const auto note : noteOns)
        {
            require(note >= 50 && note <= 86,
                    "D aeolian emitted note outside pitch clamp: "
                        + juce::String(note)
                        + " anchor_policy="
                        + juce::String(anchorPolicy));
            require(isDAeolianNote(note),
                    "D aeolian emitted non-scale note: "
                        + juce::String(note)
                        + " anchor_policy="
                        + juce::String(anchorPolicy)
                        + " notes="
                        + joinNotes(noteOns));
        }
    }
}

void verifyEditor()
{
    MrgeeJsfxAudioProcessor processor;
    auto editor = std::unique_ptr<juce::AudioProcessorEditor>(processor.createEditor());
    require(editor != nullptr, "Failed to create editor");
    require(editor->isResizable(), "Editor is not host-resizable");

    lintComponentBounds(*editor, "editor.initial");
    lintEditor(*editor, 960, 620);
    lintEditor(*editor, 560, 420);
    renderEditorSnapshot(*editor,
                         juce::File::getCurrentWorkingDirectory()
                             .getChildFile("build")
                             .getChildFile("direct_editor.png"));
}

void verify()
{
    MrgeeJsfxAudioProcessor processor;
    require(processor.getJsfxHost().hasRuntime(),
            "ysfx runtime did not load: " + processor.getJsfxHost().getStatusMessage());

    verifyScaleFilesAreBundled(processor);
    verifyScaleSelector(processor);
    verifyDAeolianSnapping();
    verifyEditor();
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    try
    {
        verify();
        std::cout << "midi_splash_smoke: OK" << std::endl;
        return 0;
    }
    catch (const CheckFailure& failure)
    {
        std::cerr << "midi_splash_smoke: FAIL: " << failure.what() << std::endl;
    }
    catch (const std::exception& failure)
    {
        std::cerr << "midi_splash_smoke: ERROR: " << failure.what() << std::endl;
    }

    return 1;
}
