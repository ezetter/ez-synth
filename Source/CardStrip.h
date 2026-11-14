#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

/** A row of cards or panels, all the same size, more of them than fit, so a few show at a time and the row scrolls
    sideways to show the others, with a thin scroll bar under it. Once it stops, it settles with a whole card at
    each place, so the cards shown are always the same size and in the same places.
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

    /** The first card shown in full. */
    int getFirstShown() const;
    int getNumShown() const noexcept   { return cardsShown; }
    int getNumCards() const noexcept   { return (int) cards.size(); }

    /** Scrolls so a card is the first shown, or as near as it can be, its last card at the end. */
    void scrollTo (int firstCard);

    /** How wide each card is, for a strip of a width. */
    int getCardWidth() const;

    void resized() override;

private:
    /** Lets the strip know when it's scrolled, so it can settle on a whole card. */
    class View final : public juce::Viewport
    {
    public:
        explicit View (CardStrip& s) : strip (s) {}
        void visibleAreaChanged (const juce::Rectangle<int>&) override   { strip.startTimer (150); }

    private:
        CardStrip& strip;
    };

    void timerCallback() override;
    int getStep() const   { return getCardWidth() + gap; }

    const int cardsShown;
    int gap = 8, underGap = 12;
    std::vector<juce::Component*> cards;
    juce::Component content;
    View view { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CardStrip)
};
