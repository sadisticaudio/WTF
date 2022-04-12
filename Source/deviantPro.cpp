#include "RoutingMatrix.h"

void sadistic::LeftEmpiricalLAF::drawLabel (Graphics& g, Label& label) {
    auto text { label.getText().getDoubleValue() };
    auto* parent = label.getParentComponent();
    auto parentBounds = parent->getLocalBounds();
    auto localArea = label.getLocalBounds().withCentre({label.getLocalBounds().getWidth()/2, parentBounds.getHeight()/2});
    auto textArea = localArea.reduced(5);
    g.setFont (getSadisticFont().withHeight(static_cast<float>(textArea.getHeight())));
    if (! label.isBeingEdited()) {
        g.setColour (Colours::grey.withAlpha(label.isEnabled() ? 1.0f : 0.5f));
        g.drawFittedText (freqString(text), textArea, Justification::centred, 1, 0.05f);
        g.setColour (Colours::white.darker());
    }
    else if (label.isEnabled()) {
        g.setColour (Colours::white.darker());
        g.drawFittedText (freqString(text), textArea, Justification::centred, 1, 0.05f);
    }
    Path rectangle;
    g.setColour(Colours::white.darker());
    rectangle.addRoundedRectangle(localArea, 5);
    g.strokePath(rectangle, PathStrokeType(3.f));
    
}

void sadistic::RightEmpiricalLAF::drawLabel (Graphics& g, Label& label) {
    auto text { label.getText().getDoubleValue() };
    auto* parent = label.getParentComponent();
    auto parentBounds = parent->getLocalBounds();
    auto localArea = label.getLocalBounds().withCentre({ label.getLocalBounds().getRight() - label.getLocalBounds().getWidth()/2, parentBounds.getHeight()/2});
    auto textArea = localArea.reduced(5);
    g.setFont (getSadisticFont().withHeight(static_cast<float>(textArea.getHeight())));
    if (! label.isBeingEdited()) {
        g.setColour (Colours::grey.withAlpha(label.isEnabled() ? 1.0f : 0.5f));
        g.drawFittedText (freqString(text), textArea, Justification::centred, 1, 0.15f);
        g.setColour (Colours::white.darker());
    }
    else if (label.isEnabled()) {
        g.setColour (Colours::white.darker());
        g.drawFittedText (freqString(text), textArea, Justification::centred, 1, 0.05f);
    }
    Path rectangle;
    g.setColour(Colours::white.darker());
    rectangle.addRoundedRectangle(localArea, 5);
    g.strokePath(rectangle, PathStrokeType(3.f));
}

void sadistic::showInteger111Value(Slider& slider, Label& valueLabel, Label& suffixLabel)   {
    valueLabel.setText (String(roundToInt(slider.getValue() * 111.0)), dontSendNotification);
    suffixLabel.setText (slider.getTextValueSuffix(), dontSendNotification);};

void sadistic::RoutingMatrix::InsertionPoint::itemDropped (const juce::DragAndDropTarget::SourceDetails& details) {
    std::cout << "ItemDropped!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n";
    if(details.sourceComponent != this) {
        for(int i = 0; i < numFX; ++i) {
            if(details.sourceComponent == &matrix.effects[i]){
                auto destinationRoute { effect ? (int)effect->routeSlider.getValue() : route };
                auto destinationIndex { effect ? (int)effect->indexSlider.getValue() : matrix.fx[route].size()-1 };
                matrix.moveEffect(&matrix.effects[i], destinationRoute, destinationIndex);
            }
        }
    }
    matrix.repaint();
}
bool sadistic::RoutingMatrix::EffectRoutingState::operator<(const EffectRoutingState& other) const {
    if (route < other.route) return true;
    else if (route == other.route && index < other.index) return true;
    return false;
}

void sadistic::RoutingMatrix::Effect::mouseDrag (const MouseEvent& e) {
    std::cout << "mouseDrag():" << getX() << " " << getY() << " " << e.x - getWidth()/2 << " " << e.y - getHeight()/2 << std::endl;
    matrix.startDragging(effectID, this);
}

void sadistic::RoutingMatrix::Effect::mouseDown (const MouseEvent& e) {
    std::cout << "mouseDrag():" << getX() << " " << getY() << " " << e.x - getWidth()/2 << " " << e.y - getHeight()/2 << std::endl;
    t1 = hi_res::now();
}

void sadistic::RoutingMatrix::Effect::mouseUp (const MouseEvent& e) {
    std::cout << "mouseUp():" << getX() << " " << getY() << " " << e.x - getWidth()/2 << " " << e.y - getHeight()/2 << std::endl;
    t2 = hi_res::now();
    duration<double, std::milli> ms_double = t2 - t1;
    if(ms_double.count() < 100.0) { bool enabled = (bool)enabledSlider.getValue(); enabledSlider.setValue(enabled ? false : true); if (!enabled) setAlpha(1.f); else setAlpha(0.5f); }
    for (auto& i : matrix.effects) i.insertionPoint.setTransparency(0.f);
    for (auto& i : matrix.endPoint) i.setTransparency(0.f);
    matrix.repaint();
}

void sadistic::RoutingMatrix::InsertionPoint::itemDragEnter (const SourceDetails& /*dragSourceDetails*/) {
    std::cout << "DragAndDropCarrier():: itemDragEnter()" << std::endl;
    if (effect) {
        effect->setBounds(effect->getBounds().translated(20, 0));
        int idx { (int)effect->indexSlider.getValue() };
        if (idx < matrix.fx[route].size()-1 && idx > 0) matrix.fx[route][idx-1]->setBounds(matrix.fx[route][idx-1]->getBounds().translated(-15, 0));
    }
    //what a fucking mess
    else if (matrix.fx[route].size()-1 > 0) matrix.fx[route][matrix.fx[route].size() - 2]->setBounds(matrix.fx[route][matrix.fx[route].size()-2]->getBounds().translated(-15, 0));
    setTransparency(0.3f);
    repaint(); }

void sadistic::RoutingMatrix::InsertionPoint::itemDragExit (const SourceDetails& /*dragSourceDetails*/) {
    std::cout << "DragAndDropCarrier():: itemDragExit()" << std::endl;
    if (effect) {
        effect->setBounds(effect->getBounds().translated(-20, 0));
        int idx { (int)effect->indexSlider.getValue() };
        if (idx < matrix.fx[route].size()-1 && idx > 0) matrix.fx[route][idx-1]->setBounds(matrix.fx[route][idx-1]->getBounds().translated(15, 0));
    }
    else if (matrix.fx[route].size()-1 > 0) matrix.fx[route][matrix.fx[route].size() - 2]->setBounds(matrix.fx[route][matrix.fx[route].size()-2]->getBounds().translated(15, 0));
    setTransparency(0.f);
    repaint(); }
