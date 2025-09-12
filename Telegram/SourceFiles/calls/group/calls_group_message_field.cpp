/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "calls/group/calls_group_message_field.h"

#include "chat_helpers/compose/compose_show.h"
#include "lang/lang_keys.h"
#include "ui/controls/emoji_button.h"
#include "ui/controls/send_button.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/painter.h"
#include "ui/ui_utility.h"
#include "styles/style_chat_helpers.h"
#include "styles/style_chat.h"
#include "styles/style_media_view.h"

namespace Calls::Group {

MessageField::MessageField(
	not_null<QWidget*> parent,
	std::shared_ptr<ChatHelpers::Show> show)
: _parent(parent)
, _show(std::move(show))
, _wrap(std::make_unique<Ui::RpWidget>(_parent)) {
	createControls();
}

void MessageField::createControls() {
	setupBackground();

	const auto &st = st::storiesComposeControls;
	_field = Ui::CreateChild<Ui::InputField>(
		_wrap.get(),
		st.field,
		Ui::InputField::Mode::MultiLine,
		tr::lng_message_ph());
	_field->setMinHeight(
		st::historySendSize.height() - 2 * st::historySendPadding);
	_field->setMaxHeight(st::historyComposeFieldMaxHeight);
	_field->setDocumentMargin(4.);
	_field->setAdditionalMargin(style::ConvertScale(4) - 4);

	_send = Ui::CreateChild<Ui::SendButton>(_wrap.get(), st.send);

	_emojiToggle = Ui::CreateChild<Ui::EmojiButton>(_wrap.get(), st.emoji);

	_width.value(
	) | rpl::filter(
		rpl::mappers::_1 > 0
	) | rpl::start_with_next([=](int newWidth) {
		_wrap->resizeToWidth(newWidth);

		const auto fieldWidth = newWidth
			- st::historySendPadding
			- _emojiToggle->width()
			- _send->width();
		_field->resizeToWidth(fieldWidth);
		_field->moveToLeft(
			st::historySendPadding,
			st::historySendPadding,
			newWidth);
		_send->moveToRight(0, 0);
		_emojiToggle->moveToRight(_send->width(), 0);
	}, _lifetime);

	_field->heightValue() | rpl::start_with_next([=](int height) {
		_wrap->resize(_wrap->width(), height + 2 * st::historySendPadding);
	}, _lifetime);

	rpl::merge(
		_field->submits() | rpl::to_empty,
		_send->clicks() | rpl::to_empty
	) | rpl::start_with_next([=] {
		_submitted.fire(_field->getTextWithAppliedMarkdown());
	}, _lifetime);
}

void MessageField::setupBackground() {
	_wrap->paintRequest() | rpl::start_with_next([=] {
		const auto radius = st::historySendSize.height() / 2.;
		auto p = QPainter(_wrap.get());
		auto hq = PainterHighQualityEnabler(p);

		p.setPen(Qt::NoPen);
		p.setBrush(st::storiesComposeBg);
		p.drawRoundedRect(_wrap->rect(), radius, radius);
	}, _lifetime);
}

void MessageField::resizeToWidth(int newWidth) {
	_width = newWidth;
	if (_wrap->isHidden()) {
		Ui::SendPendingMoveResizeEvents(_wrap.get());
	}
}

void MessageField::move(int x, int y) {
	_wrap->move(x, y);
}

void MessageField::toggle(bool shown) {
	if (_shown == shown) {
		return;
	} else if (shown) {
		Assert(_width.current() > 0);
		Ui::SendPendingMoveResizeEvents(_wrap.get());
	}
	_shown = shown;
	shownAnimationCallback();
	//_shownAnimation.start(
	//	[=] { shownAnimationCallback(); },
	//	shown ? 0. : 1.,
	//	shown ? 1. : 0.,
	//	st::slideWrapDuration,
	//	anim::easeOutCirc);
}

void MessageField::raise() {
	_wrap->raise();
}

void MessageField::shownAnimationCallback() {
	if (_shownAnimation.animating()) {
		_wrap->update();
	} else if (_shown) {
		_wrap->show();
		_field->setFocusFast();
	} else {
		_closed.fire({});
	}
}

int MessageField::height() const {
	return _shownAnimation.value(_shown ? 1. : 0.) * _wrap->height();
}

rpl::producer<int> MessageField::heightValue() const {
	return _wrap->heightValue();
}

rpl::producer<TextWithTags> MessageField::submitted() const {
	return _submitted.events();
}

rpl::producer<> MessageField::closed() const {
	return _closed.events();
}

rpl::lifetime &MessageField::lifetime() {
	return _lifetime;
}

} // namespace Calls::Group
