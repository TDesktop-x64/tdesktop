/*
This file is part of 64Gram Desktop,
the unofficial app based on Telegram Desktop.
For license and copyright information please follow this link:
https://github.com/TDesktop-x64/tdesktop/blob/dev/LEGAL
*/
#include "boxes/message_filter_box.h"

#include "lang/lang_keys.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "boxes/peer_list_box.h"
#include "boxes/filters/edit_filter_chats_list.h"
#include "data/data_chat_filters.h"
#include "data/data_session.h"
#include "data/data_premium_limits.h"
#include "history/history.h"
#include "main/main_session.h"
#include "facades.h"
#include "styles/style_layers.h"
#include "styles/style_boxes.h"
#include "styles/style_settings.h"
#include "styles/style_chat_helpers.h"
#include "core/enhanced_settings.h"
#include "facades.h"
#include "window/window_session_controller.h"

#include <QtCore/QUuid>

MessageFilterListBox::MessageFilterListBox(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: _controller(controller) {
}

void MessageFilterListBox::prepare() {
	setTitle(tr::lng_filter_manage_title());

	addButton(tr::lng_filter_add(), [=] { addFilter(); });
	addButton(tr::lng_close(), [=] { closeBox(); });

	_list.create(this);
	_list->resizeToWidth(st::boxWidth);
	_list->show();

	refreshList();
	_list->resizeToWidth(st::boxWidth);
	_prepared = true;

	const auto height = std::min(600, _list->height() + st::boxPadding.top() + st::boxPadding.bottom());
	setDimensions(st::boxWidth, std::max(height, st::boxPadding.top() + st::boxPadding.bottom() + 50));
}

void MessageFilterListBox::showEvent(QShowEvent *e) {
	Ui::BoxContent::showEvent(e);
	if (_prepared) {
		refreshList();
	}
}

void MessageFilterListBox::refreshList() {
	_list->clear();

	const auto filters = EnhancedSettings::GetMessageFilters();
	
	if (filters.isEmpty()) {
		_list->add(object_ptr<Ui::FlatLabel>(
			_list,
			tr::lng_filter_no_filters(tr::now),
			st::boxLabel));
		return;
	}

	for (int i = 0; i < filters.size(); ++i) {
		const auto &filter = filters[i];
		const auto row = _list->add(object_ptr<Ui::SettingsButton>(
			_list,
			rpl::single(filter.name),
			st::settingsButton));
		
		row->setClickedCallback([=, id = filter.id] {
			editFilter(id);
		});

		// Add up/down text links for reordering
		const auto upBtn = Ui::CreateChild<Ui::LinkButton>(
			row,
			QString::fromUtf8("↑"));
		upBtn->show();
		upBtn->setVisible(i > 0); // Hide for first item
		upBtn->setClickedCallback([=, id = filter.id] {
			moveFilter(id, -1);
		});

		const auto downBtn = Ui::CreateChild<Ui::LinkButton>(
			row,
			QString::fromUtf8("↓"));
		downBtn->show();
		downBtn->setVisible(i < filters.size() - 1); // Hide for last item
		downBtn->setClickedCallback([=, id = filter.id] {
			moveFilter(id, 1);
		});

		// Add delete button
		const auto deleteBtn = Ui::CreateChild<Ui::IconButton>(
			row,
			st::filtersRemove);
		deleteBtn->show(); // Explicitly show the delete button
		deleteBtn->setClickedCallback([=, id = filter.id] {
			deleteFilter(id);
		});

		row->widthValue(
		) | rpl::start_with_next([=](int width) {
			const auto right = st::settingsButton.padding.right();
			const auto top = (row->height() - deleteBtn->height()) / 2;
			
			// Position delete button at far right
			deleteBtn->moveToRight(right, top);
			deleteBtn->raise();
			
			// Position down button to left of delete
			if (downBtn->isVisible()) {
				const auto downTop = (row->height() - downBtn->height()) / 2;
				downBtn->moveToRight(right + deleteBtn->width() + 8, downTop);
				downBtn->raise();
			}
			
			// Position up button to left of down
			if (upBtn->isVisible()) {
				const auto upTop = (row->height() - upBtn->height()) / 2;
				const auto downWidth = downBtn->isVisible() ? downBtn->width() + 8 : 0;
				upBtn->moveToRight(right + deleteBtn->width() + 8 + downWidth, upTop);
				upBtn->raise();
			}
		}, deleteBtn->lifetime());
	}
}

void MessageFilterListBox::addFilter() {
	MessageFilters::MessageFilter newFilter;
	newFilter.id = QUuid::createUuid().toString();
	newFilter.name = "New Filter";
	newFilter.regex = "";
	newFilter.mode = MessageFilters::FilterMode::Blacklist;
	newFilter.displayMode = MessageFilters::FilterDisplayMode::Hide;
	newFilter.order = EnhancedSettings::GetMessageFilters().size();
	newFilter.enabled = true;

	getDelegate()->show(Box<MessageFilterEditBox>(_controller, newFilter, true), Ui::LayerOption::KeepOther);
}

void MessageFilterListBox::editFilter(const QString &filterId) {
	const auto filters = EnhancedSettings::GetMessageFilters();
	for (const auto &filter : filters) {
		if (filter.id == filterId) {
			getDelegate()->show(Box<MessageFilterEditBox>(_controller, filter, false), Ui::LayerOption::KeepOther);
			return;
		}
	}
}

void MessageFilterListBox::deleteFilter(const QString &filterId) {
	getDelegate()->show(Ui::MakeConfirmBox({
		.text = tr::lng_filter_delete_confirm(tr::now),
		.confirmed = [=, this](Fn<void()> close) {
			EnhancedSettings::DeleteMessageFilter(filterId);
			refreshList();
			_list->resizeToWidth(st::boxWidth);
			const auto height = std::min(600, _list->height() + st::boxPadding.top() + st::boxPadding.bottom());
			setDimensions(st::boxWidth, std::max(height, st::boxPadding.top() + st::boxPadding.bottom() + 50));
			close();
		},
		.confirmText = tr::lng_box_delete(tr::now),
	}), Ui::LayerOption::KeepOther);
}

void MessageFilterListBox::moveFilter(const QString &filterId, int direction) {
	auto filters = EnhancedSettings::GetMessageFilters();
	int index = -1;
	
	// Find the filter index
	for (int i = 0; i < filters.size(); ++i) {
		if (filters[i].id == filterId) {
			index = i;
			break;
		}
	}
	
	if (index < 0) return;
	
	const int newIndex = index + direction;
	if (newIndex < 0 || newIndex >= filters.size()) return;
	
	// Swap the filters
	std::swap(filters[index], filters[newIndex]);
	
	// Update order field for both filters
	filters[index].order = index;
	filters[newIndex].order = newIndex;
	
	// Extract filter IDs in the new order
	QVector<QString> filterIds;
	for (const auto &filter : filters) {
		filterIds.append(filter.id);
	}
	
	// Reorder all filters in the storage
	EnhancedSettings::ReorderFilters(filterIds);
	
	// Refresh the UI
	refreshList();
	_list->resizeToWidth(st::boxWidth);
}

// MessageFilterEditBox implementation

MessageFilterEditBox::MessageFilterEditBox(
	QWidget *parent,
	not_null<Window::SessionController*> controller,
	const MessageFilters::MessageFilter &filter,
	bool isNew)
: _controller(controller)
, _filter(filter)
, _isNew(isNew)
, _name(this, st::defaultInputField, rpl::single(QString()), filter.name)
, _regex(this, st::defaultInputField, tr::lng_filter_regex(), filter.regex)
, _replacementText(this, st::defaultInputField, tr::lng_filter_replacement_text(), filter.replacementText) {
}

void MessageFilterEditBox::prepare() {
	setTitle(_isNew ? tr::lng_filter_add() : tr::lng_filter_edit());

	addButton(tr::lng_settings_save(), [=] { save(); });
	addButton(tr::lng_cancel(), [=] { closeBox(); });

	auto y = st::boxPadding.top();

	// Name field
	_name->moveToLeft(st::boxPadding.left(), y);
	_name->resize(st::boxWidth - 2 * st::boxPadding.left(), _name->height());
	y += _name->height() + st::boxMediumSkip;

	// Regex field
	_regex->moveToLeft(st::boxPadding.left(), y);
	_regex->resize(st::boxWidth - 2 * st::boxPadding.left(), _regex->height());
	y += _regex->height() + st::boxMediumSkip;

	// Replacement text field (for Replace mode)
	_replacementText->moveToLeft(st::boxPadding.left(), y);
	_replacementText->resize(st::boxWidth - 2 * st::boxPadding.left(), _replacementText->height());
	y += _replacementText->height() + st::boxMediumSkip;

	// Global checkbox (checked if BOTH chatIds AND userIds are empty)
	_global.create(
		this,
		tr::lng_filter_global(tr::now),
		_filter.chatIds.isEmpty() && _filter.userIds.isEmpty(),
		st::defaultCheckbox);
	_global->moveToLeft(st::boxPadding.left(), y);
	_global->checkedChanges(
	) | rpl::start_with_next([=](bool checked) {
		updateGlobalState();
	}, _global->lifetime());
	y += _global->heightNoMargins() + st::boxMediumSkip;

	// Chat selection button
	_chatSelectBtn = Ui::CreateChild<Ui::LinkButton>(
		this,
		tr::lng_filter_select_chats(tr::now));
	_chatSelectBtn->setClickedCallback([=] { selectChats(); });
	_chatSelectBtn->moveToLeft(st::boxPadding.left(), y);
	y += _chatSelectBtn->height() + st::boxLittleSkip;

	// Chat label (shows selected chats)
	_chatLabel = Ui::CreateChild<Ui::FlatLabel>(
		this,
		QString(),
		st::boxLabel);
	_chatLabel->moveToLeft(st::boxPadding.left(), y);
	updateChatLabel();
	y += _chatLabel->height() + st::boxMediumSkip;

	// User selection button
	_userSelectBtn = Ui::CreateChild<Ui::LinkButton>(
		this,
		tr::lng_filter_select_users(tr::now));
	_userSelectBtn->setClickedCallback([=] { selectUsers(); });
	_userSelectBtn->moveToLeft(st::boxPadding.left(), y);
	y += _userSelectBtn->height() + st::boxLittleSkip;

	// User label (shows selected users)
	_userLabel = Ui::CreateChild<Ui::FlatLabel>(
		this,
		QString(),
		st::boxLabel);
	_userLabel->moveToLeft(st::boxPadding.left(), y);
	updateUserLabel();
	y += _userLabel->height() + st::boxMediumSkip;

	// Mode radio buttons
	auto modeLabel = Ui::CreateChild<Ui::FlatLabel>(
		this,
		tr::lng_filter_mode(tr::now),
		st::boxLabel);
	modeLabel->moveToLeft(st::boxPadding.left(), y);
	y += modeLabel->height() + st::boxLittleSkip;

	_modeGroup = std::make_shared<Ui::RadiobuttonGroup>(
		static_cast<int>(_filter.mode));

	auto whitelistBtn = Ui::CreateChild<Ui::Radiobutton>(
		this,
		_modeGroup,
		static_cast<int>(MessageFilters::FilterMode::Whitelist),
		tr::lng_filter_mode_whitelist(tr::now),
		st::defaultCheckbox);
	whitelistBtn->moveToLeft(st::boxPadding.left(), y);
	y += whitelistBtn->heightNoMargins() + st::boxLittleSkip;

	auto blacklistBtn = Ui::CreateChild<Ui::Radiobutton>(
		this,
		_modeGroup,
		static_cast<int>(MessageFilters::FilterMode::Blacklist),
		tr::lng_filter_mode_blacklist(tr::now),
		st::defaultCheckbox);
	blacklistBtn->moveToLeft(st::boxPadding.left(), y);
	y += blacklistBtn->heightNoMargins() + st::boxLittleSkip;

	auto replaceBtn = Ui::CreateChild<Ui::Radiobutton>(
		this,
		_modeGroup,
		static_cast<int>(MessageFilters::FilterMode::Replace),
		tr::lng_filter_mode_replace(tr::now),
		st::defaultCheckbox);
	replaceBtn->moveToLeft(st::boxPadding.left(), y);
	y += replaceBtn->heightNoMargins() + st::boxMediumSkip;

	// Listen for mode changes to show/hide replacement text field
	_modeGroup->setChangedCallback([=](int value) {
		updateModeState();
	});

	// Display mode radio buttons
	auto displayLabel = Ui::CreateChild<Ui::FlatLabel>(
		this,
		tr::lng_filter_display(tr::now),
		st::boxLabel);
	displayLabel->moveToLeft(st::boxPadding.left(), y);
	y += displayLabel->height() + st::boxLittleSkip;

	_displayGroup = std::make_shared<Ui::RadiobuttonGroup>(
		static_cast<int>(_filter.displayMode));

	auto hideBtn = Ui::CreateChild<Ui::Radiobutton>(
		this,
		_displayGroup,
		static_cast<int>(MessageFilters::FilterDisplayMode::Hide),
		tr::lng_filter_display_hide(tr::now),
		st::defaultCheckbox);
	hideBtn->moveToLeft(st::boxPadding.left(), y);
	y += hideBtn->heightNoMargins() + st::boxLittleSkip;

	auto dimBtn = Ui::CreateChild<Ui::Radiobutton>(
		this,
		_displayGroup,
		static_cast<int>(MessageFilters::FilterDisplayMode::Dim),
		tr::lng_filter_display_dim(tr::now),
		st::defaultCheckbox);
	dimBtn->moveToLeft(st::boxPadding.left(), y);
	y += dimBtn->heightNoMargins() + st::boxMediumSkip;

	// Enabled checkbox
	_enabled.create(
		this,
		tr::lng_filter_enabled(tr::now),
		_filter.enabled,
		st::defaultCheckbox);
	_enabled->moveToLeft(st::boxPadding.left(), y);
	y += _enabled->heightNoMargins() + st::boxMediumSkip;

	setDimensions(st::boxWidth, y);
	
	// Initialize visibility based on global checkbox state
	updateGlobalState();
	
	// Initialize visibility based on mode
	updateModeState();
}

void MessageFilterEditBox::setInnerFocus() {
	_name->setFocusFast();
}

void MessageFilterEditBox::save() {
	_filter.name = _name->getLastText();
	_filter.regex = _regex->getLastText();
	_filter.replacementText = _replacementText->getLastText();
	_filter.mode = static_cast<MessageFilters::FilterMode>(_modeGroup->current());
	_filter.displayMode = static_cast<MessageFilters::FilterDisplayMode>(_displayGroup->current());
	_filter.enabled = _enabled->checked();

	// Clear chatIds and userIds if global is checked
	if (_global->checked()) {
		_filter.chatIds.clear();
		_filter.userIds.clear();
	}

	if (_isNew) {
		EnhancedSettings::AddMessageFilter(_filter);
	} else {
		EnhancedSettings::UpdateMessageFilter(_filter);
	}

	closeBox();
}

void MessageFilterEditBox::selectUsers() {
	// Convert current user IDs to history set
	const auto session = &_controller->session();
	auto currentUsers = base::flat_set<not_null<History*>>();
	for (const auto &userId : _filter.userIds) {
		const auto peerId = PeerId(userId);
		if (const auto peer = session->data().peerLoaded(peerId)) {
			if (peer->isUser()) {
				currentUsers.insert(session->data().history(peer));
			}
		}
	}
	
	const auto limit = Data::PremiumLimits(session).dialogFiltersChatsCurrent();
	const auto showLimitReached = [=] {
		// Limit reached - for now, just do nothing
	};
	
	auto controller = std::make_unique<EditFilterChatsListController>(
		session,
		tr::lng_filter_select_users(),
		Data::ChatFilter::Flag::Contacts  // Enable users
			| Data::ChatFilter::Flag::NonContacts
			| Data::ChatFilter::Flag::Bots,
		Data::ChatFilter::Flags(0),
		currentUsers,
		limit,
		showLimitReached);
	
	auto initBox = [=, this](not_null<PeerListBox*> box) {
		box->setCloseByOutsideClick(false);
		box->addButton(tr::lng_settings_save(), crl::guard(this, [=, this] {
			const auto peers = box->collectSelectedRows();
			_filter.userIds.clear();
			for (const auto &peer : peers) {
				if (peer->isUser()) {
					_filter.userIds.insert(static_cast<int64>(peer->id.value));
				}
			}
			updateUserLabel();
			box->closeBox();
		}));
		box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
	};
	
	getDelegate()->show(
		Box<PeerListBox>(std::move(controller), std::move(initBox)),
		Ui::LayerOption::KeepOther);
}

void MessageFilterEditBox::updateUserLabel() {
	if (_filter.userIds.isEmpty()) {
		_userLabel->setText(tr::lng_filter_no_users_selected(tr::now));
	} else {
		const auto count = _filter.userIds.size();
		_userLabel->setText(QString::number(count) + " " + tr::lng_filter_users_selected(tr::now));
	}
}

void MessageFilterEditBox::selectChats() {
	// Convert current chat IDs to history set
	const auto session = &_controller->session();
	auto currentChats = base::flat_set<not_null<History*>>();
	for (const auto &chatId : _filter.chatIds) {
		const auto peerId = PeerId(chatId);
		if (const auto peer = session->data().peerLoaded(peerId)) {
			currentChats.insert(session->data().history(peer));
		}
	}
	
	const auto limit = Data::PremiumLimits(session).dialogFiltersChatsCurrent();
	const auto showLimitReached = [=] {
		// Limit reached - for now, just do nothing
	};
	
	auto controller = std::make_unique<EditFilterChatsListController>(
		session,
		tr::lng_filter_select_chats(),
		Data::ChatFilter::Flag::Contacts  // Enable all types
			| Data::ChatFilter::Flag::NonContacts
			| Data::ChatFilter::Flag::Groups
			| Data::ChatFilter::Flag::Channels
			| Data::ChatFilter::Flag::Bots,
		Data::ChatFilter::Flags(0),  // No types selected by default
		currentChats,
		limit,
		showLimitReached);
	
	auto initBox = [=, this](not_null<PeerListBox*> box) {
		box->setCloseByOutsideClick(false);
		box->addButton(tr::lng_settings_save(), crl::guard(this, [=, this] {
			const auto peers = box->collectSelectedRows();
			_filter.chatIds.clear();
			for (const auto &peer : peers) {
				_filter.chatIds.insert(static_cast<int64>(peer->id.value));
			}
			updateChatLabel();
			box->closeBox();
		}));
		box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
	};
	
	getDelegate()->show(
		Box<PeerListBox>(std::move(controller), std::move(initBox)),
		Ui::LayerOption::KeepOther);
}

void MessageFilterEditBox::updateChatLabel() {
	if (_filter.chatIds.isEmpty()) {
		_chatLabel->setText(tr::lng_filter_no_chats_selected(tr::now));
	} else {
		const auto count = _filter.chatIds.size();
		_chatLabel->setText(QString::number(count) + " " + tr::lng_filter_chats_selected(tr::now));
	}
}

void MessageFilterEditBox::updateGlobalState() {
	const auto isGlobal = _global->checked();
	
	// When global is checked, hide both chat and user selectors
	// When unchecked, show them
	if (isGlobal) {
		_chatSelectBtn->hide();
		_chatLabel->hide();
		_userSelectBtn->hide();
		_userLabel->hide();
		_filter.chatIds.clear();
		_filter.userIds.clear();
	} else {
		_chatSelectBtn->show();
		_chatLabel->show();
		_userSelectBtn->show();
		_userLabel->show();
		updateChatLabel();
		updateUserLabel();
	}
	
	// Force layout update
	update();
}

void MessageFilterEditBox::updateModeState() {
	const auto mode = static_cast<MessageFilters::FilterMode>(_modeGroup->current());
	const auto isReplace = (mode == MessageFilters::FilterMode::Replace);
	
	// Show replacement text field only when Replace mode is selected
	if (isReplace) {
		_replacementText->show();
	} else {
		_replacementText->hide();
	}
	
	// Force layout update
	update();
}

