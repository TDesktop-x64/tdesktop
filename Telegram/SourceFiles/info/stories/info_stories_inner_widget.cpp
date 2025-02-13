/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "info/stories/info_stories_inner_widget.h"

#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/data_stories.h"
#include "data/data_user.h"
#include "dialogs/ui/dialogs_stories_content.h"
#include "dialogs/ui/dialogs_stories_list.h"
#include "info/media/info_media_buttons.h"
#include "info/media/info_media_list_widget.h"
#include "info/profile/info_profile_actions.h"
#include "info/profile/info_profile_icon.h"
#include "info/profile/info_profile_values.h"
#include "info/profile/info_profile_widget.h"
#include "info/stories/info_stories_widget.h"
#include "info/info_controller.h"
#include "info/info_memento.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "settings/settings_common.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/vertical_list.h"
#include "styles/style_dialogs.h"
#include "styles/style_info.h"
#include "styles/style_settings.h"

namespace Info::Stories {

class EmptyWidget : public Ui::RpWidget {
public:
	EmptyWidget(QWidget *parent);

	void setFullHeight(rpl::producer<int> fullHeightValue);

protected:
	int resizeGetHeight(int newWidth) override;

	void paintEvent(QPaintEvent *e) override;

private:
	object_ptr<Ui::FlatLabel> _text;
	int _height = 0;

};

EmptyWidget::EmptyWidget(QWidget *parent)
: RpWidget(parent)
, _text(this, st::infoEmptyLabel) {
}

void EmptyWidget::setFullHeight(rpl::producer<int> fullHeightValue) {
	std::move(
		fullHeightValue
	) | rpl::start_with_next([this](int fullHeight) {
		// Make icon center be on 1/3 height.
		auto iconCenter = fullHeight / 3;
		auto iconHeight = st::infoEmptyStories.height();
		auto iconTop = iconCenter - iconHeight / 2;
		_height = iconTop + st::infoEmptyIconTop;
		resizeToWidth(width());
	}, lifetime());
}

int EmptyWidget::resizeGetHeight(int newWidth) {
	auto labelTop = _height - st::infoEmptyLabelTop;
	auto labelWidth = newWidth - 2 * st::infoEmptyLabelSkip;
	_text->resizeToNaturalWidth(labelWidth);

	auto labelLeft = (newWidth - _text->width()) / 2;
	_text->moveToLeft(labelLeft, labelTop, newWidth);

	update();
	return _height;
}

void EmptyWidget::paintEvent(QPaintEvent *e) {
	auto p = QPainter(this);

	const auto iconLeft = (width() - st::infoEmptyStories.width()) / 2;
	const auto iconTop = height() - st::infoEmptyIconTop;
	st::infoEmptyStories.paint(p, iconLeft, iconTop, width());
}

InnerWidget::InnerWidget(
	QWidget *parent,
	not_null<Controller*> controller)
: RpWidget(parent)
, _controller(controller)
, _empty(this) {
	_empty->heightValue(
	) | rpl::start_with_next(
		[this] { refreshHeight(); },
		_empty->lifetime());
	_list = setupList();
}

void InnerWidget::setupTop() {
	const auto key = _controller->key();
	const auto peer = key.storiesPeer();
	if (peer && key.storiesTab() == Stories::Tab::Saved && _isStackBottom) {
		if (peer->isSelf()) {
			createProfileTop();
		} else if (peer->owner().stories().hasArchive(peer)) {
			createButtons();
		} else {
			_top.destroy();
			refreshHeight();
		}
	} else if (peer && key.storiesTab() == Stories::Tab::Archive) {
		createAboutArchive();
	} else {
		_top.destroy();
		refreshHeight();
	}
}

void InnerWidget::startTop() {
	_top.create(this);
	_top->show();
	_topHeight = _top->heightValue();
}

void InnerWidget::createProfileTop() {
	const auto key = _controller->key();
	const auto peer = key.storiesPeer();

	startTop();
	Profile::AddCover(_top, _controller, peer, nullptr);
	Profile::AddDetails(_top, _controller, peer, nullptr, { v::null });

	auto tracker = Ui::MultiSlideTracker();
	const auto dividerWrap = _top->add(
		object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
			_top,
			object_ptr<Ui::VerticalLayout>(_top)));
	const auto divider = dividerWrap->entity();
	Ui::AddDivider(divider);
	Ui::AddSkip(divider);

	addGiftsButton(tracker);
	addArchiveButton(tracker);
	addRecentButton(tracker);

	dividerWrap->toggleOn(tracker.atLeastOneShownValue());

	finalizeTop();
}

void InnerWidget::createButtons() {
	startTop();
	auto tracker = Ui::MultiSlideTracker();
	addArchiveButton(tracker);
	addRecentButton(tracker);
	finalizeTop();
}

