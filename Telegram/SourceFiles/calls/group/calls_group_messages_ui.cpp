/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "calls/group/calls_group_messages_ui.h"

#include "boxes/premium_preview_box.h"
#include "calls/group/calls_group_messages.h"
#include "chat_helpers/compose/compose_show.h"
#include "chat_helpers/emoji_suggestions_widget.h"
#include "chat_helpers/message_field.h"
#include "chat_helpers/tabbed_panel.h"
#include "chat_helpers/tabbed_selector.h"
#include "core/ui_integration.h"
#include "data/stickers/data_custom_emoji.h"
#include "data/stickers/data_stickers.h"
#include "data/data_document.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/controls/emoji_button.h"
#include "ui/controls/send_button.h"
#include "ui/effects/animations.h"
#include "ui/text/text_utilities.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/scroll_area.h"
#include "ui/painter.h"
#include "ui/ui_utility.h"
#include "ui/userpic_view.h"
#include "styles/style_calls.h"
#include "styles/style_chat_helpers.h"
#include "styles/style_chat.h"
#include "styles/style_media_view.h"

namespace Calls::Group {
namespace {

constexpr auto kMessageBgOpacity = 0.8;

[[nodiscard]] int CountMessageRadius() {
	const auto minHeight = st::groupCallMessagePadding.top()
		+ st::messageTextStyle.font->height
		+ st::groupCallMessagePadding.bottom();
	return minHeight / 2;
}

} // namespace

struct MessagesUi::MessageView {
	int id = 0;
	PeerData *from = nullptr;
	Ui::Animations::Simple toggleAnimation;
	Ui::Animations::Simple sentAnimation;
	Ui::Animations::Basic sendingAnimation;
	Ui::PeerUserpicView view;
	Ui::Text::String text;
	int top = 0;
	int width = 0;
	int left = 0;
	int height = 0;
	int realHeight = 0;
	bool removed = false;
	bool sending = false;
	bool failed = false;
};

MessagesUi::MessagesUi(
	not_null<QWidget*> parent,
	std::shared_ptr<ChatHelpers::Show> show,
	rpl::producer<std::vector<Message>> messages)
: _parent(parent)
, _show(std::move(show))
, _messageBg([] {
	auto result = st::groupCallBg->c;
	result.setAlphaF(kMessageBgOpacity);
	return result;
})
, _messageBgRect(CountMessageRadius(), _messageBg.color()) {
	setupList(std::move(messages));
}

MessagesUi::~MessagesUi() = default;

void MessagesUi::setupList(rpl::producer<std::vector<Message>> messages) {
	std::move(
		messages
	) | rpl::start_with_next([=](std::vector<Message> &&list) {
		auto from = begin(list);
		auto till = end(list);
		for (auto &entry : _views) {
			if (!entry.removed) {
				const auto id = entry.id;
				const auto i = ranges::find(from, till, id, &Message::id);
				if (i == till) {
					toggleMessage(entry, false);
				} else if (entry.failed != i->failed) {
					setContentFailed(entry);
					repaintMessage(entry.id);
				} else if (entry.sending != (i->date == 0)) {
					animateMessageSent(entry);
				}
				if (i == from) {
					++from;
				}
			}
		}
		for (auto i = from; i != till; ++i) {
			if (!ranges::contains(_views, i->id, &MessageView::id)) {
				appendMessage(*i);
			}
		}
	}, _lifetime);
}

void MessagesUi::animateMessageSent(MessageView &entry) {
	const auto id = entry.id;
	entry.sending = false;
	entry.sentAnimation.start([=] {
		repaintMessage(id);
	}, 0., 1, st::slideWrapDuration, anim::easeOutCirc);
}

void MessagesUi::updateMessageSize(MessageView &entry) {
	const auto &padding = st::groupCallMessagePadding;

	const auto widthSkip = padding.left() + padding.right();
	const auto inner = _width - widthSkip;
	entry.width = std::min(inner, entry.text.maxWidth()) + widthSkip;
	entry.left = (_width - entry.width) / 2;

	const auto skip = st::groupCallMessageSkip;
	const auto textHeight = entry.text.countHeight(entry.width);
	entry.realHeight = skip
		+ padding.top()
		+ textHeight
		+ padding.bottom();
	entry.height = entry.toggleAnimation.animating()
		? anim::interpolate(
			0,
			entry.realHeight,
			entry.toggleAnimation.value(entry.removed ? 0. : 1.))
		: entry.realHeight;
}

void MessagesUi::setContentFailed(MessageView &entry) {
	const auto manager = &entry.from->owner().customEmojiManager();
	entry.text = Ui::Text::String(
		st::messageTextStyle,
		TextWithEntities().append(
			QString::fromUtf8("\xe2\x9d\x97\xef\xb8\x8f")
		).append(' ').append(
			Ui::Text::Italic(u"Failed to send the message."_q)),
		kMarkupTextOptions,
		st::groupCallWidth / 2);
}

void MessagesUi::toggleMessage(MessageView &entry, bool shown) {
	const auto id = entry.id;
	entry.removed = !shown;
	entry.toggleAnimation.start(
		[=] { repaintMessage(id); },
		shown ? 0. : 1.,
		shown ? 1. : 0.,
		st::slideWrapDuration,
		anim::easeOutCirc);
}

void MessagesUi::repaintMessage(int id) {
	auto i = ranges::find(_views, id, &MessageView::id);
	if (i == end(_views)) {
		return;
	} else if (i->removed && !i->toggleAnimation.animating()) {
		const auto top = i->top;
		i = _views.erase(i);
		recountHeights(i, top);
		return;
	}
	if (!i->sending && !i->sentAnimation.animating()) {
		i->sendingAnimation.stop();
	}
	if (i->toggleAnimation.animating() || i->height != i->realHeight) {
		const auto shown = i->toggleAnimation.value(i->removed ? 0. : 1.);
		const auto height = anim::interpolate(0, i->realHeight, shown);
		if (i->height != height) {
			i->height = height;
			recountHeights(i, i->top);
			return;
		}
	}
	_messages->update(0, i->top, _messages->width(), i->height);
}

void MessagesUi::recountHeights(
		std::vector<MessageView>::iterator i,
		int top) {
	auto from = top;
	for (auto e = end(_views); i != e; ++i) {
		i->top = top;
		top += i->height;
	}
	if (_views.empty()) {
		delete base::take(_messages);
		_scroll = nullptr;
	} else {
		updateGeometries();
		_messages->update(0, from, _messages->width(), top - from);
	}
}

void MessagesUi::appendMessage(const Message &data) {
	const auto top = _views.empty()
		? 0
		: (_views.back().top + _views.back().height);

	if (!_scroll) {
		setupMessagesWidget();
	}

	auto &entry = _views.emplace_back();
	const auto id = entry.id = data.id;
	const auto repaint = [=] {
		repaintMessage(id);
	};
	entry.from = data.peer;
	entry.sending = !data.date;
	entry.failed = data.failed;
	if (entry.failed) {
		setContentFailed(entry);
	} else {
		const auto manager = &entry.from->owner().customEmojiManager();
		entry.text = Ui::Text::String(
			st::messageTextStyle,
			Ui::Text::SingleCustomEmoji(
				manager->peerUserpicEmojiData(entry.from),
				u"@"_q).append(' ').append(
					Ui::Text::Bold(data.peer->shortName())
				).append(' ').append(data.text),
			kMarkupTextOptions,
			st::groupCallWidth / 2,
			Core::TextContext({
				.session = &_show->session(),
				.repaint = repaint,
			}));
	}
	entry.top = top;
	updateMessageSize(entry);
	if (entry.sending) {
		entry.sendingAnimation.init(repaint);
		entry.sendingAnimation.start();
	}
	entry.height = 0;
	toggleMessage(entry, true);
}

void MessagesUi::setupMessagesWidget() {
	_scroll = std::make_unique<Ui::ScrollArea>(_parent);
	_messages = _scroll->setOwnedWidget(
		object_ptr<Ui::RpWidget>(_scroll.get()));

	_messages->paintRequest() | rpl::start_with_next([=](QRect clip) {
		auto p = QPainter(_messages);
		auto hq = PainterHighQualityEnabler(p);
		const auto skip = st::groupCallMessageSkip;
		const auto padding = st::groupCallMessagePadding;
		const auto widthSkip = padding.left() + padding.right();
		for (auto &entry : _views) {
			if (entry.height <= skip) {
				continue;
			}
			const auto use = entry.realHeight - skip;
			const auto width = entry.width;
			p.setBrush(st::radialBg);
			p.setPen(Qt::NoPen);
			const auto scaled = (entry.height < entry.realHeight);
			if (scaled) {
				const auto used = entry.height - skip;
				const auto mx = scaled ? (entry.left + (width / 2)) : 0;
				const auto my = scaled ? (entry.top + (used / 2)) : 0;
				const auto scale = used / float64(use);
				p.save();
				p.translate(mx, my);
				p.scale(scale, scale);
				p.setOpacity(scale);
				p.translate(-mx, -my);
			}
			_messageBgRect.paint(p, { entry.left, entry.top, width, use });

			p.setPen(st::radialFg);
			entry.text.draw(p, {
				.position = {
					entry.left + padding.left(),
					entry.top + padding.top()
				},
				.availableWidth = entry.width - widthSkip,
			});

			p.restore();
		}
	}, _lifetime);

	//_scroll->setAttribute(Qt::WA_TransparentForMouseEvents);
	_scroll->show();
	applyWidth();
}

void MessagesUi::applyWidth() {
	if (!_scroll || _width < st::groupCallWidth * 2 / 3) {
		return;
	}
	auto top = 0;
	for (auto &entry : _views) {
		updateMessageSize(entry);
		entry.top = top;
		top += entry.height;
	}
	updateGeometries();
}

void MessagesUi::updateGeometries() {
	const auto scrollBottom = (_scroll->scrollTop() + _scroll->height());
	const auto atBottom = (scrollBottom >= _messages->height());

	const auto height = _views.empty()
		? 0
		: (_views.back().top + _views.back().height);
	_messages->setGeometry(0, 0, _width, height);

	const auto min = std::min(height, _availableHeight);
	_scroll->setGeometry(_left, _bottom - min, _width, min);

	if (atBottom) {
		_scroll->scrollToY(std::max(height - _scroll->height(), 0));
	}
}

void MessagesUi::move(int left, int bottom, int width, int availableHeight) {
	if (_left != left
		|| _bottom != bottom
		|| _width != width
		|| _availableHeight != availableHeight) {
		_left = left;
		_bottom = bottom;
		_width = width;
		_availableHeight = availableHeight;
		applyWidth();
	}
}

void MessagesUi::raise() {
	if (_scroll) {
		_scroll->raise();
	}
}

rpl::lifetime &MessagesUi::lifetime() {
	return _lifetime;
}

} // namespace Calls::Group
