/*
This file is part of Yukigram Desktop,
the unofficial app based on Telegram Desktop.
For license and copyright information please follow this link:
https://github.com/nicennnnnnnlee/nicennnnnnnlee.github.io/blob/master/LICENSE
*/
#pragma once

#include "ui/text/text_entity.h"

namespace PanguSpacing {

// Apply pangu spacing to plain text (insert spaces between CJK and ASCII alphanumeric characters [A-Za-z0-9]).
[[nodiscard]] QString SpacingText(const QString &text);

// Apply pangu spacing to TextWithEntities (CJK vs. ASCII alphanumerics), adjusting entity offsets accordingly.
[[nodiscard]] TextWithEntities SpacingTextWithEntities(const TextWithEntities &textWithEntities);

} // namespace PanguSpacing
