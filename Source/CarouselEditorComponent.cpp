#include "CarouselEditorComponent.h"

#include <algorithm>
#include <cmath>

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
}

CarouselDocument CarouselDocument::createDefault()
{
    CarouselDocument d;
    d.items = { { 1, ItemType::orbit, 4, 3, 60, 0, -1, 1.0f, 0.25f },
                { 2, ItemType::tone, 5, 3, 60, 1 },
                { 3, ItemType::tone, 2, 2, 55, 2 },
                { 4, ItemType::post, 7, 3 },
                { 5, ItemType::post, 8, 5 } };
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
        node.setProperty ("euclidean", item.euclidean, nullptr); root.addChild (node, -1, nullptr);
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
        if (node.hasType ("item")) d.items.push_back ({ static_cast<int> (node["id"]), static_cast<ItemType> (static_cast<int> (node["type"])),
            static_cast<float> (node["x"]), static_cast<float> (node["y"]), static_cast<int> (node["midi"]), static_cast<int> (node["voice"]),
            static_cast<int> (node["owner"]), static_cast<float> (node["radius"]), static_cast<float> (node["speed"]), static_cast<float> (node["phase"]), static_cast<bool> (node["euclidean"]) });
    return d;
}

CarouselEditorComponent::CarouselEditorComponent()
{
    setWantsKeyboardFocus (true); setFocusContainerType (FocusContainerType::focusContainer);
    for (auto* b : { &selectButton, &toneButton, &orbitButton, &postButton }) { addAndMakeVisible (*b); b->setClickingTogglesState (true); b->setRadioGroupId (901); }
    selectButton.setToggleState (true, juce::dontSendNotification);
    selectButton.onClick = [this] { setTool (Tool::select); }; toneButton.onClick = [this] { setTool (Tool::tone); };
    orbitButton.onClick = [this] { setTool (Tool::orbit); }; postButton.onClick = [this] { setTool (Tool::post); };
    for (auto* b : { &playButton, &clearButton, &deleteButton, &fitButton }) addAndMakeVisible (*b);
    playButton.onClick = [this] { setRunning (! running); }; clearButton.onClick = [this] { document.items.clear(); selected = -1; changed(); };
    deleteButton.onClick = [this] { deleteSelected(); };
    fitButton.onClick = [this] { zoom = 1.0f; pan = {}; repaint(); };
    titleLabel.setText ("Carousel", juce::dontSendNotification); titleLabel.setFont (juce::FontOptions (18.0f).withStyle ("Bold")); titleLabel.setColour (juce::Label::textColourId, text()); addAndMakeVisible (titleLabel);
    detailLabel.setColour (juce::Label::textColourId, muted()); detailLabel.setFont (juce::FontOptions (12.0f)); addAndMakeVisible (detailLabel);
    for (auto [label, caption] : std::initializer_list<std::pair<juce::Label*, const char*>> { {&globalLabel,"FIELD"},{&selectionLabel,"SELECTION"} }) { label->setText(caption,juce::dontSendNotification);label->setColour(juce::Label::textColourId,accent());label->setFont(juce::FontOptions(11.0f).withStyle("Bold"));addAndMakeVisible(*label); }
    for (auto [label, caption] : std::initializer_list<std::pair<juce::Label*, const char*>> { {&bpmLabel,"BPM"},{&columnsLabel,"Columns"},{&rowsLabel,"Rows"},{&pitchLabel,"Pitch"},{&speedLabel,"Tempo multiple"},{&radiusLabel,"Radius"},{&countLabel,"Orbit tones"} }) { label->setText(caption,juce::dontSendNotification);label->setColour(juce::Label::textColourId,muted());label->setFont(juce::FontOptions(11.0f));addAndMakeVisible(*label); }
    auto setup = [this] (juce::Slider& s, double lo, double hi, double step) { s.setRange (lo, hi, step); s.setSliderStyle (juce::Slider::LinearHorizontal); s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 54, 22); s.setColour (juce::Slider::trackColourId, orbit()); s.setColour (juce::Slider::thumbColourId, accent()); s.setColour (juce::Slider::textBoxTextColourId, text()); s.setColour (juce::Slider::textBoxBackgroundColourId, surface()); addAndMakeVisible (s); };
    setup (bpmSlider, 40, 220, 1); setup (pitchSlider, 36, 96, 1); setup (speedSlider, -2, 2, .25); setup (radiusSlider, .5, 3, .05); setup (countSlider, 0, 16, 1);
    for (int value = 4; value <= 24; ++value) columnsBox.addItem (juce::String (value), value);
    for (int value = 4; value <= 16; ++value) rowsBox.addItem (juce::String (value), value);
    for (auto* box : { &columnsBox, &rowsBox }) { box->setColour (juce::ComboBox::backgroundColourId, surface()); box->setColour (juce::ComboBox::textColourId, text()); box->setColour (juce::ComboBox::outlineColourId, grid()); addAndMakeVisible (*box); }
    bpmSlider.onValueChange = [this] { if (! suppress) { document.bpm = bpmSlider.getValue(); changed(); } };
    columnsBox.onChange = [this] { if (! suppress && columnsBox.getSelectedId() > 0) { document.columns = columnsBox.getSelectedId(); changed(); } };
    rowsBox.onChange = [this] { if (! suppress && rowsBox.getSelectedId() > 0) { document.rows = rowsBox.getSelectedId(); changed(); } };
    pitchSlider.onValueChange = [this] { if (! suppress) if (auto* i = selectedItem()) { i->midi = (int) pitchSlider.getValue(); changed(); } };
    speedSlider.onValueChange = [this] { if (! suppress) if (auto* i = selectedItem()) { i->speed = (float) speedSlider.getValue(); changed(); } };
    radiusSlider.onValueChange = [this] { if (! suppress) if (auto* i = selectedItem()) { i->radius = (float) radiusSlider.getValue(); arrangeOrbit (i->id); changed(); } };
    countSlider.onValueChange = [this] { if (! suppress) setOrbitToneCount ((int) countSlider.getValue()); };
    euclideanButton.setColour (juce::ToggleButton::textColourId, text()); addAndMakeVisible (euclideanButton);
    euclideanButton.onClick = [this] { if (! suppress) if (auto* i = selectedItem()) { i->euclidean = euclideanButton.getToggleState(); arrangeOrbit (i->id); changed(); } };
    setDocument (CarouselDocument::createDefault()); startTimerHz (60);
}

