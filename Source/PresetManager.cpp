#include "PresetManager.h"

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& state) : apvts (state)
{
    for (auto* p : apvts.processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            apvts.addParameterListener (ranged->paramID, this);
}

PresetManager::~PresetManager()
{
    for (auto* p : apvts.processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            apvts.removeParameterListener (ranged->paramID, this);
}

juce::File PresetManager::getFolder()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("Primal Rage Lab").getChildFile ("Crumble").getChildFile ("Presets");
    if (! dir.exists())
        dir.createDirectory();
    return dir;
}

juce::StringArray PresetManager::getPresetNames() const
{
    juce::StringArray names;
    for (const auto& f : getFolder().findChildFiles (juce::File::findFiles, false, juce::String ("*") + extension))
        names.add (f.getFileNameWithoutExtension());
    names.sortNatural();
    return names;
}

void PresetManager::parameterChanged (const juce::String&, float)
{
    if (juce::Time::getMillisecondCounter() > ignoreUntil.load())
        modified = true;
}

void PresetManager::markClean()
{
    ignoreUntil = juce::Time::getMillisecondCounter() + 400;
    modified = false;
}

void PresetManager::loadInit()
{
    ignoreUntil = juce::Time::getMillisecondCounter() + 400;
    for (auto* p : apvts.processor.getParameters())
        p->setValueNotifyingHost (p->getDefaultValue());
    currentName = "Init";
    markClean();
}

bool PresetManager::load (const juce::String& name)
{
    const auto file = getFolder().getChildFile (name + extension);
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return false;

    ignoreUntil = juce::Time::getMillisecondCounter() + 400;
    // Start from defaults so parameters missing in older presets are reset
    for (auto* p : apvts.processor.getParameters())
        p->setValueNotifyingHost (p->getDefaultValue());
    apvts.replaceState (juce::ValueTree::fromXml (*xml));
    currentName = name;
    markClean();
    return true;
}

bool PresetManager::save (const juce::String& name)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty())
        return false;

    auto state = apvts.copyState();
    state.removeProperty ("presetName", nullptr);
    if (auto xml = state.createXml())
    {
        if (xml->writeTo (getFolder().getChildFile (clean + extension)))
        {
            currentName = clean;
            markClean();
            return true;
        }
    }
    return false;
}

bool PresetManager::remove (const juce::String& name)
{
    const bool ok = getFolder().getChildFile (name + extension).deleteFile();
    if (ok && name == currentName)
    {
        currentName = "Init";
        modified = true;
    }
    return ok;
}

bool PresetManager::rename (const juce::String& from, const juce::String& to)
{
    const auto clean = juce::File::createLegalFileName (to.trim());
    if (clean.isEmpty())
        return false;
    const auto src = getFolder().getChildFile (from + extension);
    const auto dst = getFolder().getChildFile (clean + extension);
    if (! src.existsAsFile() || dst.exists())
        return false;
    const bool ok = src.moveFileTo (dst);
    if (ok && from == currentName)
        currentName = clean;
    return ok;
}

void PresetManager::step (int direction)
{
    auto names = getPresetNames();
    if (names.isEmpty())
        return;
    int idx = names.indexOf (currentName);
    if (idx < 0)
        idx = direction > 0 ? -1 : 0;
    idx = (idx + direction + names.size()) % names.size();
    load (names[idx]);
}