void InnerWidget::addArchiveButton(Ui::MultiSlideTracker &tracker) {
	Expects(_top != nullptr);

	const auto key = _controller->key();
	const auto peer = key.storiesPeer();
	const auto stories = &peer->owner().stories();

	if (!stories->archiveCountKnown(peer->id)) {
		stories->archiveLoadMore(peer->id);
	}

	auto count = rpl::single(
		rpl::empty
	) | rpl::then(
		stories->archiveChanged(
		) | rpl::filter(
			rpl::mappers::_1 == peer->id
		) | rpl::to_empty
	) | rpl::map([=] {
		return stories->archiveCount(peer->id);
	}) | rpl::start_spawning(_top->lifetime());

	const auto archiveWrap = _top->add(
		object_ptr<Ui::SlideWrap<Ui::SettingsButton>>(
			_top,
			object_ptr<Ui::SettingsButton>(
				_top,
				tr::lng_stories_archive_button(),
				st::infoSharedMediaButton))
	)->setDuration(
		st::infoSlideDuration
	)->toggleOn(rpl::duplicate(count) | rpl::map(rpl::mappers::_1 > 0));

	const auto archive = archiveWrap->entity();
	archive->addClickHandler([=] {
		_controller->showSection(Info::Stories::Make(
			_controller->key().storiesPeer(),
			Stories::Tab::Archive));
	});
	auto label = rpl::duplicate(
		count
	) | rpl::filter(
		rpl::mappers::_1 > 0
	) | rpl::map([=](int count) {
		return (count > 0) ? QString::number(count) : QString();
	});
	::Settings::CreateRightLabel(
		archive,
		std::move(label),
		st::infoSharedMediaButton,
		tr::lng_stories_archive_button());
	object_ptr<Profile::FloatingIcon>(
		archive,
		st::infoIconMediaStoriesArchive,
		st::infoSharedMediaButtonIconPosition)->show();
	tracker.track(archiveWrap);
}

void InnerWidget::addRecentButton(Ui::MultiSlideTracker &tracker) {
	Expects(_top != nullptr);

	const auto key = _controller->key();
	const auto peer = key.storiesPeer();
	const auto recentWrap = _top->add(
		object_ptr<Ui::SlideWrap<Ui::SettingsButton>>(
			_top,
			object_ptr<Ui::SettingsButton>(
				_top,
				tr::lng_stories_recent_button(),
				st::infoSharedMediaButton)));

	using namespace Dialogs::Stories;
	auto last = LastForPeer(
		peer
	) | rpl::map([=](Content &&content) {
		for (auto &element : content.elements) {
			element.unreadCount = 0;
		}
		return std::move(content);
	}) | rpl::start_spawning(recentWrap->lifetime());
	const auto recent = recentWrap->entity();
	const auto thumbs = Ui::CreateChild<List>(
		recent,
		st::dialogsStoriesListMine,
		rpl::duplicate(last) | rpl::filter([](const Content &content) {
			return !content.elements.empty();
		}));
	thumbs->show();
	rpl::combine(
		recent->sizeValue(),
		rpl::duplicate(last)
	) | rpl::start_with_next([=](QSize size, const Content &content) {
		if (content.elements.empty()) {
			return;
		}
		const auto &small = st::dialogsStories;
		const auto height = small.photo + 2 * small.photoTop;
		const auto top = (size.height() - height) / 2;
		const auto right = st::settingsButtonRightSkip
			- small.left
			- small.photoLeft;
		const auto left = size.width() - right;
		thumbs->setLayoutConstraints({ left, top }, style::al_right);
	}, thumbs->lifetime());
	thumbs->setAttribute(Qt::WA_TransparentForMouseEvents);
	recent->addClickHandler([=] {
		_controller->parentController()->openPeerStories(peer->id);
	});
	object_ptr<Profile::FloatingIcon>(
		recent,
		st::infoIconMediaStoriesRecent,
		st::infoSharedMediaButtonIconPosition)->show();
	recentWrap->toggleOn(rpl::duplicate(
		last
	) | rpl::map([](const Content &content) {
		return !content.elements.empty();
	}));
	tracker.track(recentWrap);
}

