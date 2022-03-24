/*
This file is part of 64Gram Desktop,
the unofficial app based on Telegram Desktop.
For license and copyright information please follow this link:
https://github.com/TDesktop-x64/tdesktop/blob/dev/LEGAL
*/
#include <base/timer_rpl.h>
#include <ui/toast/toast.h>
#include <mainwindow.h>
#include <QJsonArray>
#include <QJsonDocument>
#include "settings/settings_enhanced.h"

#include "settings/settings_common.h"
#include "settings/settings_chat.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/wrap/slide_wrap.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/continuous_sliders.h"
#include "ui/text/text_utilities.h" // Ui::Text::ToUpper
#include "boxes/connection_box.h"
#include "boxes/enhanced_options_box.h"
#include "boxes/about_box.h"
#include "ui/boxes/confirm_box.h"
#include "platform/platform_specific.h"
#include "window/window_session_controller.h"
#include "lang/lang_keys.h"
#include "core/update_checker.h"
#include "core/enhanced_settings.h"
#include "core/application.h"
#include "storage/localstorage.h"
#include "data/data_session.h"
#include "main/main_session.h"
#include "layout/layout_item_base.h"
#include "facades.h"
#include "styles/style_settings.h"
#include "apiwrap.h"
#include "api/api_blocked_peers.h"

namespace Settings {

	void Enhanced::SetupEnhancedNetwork(not_null<Ui::VerticalLayout *> container) {
		const auto wrap = container->add(
				object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
						container,
						object_ptr<Ui::VerticalLayout>(container)));
		const auto inner = wrap->entity();

		AddDividerText(inner, tr::lng_settings_restart_hint());
		AddSkip(container);
		AddSubsectionTitle(container, tr::lng_settings_network());

		auto boostBtn = AddButtonWithLabel(
				container,
				tr::lng_settings_net_speed_boost(),
				rpl::single(NetBoostBox::BoostLabel(cNetSpeedBoost())),
				st::settingsButton
		);
		boostBtn->setColorOverride(QColor(255, 0, 0));
		boostBtn->addClickHandler([=] {
			Ui::show(Box<NetBoostBox>());
		});

