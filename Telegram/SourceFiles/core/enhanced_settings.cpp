/*
This file is part of 64Gram Desktop,
the unofficial app based on Telegram Desktop.
For license and copyright information please follow this link:
https://github.com/TDesktop-x64/tdesktop/blob/dev/LEGAL
*/
#include "core/enhanced_settings.h"

#include "mainwindow.h"
#include "mainwidget.h"
#include "window/window_controller.h"
#include "core/application.h"
#include "base/parse_helper.h"
#include "facades.h"
#include "ui/widgets/input_fields.h"
#include "lang/lang_cloud_manager.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonValue>

namespace EnhancedSettings {
	namespace {

		constexpr auto kWriteJsonTimeout = crl::time(5000);

		QString DefaultFilePath() {
			return cWorkingDir() + qsl("tdata/enhanced-settings-default.json");
		}

		QString CustomFilePath() {
			return cWorkingDir() + qsl("tdata/enhanced-settings-custom.json");
		}

		bool DefaultFileIsValid() {
			QFile file(DefaultFilePath());
			if (!file.open(QIODevice::ReadOnly)) {
				return false;
			}
			auto error = QJsonParseError{0, QJsonParseError::NoError};
			const auto document = QJsonDocument::fromJson(
					base::parse::stripComments(file.readAll()),
					&error);
			file.close();

			if (error.error != QJsonParseError::NoError || !document.isObject()) {
				return false;
			}
			const auto settings = document.object();

			return true;
		}

		void WriteDefaultCustomFile() {
			const auto path = CustomFilePath();
			auto input = QFile(":/misc/default_enhanced-settings-custom.json");
			auto output = QFile(path);
			if (input.open(QIODevice::ReadOnly) && output.open(QIODevice::WriteOnly)) {
				output.write(input.readAll());
			}
		}

		bool ReadOption(QJsonObject obj, QString key, std::function<void(QJsonValue)> callback) {
			const auto it = obj.constFind(key);
			if (it == obj.constEnd()) {
				return false;
			}
			callback(*it);
			return true;
		}

		bool ReadObjectOption(QJsonObject obj, QString key, std::function<void(QJsonObject)> callback) {
			auto readResult = false;
			auto readValueResult = ReadOption(obj, key, [&](QJsonValue v) {
				if (v.isObject()) {
					callback(v.toObject());
					readResult = true;
				}
			});
			return (readValueResult && readResult);
		}

		bool ReadArrayOption(QJsonObject obj, QString key, std::function<void(QJsonArray)> callback) {
			auto readResult = false;
			auto readValueResult = ReadOption(obj, key, [&](QJsonValue v) {
				if (v.isArray()) {
					callback(v.toArray());
					readResult = true;
				}
			});
			return (readValueResult && readResult);
		}

		bool ReadStringOption(QJsonObject obj, QString key, std::function<void(QString)> callback) {
			auto readResult = false;
			auto readValueResult = ReadOption(obj, key, [&](QJsonValue v) {
				if (v.isString()) {
					callback(v.toString());
					readResult = true;
				}
			});
			return (readValueResult && readResult);
		}

		bool ReadIntOption(QJsonObject obj, QString key, std::function<void(int)> callback) {
			auto readResult = false;
			auto readValueResult = ReadOption(obj, key, [&](QJsonValue v) {
				if (v.isDouble()) {
					callback(v.toInt());
					readResult = true;
				}
			});
			return (readValueResult && readResult);
		}

		bool ReadBoolOption(QJsonObject obj, QString key, std::function<void(bool)> callback) {
			auto readResult = false;
			auto readValueResult = ReadOption(obj, key, [&](QJsonValue v) {
				if (v.isBool()) {
					callback(v.toBool());
					readResult = true;
				}
			});
			return (readValueResult && readResult);
		}

		std::unique_ptr<Manager> Data;

	} // namespace

	Manager::Manager() {
		_jsonWriteTimer.setSingleShot(true);
		connect(&_jsonWriteTimer, SIGNAL(timeout()), this, SLOT(writeTimeout()));
	}

	void Manager::fill() {
		if (!DefaultFileIsValid()) {
			writeDefaultFile();
		}
		if (!readCustomFile()) {
			WriteDefaultCustomFile();
		}
	}

	void Manager::write(bool force) {
		if (force && _jsonWriteTimer.isActive()) {
			_jsonWriteTimer.stop();
			writeTimeout();
		} else if (!force && !_jsonWriteTimer.isActive()) {
			_jsonWriteTimer.start(kWriteJsonTimeout);
		}
	}

