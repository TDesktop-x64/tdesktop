/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "calls/group/calls_group_messages.h"

#include "apiwrap.h"
#include "api/api_text_entities.h"
#include "base/unixtime.h"
#include "calls/group/calls_group_call.h"
#include "data/data_group_call.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "main/main_session.h"
#include "mtproto/sender.h"
#include "ui/ui_utility.h"

namespace Calls::Group {

Messages::Messages(not_null<GroupCall*> call, not_null<MTP::Sender*> api)
: _call(call)
, _api(api) {
	Ui::PostponeCall(_call, [=] {
		_call->real(
		) | rpl::start_with_next([=](not_null<Data::GroupCall*> call) {
			_real = call;
			if (ready()) {
				sendPending();
			} else {
				Unexpected("Not ready call.");
			}
		}, _lifetime);
	});
}

bool Messages::ready() const {
	return _real && (!_call->conference() || _call->e2eEncryptDecrypt());
}

void Messages::send(TextWithTags text) {
	if (!ready()) {
		_pending.push_back(std::move(text));
		return;
	}

	auto prepared = TextWithEntities{
		text.text,
		TextUtilities::ConvertTextTagsToEntities(text.tags)
	};
	auto serialized = MTPTextWithEntities(MTP_textWithEntities(
		MTP_string(prepared.text),
		Api::EntitiesToMTP(
			&_real->session(),
			prepared.entities,
			Api::ConvertOption::SkipLocal)));

	const auto id = ++_autoincrementId;
	const auto from = _call->peer()->session().user();
	_messages.push_back({
		.id = id,
		.peer = from,
		.text = std::move(prepared),
	});

	if (!_call->conference()) {
		_api->request(MTPphone_SendGroupCallMessage(
			_call->inputCall(),
			serialized
		)).done([=](const MTPBool &, const MTP::Response &response) {
			sent(id, response);
		}).fail([=] {
			failed(id);
		}).send();
	} else {
		auto counter = ::tl::details::LengthCounter();
		serialized.write(counter);
		auto buffer = mtpBuffer();
		buffer.reserve(counter.length);
		serialized.write(buffer);
		const auto view = bytes::make_span(buffer);
		auto v = std::vector<std::uint8_t>(view.size());
		bytes::copy(bytes::make_span(v), view);

		const auto userId = peerToUser(from->id).bare;
		const auto encrypt = _call->e2eEncryptDecrypt();
		const auto encrypted = encrypt(v, int64_t(userId), true, 0);

		_api->request(MTPphone_SendGroupCallEncryptedMessage(
			_call->inputCall(),
			MTP_bytes(bytes::make_span(encrypted))
		)).done([=](const MTPBool &, const MTP::Response &response) {
			sent(id, response);
		}).fail([=] {
			failed(id);
		}).send();
	}
	pushChanges();
}

void Messages::received(const MTPDupdateGroupCallMessage &data) {
	if (!ready()) {
		return;
	}
	received(data.vfrom_id(), data.vmessage());
	pushChanges();
}

void Messages::received(const MTPDupdateGroupCallEncryptedMessage &data) {
	if (!ready()) {
		return;
	}
	const auto fromId = data.vfrom_id();
	const auto &bytes = data.vencrypted_message().v;
	auto v = std::vector<std::uint8_t>(bytes.size());
	bytes::copy(bytes::make_span(v), bytes::make_span(bytes));

	const auto userId = peerToUser(peerFromMTP(fromId)).bare;
	const auto decrypt = _call->e2eEncryptDecrypt();
	const auto decrypted = decrypt(v, int64_t(userId), false, 0);
	if (decrypted.empty() || decrypted.size() % 4 != 0) {
		LOG(("API Error: Wrong decrypted message size: %1"
			).arg(decrypted.size()));
		return;
	}
	auto info = reinterpret_cast<const mtpPrime*>(decrypted.data());

	auto text = MTPTextWithEntities();
	if (!text.read(info, info + (decrypted.size() / 4))) {
		LOG(("API Error: Can't parse decrypted message"));
		return;
	}
	received(fromId, text);
	pushChanges();
}

void Messages::received(
		const MTPPeer &from,
		const MTPTextWithEntities &message) {
	const auto peer = _call->peer();
	if (peerFromMTP(from) == peer->session().userPeerId()) {
		// Our own we add only locally.
		return;
	}
	const auto id = ++_autoincrementId;
	_messages.push_back({
		.id = id,
		.date = base::unixtime::now(),
		.peer = peer->owner().peer(peerFromMTP(from)),
		.text = Api::ParseTextWithEntities(&peer->session(), message),
	});
}

rpl::producer<std::vector<Message>> Messages::listValue() const {
	return _changes.events_starting_with_copy(_messages);
}

void Messages::sendPending() {
	Expects(_real != nullptr);

	for (auto &pending : base::take(_pending)) {
		send(std::move(pending));
	}
}

void Messages::pushChanges() {
	_changes.fire_copy(_messages);
}

void Messages::sent(int id, const MTP::Response &response) {
	const auto i = ranges::find(_messages, id, &Message::id);
	if (i != end(_messages)) {
		i->date = Api::UnixtimeFromMsgId(response.outerMsgId);
		pushChanges();
	}
}

void Messages::failed(int id) {
	const auto i = ranges::find(_messages, id, &Message::id);
	if (i != end(_messages)) {
		i->failed = true;
		pushChanges();
	}
}

} // namespace Calls::Group
