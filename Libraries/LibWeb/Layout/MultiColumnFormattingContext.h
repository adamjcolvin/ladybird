/*
 * Copyright (c) 2026, Adam Colvin <adamjcolvin@proton.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibWeb/Layout/FormattingContext.h>

namespace Web::Layout {

// https://www.w3.org/TR/css-multicol-1/
class MultiColumnFormattingContext final : public FormattingContext {
public:
    explicit MultiColumnFormattingContext(LayoutState&, LayoutMode, Box const&, FormattingContext* parent);
    ~MultiColumnFormattingContext();

    virtual void run(AvailableSpace const&) override;
    virtual CSSPixels automatic_content_width() const override;
    virtual CSSPixels automatic_content_height() const override;

private:
    CSSPixels m_content_height { 0 };
};

}
