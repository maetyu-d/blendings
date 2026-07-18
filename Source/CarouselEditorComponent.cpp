#include "CarouselEditorComponent.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
juce::Colour bg() { return juce::Colour (0xff0d1411); }
juce::Colour surface() { return juce::Colour (0xff131c18); }
juce::Colour grid() { return juce::Colour (0x284f786d); }
juce::Colour text() { return juce::Colour (0xffedf4ee); }
juce::Colour muted() { return juce::Colour (0xff84928b); }
juce::Colour accent() { return juce::Colour (0xffffc857); }
juce::Colour orbit() { return juce::Colour (0xff45d7c0); }
juce::Colour post() { return juce::Colour (0xffff6b8a); }
juce::Colour selectionBlue() { return juce::Colour (0xff1687f8); }
constexpr int carouselGridSubdivisions = 4;

float snapCarouselCoordinate (float value)
{
    return std::round (value * (float) carouselGridSubdivisions) / (float) carouselGridSubdivisions;
}

juce::String defaultCarouselScCode()
{
    return "var env = EnvGen.kr(Env.perc(0.005, sustain.max(0.08)), doneAction: 2);\n"
           "var body = SinOsc.ar(pitch.midicps) + (Pulse.ar(pitch.midicps * 0.5, 0.35) * 0.12);\n"
           "Out.ar(out, Pan2.ar(body * env * amp, pan));";
}

juce::String defaultCarouselPdPatch()
{
    return R"PD(#N canvas 120 120 420 300 10;
#X obj 40 36 r trigger;
#X obj 40 72 t f b;
#X obj 40 108 mtof;
#X obj 40 144 osc~ 440;
#X msg 152 108 0 \, 0.22 5 \, 0 420 5;
#X obj 152 144 vline~;
#X obj 88 184 *~;
#X obj 88 224 dac~;
#X connect 0 0 1 0;
#X connect 1 0 2 0;
#X connect 1 1 4 0;
#X connect 2 0 3 0;
#X connect 3 0 6 0;
#X connect 4 0 5 0;
#X connect 5 0 6 1;
#X connect 6 0 7 0;
#X connect 6 0 7 1;
)PD";
}

void ensureToneSources (CarouselDocument::Item& item)
{
    if (item.type == CarouselDocument::ItemType::post)
    {
        item.ownerOrbit = -1;
        return;
    }
    if (item.type != CarouselDocument::ItemType::tone) return;
    if (item.scCode.trim().isEmpty()) item.scCode = defaultCarouselScCode();
    if (item.pdPatch.trim().isEmpty()) item.pdPatch = defaultCarouselPdPatch();
}
}

CarouselDocument CarouselDocument::createDefault()
{
    CarouselDocument d;
    Item orbitItem; orbitItem.id = 1; orbitItem.type = ItemType::orbit; orbitItem.x = 4; orbitItem.y = 3; orbitItem.radius = 1.0f; orbitItem.speed = 0.25f;
    Item firstTone; firstTone.id = 2; firstTone.type = ItemType::tone; firstTone.x = 5; firstTone.y = 3; firstTone.midi = 60; firstTone.voice = 1;
    Item secondTone; secondTone.id = 3; secondTone.type = ItemType::tone; secondTone.x = 2; secondTone.y = 2; secondTone.midi = 55; secondTone.voice = 2;
    Item firstPost; firstPost.id = 4; firstPost.type = ItemType::post; firstPost.x = 7; firstPost.y = 3;
    Item secondPost; secondPost.id = 5; secondPost.type = ItemType::post; secondPost.x = 8; secondPost.y = 5;
    d.items = { std::move (orbitItem), std::move (firstTone), std::move (secondTone), std::move (firstPost), std::move (secondPost) };
    for (auto& item : d.items) ensureToneSources (item);
    return d;
}

juce::ValueTree CarouselDocument::toValueTree() const
{
    juce::ValueTree root ("carousel");
    root.setProperty ("columns", columns, nullptr); root.setProperty ("rows", rows, nullptr);
    root.setProperty ("bpm", bpm, nullptr); root.setProperty ("nextId", nextId, nullptr);
    for (const auto& item : items)
    {
        juce::ValueTree node ("item");
        node.setProperty ("id", item.id, nullptr); node.setProperty ("type", static_cast<int> (item.type), nullptr);
        node.setProperty ("x", item.x, nullptr); node.setProperty ("y", item.y, nullptr);
        node.setProperty ("midi", item.midi, nullptr); node.setProperty ("voice", item.voice, nullptr);
        node.setProperty ("owner", item.ownerOrbit, nullptr); node.setProperty ("radius", item.radius, nullptr);
        node.setProperty ("speed", item.speed, nullptr); node.setProperty ("phase", item.phase, nullptr);
        node.setProperty ("euclidean", item.euclidean, nullptr);
        node.setProperty ("playback", static_cast<int> (item.playback), nullptr);
        node.setProperty ("duration", item.durationSeconds, nullptr);
        node.setProperty ("scCode", item.scCode, nullptr);
        node.setProperty ("pdPatch", item.pdPatch, nullptr);
        root.addChild (node, -1, nullptr);
    }
    return root;
}

