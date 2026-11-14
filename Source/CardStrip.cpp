#include "CardStrip.h"

namespace
{
    /** How long after the last swipe, or its glide, the row starts easing to a whole card. A trackpad sends a swipe
        as many small movements, and its glide after the fingers lift as more, so this is long enough not to cut
        in between them.
    */
    constexpr juce::uint32 settleDelayMs = 220;

    /** How far a swipe moves the row, in pixels for each unit of the wheel's movement, as a scrolling list moves. */
    constexpr double pixelsPerWheelUnit = 260.0;
}

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

int CardStrip::getFurthest() const
{
    return juce::jmax (0, (int) cards.size() - cardsShown) * getStep();
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
    stopTimer();
    easingTo = -1;
    scrollTo (getFirstShown());
}

int CardStrip::getNearestCard (double at) const
{
    return juce::jlimit (0, juce::jmax (0, (int) cards.size() - cardsShown), juce::roundToInt (at / (double) getStep()));
}

int CardStrip::getFirstShown() const
{
    return getNearestCard ((double) view.getViewPositionX());
}

void CardStrip::moveTo (double newPosition)
{
    position = juce::jlimit (0.0, (double) getFurthest(), newPosition);
    const juce::ScopedValueSetter<bool> movingItself (moving, true);
    view.setViewPosition (juce::roundToInt (position), 0);
}

void CardStrip::scrollTo (int firstCard)
{
    moveTo ((double) (getNearestCard ((double) firstCard * getStep()) * getStep()));
}

void CardStrip::scrollToShow (int card)
{
    const auto first = getFirstShown();

    if (card < first)
        scrollTo (card);
    else if (card >= first + cardsShown)
        scrollTo (card - cardsShown + 1);
}

void CardStrip::easeTo (int firstCard)
{
    easingTo = firstCard;
    startTimerHz (60);
}

void CardStrip::scrollByWheel (const juce::MouseWheelDetails& wheel)
{
    if ((int) cards.size() <= cardsShown)
        return;

    // A sideways swipe moves the row, and so does an up-and-down one, there being nothing up and down to scroll.
    // The deltas already follow the system's scroll direction, as a scrolling list's do.
    const auto amount = std::abs (wheel.deltaX) > std::abs (wheel.deltaY) ? -wheel.deltaX : -wheel.deltaY;

    if (juce::exactlyEqual (amount, 0.0f))
        return;

    // A mouse wheel's click moves a whole card, easing there.
    if (! wheel.isSmooth)
    {
        const auto from = easingTo >= 0 ? easingTo : getFirstShown();
        easeTo (juce::jlimit (0, juce::jmax (0, (int) cards.size() - cardsShown), from + (amount > 0.0f ? 1 : -1)));
        return;
    }

    // A trackpad's swipe, and its glide, follow the fingers, and the row settles once they're over.
    easingTo = -1;
    moveTo (position + (double) amount * pixelsPerWheelUnit);
    lastWheelTime = juce::Time::getMillisecondCounter();
    startTimerHz (60);
}

void CardStrip::View::visibleAreaChanged (const juce::Rectangle<int>& area)
{
    // Moved by its scroll bar, the row settles once the bar's let go.
    if (! strip.moving)
    {
        strip.position = (double) area.getX();
        strip.easingTo = -1;
        strip.lastWheelTime = juce::Time::getMillisecondCounter();
        strip.startTimerHz (60);
    }
}

void CardStrip::settleNow()
{
    stopTimer();
    scrollTo (easingTo >= 0 ? easingTo : getNearestCard (position));
    easingTo = -1;
}

void CardStrip::timerCallback()
{
    if (easingTo < 0)
    {
        // Still swiping, gliding, or dragging the scroll bar.
        if (juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown()
             || juce::Time::getMillisecondCounter() - lastWheelTime < settleDelayMs)
            return;

        easingTo = getNearestCard (position);
    }

    // Eases a good part of the way each frame, so it slows as it arrives.
    const auto target = (double) (easingTo * getStep());
    const auto next = position + (target - position) * 0.3;

    if (std::abs (target - next) < 0.75)
    {
        moveTo (target);
        stopTimer();
        easingTo = -1;
    }
    else
    {
        moveTo (next);
    }
}