		AddSkip(container);
	}

	void Enhanced::writeBlocklistFile() {
		QFile file(cWorkingDir() + qsl("tdata/blocklist.json"));
		if (file.open(QIODevice::WriteOnly)) {
			auto toArray = [&] {
				QJsonArray array;
				for (auto id : blockList) {
					array.append(id);
				}
				return array;
			};
			auto doc = QJsonDocument(toArray());
			file.write(doc.toJson(QJsonDocument::Compact));
			file.close();
			Ui::Toast::Show("Restart in 3 seconds!");
			QTimer::singleShot(3 * 1000, []{ Core::Restart(); });
		} else {
			Ui::Toast::Show("Failed to save blocklist.");
		}
	}

	void Enhanced::reqBlocked(int offset) {
		if (_requestId) {
			return;
		}
		_requestId = App::wnd()->sessionController()->session().api().request(MTPcontacts_GetBlocked(
				MTP_int(offset),
				MTP_int(100)
		)).done([=](const MTPcontacts_Blocked &result) {
			_requestId = 0;
			result.match([&](const MTPDcontacts_blockedSlice& data) { // Incomplete list of blocked users response.
				blockCount = data.vcount().v;
				for (const auto& user : data.vusers().v) {
					blockList.append(int64(UserId(user.c_user().vid().v).bare));
				}
				if (blockCount > blockList.length()) {
					reqBlocked(offset+100);
				} else {
					writeBlocklistFile();
				}
			}, [&](const MTPDcontacts_blocked& data) { // 	Full list of blocked users response.
				for (const auto& user : data.vusers().v) {
					blockList.append(int64(UserId(user.c_user().vid().v).bare));
				}
				writeBlocklistFile();
			});
		}).fail([=] {
			_requestId = 0;
		}).send();
	}

	void Enhanced::SetupEnhancedMessages(not_null<Ui::VerticalLayout *> container) {
		AddDivider(container);
		AddSkip(container);
		AddSubsectionTitle(container, tr::lng_settings_messages());

		const auto wrap = container->add(
				object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
						container,
						object_ptr<Ui::VerticalLayout>(container)));
		const auto inner = wrap->entity();

		auto MsgIdBtn = AddButton(
				inner,
				tr::lng_settings_show_message_id(),
				st::settingsButton
		);
		MsgIdBtn->setColorOverride(QColor(255, 0, 0));
		MsgIdBtn->toggleOn(
				rpl::single(cShowMessagesID())
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != cShowMessagesID());
		}) | rpl::start_with_next([=](bool toggled) {
			cSetShowMessagesID(toggled);
			EnhancedSettings::Write();
			Core::Restart();
		}, container->lifetime());

		AddButton(
				inner,
				tr::lng_settings_show_repeater_option(),
				st::settingsButton
		)->toggleOn(
				rpl::single(cShowRepeaterOption())
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != cShowRepeaterOption());
		}) | rpl::start_with_next([=](bool toggled) {
			cSetShowRepeaterOption(toggled);
			EnhancedSettings::Write();
		}, container->lifetime());

		if (cShowRepeaterOption()) {
			AddButton(
					inner,
					tr::lng_settings_repeater_reply_to_orig_msg(),
					st::settingsButton
			)->toggleOn(
					rpl::single(cRepeaterReplyToOrigMsg())
			)->toggledChanges(
			) | rpl::filter([=](bool toggled) {
				return (toggled != cRepeaterReplyToOrigMsg());
			}) | rpl::start_with_next([=](bool toggled) {
				cSetRepeaterReplyToOrigMsg(toggled);
				EnhancedSettings::Write();
			}, container->lifetime());
		}

		auto value = rpl::single(
				AlwaysDeleteBox::DeleteLabel(cAlwaysDeleteFor())
		) | rpl::then(
				_AlwaysDeleteChanged.events()
		) | rpl::map([] {
			return AlwaysDeleteBox::DeleteLabel(cAlwaysDeleteFor());
		});

		auto btn = AddButtonWithLabel(
				container,
				tr::lng_settings_always_delete_for(),
				std::move(value),
				st::settingsButton
		);
		btn->events(
		) | rpl::start_with_next([=]() {
			_AlwaysDeleteChanged.fire({});
		}, container->lifetime());
		btn->addClickHandler([=] {
			Ui::show(Box<AlwaysDeleteBox>());
		});

		AddButton(
				inner,
				tr::lng_settings_disable_cloud_draft_sync(),
				st::settingsButton
		)->toggleOn(
				rpl::single(cDisableCloudDraftSync())
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != cDisableCloudDraftSync());
		}) | rpl::start_with_next([=](bool toggled) {
			cSetDisableCloudDraftSync(toggled);
			EnhancedSettings::Write();
		}, container->lifetime());

		AddSkip(container);

		AddButton(
				inner,
				tr::lng_settings_hide_classic_forward(),
				st::settingsButton
		)->toggleOn(
				rpl::single(cHideClassicFwd())
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != cHideClassicFwd());
		}) | rpl::start_with_next([=](bool toggled) {
			cSetHideClassicFwd(toggled);
			EnhancedSettings::Write();
		}, container->lifetime());

		AddButton(
				inner,
				tr::lng_settings_disable_link_warning(),
				st::settingsButton
		)->toggleOn(
				rpl::single(cDisableLinkWarning())
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != cDisableLinkWarning());
		}) | rpl::start_with_next([=](bool toggled) {
			cSetDisableLinkWarning(toggled);
			EnhancedSettings::Write();
		}, container->lifetime());

		auto hideBtn = AddButton(
			inner,
			tr::lng_settings_hide_messages(),
			st::settingsButton
		);
		hideBtn->setColorOverride(QColor(255, 0, 0));
		hideBtn->toggleOn(
				rpl::single(cBlockedUserSpoilerMode())
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != cBlockedUserSpoilerMode());
		}) | rpl::start_with_next([=](bool toggled) {
			cSetBlockedUserSpoilerMode(toggled);
			EnhancedSettings::Write();
			if (toggled) {
				Ui::Toast::Show("Please wait a moment, fetching blocklist...");

				App::wnd()->sessionController()->session().api().blockedPeers().slice() | rpl::take(
					1
				) | rpl::start_with_next([&](const Api::BlockedPeers::Slice &result) {
					if (blockList.length() == result.total) {
						return;
					}
					blockList = QList<int64>();
					reqBlocked(0);
				}, lifetime());
			}
		}, container->lifetime());

		AddDividerText(inner, tr::lng_settings_hide_messages_desc());
	}

	void Enhanced::SetupEnhancedButton(not_null<Ui::VerticalLayout *> container) {
		AddDivider(container);
		AddSkip(container);
		AddSubsectionTitle(container, tr::lng_settings_button());

		const auto wrap = container->add(
				object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
						container,
						object_ptr<Ui::VerticalLayout>(container)));
		const auto inner = wrap->entity();

		auto EmojiBtn = AddButton(
				inner,
				tr::lng_settings_show_emoji_button_as_text(),
				st::settingsButton
		);
		EmojiBtn->setColorOverride(QColor(255, 0, 0));
		EmojiBtn->toggleOn(
				rpl::single(cShowEmojiButtonAsText())
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != cShowEmojiButtonAsText());
		}) | rpl::start_with_next([=](bool toggled) {
			cSetShowEmojiButtonAsText(toggled);
			EnhancedSettings::Write();
			Core::Restart();
		}, container->lifetime());

		AddDividerText(inner, tr::lng_show_emoji_button_as_text_desc());

		AddButton(
				inner,
				tr::lng_settings_show_scheduled_button(),
				st::settingsButton
		)->toggleOn(
				rpl::single(cShowScheduledButton())
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != cShowScheduledButton());
		}) | rpl::start_with_next([=](bool toggled) {
			cSetShowScheduledButton(toggled);
			EnhancedSettings::Write();
		}, container->lifetime());

		AddSkip(container);
	}

	void Enhanced::SetupEnhancedVoiceChat(not_null<Ui::VerticalLayout *> container) {
		AddDivider(container);
		AddSkip(container);
		AddSubsectionTitle(container, tr::lng_settings_voice_chat());

		const auto wrap = container->add(
				object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
						container,
						object_ptr<Ui::VerticalLayout>(container)));
		const auto inner = wrap->entity();

		AddButton(
				inner,
				tr::lng_settings_radio_controller(),
				st::settingsButton
		)->addClickHandler([=] {
			Ui::show(Box<RadioController>());
		});

		AddDividerText(inner, tr::lng_radio_controller_desc());

		AddButton(
				inner,
				tr::lng_settings_auto_unmute(),
				st::settingsButton
		)->toggleOn(
				rpl::single(cAutoUnmute())
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != cAutoUnmute());
		}) | rpl::start_with_next([=](bool toggled) {
			cSetAutoUnmute(toggled);
			EnhancedSettings::Write();
		}, container->lifetime());

		AddDividerText(inner, tr::lng_auto_unmute_desc());

		auto value = rpl::single(
				BitrateController::BitrateLabel(cVoiceChatBitrate())
		) | rpl::then(
				_BitrateChanged.events()
		) | rpl::map([=] {
			return BitrateController::BitrateLabel(cVoiceChatBitrate());
		});

		auto btn = AddButtonWithLabel(
				container,
				tr::lng_bitrate_controller(),
				std::move(value),
				st::settingsButton
		);
		btn->events(
		) | rpl::start_with_next([=]() {
			_BitrateChanged.fire({});
		}, container->lifetime());
		btn->addClickHandler([=] {
			Ui::show(Box<BitrateController>());
		});

		AddButton(
				inner,
				tr::lng_settings_enable_hd_video(),
				st::settingsButton
		)->toggleOn(
				rpl::single(cHDVideo())
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != cHDVideo());
		}) | rpl::start_with_next([=](bool toggled) {
			cSetHDVideo(toggled);
			Ui::Toast::Show(tr::lng_hd_video_hint(tr::now));
			EnhancedSettings::Write();
		}, container->lifetime());

		AddSkip(container);
	}

	void Enhanced::SetupEnhancedOthers(not_null<Window::SessionController*> controller, not_null<Ui::VerticalLayout *> container) {
		AddDivider(container);
		AddSkip(container);
		AddSubsectionTitle(container, tr::lng_settings_other());

		const auto wrap = container->add(
				object_ptr<Ui::SlideWrap<Ui::VerticalLayout>>(
						container,
						object_ptr<Ui::VerticalLayout>(container)));
		const auto inner = wrap->entity();

		AddButton(
				container,
				tr::lng_settings_hide_all_chats(),
				st::settingsButton
		)->toggleOn(
				rpl::single(cHideFilterAllChats())
		)->toggledValue(
		) | rpl::filter([](bool enabled) {
			return (enabled != cHideFilterAllChats());
		}) | rpl::start_with_next([=](bool enabled) {
			cSetHideFilterAllChats(enabled);
			EnhancedSettings::Write();
			controller->reloadFiltersMenu();
			App::wnd()->fixOrder();
		}, container->lifetime());

		AddButton(
				container,
				tr::lng_settings_replace_edit_button(),
				st::settingsButton
		)->toggleOn(
				rpl::single(cReplaceEditButton())
		)->toggledValue(
		) | rpl::filter([](bool enabled) {
			return (enabled != cReplaceEditButton());
		}) | rpl::start_with_next([=](bool enabled) {
			cSetReplaceEditButton(enabled);
			EnhancedSettings::Write();
			controller->reloadFiltersMenu();
		}, container->lifetime());

		AddButton(
				container,
				tr::lng_settings_skip_message(),
				st::settingsButton
		)->toggleOn(
				rpl::single(cSkipSc())
		)->toggledValue(
		) | rpl::filter([](bool enabled) {
			return (enabled != cSkipSc());
		}) | rpl::start_with_next([=](bool enabled) {
			cSetSkipSc(enabled);
			EnhancedSettings::Write();
		}, container->lifetime());

		AddDividerText(container, tr::lng_settings_skip_message_desc());

		AddSkip(container);
	}

	Enhanced::Enhanced(
			QWidget *parent,
			not_null<Window::SessionController *> controller)
			: Section(parent) {
		setupContent(controller);
	}

	void Enhanced::setupContent(not_null<Window::SessionController *> controller) {
		const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);

		SetupEnhancedNetwork(content);
		SetupEnhancedMessages(content);
		SetupEnhancedButton(content);
		SetupEnhancedVoiceChat(content);
		SetupEnhancedOthers(controller, content);

		Ui::ResizeFitChild(this, content);
	}
} // namespace Settings

