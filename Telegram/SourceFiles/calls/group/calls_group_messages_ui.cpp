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
#include "ui/effects/radial_animation.h"
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

#include <QtGui/QGuiApplication>
#include <QtGui/QWindow>

namespace Calls::Group {
namespace {

constexpr auto kMessageBgOpacity = 0.8;

[[nodiscard]] int CountMessageRadius() {
	const auto minHeight = st::groupCallMessagePadding.top()
		+ st::messageTextStyle.font->height
		+ st::groupCallMessagePadding.bottom();
	return minHeight / 2;
}

void ReceiveOnlyWheelEvents(not_null<Ui::ScrollArea*> scroll) {
	class EventFilter final : public QObject {
	public:
		explicit EventFilter(not_null<Ui::ScrollArea*> scroll)
		: QObject(scroll) {
		}

		bool eventFilter(QObject *watched, QEvent *event) {
			if (event->type() != QEvent::Wheel) {
				return false;
			}
			const auto e = static_cast<QWheelEvent*>(event);
			Assert(parent()->isWidgetType());
			const auto scroll = static_cast<Ui::ScrollArea*>(parent());
			if (watched != scroll->window()->windowHandle()
				|| !scroll->scrollTopMax()) {
				return false;
			}
			const auto global = e->globalPos();
			const auto viewport = scroll->viewport();
			const auto local = viewport->mapFromGlobal(global);
			if (!viewport->rect().contains(local)) {
				return false;
			}
			auto ev = QWheelEvent(
				local,
				global,
				e->pixelDelta(),
				e->angleDelta(),
				e->delta(),
				e->orientation(),
				e->buttons(),
				e->modifiers(),
				e->phase(),
				e->source());
			ev.setTimestamp(crl::now());
			QGuiApplication::sendEvent(viewport, &ev);
			return true;
		}
	};

	scroll->setAttribute(Qt::WA_TransparentForMouseEvents);
	qApp->installEventFilter(new EventFilter(scroll));
}

} // namespace

struct MessagesUi::MessageView {
	int id = 0;
	PeerData *from = nullptr;
	Ui::Animations::Simple toggleAnimation;
	Ui::Animations::Simple sentAnimation;
	std::unique_ptr<Ui::InfiniteRadialAnimation> sendingAnimation;
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
					continue;
				} else if (entry.failed != i->failed) {
					setContentFailed(entry);
					updateMessageSize(entry);
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
	}, 0., 1, st::slideDuration, anim::easeOutCirc);
	repaintMessage(id);
}

void MessagesUi::updateMessageSize(MessageView &entry) {
	const auto &padding = st::groupCallMessagePadding;

	const auto hasUserpic = !entry.failed;
	const auto userpicPadding = st::groupCallUserpicPadding;
	const auto userpicSize = st::groupCallUserpic;
	const auto leftSkip = hasUserpic
		? (userpicPadding.left() + userpicSize + userpicPadding.right())
		: padding.left();
	const auto widthSkip = leftSkip + padding.right();
	const auto inner = _width - widthSkip;

	const auto size = Ui::Text::CountOptimalTextSize(
		entry.text,
		std::min(st::groupCallWidth / 2, inner),
		inner);

	const auto textHeight = size.height();
	entry.width = size.width() + widthSkip;
	entry.left = (_width - entry.width) / 2;

	const auto contentHeight = padding.top() + textHeight + padding.bottom();
	const auto userpicHeight = hasUserpic
		? (userpicPadding.top() + userpicSize + userpicPadding.bottom())
		: 0;

	const auto skip = st::groupCallMessageSkip;
	entry.realHeight = skip + std::max(contentHeight, userpicHeight);
}

bool MessagesUi::updateMessageHeight(MessageView &entry) {
	const auto height = entry.toggleAnimation.animating()
		? anim::interpolate(
			0,
			entry.realHeight,
			entry.toggleAnimation.value(entry.removed ? 0. : 1.))
		: entry.realHeight;
	if (entry.height == height) {
		return false;
	}
	entry.height = height;
	return true;
}

void MessagesUi::setContentFailed(MessageView &entry) {
	entry.failed = true;
	entry.text = Ui::Text::String(
		st::messageTextStyle,
		TextWithEntities().append(
			QString::fromUtf8("\xe2\x9d\x97\xef\xb8\x8f")
		).append(' ').append(
			Ui::Text::Italic(u"Failed to send the message."_q)),
		kMarkupTextOptions,
		st::groupCallWidth / 4);
}

