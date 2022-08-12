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

		loadSettings(settings);

		ReadOption(settings, "net_speed_boost", [&](auto v) {
			if (v.isDouble()) {
				int value = v.toInt();
				if (value < 0) {
					SetNetworkBoost(0);
				} else if (value > 3) {
					SetNetworkBoost(3);
				} else {
					SetNetworkBoost(value);
				}
			}
		});

		ReadOption(settings, "bitrate", [&](auto v) {
			if (v.isDouble()) {
				int value = v.toInt();
				if (value < 0) {
					gEnhancedOptions.insert("bitrate", 0);
				} else if (value > 7) {
					gEnhancedOptions.insert("bitrate", 7);
				} else {
					gEnhancedOptions.insert("bitrate", value);
				}
			}
		});

		ReadStringOption(settings, "radio_controller", [&](auto v) {
			if (v.isEmpty()) {
				SetEnhancedValue("radio_controller", "http://localhost:2468");
			}
		});

		ReadBoolOption(settings, "blocked_user_spoiler_mode", [&](auto v) {
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
		settings.insert(qsl("replace_edit_button"), false);
		settings.insert(qsl("hd_video"), false);
		settings.insert(qsl("skip_to_next"), false);
		settings.insert(qsl("disable_link_warning"), false);
		settings.insert(qsl("blocked_user_spoiler_mode"), false);
		settings.insert(qsl("disable_premium_animation"), false);

		auto document = QJsonDocument();
		document.setObject(settings);
		file.write(document.toJson(QJsonDocument::Indented));

		loadSettings(settings);
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
		settings.insert(qsl("net_speed_boost"), GetEnhancedInt("net_speed_boost"));
		settings.insert(qsl("show_messages_id"), GetEnhancedBool("show_messages_id"));
		settings.insert(qsl("show_repeater_option"), GetEnhancedBool("show_repeater_option"));
		settings.insert(qsl("show_emoji_button_as_text"), GetEnhancedBool("show_emoji_button_as_text"));
		settings.insert(qsl("always_delete_for"), GetEnhancedInt("always_delete_for"));
		settings.insert(qsl("show_phone_number"), GetEnhancedBool("show_phone_number"));
		settings.insert(qsl("repeater_reply_to_orig_msg"), GetEnhancedBool("repeater_reply_to_orig_msg"));
		settings.insert(qsl("disable_cloud_draft_sync"), GetEnhancedBool("disable_cloud_draft_sync"));
		settings.insert(qsl("hide_classic_fwd"), GetEnhancedBool("hide_classic_fwd"));
		settings.insert(qsl("show_scheduled_button"), GetEnhancedBool("show_scheduled_button"));
		settings.insert(qsl("stereo_mode"), GetEnhancedBool("show_scheduled_button"));
		settings.insert(qsl("radio_controller"), GetEnhancedString("radio_controller"));
		settings.insert(qsl("auto_unmute"), GetEnhancedBool("show_scheduled_button"));
		settings.insert(qsl("bitrate"), GetEnhancedInt("bitrate"));
		settings.insert(qsl("hide_all_chats"), GetEnhancedBool("hide_all_chats"));
		settings.insert(qsl("replace_edit_button"), GetEnhancedBool("replace_edit_button"));
		settings.insert(qsl("hd_video"), GetEnhancedBool("hd_video"));
		settings.insert(qsl("skip_to_next"), GetEnhancedBool("skip_to_next"));
		settings.insert(qsl("disable_link_warning"), GetEnhancedBool("disable_link_warning"));
		settings.insert(qsl("blocked_user_spoiler_mode"), GetEnhancedBool("blocked_user_spoiler_mode"));
		settings.insert(qsl("disable_premium_animation"), GetEnhancedBool("disable_premium_animation"));

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