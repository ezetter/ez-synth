#include "CardStrip.h"

CardStrip::CardStrip (int shown)
    : cardsShown (shown)
{
    view.setViewedComponent (&content, false);
    view.setScrollBarsShown (false, true);
    view.setScrollBarThickness (6);
    view.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::never);
    addAndMakeVisible (view);
}

void CardStrip::addCard (juce::Component& card)
{
    cards.push_back (&card);
    content.addAndMakeVisible (card);
    resized();
}

void CardStrip::setGaps (int betweenCards, int underCards)
{
    gap = betweenCards;
    underGap = underCards;
    resized();
}

int CardStrip::getCardWidth() const
{
    return juce::jmax (1, (getWidth() - gap * (cardsShown - 1)) / cardsShown);
}

void CardStrip::resized()
{
    // The scroll bar sits in the middle of the gap under the cards.
    const auto cardHeight = juce::jmax (0, getHeight() - underGap);
    const auto thickness = juce::jlimit (1, 6, underGap);
    view.setScrollBarThickness (thickness);
    view.setBounds (getLocalBounds().withTrimmedBottom ((underGap - thickness) / 2));
    const auto width = getCardWidth();
    const auto count = (int) cards.size();
    content.setSize (count * width + juce::jmax (0, count - 1) * gap, view.getHeight() - thickness);

    for (int i = 0; i < count; ++i)
        cards[(size_t) i]->setBounds (i * (width + gap), 0, width, cardHeight);

    // The scroll bar runs the strip's whole width, only when there are cards out of sight.
    view.setScrollBarsShown (false, count > cardsShown);
    scrollTo (getFirstShown());
}

int CardStrip::getFirstShown() const
{
    return juce::jlimit (0, juce::jmax (0, (int) cards.size() - cardsShown),
                         juce::roundToInt ((double) view.getViewPositionX() / (double) getStep()));
}

void CardStrip::scrollTo (int firstCard)
{
    const auto first = juce::jlimit (0, juce::jmax (0, (int) cards.size() - cardsShown), firstCard);
    const auto x = first * getStep();

    if (view.getViewPositionX() != x)
        view.setViewPosition (x, 0);
}

void CardStrip::scrollToShow (int card)
{
    const auto first = getFirstShown();

    if (card < first)
        scrollTo (card);
    else if (card >= first + cardsShown)
        scrollTo (card - cardsShown + 1);
}

void CardStrip::timerCallback()
{
    // Not while the scroll bar's being dragged: it settles once it's let go.
    if (juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
        return;

    stopTimer();
    scrollTo (getFirstShown());
}