CarouselDocument CarouselDocument::fromValueTree (const juce::ValueTree& tree)
{
    if (! tree.isValid() || ! tree.hasType ("carousel")) return createDefault();
    CarouselDocument d; d.columns = juce::jlimit (4, 24, static_cast<int> (tree["columns"]));
    d.rows = juce::jlimit (4, 16, static_cast<int> (tree["rows"])); d.bpm = juce::jlimit (40.0, 220.0, static_cast<double> (tree["bpm"]));
    d.nextId = juce::jmax (1, static_cast<int> (tree["nextId"]));
    for (const auto& node : tree)
        if (node.hasType ("item"))
        {
            Item item;
            item.id = static_cast<int> (node["id"]); item.type = static_cast<ItemType> (juce::jlimit (0, 3, static_cast<int> (node["type"])));
            item.x = static_cast<float> (node["x"]); item.y = static_cast<float> (node["y"]);
            item.midi = static_cast<int> (node["midi"]); item.voice = static_cast<int> (node["voice"]);
            item.ownerOrbit = static_cast<int> (node["owner"]); item.radius = static_cast<float> (node["radius"]);
            item.speed = static_cast<float> (node["speed"]); item.phase = static_cast<float> (node["phase"]);
            item.euclidean = static_cast<bool> (node["euclidean"]);
            item.playback = static_cast<PlaybackType> (juce::jlimit (0, 2, static_cast<int> (node.getProperty ("playback", 0))));
            item.durationSeconds = juce::jlimit (0.05f, 30.0f, static_cast<float> (node.getProperty ("duration", 1.0f)));
            item.scCode = node.getProperty ("scCode").toString(); item.pdPatch = node.getProperty ("pdPatch").toString();
            ensureToneSources (item);
            d.items.push_back (std::move (item));
        }
    return d;
}

CarouselEditorComponent::CarouselEditorComponent()
{
    setWantsKeyboardFocus (true); setFocusContainerType (FocusContainerType::focusContainer);
    selectButton.setButtonText ("Select"); toneButton.setButtonText ("Tone");
    orbitButton.setButtonText ("Orbit"); postButton.setButtonText ("Post"); plankButton.setButtonText ("Plank");
    playButton.setButtonText ("Play"); fitButton.setButtonText ("Fit"); clearButton.setButtonText ("Clear");
    selectButton.setTooltip ("Select and move Carousel objects");
    toneButton.setTooltip ("Place a tone, or mount one on a plank endpoint");
    orbitButton.setTooltip ("Place a carousel, or mount one on a plank endpoint");
    postButton.setTooltip ("Place a fixed collision post");
    plankButton.setTooltip ("Extend an empty mounting plank from a carousel rim");
    fitButton.setTooltip ("Fit the complete Carousel field");
    clearButton.setTooltip ("Clear the Carousel field");
    for (auto* b : { &selectButton, &toneButton, &orbitButton, &postButton, &plankButton }) { addAndMakeVisible (*b); b->setClickingTogglesState (true); b->setRadioGroupId (901); }
    for (auto* b : { &selectButton, &toneButton, &orbitButton, &postButton, &plankButton })
    {
        b->setColour (juce::TextButton::buttonColourId, surface());
        b->setColour (juce::TextButton::buttonOnColourId, selectionBlue());
        b->setColour (juce::TextButton::textColourOffId, text().withAlpha (0.78f));
        b->setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    }
    selectButton.setToggleState (true, juce::dontSendNotification);
    selectButton.onClick = [this] { setTool (Tool::select); }; toneButton.onClick = [this] { setTool (Tool::tone); };
    orbitButton.onClick = [this] { setTool (Tool::orbit); }; postButton.onClick = [this] { setTool (Tool::post); };
    plankButton.onClick = [this] { setTool (Tool::plank); };
    for (auto* b : { &playButton, &clearButton, &deleteButton, &fitButton, &resetCodeButton, &soundButton, &auditionButton }) addAndMakeVisible (*b);
    playButton.onClick = [this] { setRunning (! running); }; clearButton.onClick = [this] { document.items.clear(); selected = -1; changed(); };
    deleteButton.onClick = [this] { deleteSelected(); };
    fitButton.onClick = [this] { zoom = 1.0f; pan = {}; repaint(); };
    titleLabel.setText ("Carousel", juce::dontSendNotification); titleLabel.setFont (juce::FontOptions (18.0f).withStyle ("Bold")); titleLabel.setColour (juce::Label::textColourId, text()); addAndMakeVisible (titleLabel);
    detailLabel.setColour (juce::Label::textColourId, muted()); detailLabel.setFont (juce::FontOptions (12.0f)); addAndMakeVisible (detailLabel);
    for (auto [label, caption] : std::initializer_list<std::pair<juce::Label*, const char*>> { {&globalLabel,"FIELD"},{&selectionLabel,"INSPECTOR"} }) { label->setText(caption,juce::dontSendNotification);label->setColour(juce::Label::textColourId,text().withAlpha(.82f));label->setFont(juce::FontOptions(10.0f).withStyle("Bold"));addAndMakeVisible(*label); }
    for (auto [label, caption] : std::initializer_list<std::pair<juce::Label*, const char*>> { {&bpmLabel,"BPM"},{&columnsLabel,"Columns"},{&rowsLabel,"Rows"},{&pitchLabel,"Pitch"},{&playbackLabel,"Playback"},{&durationLabel,"One-shot duration"},{&codeLabel,"Source"},{&speedLabel,"Tempo multiple"},{&radiusLabel,"Radius"},{&countLabel,"Orbit tones"} }) { label->setText(caption,juce::dontSendNotification);label->setColour(juce::Label::textColourId,muted());label->setFont(juce::FontOptions(11.0f));addAndMakeVisible(*label); }
    auto setup = [this] (juce::Slider& s, double lo, double hi, double step) { s.setRange (lo, hi, step); s.setSliderStyle (juce::Slider::LinearHorizontal); s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 54, 22); s.setColour (juce::Slider::trackColourId, orbit()); s.setColour (juce::Slider::thumbColourId, accent()); s.setColour (juce::Slider::textBoxTextColourId, text()); s.setColour (juce::Slider::textBoxBackgroundColourId, surface()); addAndMakeVisible (s); };
    setup (bpmSlider, 40, 220, 1); setup (pitchSlider, 36, 96, 1); setup (durationSlider, 0.05, 30.0, 0.05); setup (speedSlider, -2, 2, .25); setup (radiusSlider, .5, 3, .05); setup (countSlider, 0, 16, 1);
    durationSlider.setTextValueSuffix (" s");
    for (int value = 4; value <= 24; ++value) columnsBox.addItem (juce::String (value), value);
    for (int value = 4; value <= 16; ++value) rowsBox.addItem (juce::String (value), value);
    playbackBox.addItem ("Tuned synth", 1); playbackBox.addItem ("SuperCollider", 2); playbackBox.addItem ("Pure Data", 3);
    for (auto* box : { &columnsBox, &rowsBox, &playbackBox }) { box->setColour (juce::ComboBox::backgroundColourId, surface()); box->setColour (juce::ComboBox::textColourId, text()); box->setColour (juce::ComboBox::outlineColourId, grid()); addAndMakeVisible (*box); }
    codeEditor.setMultiLine (true); codeEditor.setReturnKeyStartsNewLine (true); codeEditor.setScrollbarsShown (true);
    codeEditor.setFont (juce::FontOptions (12.0f));
    codeEditor.setColour (juce::TextEditor::backgroundColourId, bg()); codeEditor.setColour (juce::TextEditor::textColourId, text());
    codeEditor.setColour (juce::TextEditor::outlineColourId, grid()); codeEditor.setColour (juce::TextEditor::focusedOutlineColourId, accent());
    addAndMakeVisible (codeEditor);
    bpmSlider.onValueChange = [this] { if (! suppress) { document.bpm = bpmSlider.getValue(); changed(); } };
    columnsBox.onChange = [this] { if (! suppress && columnsBox.getSelectedId() > 0) { document.columns = columnsBox.getSelectedId(); changed(); } };
    rowsBox.onChange = [this] { if (! suppress && rowsBox.getSelectedId() > 0) { document.rows = rowsBox.getSelectedId(); changed(); } };
    pitchSlider.onValueChange = [this] { if (! suppress) if (auto* i = selectedItem()) { i->midi = (int) pitchSlider.getValue(); changed(); } };
    playbackBox.onChange = [this]
    {
        if (suppress || playbackBox.getSelectedId() <= 0) return;
        if (auto* i = selectedItem())
        {
            i->playback = static_cast<CarouselDocument::PlaybackType> (playbackBox.getSelectedId() - 1);
            ensureToneSources (*i); changed(); refreshInspector();
        }
    };
    durationSlider.onValueChange = [this] { if (! suppress) if (auto* i = selectedItem()) { i->durationSeconds = static_cast<float> (durationSlider.getValue()); changed(); } };
    codeEditor.onTextChange = [this]
    {
        if (suppress) return;
        if (auto* i = selectedItem())
        {
            if (i->playback == CarouselDocument::PlaybackType::superCollider) i->scCode = codeEditor.getText();
            else if (i->playback == CarouselDocument::PlaybackType::pureData) i->pdPatch = codeEditor.getText();
            changed();
        }
    };
    resetCodeButton.onClick = [this] { resetSelectedToneCode(); };
    soundButton.onClick = [this] { openSelectedToneSoundEditor(); };
    auditionButton.onClick = [this] { if (const auto* item = selectedItem()) triggerTone (*item); };
    speedSlider.onValueChange = [this] { if (! suppress) if (auto* i = selectedItem()) { i->speed = (float) speedSlider.getValue(); changed(); } };
    radiusSlider.onValueChange = [this] { if (! suppress) if (auto* i = selectedItem()) { i->radius = (float) radiusSlider.getValue(); arrangeOrbit (i->id); positionPlanksForOrbit (i->id); changed(); } };
    countSlider.onValueChange = [this] { if (! suppress) setOrbitToneCount ((int) countSlider.getValue()); };
    euclideanButton.setColour (juce::ToggleButton::textColourId, text()); addAndMakeVisible (euclideanButton);
    euclideanButton.onClick = [this] { if (! suppress) if (auto* i = selectedItem()) { i->euclidean = euclideanButton.getToggleState(); arrangeOrbit (i->id); changed(); } };
    setDocument (CarouselDocument::createDefault()); startTimerHz (60);
}

