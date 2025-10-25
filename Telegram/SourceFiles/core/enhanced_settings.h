/*
This file is part of 64Gram Desktop,
the unofficial app based on Telegram Desktop.
For license and copyright information please follow this link:
https://github.com/TDesktop-x64/tdesktop/blob/dev/LEGAL
*/
#pragma once

#include <QtCore/QTimer>
#include "data/filters/message_filter.h"

namespace EnhancedSettings {

	// Soft mute data structure
	struct SoftMuteState {
		bool enabled = false;
		int period = 0; // in seconds
		int64 lastNotificationTime = 0; // unix timestamp
		int suppressionMode = 0; // 0 = silent (badge only), 1 = totally hidden
	};

	class Manager : public QObject {
	Q_OBJECT

	public:
		Manager();

		void fill();

		void write(bool force = false);

		void addIdToBlocklist(int64 userId);

		void removeIdFromBlocklist(int64 userId);

	public Q_SLOTS:

		void writeTimeout();

	private:
		void writeDefaultFile();

		void writeCurrentSettings();

		bool readCustomFile();

		void readBlocklist();

		void writing();

		QTimer _jsonWriteTimer;

	};

	void Start();

	void Write();

	void Finish();
	
	// Message filter management
	[[nodiscard]] QVector<MessageFilters::MessageFilter> GetMessageFilters();
	void AddMessageFilter(const MessageFilters::MessageFilter &filter);
	void UpdateMessageFilter(const MessageFilters::MessageFilter &filter);
	void DeleteMessageFilter(const QString &filterId);
	void ReorderFilters(const QVector<QString> &filterIds);

	// Soft mute management
	[[nodiscard]] SoftMuteState GetSoftMuteState(uint64 peerId);
	void SetSoftMuteState(uint64 peerId, const SoftMuteState &state);
	void UpdateSoftMuteLastNotification(uint64 peerId, int64 timestamp);
	void RemoveSoftMute(uint64 peerId);

} // namespace EnhancedSettings