	bool Manager::readCustomFile() {
		QFile file(CustomFilePath());
		if (!file.exists()) {
			cSetEnhancedFirstRun(true);
			return false;
		}
		cSetEnhancedFirstRun(false);
		if (!file.open(QIODevice::ReadOnly)) {
			return true;
		}
		auto error = QJsonParseError{0, QJsonParseError::NoError};
		const auto document = QJsonDocument::fromJson(
				base::parse::stripComments(file.readAll()),
				&error);
		file.close();

		if (error.error != QJsonParseError::NoError) {
			return true;
		} else if (!document.isObject()) {
			return true;
		}
		const auto settings = document.object();

		if (settings.isEmpty()) {
			return true;
		}


		ReadOption(settings, "net_speed_boost", [&](auto v) {
			if (v.isString()) {

				const auto option = v.toString();
				if (option == "high") {
					SetNetworkBoost(3);
				} else if (option == "medium") {
					SetNetworkBoost(2);
				} else if (option == "low") {
					SetNetworkBoost(1);
				} else {
					SetNetworkBoost(0);
				}

			} else if (v.isNull()) {
				SetNetworkBoost(0);
			} else if (v.isDouble()) {
				SetNetworkBoost(v.toInt());
			}
		});

		ReadBoolOption(settings, "show_messages_id", [&](auto v) {
			cSetShowMessagesID(v);
		});

		ReadBoolOption(settings, "show_repeater_option", [&](auto v) {
			cSetShowRepeaterOption(v);
		});

		ReadBoolOption(settings, "show_emoji_button_as_text", [&](auto v) {
			cSetShowEmojiButtonAsText(v);
		});

		ReadOption(settings, "always_delete_for", [&](auto v) {
			if (v.isNull()) {
				SetAlwaysDelete(0);
			} else if (v.isDouble()) {
				SetAlwaysDelete(v.toInt());
			}
		});

		ReadBoolOption(settings, "show_phone_number", [&](auto v) {
			cSetShowPhoneNumber(v);
		});

		ReadBoolOption(settings, "repeater_reply_to_orig_msg", [&](auto v) {
			cSetRepeaterReplyToOrigMsg(v);
		});

		ReadBoolOption(settings, "disable_cloud_draft_sync", [&](auto v) {
			cSetDisableCloudDraftSync(v);
		});

		ReadBoolOption(settings, "hide_classic_fwd", [&](auto v) {
			cSetHideClassicFwd(v);
		});

		ReadBoolOption(settings, "show_scheduled_button", [&](auto v) {
			cSetShowScheduledButton(v);
		});

		ReadBoolOption(settings, "stereo_mode", [&](auto v) {
			cSetStereoMode(v);
		});

		ReadStringOption(settings, "radio_controller", [&](auto v) {
			if (v.isEmpty()) {
				cSetRadioController("http://localhost:2468");
			} else {
				cSetRadioController(v);
			}
		});

		ReadBoolOption(settings, "auto_unmute", [&](auto v) {
			cSetAutoUnmute(v);
		});

		ReadOption(settings, "bitrate", [&](auto v) {
			if (v.isNull()) {
				SetBitrate(0);
			} else if (v.isDouble()) {
				SetBitrate(v.toInt());
			}
		});

		ReadBoolOption(settings, "hd_video", [&](auto v) {
			cSetHDVideo(v);
		});

		ReadBoolOption(settings, "hide_all_chats", [&](auto v) {
			cSetHideFilterAllChats(v);
		});

		ReadBoolOption(settings, "skip_to_next", [&](auto v) {
			cSetSkipSc(v);
		});

		ReadBoolOption(settings, "disable_link_warning", [&](auto v) {
			cSetDisableLinkWarning(v);
		});

		ReadBoolOption(settings, "blocked_user_spoiler_mode", [&](auto v) {
			cSetBlockedUserSpoilerMode(v);
			if (v) {
				readBlocklist();
			}
		});

		return true;
	}

	void Manager::addIdToBlocklist(int64 userId) {
		QFile file(cWorkingDir() + qsl("tdata/blocklist.json"));
		if (file.open(QIODevice::WriteOnly)) {
			auto toArray = [&] {
				QJsonArray array;
				for (auto id : cBlockList()) {
					array.append(id);
				}
				array.append(userId);
				return array;
			};
			auto doc = QJsonDocument(toArray());
			file.write(doc.toJson(QJsonDocument::Compact));
			file.close();
			readBlocklist();
		}
	}

	void Manager::removeIdFromBlocklist(int64 userId) {
		QFile file(cWorkingDir() + qsl("tdata/blocklist.json"));
		if (file.open(QIODevice::WriteOnly)) {
			auto toArray = [&] {
				QJsonArray array;
				for (auto id : cBlockList()) {
					if (id != userId) {
						array.append(id);
					}
				}
				return array;
			};
			auto doc = QJsonDocument(toArray());
			file.write(doc.toJson(QJsonDocument::Compact));
			file.close();
			readBlocklist();
		}
	}

