/*
This file is part of Yukigram Desktop,
the unofficial app based on Telegram Desktop.

Pangu spacing algorithm ported from pangu.js (https://github.com/vinta/pangu.js)
Licensed under MIT License.
*/
#include "core/pangu_spacing.h"

#include <algorithm>

namespace PanguSpacing {
namespace {

bool IsCJK(uint ch) {
	return (ch >= 0x2E80 && ch <= 0x2EFF)   // CJK Radicals Supplement
		|| (ch >= 0x2F00 && ch <= 0x2FDF)    // Kangxi Radicals
		|| (ch >= 0x3040 && ch <= 0x309F)    // Hiragana
		|| (ch >= 0x30A0 && ch <= 0x30FF)    // Katakana
		|| (ch >= 0x3100 && ch <= 0x312F)    // Bopomofo
		|| (ch >= 0x3200 && ch <= 0x32FF)    // Enclosed CJK Letters and Months
		|| (ch >= 0x3400 && ch <= 0x4DBF)    // CJK Unified Ideographs Extension A
		|| (ch >= 0x4E00 && ch <= 0x9FFF)    // CJK Unified Ideographs
		|| (ch >= 0xF900 && ch <= 0xFAFF)    // CJK Compatibility Ideographs
		|| (ch >= 0xFE30 && ch <= 0xFE4F)    // CJK Compatibility Forms
		|| (ch >= 0x1F200 && ch <= 0x1F2FF)  // Enclosed Ideographic Supplement
		|| (ch >= 0x20000 && ch <= 0x2A6DF)  // CJK Unified Ideographs Extension B
		|| (ch >= 0x2A700 && ch <= 0x2B73F)  // CJK Unified Ideographs Extension C
		|| (ch >= 0x2B740 && ch <= 0x2B81F)  // CJK Unified Ideographs Extension D
		|| (ch >= 0x2B820 && ch <= 0x2CEAF)  // CJK Unified Ideographs Extension E
		|| (ch >= 0x2CEB0 && ch <= 0x2EBEF)  // CJK Unified Ideographs Extension F
		|| (ch >= 0x30000 && ch <= 0x3134F); // CJK Unified Ideographs Extension G
}

bool IsHalfWidthAlphaNumeric(uint ch) {
	return (ch >= 'A' && ch <= 'Z')
		|| (ch >= 'a' && ch <= 'z')
		|| (ch >= '0' && ch <= '9');
}

// Get the Unicode code point at a given position in the string,
// handling surrogate pairs. Returns the code point and advances pos
// past the surrogate pair if applicable.
uint CodePointAt(const QString &text, int &pos) {
	const auto high = text.at(pos).unicode();
	if (QChar::isHighSurrogate(high) && (pos + 1) < text.size()) {
		const auto low = text.at(pos + 1).unicode();
		if (QChar::isLowSurrogate(low)) {
			++pos; // consume the low surrogate
			return QChar::surrogateToUcs4(high, low);
		}
	}
	return high;
}

} // namespace

QString SpacingText(const QString &text) {
	if (text.isEmpty()) {
		return text;
	}

	auto result = QString();
	result.reserve(text.size() + text.size() / 4);

	uint prevCodePoint = 0;
	auto hasPrev = false;

	auto i = 0;
	while (i < text.size()) {
		const auto startPos = i;
		const auto codePoint = CodePointAt(text, i);

		if (hasPrev) {
			const auto prevIsCJK = IsCJK(prevCodePoint);
			const auto currIsCJK = IsCJK(codePoint);
			const auto prevIsAN = IsHalfWidthAlphaNumeric(prevCodePoint);
			const auto currIsAN = IsHalfWidthAlphaNumeric(codePoint);

			if ((prevIsCJK && currIsAN) || (prevIsAN && currIsCJK)) {
				result.append(' ');
			}
		}

		// Append the current character(s).
		if (codePoint > 0xFFFF) {
			result.append(text.at(startPos));
			result.append(text.at(startPos + 1));
		} else {
			result.append(text.at(startPos));
		}

		prevCodePoint = codePoint;
		hasPrev = true;
		++i;
	}

	return result;
}

TextWithEntities SpacingTextWithEntities(const TextWithEntities &textWithEntities) {
	if (textWithEntities.text.isEmpty()) {
		return textWithEntities;
	}

	const auto &originalText = textWithEntities.text;
	const auto &originalEntities = textWithEntities.entities;

	// First pass: find all UTF-16 positions where a space should be inserted.
	auto insertPositions = std::vector<int>();

	uint prevCodePoint = 0;
	auto hasPrev = false;

	auto i = 0;
	while (i < originalText.size()) {
		const auto startPos = i;
		const auto codePoint = CodePointAt(originalText, i);

		if (hasPrev) {
			const auto prevIsCJK = IsCJK(prevCodePoint);
			const auto currIsCJK = IsCJK(codePoint);
			const auto prevIsAN = IsHalfWidthAlphaNumeric(prevCodePoint);
			const auto currIsAN = IsHalfWidthAlphaNumeric(codePoint);

			if ((prevIsCJK && currIsAN) || (prevIsAN && currIsCJK)) {
				insertPositions.push_back(startPos);
			}
		}

		prevCodePoint = codePoint;
		hasPrev = true;
		++i;
	}

	if (insertPositions.empty()) {
		return textWithEntities;
	}

	// Build the new text with spaces inserted.
	auto newText = QString();
	newText.reserve(originalText.size() + int(insertPositions.size()));

	auto insertIdx = 0;
	for (auto j = 0; j < originalText.size(); ++j) {
		if (insertIdx < int(insertPositions.size())
			&& j == insertPositions[insertIdx]) {
			newText.append(' ');
			++insertIdx;
		}
		newText.append(originalText.at(j));
	}

	// Adjust entity offsets and lengths using binary search.
	auto newEntities = EntitiesInText();
	newEntities.reserve(originalEntities.size());

	const auto begin = insertPositions.begin();
	const auto end = insertPositions.end();

	for (const auto &entity : originalEntities) {
		const auto oldOffset = entity.offset();
		const auto oldEnd = oldOffset + entity.length();

		// Spaces inserted before entity start shift the offset.
		const auto spacesBeforeStart = int(
			std::lower_bound(begin, end, oldOffset) - begin);

		// Spaces inserted within entity range expand the length.
		const auto spacesBeforeEnd = int(
			std::lower_bound(begin, end, oldEnd) - begin);
		const auto spacesWithin = spacesBeforeEnd - spacesBeforeStart;

		newEntities.push_back(EntityInText(
			entity.type(),
			oldOffset + spacesBeforeStart,
			entity.length() + spacesWithin,
			entity.data()));
	}

	return { newText, newEntities };
}

} // namespace PanguSpacing
