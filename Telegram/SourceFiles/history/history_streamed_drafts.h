/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

class History;

class HistoryStreamedDrafts final {
public:
	explicit HistoryStreamedDrafts(not_null<History*> history);
	~HistoryStreamedDrafts();

	void apply(
		MsgId rootId,
		PeerId fromId,
		TimeId when,
		const MTPDsendMessageTextDraftAction &data);

private:
	struct Draft {
		not_null<HistoryItem*> message;
		uint64 randomId = 0;
		crl::time updated = 0;
	};

	const not_null<History*> _history;
	base::flat_map<MsgId, Draft> _drafts;

};