void InnerWidget::addGiftsButton(Ui::MultiSlideTracker &tracker) {
	Expects(_top != nullptr);

	const auto key = _controller->key();
	const auto peer = key.storiesPeer();
	const auto user = peer->asUser();
	Assert(user != nullptr);

	auto count = Profile::PeerGiftsCountValue(
		user
	) | rpl::start_spawning(_top->lifetime());

	const auto giftsWrap = _top->add(
		object_ptr<Ui::SlideWrap<Ui::SettingsButton>>(
			_top,
			object_ptr<Ui::SettingsButton>(
				_top,
				tr::lng_peer_gifts_title(),
				st::infoSharedMediaButton))
	)->setDuration(
		st::infoSlideDuration
	)->toggleOn(rpl::duplicate(count) | rpl::map(rpl::mappers::_1 > 0));

	const auto gifts = giftsWrap->entity();
	gifts->addClickHandler([=] {
		_controller->showSection(
			std::make_shared<Info::Memento>(
				user,
				Section::Type::PeerGifts));
	});
	auto label = rpl::duplicate(
		count
	) | rpl::filter(
		rpl::mappers::_1 > 0
	) | rpl::map([=](int count) {
		return (count > 0) ? QString::number(count) : QString();
	});
	::Settings::CreateRightLabel(
		gifts,
		std::move(label),
		st::infoSharedMediaButton,
		tr::lng_stories_archive_button());
	object_ptr<Profile::FloatingIcon>(
		gifts,
		st::infoIconMediaGifts,
		st::infoSharedMediaButtonIconPosition)->show();
	tracker.track(giftsWrap);
}

void InnerWidget::finalizeTop() {
	Ui::AddSkip(_top, st::infoProfileSkip);
	Ui::AddDivider(_top);

	_top->resizeToWidth(width());

	_top->heightValue(
	) | rpl::start_with_next([=] {
		refreshHeight();
	}, _top->lifetime());
}

void InnerWidget::createAboutArchive() {
	startTop();

	const auto peer = _controller->key().storiesPeer();
	_top->add(object_ptr<Ui::DividerLabel>(
		_top,
		object_ptr<Ui::FlatLabel>(
			_top,
			(peer->isChannel()
				? tr::lng_stories_channel_archive_about
				: tr::lng_stories_archive_about)(),
			st::infoStoriesAboutArchive),
		st::infoStoriesAboutArchivePadding));

	finalizeTop();
}

void InnerWidget::visibleTopBottomUpdated(
		int visibleTop,
		int visibleBottom) {
	setChildVisibleTopBottom(_list, visibleTop, visibleBottom);
}

bool InnerWidget::showInternal(not_null<Memento*> memento) {
	if (memento->section().type() == Section::Type::Stories) {
		restoreState(memento);
		return true;
	}
	return false;
}

object_ptr<Media::ListWidget> InnerWidget::setupList() {
	auto result = object_ptr<Media::ListWidget>(
		this,
		_controller);
	result->heightValue(
	) | rpl::start_with_next(
		[this] { refreshHeight(); },
		result->lifetime());
	using namespace rpl::mappers;
	result->scrollToRequests(
	) | rpl::map([widget = result.data()](int to) {
		return Ui::ScrollToRequest {
			widget->y() + to,
			-1
		};
	}) | rpl::start_to_stream(
		_scrollToRequests,
		result->lifetime());
	_selectedLists.fire(result->selectedListValue());
	_listTops.fire(result->topValue());
	return result;
}

void InnerWidget::saveState(not_null<Memento*> memento) {
	_list->saveState(&memento->media());
}

void InnerWidget::restoreState(not_null<Memento*> memento) {
	_list->restoreState(&memento->media());
}

rpl::producer<SelectedItems> InnerWidget::selectedListValue() const {
	return _selectedLists.events_starting_with(
		_list->selectedListValue()
	) | rpl::flatten_latest();
}

void InnerWidget::selectionAction(SelectionAction action) {
	_list->selectionAction(action);
}

InnerWidget::~InnerWidget() = default;

int InnerWidget::resizeGetHeight(int newWidth) {
	_inResize = true;
	auto guard = gsl::finally([this] { _inResize = false; });

	if (_top) {
		_top->resizeToWidth(newWidth);
	}
	_list->resizeToWidth(newWidth);
	_empty->resizeToWidth(newWidth);
	return recountHeight();
}

void InnerWidget::refreshHeight() {
	if (_inResize) {
		return;
	}
	resize(width(), recountHeight());
}

int InnerWidget::recountHeight() {
	auto top = 0;
	if (_top) {
		_top->moveToLeft(0, top);
		top += _top->heightNoMargins() - st::lineWidth;
	}
	auto listHeight = 0;
	if (_list) {
		_list->moveToLeft(0, top);
		listHeight = _list->heightNoMargins();
		top += listHeight;
	}
	if (listHeight > 0) {
		_empty->hide();
	} else {
		_empty->show();
		_empty->moveToLeft(0, top);
		top += _empty->heightNoMargins();
	}
	return top;
}

void InnerWidget::setScrollHeightValue(rpl::producer<int> value) {
	using namespace rpl::mappers;
	_empty->setFullHeight(rpl::combine(
		std::move(value),
		_listTops.events_starting_with(
			_list->topValue()
		) | rpl::flatten_latest(),
		_topHeight.value(),
		_1 - _2 + _3));
}

rpl::producer<Ui::ScrollToRequest> InnerWidget::scrollToRequests() const {
	return _scrollToRequests.events();
}

} // namespace Info::Stories
