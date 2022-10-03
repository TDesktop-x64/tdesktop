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
				rpl::single(NetBoostBox::BoostLabel(GetEnhancedInt("net_speed_boost"))),
				st::settingsButtonNoIcon
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
				st::settingsButtonNoIcon
		);
		MsgIdBtn->setColorOverride(QColor(255, 0, 0));
		MsgIdBtn->toggleOn(
				rpl::single(GetEnhancedBool("show_messages_id"))
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != GetEnhancedBool("show_messages_id"));
		}) | rpl::start_with_next([=](bool toggled) {
			SetEnhancedValue("show_messages_id", toggled);
			EnhancedSettings::Write();
			Core::Restart();
		}, container->lifetime());

		AddButton(
				inner,
				tr::lng_settings_show_repeater_option(),
				st::settingsButtonNoIcon
		)->toggleOn(
				rpl::single(GetEnhancedBool("show_repeater_option"))
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != GetEnhancedBool("show_repeater_option"));
		}) | rpl::start_with_next([=](bool toggled) {
			SetEnhancedValue("show_repeater_option", toggled);
			EnhancedSettings::Write();
		}, container->lifetime());

		if (GetEnhancedBool("show_repeater_option")) {
			AddButton(
					inner,
					tr::lng_settings_repeater_reply_to_orig_msg(),
					st::settingsButtonNoIcon
			)->toggleOn(
					rpl::single(GetEnhancedBool("repeater_reply_to_orig_msg"))
			)->toggledChanges(
			) | rpl::filter([=](bool toggled) {
				return (toggled != GetEnhancedBool("repeater_reply_to_orig_msg"));
			}) | rpl::start_with_next([=](bool toggled) {
				SetEnhancedValue("repeater_reply_to_orig_msg", toggled);
				EnhancedSettings::Write();
			}, container->lifetime());
		}

		auto value = rpl::single(
				AlwaysDeleteBox::DeleteLabel(GetEnhancedInt("always_delete_for"))
		) | rpl::then(
				_AlwaysDeleteChanged.events()
		) | rpl::map([] {
			return AlwaysDeleteBox::DeleteLabel(GetEnhancedInt("always_delete_for"));
		});

		auto btn = AddButtonWithLabel(
				container,
				tr::lng_settings_always_delete_for(),
				std::move(value),
				st::settingsButtonNoIcon
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
				st::settingsButtonNoIcon
		)->toggleOn(
				rpl::single(GetEnhancedBool("disable_cloud_draft_sync"))
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != GetEnhancedBool("disable_cloud_draft_sync"));
		}) | rpl::start_with_next([=](bool toggled) {
			SetEnhancedValue("disable_cloud_draft_sync", toggled);
			EnhancedSettings::Write();
		}, container->lifetime());

		AddSkip(container);

		AddButton(
				inner,
				tr::lng_settings_hide_classic_forward(),
				st::settingsButtonNoIcon
		)->toggleOn(
				rpl::single(GetEnhancedBool("hide_classic_fwd"))
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != GetEnhancedBool("hide_classic_fwd"));
		}) | rpl::start_with_next([=](bool toggled) {
			SetEnhancedValue("hide_classic_fwd", toggled);
			EnhancedSettings::Write();
		}, container->lifetime());

		AddButton(
				inner,
				tr::lng_settings_disable_link_warning(),
				st::settingsButtonNoIcon
		)->toggleOn(
				rpl::single(GetEnhancedBool("disable_link_warning"))
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != GetEnhancedBool("disable_link_warning"));
		}) | rpl::start_with_next([=](bool toggled) {
			SetEnhancedValue("disable_link_warning", toggled);
			EnhancedSettings::Write();
		}, container->lifetime());

		AddButton(
				inner,
				tr::lng_settings_disable_premium_animation(),
				st::settingsButtonNoIcon
		)->toggleOn(
				rpl::single(GetEnhancedBool("disable_premium_animation"))
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != GetEnhancedBool("disable_premium_animation"));
		}) | rpl::start_with_next([=](bool toggled) {
			SetEnhancedValue("disable_premium_animation", toggled);
			EnhancedSettings::Write();
		}, container->lifetime());

		auto secondsBtn = AddButton(
			inner,
			tr::lng_settings_show_seconds(),
			st::settingsButtonNoIcon
		);
		secondsBtn->setColorOverride(QColor(255, 0, 0));
		secondsBtn->toggleOn(
			rpl::single(GetEnhancedBool("show_seconds"))
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != GetEnhancedBool("show_seconds"));
		}) | rpl::start_with_next([=](bool toggled) {
			SetEnhancedValue("show_seconds", toggled);
			EnhancedSettings::Write();
			QTimer::singleShot(1 * 1000, []{ Core::Restart(); });
		}, container->lifetime());

		auto hideBtn = AddButton(
			inner,
			tr::lng_settings_hide_messages(),
			st::settingsButtonNoIcon
		);
		hideBtn->setColorOverride(QColor(255, 0, 0));
		hideBtn->toggleOn(
				rpl::single(GetEnhancedBool("blocked_user_spoiler_mode"))
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != GetEnhancedBool("blocked_user_spoiler_mode"));
		}) | rpl::start_with_next([=](bool toggled) {
			SetEnhancedValue("blocked_user_spoiler_mode", toggled);
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
				}, container->lifetime());
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
				st::settingsButtonNoIcon
		);
		EmojiBtn->setColorOverride(QColor(255, 0, 0));
		EmojiBtn->toggleOn(
				rpl::single(GetEnhancedBool("show_emoji_button_as_text"))
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != GetEnhancedBool("show_emoji_button_as_text"));
		}) | rpl::start_with_next([=](bool toggled) {
			SetEnhancedValue("show_emoji_button_as_text", toggled);
			EnhancedSettings::Write();
			Core::Restart();
		}, container->lifetime());

		AddDividerText(inner, tr::lng_show_emoji_button_as_text_desc());

		AddButton(
				inner,
				tr::lng_settings_show_scheduled_button(),
				st::settingsButtonNoIcon
		)->toggleOn(
				rpl::single(GetEnhancedBool("show_scheduled_button"))
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != GetEnhancedBool("show_scheduled_button"));
		}) | rpl::start_with_next([=](bool toggled) {
			SetEnhancedValue("show_scheduled_button", toggled);
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
				st::settingsButtonNoIcon
		)->addClickHandler([=] {
			Ui::show(Box<RadioController>());
		});

		AddDividerText(inner, tr::lng_radio_controller_desc());

		AddButton(
				inner,
				tr::lng_settings_auto_unmute(),
				st::settingsButtonNoIcon
		)->toggleOn(
				rpl::single(GetEnhancedBool("show_scheduled_button"))
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != GetEnhancedBool("show_scheduled_button"));
		}) | rpl::start_with_next([=](bool toggled) {
			SetEnhancedValue("show_scheduled_button", toggled);
			EnhancedSettings::Write();
		}, container->lifetime());

		AddDividerText(inner, tr::lng_auto_unmute_desc());

		auto value = rpl::single(
				BitrateController::BitrateLabel(GetEnhancedInt("bitrate"))
		) | rpl::then(
				_BitrateChanged.events()
		) | rpl::map([=] {
			return BitrateController::BitrateLabel(GetEnhancedInt("bitrate"));
		});

		auto btn = AddButtonWithLabel(
				container,
				tr::lng_bitrate_controller(),
				std::move(value),
				st::settingsButtonNoIcon
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
				st::settingsButtonNoIcon
		)->toggleOn(
				rpl::single(GetEnhancedBool("hd_video"))
		)->toggledChanges(
		) | rpl::filter([=](bool toggled) {
			return (toggled != GetEnhancedBool("hd_video"));
		}) | rpl::start_with_next([=](bool toggled) {
			SetEnhancedValue("hd_video", toggled);
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
				st::settingsButtonNoIcon
		)->toggleOn(
				rpl::single(GetEnhancedBool("hide_all_chats"))
		)->toggledValue(
		) | rpl::filter([](bool enabled) {
			return (enabled != GetEnhancedBool("hide_all_chats"));
		}) | rpl::start_with_next([=](bool enabled) {
			//Ui::Toast::Show(tr::lng_settings_experimental_irrelevant(tr::now));
			SetEnhancedValue("hide_all_chats", enabled);
			EnhancedSettings::Write();
			controller->reloadFiltersMenu();
			App::wnd()->fixOrder();
		}, container->lifetime());

		AddButton(
				container,
				tr::lng_settings_replace_edit_button(),
				st::settingsButtonNoIcon
		)->toggleOn(
				rpl::single(GetEnhancedBool("replace_edit_button"))
		)->toggledValue(
		) | rpl::filter([](bool enabled) {
			return (enabled != GetEnhancedBool("replace_edit_button"));
		}) | rpl::start_with_next([=](bool enabled) {
			SetEnhancedValue("replace_edit_button", enabled);
			EnhancedSettings::Write();
			controller->reloadFiltersMenu();
		}, container->lifetime());

		AddButton(
				container,
				tr::lng_settings_skip_message(),
				st::settingsButtonNoIcon
		)->toggleOn(
				rpl::single(GetEnhancedBool("skip_to_next"))
		)->toggledValue(
		) | rpl::filter([](bool enabled) {
			return (enabled != GetEnhancedBool("skip_to_next"));
		}) | rpl::start_with_next([=](bool enabled) {
			SetEnhancedValue("skip_to_next", enabled);
			EnhancedSettings::Write();
		}, container->lifetime());

		AddDividerText(container, tr::lng_settings_skip_message_desc());

		AddSkip(container);
	}

	rpl::producer<QString> Enhanced::title() {
		return tr::lng_settings_enhanced();
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

