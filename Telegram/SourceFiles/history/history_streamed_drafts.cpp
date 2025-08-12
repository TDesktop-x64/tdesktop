/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "history/history_streamed_drafts.h"

#include "api/api_text_entities.h"
#include "data/data_session.h"
#include "history/history.h"
#include "history/history_item.h"

HistoryStreamedDrafts::HistoryStreamedDrafts(not_null<History*> history)
: _history(history) {
}

HistoryStreamedDrafts::~HistoryStreamedDrafts() {
	for (const auto &[rootId, draft] : base::take(_drafts)) {
		draft.message->destroy();
	}
}

void HistoryStreamedDrafts::apply(
		MsgId rootId,
		PeerId fromId,
		TimeId when,
		const MTPDsendMessageTextDraftAction &data) {
	const auto now = crl::now();
	const auto text = Api::ParseTextWithEntities(
		&_history->session(),
		data.vtext());
	const auto randomId = data.vrandom_id().v;
	const auto i = _drafts.find(rootId);
	if (i != end(_drafts)) {
		if (when && i->second.randomId == randomId) {
			i->second.message->setText(text);
			i->second.updated = now;
			return;
		}
		i->second.message->destroy();
		_drafts.erase(i);
	}
	if (!when) {
		return;
	}
	_drafts.emplace(rootId, Draft{
		.message = _history->addNewLocalMessage({
			.id = _history->owner().nextLocalMessageId(),
			.flags = MessageFlag::Local,
			.from = fromId,
			.replyTo = { .topicRootId = rootId },
			.date = when,
		}, text, MTP_messageMediaEmpty()),
		.randomId = randomId,
		.updated = now,
	});
}