	void Manager::readBlocklist() {
		QFile block(cWorkingDir() + qsl("tdata/blocklist.json"));
		if (block.open(QIODevice::ReadOnly)) {
			auto doc = QJsonDocument::fromJson(block.readAll());
			block.close();
			auto toList = [=] {
				QList<int64> blockList;
				for (const auto id : doc.array()) {
					blockList.append(int64(id.toDouble()));
				}
				return blockList;
			};
			cSetBlockList(toList());
		}
	}

	void Manager::writeDefaultFile() {
		auto file = QFile(DefaultFilePath());
		if (!file.open(QIODevice::WriteOnly)) {
			return;
		}
		const char *defaultHeader = R"HEADER(
// This is a list of default options for 64Gram Desktop
// Please don't modify it, its content is not used in any way
// You can place your own options in the 'enhanced-settings-custom.json' file
)HEADER";
		file.write(defaultHeader);

		auto settings = QJsonObject();
		settings.insert(qsl("net_speed_boost"), 0);
		settings.insert(qsl("show_messages_id"), false);
		settings.insert(qsl("show_repeater_option"), false);
		settings.insert(qsl("show_emoji_button_as_text"), false);
		settings.insert(qsl("always_delete_for"), 0);
		settings.insert(qsl("show_phone_number"), true);
		settings.insert(qsl("repeater_reply_to_orig_msg"), false);
		settings.insert(qsl("disable_cloud_draft_sync"), false);
		settings.insert(qsl("hide_classic_fwd"), false);
		settings.insert(qsl("show_scheduled_button"), false);
		settings.insert(qsl("stereo_mode"), false);
		settings.insert(qsl("radio_controller"), "http://localhost:2468");
		settings.insert(qsl("auto_unmute"), false);
		settings.insert(qsl("bitrate"), 0);
		settings.insert(qsl("hide_all_chats"), false);
		settings.insert(qsl("hd_video"), false);
		settings.insert(qsl("skip_to_next"), false);
		settings.insert(qsl("disable_link_warning"), false);
		settings.insert(qsl("blocked_user_spoiler_mode"), false);

		auto document = QJsonDocument();
		document.setObject(settings);
		file.write(document.toJson(QJsonDocument::Indented));
	}

	void Manager::writeCurrentSettings() {
		auto file = QFile(CustomFilePath());
		if (!file.open(QIODevice::WriteOnly)) {
			return;
		}
		if (_jsonWriteTimer.isActive()) {
			writing();
		}
		const char *customHeader = R"HEADER(
// This file was automatically generated from current settings
// It's better to edit it with app closed, so there will be no rewrites
// You should restart app to see changes
)HEADER";
		file.write(customHeader);

		auto settings = QJsonObject();
		settings.insert(qsl("net_speed_boost"), cNetSpeedBoost());
		settings.insert(qsl("show_messages_id"), cShowMessagesID());
		settings.insert(qsl("show_repeater_option"), cShowRepeaterOption());
		settings.insert(qsl("show_emoji_button_as_text"), cShowEmojiButtonAsText());
		settings.insert(qsl("always_delete_for"), cAlwaysDeleteFor());
		settings.insert(qsl("show_phone_number"), cShowPhoneNumber());
		settings.insert(qsl("repeater_reply_to_orig_msg"), cRepeaterReplyToOrigMsg());
		settings.insert(qsl("disable_cloud_draft_sync"), cDisableCloudDraftSync());
		settings.insert(qsl("hide_classic_fwd"), cHideClassicFwd());
		settings.insert(qsl("show_scheduled_button"), cShowScheduledButton());
		settings.insert(qsl("stereo_mode"), cStereoMode());
		settings.insert(qsl("radio_controller"), cRadioController());
		settings.insert(qsl("auto_unmute"), cAutoUnmute());
		settings.insert(qsl("bitrate"), cVoiceChatBitrate());
		settings.insert(qsl("hide_all_chats"), cHideFilterAllChats());
		settings.insert(qsl("hd_video"), cHDVideo());
		settings.insert(qsl("skip_to_next"), cSkipSc());
		settings.insert(qsl("disable_link_warning"), cDisableLinkWarning());
		settings.insert(qsl("blocked_user_spoiler_mode"), cBlockedUserSpoilerMode());

		auto document = QJsonDocument();
		document.setObject(settings);
		file.write(document.toJson(QJsonDocument::Indented));
	}

	void Manager::writeTimeout() {
		writeCurrentSettings();
	}

	void Manager::writing() {
		_jsonWriteTimer.stop();
	}

	void Start() {
		if (Data) return;

		Data = std::make_unique<Manager>();
		Data->fill();
	}

	void Write() {
		if (!Data) return;

		Data->write();
	}

	void Finish() {
		if (!Data) return;

		Data->write(true);
	}

} // namespace EnhancedSettings