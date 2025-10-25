/*
This file is part of 64Gram Desktop,
the unofficial app based on Telegram Desktop.
For license and copyright information please follow this link:
https://github.com/TDesktop-x64/tdesktop/blob/dev/LEGAL
*/
#pragma once

#include <QtCore/QString>
#include <QtCore/QSet>
#include <QtCore/QRegularExpression>

namespace MessageFilters {

enum class FilterMode {
	Whitelist,
	Blacklist,
	Replace
};

enum class FilterDisplayMode {
	Hide,
	Dim
};

struct MessageFilter {
	QString id;
	QString name;
	QString regex;
	QString replacementText; // Text to replace matches with (for Replace mode)
	QSet<int64> userIds;
	QSet<int64> chatIds; // empty = global
	FilterMode mode;
	FilterDisplayMode displayMode;
	int order;
	bool enabled;
};

} // namespace MessageFilters

