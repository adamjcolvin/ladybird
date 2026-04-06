/*
 * Copyright (c) 2026, Adam Colvin <adamjcolvin@proton.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibWeb/CSS/ComputedProperties.h>
#include <LibWeb/Layout/BlockContainer.h>
#include <LibWeb/Layout/BlockFormattingContext.h>
#include <LibWeb/Layout/MultiColumnFormattingContext.h>

namespace Web::Layout {

MultiColumnFormattingContext::MultiColumnFormattingContext(LayoutState& state, LayoutMode layout_mode, Box const& box, FormattingContext* parent)
    : FormattingContext(Type::MultiColumn, layout_mode, state, box, parent)
{
}

MultiColumnFormattingContext::~MultiColumnFormattingContext() = default;

CSSPixels MultiColumnFormattingContext::automatic_content_width() const
{
    return m_state.get(context_box()).content_width();
}

CSSPixels MultiColumnFormattingContext::automatic_content_height() const
{
    return m_content_height;
}

// https://www.w3.org/TR/css-multicol-1/
void MultiColumnFormattingContext::run(AvailableSpace const& available_space)
{
    auto& root_box = static_cast<BlockContainer const&>(context_box());
    auto const& computed = root_box.computed_values();
    auto available_width = available_space.width.to_px_or_zero();

    // https://www.w3.org/TR/css-multicol-1/#column-gaps-and-rules
    // In a multi-column formatting context the used value of normal for the column-gap property is 1em.
    CSSPixels gap = 0;
    computed.column_gap().visit(
        [&](CSS::NormalGap) { gap = computed.font_size(); },
        [&](CSS::LengthPercentage const& lp) { gap = lp.to_px(root_box, available_width); });

    // https://www.w3.org/TR/css-multicol-1/#the-number-and-width-of-columns
    // FIXME: When column-count is auto and column-width is specified, derive how many columns fit from the available width (not N=1 below).
    int column_count = computed.column_count().is_auto() ? 1 : computed.column_count().value();
    auto const width_after_gaps = available_width - CSSPixels(column_count - 1) * gap;
    auto column_width = max(CSSPixels(width_after_gaps / column_count), CSSPixels(0));

    // AD-HOC: Lay out children in one narrow column first so the BFC gives us vertical positions
    // and a total height; the pass below then slices that flow into balanced columns by reassigning offsets.
    auto block_formatting_context = make<BlockFormattingContext>(m_state, m_layout_mode, root_box, this);
    block_formatting_context->run(AvailableSpace(AvailableSize::make_definite(column_width), AvailableSize::make_indefinite()));

    CSSPixels total_content_height = 0;
    root_box.for_each_child_of_type<Box>([&](Box const& child) {
        if (child.is_absolutely_positioned())
            return IterationDecision::Continue;
        auto const& child_state = m_state.get(child);
        total_content_height = max(total_content_height,
            child_state.offset.y() + child_state.content_height() + child_state.margin_box_bottom());
        return IterationDecision::Continue;
    });

    // https://www.w3.org/TR/css-multicol-1/#column-fill
    // If columns are balanced, user agents should try to minimize variations in column height, while honoring
    // forced breaks, widows and orphans, and other properties that may affect column heights.
    // FIXME: Respect column-fill: auto (fill columns sequentially up to available height).
    CSSPixels target_column_height = column_count > 0
        ? CSSPixels::nearest_value_for(ceil(total_content_height.to_double() / column_count))
        : total_content_height;

    // AD-HOC: Redistribute children into columns by adjusting their x/y offsets.
    // When a child's top edge crosses a column boundary, advance to the next column.
    int current_column = 0;
    CSSPixels column_start_y = 0;

    root_box.for_each_child_of_type<Box>([&](Box& child) {
        if (child.is_absolutely_positioned())
            return IterationDecision::Continue;

        auto& child_state = m_state.get_mutable(child);

        while (current_column < column_count - 1
            && child_state.offset.y() >= CSSPixels(current_column + 1) * target_column_height) {
            ++current_column;
            column_start_y = child_state.offset.y();
        }

        child_state.offset = CSSPixelPoint(
            CSSPixels(current_column) * (column_width + gap),
            child_state.offset.y() - column_start_y);

        m_content_height = max(m_content_height,
            child_state.offset.y() + child_state.content_height() + child_state.margin_box_bottom());

        return IterationDecision::Continue;
    });
}

}
