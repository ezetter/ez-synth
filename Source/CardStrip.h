#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

/** A row of cards or panels, all the same size, more of them than fit, so a few show at a time and the row scrolls
    sideways to show the others, with a thin scroll bar under it. A swipe on a trackpad moves the row with the
    fingers, and once the swipe and its glide have finished, the row eases to the nearest whole card, so the cards
    shown are always the same size and in the same places. A mouse wheel's click moves it a card at a time.
*/
class CardStrip final : public juce::Component,
                        private juce::Timer
{
public:
    explicit CardStrip (int cardsShown);

    /** Adds a card to the end of the row. The strip doesn't own it. */
    void addCard (juce::Component&);

    /** Sets the gap between cards, and the height of the gap under them, where the scroll bar sits. */
    void setGaps (int betweenCards, int underCards);

    /** Scrolls, if need be, so a card shows in full. */
    void scrollToShow (int card);

    /** The first card shown in full, or nearest to it while the row's moving. */
    int getFirstShown() const;
    int getNumShown() const noexcept   { return cardsShown; }
    int getNumCards() const noexcept   { return (int) cards.size(); }

    /** Scrolls straight to have a card first, or as near as it can be, its last card at the end. */
    void scrollTo (int firstCard);

    /** How far the row is scrolled, in pixels. */
    int getScrollPosition() const   { return view.getViewPositionX(); }

    /** Moves the row as a swipe or a turn of a mouse wheel does. */
    void scrollByWheel (const juce::MouseWheelDetails&);

    /** Whether the row's waiting for a swipe to finish, or easing to a whole card. */
    bool isSettling() const noexcept   { return isTimerRunning(); }

    /** Finishes settling straight away, as it would once a swipe was over, for testing. */
    void settleNow();

    /** How wide each card is, for a strip of a width. */
    int getCardWidth() const;

    void resized() override;

private:
    /** Takes the wheel and swipes for the strip, and lets it know when its scroll bar's dragged. */
    class View final : public juce::Viewport
    {
    public:
        explicit View (CardStrip& s) : strip (s) {}
        void visibleAreaChanged (const juce::Rectangle<int>&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override   { strip.scrollByWheel (wheel); }

    private:
        CardStrip& strip;
    };

    void timerCallback() override;
    int getStep() const   { return getCardWidth() + gap; }
    int getFurthest() const;
    int getNearestCard (double position) const;
    void moveTo (double position);
    void easeTo (int firstCard);

    const int cardsShown;
    int gap = 8, underGap = 12;
    std::vector<juce::Component*> cards;
    juce::Component content;
    View view { *this };

    /** Where the row is, to a fraction of a pixel, so small swipes add up. */
    double position = 0.0;
    bool moving = false;

    /** When the last swipe came, and the card being eased to, if any. */
    juce::uint32 lastWheelTime = 0;
    int easingTo = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CardStrip)
};
