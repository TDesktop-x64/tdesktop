/*
This file is part of Yukigram Desktop,
the unofficial app based on Telegram Desktop.
For license and copyright information please follow this link:
https://github.com/nicennnnnnnlee/nicennnnnnnlee.github.io/blob/master/LICENSE
*/
#pragma once

#include "ui/text/text_entity.h"

namespace PanguSpacing {

// Apply pangu spacing to plain text (insert spaces between CJK and half-width characters).
[[nodiscard]] QString SpacingText(const QString &text);

// Apply pangu spacing to TextWithEntities, adjusting entity offsets accordingly.
[[nodiscard]] TextWithEntities SpacingTextWithEntities(const TextWithEntities &textWithEntities);

} // namespace PanguSpacing