CarouselEditorComponent::~CarouselEditorComponent() { stopTimer(); }
void CarouselEditorComponent::setDocument (const CarouselDocument& d) { suppress = true; document = d; for (auto& item : document.items) ensureToneSources (item); selected = -1; bpmSlider.setValue (d.bpm); columnsBox.setSelectedId (d.columns, juce::dontSendNotification); rowsBox.setSelectedId (d.rows, juce::dontSendNotification); suppress = false; refreshInspector(); repaint(); }
void CarouselEditorComponent::setRunning (bool value) { running = value; contacts.clear(); lastTime = juce::Time::getMillisecondCounterHiRes(); playButton.setButtonText (running ? "stop" : "play"); repaint(); }
void CarouselEditorComponent::setTool (Tool value) { tool = value; repaint(); }
CarouselDocument::Item* CarouselEditorComponent::selectedItem() { for (auto& i : document.items) if (i.id == selected) return &i; return nullptr; }
const CarouselDocument::Item* CarouselEditorComponent::selectedItem() const { for (const auto& i : document.items) if (i.id == selected) return &i; return nullptr; }
juce::String CarouselEditorComponent::noteName (int midi) { static const char* n[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; return n[midi % 12] + juce::String (midi / 12 - 1); }
juce::Colour CarouselEditorComponent::itemColour (const CarouselDocument::Item& i) { static const juce::Colour c[] { juce::Colour (0xffffc857), juce::Colour (0xff47b8ff), juce::Colour (0xff9ee85f), juce::Colour (0xffb987ff) }; return c[i.voice & 3]; }
juce::Rectangle<float> CarouselEditorComponent::fieldViewport() const
{
    constexpr float header = 58.0f, rail = 72.0f, inspector = 286.0f, margin = 16.0f;
    return { rail + margin, header + margin,
             juce::jmax (120.0f, (float) getWidth() - rail - inspector - margin * 2.0f),
             juce::jmax (120.0f, (float) getHeight() - header - margin * 2.0f) };
}
float CarouselEditorComponent::cellSize() const { const auto v=fieldViewport();return juce::jmin (v.getWidth()/static_cast<float>(document.columns),v.getHeight()/static_cast<float>(document.rows))*zoom; }
juce::Rectangle<float> CarouselEditorComponent::gridBounds() const { const auto v=fieldViewport();const auto c=cellSize(),w=c*static_cast<float>(document.columns),h=c*static_cast<float>(document.rows);return {v.getCentreX()-w*.5f+pan.x,v.getCentreY()-h*.5f+pan.y,w,h}; }
juce::Point<float> CarouselEditorComponent::screenPoint (juce::Point<float> p) const { const auto b = gridBounds(); const auto c = cellSize(); return { b.getX() + p.x * c, b.getY() + p.y * c }; }
juce::Point<float> CarouselEditorComponent::gridPoint (juce::Point<float> p) const { const auto b = gridBounds(); const auto c = cellSize(); return { (p.x - b.getX()) / c, (p.y - b.getY()) / c }; }

void CarouselEditorComponent::paint (juce::Graphics& g)
{
    g.fillAll (bg());
    g.setColour(surface()); g.fillRect (0, 0, getWidth(), 58);
    g.setColour(grid().withAlpha(.72f)); g.drawHorizontalLine(57,0,(float)getWidth());

    const auto rail = juce::Rectangle<float> (0.0f, 58.0f, 72.0f, (float) getHeight() - 58.0f);
    g.setColour (surface().withAlpha (.92f)); g.fillRect (rail);
    g.setColour (grid().withAlpha (.58f)); g.drawVerticalLine (71, rail.getY(), rail.getBottom());
    g.drawHorizontalLine (fitButton.getY() - 9, 12.0f, 60.0f);

    const auto inspector=juce::Rectangle<float>((float)getWidth()-286.0f,58.0f,286.0f,(float)getHeight()-58.0f);
    g.setColour(surface().withAlpha(.82f)); g.fillRect(inspector);
    g.setColour(grid().withAlpha(.72f)); g.drawVerticalLine((int)inspector.getX(),inspector.getY(),inspector.getBottom());

    auto transport = playButton.getBounds().getUnion (bpmSlider.getBounds()).expanded (7, 4).toFloat();
    g.setColour (bg().withAlpha (.58f)); g.fillRoundedRectangle (transport, 7.0f);
    g.setColour (grid().withAlpha (.52f)); g.drawRoundedRectangle (transport, 7.0f, .8f);

    g.saveState();g.reduceClipRegion(fieldViewport().toNearestInt());
    auto b = gridBounds();
    g.setColour (juce::Colours::black.withAlpha (.18f)); g.fillRoundedRectangle (b.expanded (5).translated (0, 2), 8.0f);
    g.setColour (surface().withAlpha (.72f)); g.fillRoundedRectangle (b, 5.0f);
    const auto subdivisionSize = cellSize() / (float) carouselGridSubdivisions;
    if (subdivisionSize >= 4.0f)
    {
        g.setColour (grid().withAlpha (.28f));
        for (int x = 1; x < document.columns * carouselGridSubdivisions; ++x)
            if (x % carouselGridSubdivisions != 0)
                g.drawVerticalLine (juce::roundToInt (b.getX() + (float) x * subdivisionSize), b.getY(), b.getBottom());
        for (int y = 1; y < document.rows * carouselGridSubdivisions; ++y)
            if (y % carouselGridSubdivisions != 0)
                g.drawHorizontalLine (juce::roundToInt (b.getY() + (float) y * subdivisionSize), b.getX(), b.getRight());
    }
    g.setColour (grid().withAlpha(.82f));
    for (int x = 0; x <= document.columns; ++x) g.drawVerticalLine ((int) (b.getX() + static_cast<float> (x) * cellSize()), b.getY(), b.getBottom());
    for (int y = 0; y <= document.rows; ++y) g.drawHorizontalLine ((int) (b.getY() + static_cast<float> (y) * cellSize()), b.getX(), b.getRight());
    for (const auto& i : document.items)
    {
        const auto p = screenPoint ({ i.x, i.y }); const bool sel = i.id == selected;
        if (i.type == CarouselDocument::ItemType::orbit) { g.setColour ((sel ? accent() : orbit()).withAlpha (.86f)); g.drawEllipse (p.x-i.radius*cellSize(),p.y-i.radius*cellSize(),i.radius*cellSize()*2,i.radius*cellSize()*2,sel?2.2f:1.2f); g.fillEllipse (p.x-4,p.y-4,8,8); for(int k=0;k<3;k++){const auto a=i.phase+static_cast<float>(k)*juce::MathConstants<float>::twoPi/3.0f;g.drawLine(p.x,p.y,p.x+std::cos(a)*18,p.y+std::sin(a)*18,1.2f);} }
        else if (i.type == CarouselDocument::ItemType::post) { g.setColour (sel ? accent() : post()); g.fillEllipse (p.x-7,p.y-7,14,14); g.drawEllipse (p.x-13,p.y-13,26,26,1); }
        else
        {
            if (i.type == CarouselDocument::ItemType::plank)
                if (const auto owner = std::find_if (document.items.begin(), document.items.end(), [&i] (const auto& candidate) { return candidate.id == i.ownerOrbit && candidate.type == CarouselDocument::ItemType::orbit; }); owner != document.items.end())
                {
                    const auto rim = screenPoint ({ owner->x + std::cos (i.phase) * owner->radius,
                                                    owner->y + std::sin (i.phase) * owner->radius });
                    g.setColour (juce::Colours::black.withAlpha (.52f)); g.drawLine ({ rim, p }, 8.0f);
                    g.setColour (itemColour (i).withAlpha (.92f)); g.drawLine ({ rim, p }, 5.0f);
                }
            if (i.type == CarouselDocument::ItemType::plank)
            {
                g.setColour (surface()); g.fillEllipse (p.x-9,p.y-9,18,18);
                g.setColour (sel ? accent() : itemColour (i)); g.drawEllipse (p.x-9,p.y-9,18,18,2.0f);
                g.fillEllipse (p.x-2,p.y-2,4,4);
            }
            else
            {
                g.setColour (itemColour (i)); g.fillEllipse (p.x-12,p.y-12,24,24); g.setColour (bg()); g.setFont (10); g.drawText (noteName (i.midi), juce::Rectangle<float> (p.x-22,p.y-7,44.0f,14.0f), juce::Justification::centred); if (i.playback != CarouselDocument::PlaybackType::synth) { const auto tag = i.playback == CarouselDocument::PlaybackType::superCollider ? "SC" : "PD"; g.setColour (surface()); g.fillRoundedRectangle (p.x+7,p.y-17,19,11,3); g.setColour (itemColour(i)); g.setFont (juce::FontOptions (7.5f).withStyle ("Bold")); g.drawText (tag, juce::Rectangle<float> (p.x+7,p.y-17,19,11), juce::Justification::centred); } if (const auto active = activeToneUntilMs.find (i.id); active != activeToneUntilMs.end() && active->second > juce::Time::getMillisecondCounterHiRes()) { g.setColour (juce::Colours::white.withAlpha (0.88f)); g.drawEllipse (p.x-20,p.y-20,40,40,3.0f); } if(sel){g.setColour(accent());g.drawEllipse(p.x-17,p.y-17,34,34,1.5f);}
            }
        }
    }
    g.restoreState();
}

void CarouselEditorComponent::resized()
{
    titleLabel.setBounds (18, 10, 126, 36);
    selectButton.setBounds(9,74,54,40);toneButton.setBounds(9,122,54,40);orbitButton.setBounds(9,170,54,40);postButton.setBounds(9,218,54,40);plankButton.setBounds(9,266,54,40);
    fitButton.setBounds(9,330,54,38);clearButton.setBounds(9,376,54,38);
    globalLabel.setVisible(false);
    playButton.setBounds(160,13,68,32);
    bpmLabel.setBounds(244,13,28,32);bpmSlider.setBounds(274,13,126,32);
    columnsLabel.setBounds(420,13,46,32);columnsBox.setBounds(468,13,72,32);
    rowsLabel.setBounds(558,13,32,32);rowsBox.setBounds(592,13,72,32);
    const int ix=getWidth()-266, iw=246;selectionLabel.setBounds(ix,78,iw,18);detailLabel.setBounds(ix,102,iw,28);
    soundButton.setBounds(ix,142,(iw-8)/2,34);auditionButton.setBounds(ix+(iw-8)/2+8,142,(iw-8)/2,34);
    speedLabel.setBounds(ix,150,iw,16);speedSlider.setBounds(ix,166,iw,28);radiusLabel.setBounds(ix,206,iw,16);radiusSlider.setBounds(ix,222,iw,28);countLabel.setBounds(ix,262,iw,16);countSlider.setBounds(ix,278,iw,28);euclideanButton.setBounds(ix,318,iw,28);deleteButton.setBounds(ix,getHeight()-58,iw,30);
}

void CarouselEditorComponent::mouseDown (const juce::MouseEvent& e)
{
    if(!fieldViewport().contains(e.position))return;grabKeyboardFocus(); if(e.mods.isRightButtonDown()||e.mods.isMiddleButtonDown()){panning=true;mouseStart=e.position;panStart=pan;return;}
    const auto q=gridPoint(e.position); CarouselDocument::Item* near=nullptr;float best=.45f;for(auto i=document.items.rbegin();i!=document.items.rend();++i){const auto d=std::hypot(i->x-q.x,i->y-q.y);if(d<best){best=d;near=&*i;}}
    const auto attachingToPlank = near != nullptr && near->type == CarouselDocument::ItemType::plank && (tool == Tool::tone || tool == Tool::orbit);
    if(near && !attachingToPlank){selected=near->id;if(tool==Tool::select){dragged=near->id;dragOffset={near->x-q.x,near->y-q.y};if(e.getNumberOfClicks()>1&&near->type==CarouselDocument::ItemType::tone)triggerTone(*near);}else if(near->type==CarouselDocument::ItemType::tone)triggerTone(*near);refreshInspector();repaint();return;}
    if(tool==Tool::select){selected=-1;refreshInspector();repaint();return;}

    CarouselDocument::Item n;
    n.id=document.nextId++;
    n.type=tool==Tool::tone?CarouselDocument::ItemType::tone:tool==Tool::orbit?CarouselDocument::ItemType::orbit:tool==Tool::post?CarouselDocument::ItemType::post:CarouselDocument::ItemType::plank;
    n.x=juce::jlimit(0.0f,(float)document.columns,snapCarouselCoordinate(q.x));n.y=juce::jlimit(0.0f,(float)document.rows,snapCarouselCoordinate(q.y));
    n.midi=48+(n.id%8)*3;n.voice=n.id%4;
    if ((n.type == CarouselDocument::ItemType::orbit || n.type == CarouselDocument::ItemType::tone) && attachingToPlank)
    {
        n.x = near->x; n.y = near->y; n.ownerOrbit = near->id;
    }
    if (n.type == CarouselDocument::ItemType::plank)
    {
        const auto owner = std::min_element (document.items.begin(), document.items.end(), [&q] (const auto& a, const auto& b)
        {
            const auto da = a.type == CarouselDocument::ItemType::orbit ? std::hypot (a.x-q.x,a.y-q.y) : std::numeric_limits<float>::max();
            const auto db = b.type == CarouselDocument::ItemType::orbit ? std::hypot (b.x-q.x,b.y-q.y) : std::numeric_limits<float>::max();
            return da < db;
        });
        if (owner == document.items.end() || owner->type != CarouselDocument::ItemType::orbit)
        {
            --document.nextId; detailLabel.setText ("Place a carousel before adding a plank", juce::dontSendNotification); return;
        }
        n.ownerOrbit = owner->id;
        updatePlankGeometry (n);
    }
    ensureToneSources(n);selected=n.id;document.items.push_back(n);changed();refreshInspector();
}
void CarouselEditorComponent::mouseDrag (const juce::MouseEvent& e) { if(panning){pan=panStart+(e.position-mouseStart);repaint();return;}if(dragged<0)return;const auto q=gridPoint(e.position);for(auto&i:document.items)if(i.id==dragged){const auto nx=juce::jlimit(0.0f,(float)document.columns,q.x+dragOffset.x),ny=juce::jlimit(0.0f,(float)document.rows,q.y+dragOffset.y),dx=nx-i.x,dy=ny-i.y;i.x=nx;i.y=ny;translateDependents(i.id,dx,dy);changed();break;} }
void CarouselEditorComponent::mouseUp (const juce::MouseEvent&) { if(panning){panning=false;return;}if(auto*i=selectedItem()){i->x=snapCarouselCoordinate(i->x);i->y=snapCarouselCoordinate(i->y);if(i->type==CarouselDocument::ItemType::orbit)arrangeOrbit(i->id);else if(i->type==CarouselDocument::ItemType::plank)updatePlankGeometry(*i);if(i->ownerOrbit>=0&&(i->type==CarouselDocument::ItemType::tone||i->type==CarouselDocument::ItemType::orbit))if(const auto mount=std::find_if(document.items.begin(),document.items.end(),[i](const auto& candidate){return candidate.id==i->ownerOrbit&&candidate.type==CarouselDocument::ItemType::plank;});mount!=document.items.end()){const auto dx=mount->x-i->x,dy=mount->y-i->y;i->x=mount->x;i->y=mount->y;translateDependents(i->id,dx,dy);}}dragged=-1;changed(); }
void CarouselEditorComponent::mouseWheelMove (const juce::MouseEvent& e,const juce::MouseWheelDetails&w){if(!fieldViewport().contains(e.position))return;const auto before=gridPoint(e.position);const auto old=zoom;zoom=juce::jlimit(.45f,3.0f,zoom*std::pow(1.15f,w.deltaY*4));if(!juce::approximatelyEqual(old,zoom))pan+=e.position-screenPoint(before);repaint();}
bool CarouselEditorComponent::keyPressed (const juce::KeyPress& k){if(k==juce::KeyPress::backspaceKey||k==juce::KeyPress::deleteKey){deleteSelected();return true;}if(k==juce::KeyPress::spaceKey){setRunning(!running);return true;}return false;}
void CarouselEditorComponent::deleteSelected(){const int id=selected;if(id<0)return;std::set<int> removed{id};for(bool added=true;added;){added=false;for(const auto&i:document.items)if(i.ownerOrbit>=0&&removed.count(i.ownerOrbit)&&!removed.count(i.id)){removed.insert(i.id);added=true;}}document.items.erase(std::remove_if(document.items.begin(),document.items.end(),[&removed](const auto&i){return removed.count(i.id)>0;}),document.items.end());selected=-1;changed();refreshInspector();}
int CarouselEditorComponent::orbitToneCount(int id)const{int n=0;for(const auto&i:document.items)if(i.type==CarouselDocument::ItemType::tone&&i.ownerOrbit==id)++n;return n;}
void CarouselEditorComponent::setOrbitToneCount(int count){auto*o=selectedItem();if(!o||o->type!=CarouselDocument::ItemType::orbit)return;int n=orbitToneCount(o->id);while(n<count){CarouselDocument::Item i;i.id=document.nextId++;i.type=CarouselDocument::ItemType::tone;i.ownerOrbit=o->id;i.midi=48+(n%8)*3;i.voice=n%4;ensureToneSources(i);document.items.push_back(i);++n;}while(n>count){auto it=std::find_if(document.items.rbegin(),document.items.rend(),[o](const auto&i){return i.type==CarouselDocument::ItemType::tone&&i.ownerOrbit==o->id;});if(it==document.items.rend())break;document.items.erase(std::next(it).base());--n;}arrangeOrbit(o->id);changed();}
void CarouselEditorComponent::arrangeOrbit(int id){auto o=std::find_if(document.items.begin(),document.items.end(),[id](const auto&i){return i.id==id&&i.type==CarouselDocument::ItemType::orbit;});if(o==document.items.end())return;std::vector<CarouselDocument::Item*> tones;for(auto&i:document.items)if(i.type==CarouselDocument::ItemType::tone&&i.ownerOrbit==id)tones.push_back(&i);for(size_t n=0;n<tones.size();++n){const auto pos=o->euclidean?(float)((int)n*16/(int)tones.size())/16.0f:(float)n/(float)tones.size(),a=pos*juce::MathConstants<float>::twoPi;tones[n]->x=o->x+std::cos(a)*o->radius;tones[n]->y=o->y+std::sin(a)*o->radius;}}
void CarouselEditorComponent::updatePlankGeometry (CarouselDocument::Item& plank)
{
    if (plank.type != CarouselDocument::ItemType::plank) return;
    const auto owner = std::find_if (document.items.begin(), document.items.end(), [&plank] (const auto& item) { return item.id == plank.ownerOrbit && item.type == CarouselDocument::ItemType::orbit; });
    if (owner == document.items.end()) return;
    const auto oldX = plank.x, oldY = plank.y;
    auto dx = plank.x - owner->x, dy = plank.y - owner->y;
    auto centreDistance = std::hypot (dx, dy);
    plank.phase = centreDistance > .001f ? std::atan2 (dy, dx) : plank.phase;
    plank.radius = juce::jmax (.25f, centreDistance - owner->radius);
    centreDistance = owner->radius + plank.radius;
    plank.x = owner->x + std::cos (plank.phase) * centreDistance;
    plank.y = owner->y + std::sin (plank.phase) * centreDistance;
    translateDependents (plank.id, plank.x - oldX, plank.y - oldY);
}
void CarouselEditorComponent::positionPlanksForOrbit (int orbitId)
{
    const auto owner = std::find_if (document.items.begin(), document.items.end(), [orbitId] (const auto& item) { return item.id == orbitId && item.type == CarouselDocument::ItemType::orbit; });
    if (owner == document.items.end()) return;
    for (auto& plank : document.items)
        if (plank.type == CarouselDocument::ItemType::plank && plank.ownerOrbit == orbitId)
        {
            const auto oldX = plank.x, oldY = plank.y;
            const auto centreDistance = owner->radius + plank.radius;
            plank.x = owner->x + std::cos (plank.phase) * centreDistance;
            plank.y = owner->y + std::sin (plank.phase) * centreDistance;
            translateDependents (plank.id, plank.x - oldX, plank.y - oldY);
        }
}
void CarouselEditorComponent::translateDependents (int parentId, float dx, float dy)
{
    if (juce::approximatelyEqual (dx, 0.0f) && juce::approximatelyEqual (dy, 0.0f)) return;
    std::vector<int> children;
    for (auto& item : document.items)
        if (item.ownerOrbit == parentId)
        {
            item.x += dx; item.y += dy; children.push_back (item.id);
        }
    for (const auto childId : children) translateDependents (childId, dx, dy);
}
void CarouselEditorComponent::changed(){repaint();if(!suppress&&onChange)onChange(document);}

void CarouselEditorComponent::triggerTone (const CarouselDocument::Item& item)
{
    activeToneUntilMs[item.id] = juce::Time::getMillisecondCounterHiRes() + 360.0;
    repaint();
    if (onTone) onTone (item);
}

void CarouselEditorComponent::openSelectedToneSoundEditor()
{
    const auto* item = selectedItem();
    if (item == nullptr || item->type != CarouselDocument::ItemType::tone) return;
    if (openFullSoundEditor (item->id)) return;

    MusicalObjectSound state;
    state.playback = static_cast<MusicalObjectSound::Playback> ((int) item->playback);
    state.midiNote = item->midi;
    state.durationSeconds = item->durationSeconds;
    state.scSource = item->scCode;
    state.pdSource = item->pdPatch;

    const auto toneId = item->id;
    auto editor = std::make_unique<MusicalObjectEditorComponent> (std::move (state));
    const auto safeThis = juce::Component::SafePointer<CarouselEditorComponent> (this);
    auto editorRef = std::make_shared<juce::Component::SafePointer<MusicalObjectEditorComponent>> (editor.get());
    auto externalEditorPending = std::make_shared<bool> (false);
    editor->onChange = [safeThis, toneId, editorRef, externalEditorPending] (const MusicalObjectSound& sound)
    {
        if (safeThis == nullptr) return;
        for (auto& tone : safeThis->document.items)
            if (tone.id == toneId)
            {
                tone.playback = static_cast<CarouselDocument::PlaybackType> ((int) sound.playback);
                tone.midi = sound.midiNote;
                tone.durationSeconds = sound.durationSeconds;
                tone.scCode = sound.scSource;
                tone.pdPatch = sound.pdSource;
                safeThis->changed();
                safeThis->refreshInspector();
                if (tone.playback != CarouselDocument::PlaybackType::synth && ! *externalEditorPending)
                {
                    *externalEditorPending = true;
                    juce::MessageManager::callAsync ([safeThis, toneId, editorRef]
                    {
                        if (auto* editorComponent = editorRef->getComponent())
                            if (auto* callout = editorComponent->findParentComponentOfClass<juce::CallOutBox>())
                                callout->dismiss();
                        if (safeThis != nullptr) safeThis->openFullSoundEditor (toneId);
                    });
                }
                break;
            }
    };
    editor->onPreview = [safeThis, toneId] (const MusicalObjectSound&)
    {
        if (safeThis == nullptr) return;
        for (const auto& tone : safeThis->document.items)
            if (tone.id == toneId) { safeThis->triggerTone (tone); break; }
    };
    juce::CallOutBox::launchAsynchronously (std::move (editor), soundButton.getScreenBounds(), this);
}

bool CarouselEditorComponent::openFullSoundEditor (int toneId)
{
    const auto found = std::find_if (document.items.begin(), document.items.end(), [toneId] (const auto& item)
    {
        return item.id == toneId && item.type == CarouselDocument::ItemType::tone;
    });
    if (found == document.items.end()) return false;

    const auto isSc = found->playback == CarouselDocument::PlaybackType::superCollider;
    const auto isPd = found->playback == CarouselDocument::PlaybackType::pureData;
    auto& request = isSc ? onScEditorRequested : onPdEditorRequested;
    if ((! isSc && ! isPd) || request == nullptr) return false;

    const auto source = isSc ? found->scCode : found->pdPatch;
    const auto safeThis = juce::Component::SafePointer<CarouselEditorComponent> (this);
    request (source, found->durationSeconds,
             [safeThis, toneId, isSc] (const juce::String& newSource, float newDuration)
    {
        if (safeThis == nullptr) return;
        for (auto& tone : safeThis->document.items)
            if (tone.id == toneId && tone.type == CarouselDocument::ItemType::tone)
            {
                if (isSc) tone.scCode = newSource;
                else      tone.pdPatch = newSource;
                tone.durationSeconds = newDuration;
                safeThis->changed();
                safeThis->refreshInspector();
                break;
            }
    });
    return true;
}

void CarouselEditorComponent::resetSelectedToneCode()
{
    if (auto* item = selectedItem())
    {
        if (item->playback == CarouselDocument::PlaybackType::superCollider)
            item->scCode = defaultCarouselScCode();
        else if (item->playback == CarouselDocument::PlaybackType::pureData)
            item->pdPatch = defaultCarouselPdPatch();
        else
            return;

        codeEditor.setText (item->playback == CarouselDocument::PlaybackType::superCollider ? item->scCode : item->pdPatch, false);
        changed();
    }
}

void CarouselEditorComponent::refreshInspector()
{
    suppress = true;
    const auto* item = selectedItem();
    const auto tone = item != nullptr && item->type == CarouselDocument::ItemType::tone;
    const auto orbitItem = item != nullptr && item->type == CarouselDocument::ItemType::orbit;
    const auto codeTone = tone && item->playback != CarouselDocument::PlaybackType::synth;

    detailLabel.setText (! item ? "Select or place an object"
                                   : tone ? "Tone  " + noteName (item->midi)
                                          : orbitItem ? "Orbit field" : item->type == CarouselDocument::ItemType::plank ? "Empty plank mount" : "Collision post",
                         juce::dontSendNotification);

    playbackLabel.setVisible (false); playbackBox.setVisible (false);
    pitchLabel.setVisible (false); pitchSlider.setVisible (false);
    durationLabel.setVisible (false); durationSlider.setVisible (false);
    codeLabel.setVisible (false); codeEditor.setVisible (false); resetCodeButton.setVisible (false);
    soundButton.setVisible (tone); auditionButton.setVisible (tone);
    speedLabel.setVisible (orbitItem); speedSlider.setVisible (orbitItem);
    radiusLabel.setVisible (orbitItem); radiusSlider.setVisible (orbitItem);
    countLabel.setVisible (orbitItem); countSlider.setVisible (orbitItem);
    euclideanButton.setVisible (orbitItem); deleteButton.setVisible (item != nullptr);

    if (item != nullptr)
    {
        playbackBox.setSelectedId (static_cast<int> (item->playback) + 1, juce::dontSendNotification);
        pitchSlider.setValue (item->midi);
        durationSlider.setValue (item->durationSeconds);
        speedSlider.setValue (item->speed); radiusSlider.setValue (item->radius);
        countSlider.setValue (orbitToneCount (item->id));
        euclideanButton.setToggleState (item->euclidean, juce::dontSendNotification);
        if (codeTone)
            codeEditor.setText (item->playback == CarouselDocument::PlaybackType::superCollider ? item->scCode : item->pdPatch, false);
    }
    else
    {
        playbackBox.setSelectedId (0, juce::dontSendNotification);
        codeEditor.clear();
    }
    suppress = false;
}

void CarouselEditorComponent::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto activeCount = activeToneUntilMs.size();
    for (auto it = activeToneUntilMs.begin(); it != activeToneUntilMs.end();)
        it = it->second <= now ? activeToneUntilMs.erase (it) : std::next (it);
    if (activeCount != activeToneUntilMs.size()) repaint();
    if (! running) { lastTime = now; return; }
    const auto dt = static_cast<float> (juce::jmin (.035, (now - lastTime) / 1000.0));
    lastTime = now;
    std::set<juce::String> current;
    for (auto& orbitItem : document.items)
        if (orbitItem.type == CarouselDocument::ItemType::orbit)
        {
            const auto angleDelta = juce::MathConstants<float>::twoPi * static_cast<float> (document.bpm / 60.0) * orbitItem.speed * dt;
            orbitItem.phase += angleDelta;
            for (auto& tone : document.items)
                if ((tone.type == CarouselDocument::ItemType::plank && tone.ownerOrbit == orbitItem.id)
                    || (tone.type == CarouselDocument::ItemType::tone
                        && (tone.ownerOrbit == orbitItem.id || tone.ownerOrbit < 0)
                        && std::hypot (tone.x - orbitItem.x, tone.y - orbitItem.y) < orbitItem.radius + .35f))
                {
                    const auto radius = std::hypot (tone.x - orbitItem.x, tone.y - orbitItem.y);
                    const auto angle = std::atan2 (tone.y - orbitItem.y, tone.x - orbitItem.x) + angleDelta;
                    const auto oldX = tone.x, oldY = tone.y;
                    tone.x = orbitItem.x + std::cos (angle) * radius; tone.y = orbitItem.y + std::sin (angle) * radius;
                    if (tone.type == CarouselDocument::ItemType::plank) { tone.radius = juce::jmax (.25f, radius - orbitItem.radius); tone.phase = angle; }
                    translateDependents (tone.id, tone.x - oldX, tone.y - oldY);
                    const CarouselDocument::Item* sound = &tone;
                    if (tone.type == CarouselDocument::ItemType::plank)
                    {
                        const auto mountedTone = std::find_if (document.items.begin(), document.items.end(), [&tone] (const auto& item)
                        {
                            return item.type == CarouselDocument::ItemType::tone && item.ownerOrbit == tone.id;
                        });
                        if (mountedTone == document.items.end()) continue;
                        sound = &*mountedTone;
                    }
                    for (const auto& obstacle : document.items)
                        if (obstacle.id != tone.id && obstacle.id != sound->id && obstacle.id != orbitItem.id && obstacle.type != CarouselDocument::ItemType::orbit
                            && std::hypot (obstacle.x - sound->x, obstacle.y - sound->y) < .43f)
                        {
                            const auto key = juce::String (juce::jmin (sound->id, obstacle.id)) + ":" + juce::String (juce::jmax (sound->id, obstacle.id));
                            current.insert (key);
                            if (! contacts.count (key)) triggerTone (*sound);
                        }
                }
        }
    contacts = std::move (current);
    repaint();
}