CarouselEditorComponent::~CarouselEditorComponent() { stopTimer(); }
void CarouselEditorComponent::setDocument (const CarouselDocument& d) { suppress = true; document = d; selected = -1; bpmSlider.setValue (d.bpm); columnsBox.setSelectedId (d.columns, juce::dontSendNotification); rowsBox.setSelectedId (d.rows, juce::dontSendNotification); suppress = false; refreshInspector(); repaint(); }
void CarouselEditorComponent::setRunning (bool value) { running = value; contacts.clear(); lastTime = juce::Time::getMillisecondCounterHiRes(); playButton.setButtonText (running ? "stop" : "play"); repaint(); }
void CarouselEditorComponent::setTool (Tool value) { tool = value; repaint(); }
CarouselDocument::Item* CarouselEditorComponent::selectedItem() { for (auto& i : document.items) if (i.id == selected) return &i; return nullptr; }
const CarouselDocument::Item* CarouselEditorComponent::selectedItem() const { for (const auto& i : document.items) if (i.id == selected) return &i; return nullptr; }
juce::String CarouselEditorComponent::noteName (int midi) { static const char* n[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; return n[midi % 12] + juce::String (midi / 12 - 1); }
juce::Colour CarouselEditorComponent::itemColour (const CarouselDocument::Item& i) { static const juce::Colour c[] { juce::Colour (0xffffc857), juce::Colour (0xff47b8ff), juce::Colour (0xff9ee85f), juce::Colour (0xffb987ff) }; return c[i.voice & 3]; }
juce::Rectangle<float> CarouselEditorComponent::fieldViewport() const { return juce::Rectangle<float> (18.0f, 74.0f, juce::jmax (120.0f, static_cast<float> (getWidth()) - 252.0f), juce::jmax (120.0f, static_cast<float> (getHeight()) - 92.0f)); }
float CarouselEditorComponent::cellSize() const { const auto v=fieldViewport();return juce::jmin (v.getWidth()/static_cast<float>(document.columns),v.getHeight()/static_cast<float>(document.rows))*zoom; }
juce::Rectangle<float> CarouselEditorComponent::gridBounds() const { const auto v=fieldViewport();const auto c=cellSize(),w=c*static_cast<float>(document.columns),h=c*static_cast<float>(document.rows);return {v.getCentreX()-w*.5f+pan.x,v.getCentreY()-h*.5f+pan.y,w,h}; }
juce::Point<float> CarouselEditorComponent::screenPoint (juce::Point<float> p) const { const auto b = gridBounds(); const auto c = cellSize(); return { b.getX() + p.x * c, b.getY() + p.y * c }; }
juce::Point<float> CarouselEditorComponent::gridPoint (juce::Point<float> p) const { const auto b = gridBounds(); const auto c = cellSize(); return { (p.x - b.getX()) / c, (p.y - b.getY()) / c }; }

void CarouselEditorComponent::paint (juce::Graphics& g)
{
    g.fillAll (bg());
    g.setColour(surface());g.fillRect(juce::Rectangle<float>(0,0,(float)getWidth(),60));
    g.setColour(grid().withAlpha(.7f));g.drawHorizontalLine(59,0,(float)getWidth());
    const auto inspector=juce::Rectangle<float>((float)getWidth()-218.0f,74.0f,200.0f,(float)getHeight()-92.0f);
    g.setColour(surface());g.fillRoundedRectangle(inspector,6.0f);g.setColour(grid().withAlpha(.9f));g.drawRoundedRectangle(inspector.reduced(.5f),6.0f,1.0f);
    g.saveState();g.reduceClipRegion(fieldViewport().toNearestInt());
    auto b = gridBounds(); g.setColour (surface()); g.fillRect (b); g.setColour (grid());
    for (int x = 0; x <= document.columns; ++x) g.drawVerticalLine ((int) (b.getX() + static_cast<float> (x) * cellSize()), b.getY(), b.getBottom());
    for (int y = 0; y <= document.rows; ++y) g.drawHorizontalLine ((int) (b.getY() + static_cast<float> (y) * cellSize()), b.getX(), b.getRight());
    for (const auto& i : document.items)
    {
        const auto p = screenPoint ({ i.x, i.y }); const bool sel = i.id == selected;
        if (i.type == CarouselDocument::ItemType::orbit) { g.setColour ((sel ? accent() : orbit()).withAlpha (.86f)); g.drawEllipse (p.x-i.radius*cellSize(),p.y-i.radius*cellSize(),i.radius*cellSize()*2,i.radius*cellSize()*2,sel?2.2f:1.2f); g.fillEllipse (p.x-4,p.y-4,8,8); for(int k=0;k<3;k++){const auto a=i.phase+static_cast<float>(k)*juce::MathConstants<float>::twoPi/3.0f;g.drawLine(p.x,p.y,p.x+std::cos(a)*18,p.y+std::sin(a)*18,1.2f);} }
        else if (i.type == CarouselDocument::ItemType::post) { g.setColour (sel ? accent() : post()); g.fillEllipse (p.x-7,p.y-7,14,14); g.drawEllipse (p.x-13,p.y-13,26,26,1); }
        else { g.setColour (itemColour (i)); g.fillEllipse (p.x-12,p.y-12,24,24); g.setColour (bg()); g.setFont (10); g.drawText (noteName (i.midi), juce::Rectangle<float> (p.x-22,p.y-7,44.0f,14.0f), juce::Justification::centred); if(sel){g.setColour(accent());g.drawEllipse(p.x-17,p.y-17,34,34,1.5f);} }
    }
    g.restoreState();
}

void CarouselEditorComponent::resized()
{
    titleLabel.setBounds (18, 14, 96, 28);
    int x=120;selectButton.setBounds(x,14,62,32);x+=68;toneButton.setBounds(x,14,54,32);x+=60;orbitButton.setBounds(x,14,54,32);x+=60;postButton.setBounds(x,14,54,32);x+=64;fitButton.setBounds(x,14,44,32);x+=52;clearButton.setBounds(x,14,68,32);
    globalLabel.setVisible(false);
    bpmLabel.setBounds(510,14,28,32);bpmSlider.setBounds(538,14,110,32);
    columnsLabel.setBounds(658,14,46,32);columnsBox.setBounds(704,14,72,32);
    rowsLabel.setBounds(790,14,32,32);rowsBox.setBounds(822,14,72,32);
    playButton.setBounds(getWidth()-78,14,60,32);
    const int ix=getWidth()-204;selectionLabel.setBounds(ix,90,174,18);detailLabel.setBounds(ix,112,174,22);pitchLabel.setBounds(ix,150,174,16);pitchSlider.setBounds(ix,166,174,28);speedLabel.setBounds(ix,150,174,16);speedSlider.setBounds(ix,166,174,28);radiusLabel.setBounds(ix,206,174,16);radiusSlider.setBounds(ix,222,174,28);countLabel.setBounds(ix,262,174,16);countSlider.setBounds(ix,278,174,28);euclideanButton.setBounds(ix,318,174,28);deleteButton.setBounds(ix,getHeight()-58,174,30);
}

void CarouselEditorComponent::mouseDown (const juce::MouseEvent& e)
{
    if(!fieldViewport().contains(e.position))return;grabKeyboardFocus(); if(e.mods.isRightButtonDown()||e.mods.isMiddleButtonDown()){panning=true;mouseStart=e.position;panStart=pan;return;}
    const auto q=gridPoint(e.position); CarouselDocument::Item* near=nullptr;float best=.45f;for(auto& i:document.items){const auto d=std::hypot(i.x-q.x,i.y-q.y);if(d<best){best=d;near=&i;}}
    if(near){selected=near->id;if(tool==Tool::select){dragged=near->id;dragOffset={near->x-q.x,near->y-q.y};}else if(near->type==CarouselDocument::ItemType::tone&&onNote)onNote(near->midi);refreshInspector();repaint();return;}
    if(tool==Tool::select){selected=-1;refreshInspector();repaint();return;} CarouselDocument::Item n; n.id=document.nextId++;n.type=tool==Tool::tone?CarouselDocument::ItemType::tone:tool==Tool::orbit?CarouselDocument::ItemType::orbit:CarouselDocument::ItemType::post;n.x=juce::jlimit(0.0f,(float)document.columns,std::round(q.x));n.y=juce::jlimit(0.0f,(float)document.rows,std::round(q.y));n.midi=48+(n.id%8)*3;n.voice=n.id%4;selected=n.id;document.items.push_back(n);changed();refreshInspector();
}
void CarouselEditorComponent::mouseDrag (const juce::MouseEvent& e) { if(panning){pan=panStart+(e.position-mouseStart);repaint();return;}if(dragged<0)return;const auto q=gridPoint(e.position);for(auto&i:document.items)if(i.id==dragged){const auto nx=juce::jlimit(0.0f,(float)document.columns,q.x+dragOffset.x),ny=juce::jlimit(0.0f,(float)document.rows,q.y+dragOffset.y),dx=nx-i.x,dy=ny-i.y;i.x=nx;i.y=ny;if(i.type==CarouselDocument::ItemType::orbit)for(auto&s:document.items)if(s.ownerOrbit==i.id){s.x+=dx;s.y+=dy;}changed();break;} }
void CarouselEditorComponent::mouseUp (const juce::MouseEvent&) { if(panning){panning=false;return;}if(auto*i=selectedItem()){i->x=std::round(i->x);i->y=std::round(i->y);if(i->type==CarouselDocument::ItemType::orbit)arrangeOrbit(i->id);}dragged=-1;changed(); }
void CarouselEditorComponent::mouseWheelMove (const juce::MouseEvent& e,const juce::MouseWheelDetails&w){if(!fieldViewport().contains(e.position))return;const auto before=gridPoint(e.position);const auto old=zoom;zoom=juce::jlimit(.45f,3.0f,zoom*std::pow(1.15f,w.deltaY*4));if(!juce::approximatelyEqual(old,zoom))pan+=e.position-screenPoint(before);repaint();}
bool CarouselEditorComponent::keyPressed (const juce::KeyPress& k){if(k==juce::KeyPress::backspaceKey||k==juce::KeyPress::deleteKey){deleteSelected();return true;}if(k==juce::KeyPress::spaceKey){setRunning(!running);return true;}return false;}
void CarouselEditorComponent::deleteSelected(){const int id=selected;if(id<0)return;document.items.erase(std::remove_if(document.items.begin(),document.items.end(),[id](const auto&i){return i.id==id||i.ownerOrbit==id;}),document.items.end());selected=-1;changed();refreshInspector();}
int CarouselEditorComponent::orbitToneCount(int id)const{int n=0;for(const auto&i:document.items)if(i.type==CarouselDocument::ItemType::tone&&i.ownerOrbit==id)++n;return n;}
void CarouselEditorComponent::setOrbitToneCount(int count){auto*o=selectedItem();if(!o||o->type!=CarouselDocument::ItemType::orbit)return;int n=orbitToneCount(o->id);while(n<count){CarouselDocument::Item i;i.id=document.nextId++;i.type=CarouselDocument::ItemType::tone;i.ownerOrbit=o->id;i.midi=48+(n%8)*3;i.voice=n%4;document.items.push_back(i);++n;}while(n>count){auto it=std::find_if(document.items.rbegin(),document.items.rend(),[o](const auto&i){return i.type==CarouselDocument::ItemType::tone&&i.ownerOrbit==o->id;});if(it==document.items.rend())break;document.items.erase(std::next(it).base());--n;}arrangeOrbit(o->id);changed();}
void CarouselEditorComponent::arrangeOrbit(int id){auto o=std::find_if(document.items.begin(),document.items.end(),[id](const auto&i){return i.id==id&&i.type==CarouselDocument::ItemType::orbit;});if(o==document.items.end())return;std::vector<CarouselDocument::Item*> tones;for(auto&i:document.items)if(i.ownerOrbit==id)tones.push_back(&i);for(size_t n=0;n<tones.size();++n){const auto pos=o->euclidean?(float)((int)n*16/(int)tones.size())/16.0f:(float)n/(float)tones.size(),a=pos*juce::MathConstants<float>::twoPi;tones[n]->x=o->x+std::cos(a)*o->radius;tones[n]->y=o->y+std::sin(a)*o->radius;}}
void CarouselEditorComponent::changed(){repaint();if(!suppress&&onChange)onChange(document);}
void CarouselEditorComponent::refreshInspector(){suppress=true;const auto*i=selectedItem();const bool tone=i&&i->type==CarouselDocument::ItemType::tone,orb=i&&i->type==CarouselDocument::ItemType::orbit;detailLabel.setText(!i?"Select or place an object":tone?"Tone  "+noteName(i->midi):orb?"Orbit field":"Collision post",juce::dontSendNotification);pitchSlider.setVisible(tone);pitchLabel.setVisible(tone);speedSlider.setVisible(orb);speedLabel.setVisible(orb);radiusSlider.setVisible(orb);radiusLabel.setVisible(orb);countSlider.setVisible(orb);countLabel.setVisible(orb);euclideanButton.setVisible(orb);deleteButton.setVisible(i);if(i){pitchSlider.setValue(i->midi);speedSlider.setValue(i->speed);radiusSlider.setValue(i->radius);countSlider.setValue(orbitToneCount(i->id));euclideanButton.setToggleState(i->euclidean,juce::dontSendNotification);}suppress=false;}
void CarouselEditorComponent::timerCallback(){const auto now=juce::Time::getMillisecondCounterHiRes();if(!running){lastTime=now;return;}const float dt=(float)juce::jmin(.035,(now-lastTime)/1000.0);lastTime=now;std::set<juce::String> current;for(auto&o:document.items)if(o.type==CarouselDocument::ItemType::orbit){const auto da=juce::MathConstants<float>::twoPi*(float)(document.bpm/60.0)*o.speed*dt;o.phase+=da;for(auto&s:document.items)if(s.type==CarouselDocument::ItemType::tone&&std::hypot(s.x-o.x,s.y-o.y)<o.radius+.35f){const auto r=std::hypot(s.x-o.x,s.y-o.y),a=std::atan2(s.y-o.y,s.x-o.x)+da;s.x=o.x+std::cos(a)*r;s.y=o.y+std::sin(a)*r;for(const auto&h:document.items)if(h.id!=s.id&&h.id!=o.id&&h.type!=CarouselDocument::ItemType::orbit&&std::hypot(h.x-s.x,h.y-s.y)<.43f){const auto key=juce::String(juce::jmin(s.id,h.id))+":"+juce::String(juce::jmax(s.id,h.id));current.insert(key);if(!contacts.count(key)&&onNote)onNote(s.midi);}}}contacts=std::move(current);repaint();}