void MessagesUi::setContent(
		MessageView &entry,
		const TextWithEntities &text) {
	entry.text = Ui::Text::String(
		st::messageTextStyle,
		text,
		kMarkupTextOptions,
		st::groupCallWidth / 4,
		Core::TextContext({
			.session = &_show->session(),
			.repaint = [this, id = entry.id] { repaintMessage(id); },
		}));
}

void MessagesUi::toggleMessage(MessageView &entry, bool shown) {
	const auto id = entry.id;
	entry.removed = !shown;
	entry.toggleAnimation.start(
		[=] { repaintMessage(id); },
		shown ? 0. : 1.,
		shown ? 1. : 0.,
		st::slideWrapDuration,
		shown ? anim::easeOutCirc : anim::easeInCirc);
	repaintMessage(id);
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
		i->sendingAnimation = nullptr;
	}
	if (i->toggleAnimation.animating() || i->height != i->realHeight) {
		if (updateMessageHeight(*i)) {
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
	if (data.failed) {
		setContentFailed(entry);
	} else {
		setContent(
			entry,
			Ui::Text::Bold(
				data.peer->shortName()).append(' ').append(data.text));
	}
	entry.top = top;
	updateMessageSize(entry);
	if (entry.sending) {
		using namespace Ui;
		const auto &st = st::defaultInfiniteRadialAnimation;
		entry.sendingAnimation = std::make_unique<InfiniteRadialAnimation>(
			repaint,
			st);
		entry.sendingAnimation->start(0);
	}
	entry.height = 0;
	toggleMessage(entry, true);
}

void MessagesUi::setupMessagesWidget() {
	_scroll = std::make_unique<Ui::ScrollArea>(
		_parent,
		st::groupCallMessagesScroll);
	const auto scroll = _scroll.get();

	ReceiveOnlyWheelEvents(scroll);

	_messages = scroll->setOwnedWidget(object_ptr<Ui::RpWidget>(scroll));

	_messages->paintRequest() | rpl::start_with_next([=](QRect clip) {
		auto p = Painter(_messages);
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

			auto leftSkip = padding.left();
			const auto hasUserpic = !entry.failed;
			if (hasUserpic) {
				const auto userpicSize = st::groupCallUserpic;
				const auto userpicPadding = st::groupCallUserpicPadding;
				const auto position = QPoint(
					entry.left + userpicPadding.left(),
					entry.top + userpicPadding.top());
				const auto rect = QRect(
					position,
					QSize(userpicSize, userpicSize));
				entry.from->paintUserpic(p, entry.view, {
					.position = position,
					.size = userpicSize,
					.shape = Ui::PeerUserpicShape::Circle,
				});
				if (const auto animation = entry.sendingAnimation.get()) {
					auto hq = PainterHighQualityEnabler(p);
					auto pen = st::groupCallBg->p;
					const auto shift = userpicPadding.left();
					pen.setWidthF(shift);
					p.setPen(pen);
					p.setBrush(Qt::NoBrush);
					const auto state = animation->computeState();
					const auto sent = entry.sending
						? 0.
						: entry.sentAnimation.value(1.);
					p.setOpacity(state.shown * (1. - sent));
					p.drawArc(
						rect.marginsRemoved({ shift, shift, shift, shift }),
						state.arcFrom,
						state.arcLength);
					p.setOpacity(1.);
				}
				leftSkip = userpicPadding.left()
					+ userpicSize
					+ userpicPadding.right();
			}

			p.setPen(st::radialFg);
			entry.text.draw(p, {
				.position = {
					entry.left + leftSkip,
					entry.top + padding.top()
				},
				.availableWidth = entry.width - widthSkip,
			});

			p.restore();
		}
	}, _lifetime);

	scroll->show();
	applyWidth();
}

void MessagesUi::applyWidth() {
	if (!_scroll || _width < st::groupCallWidth * 2 / 3) {
		return;
	}
	auto top = 0;
	for (auto &entry : _views) {
		entry.top = top;

		updateMessageSize(entry);
		updateMessageHeight(entry);

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
