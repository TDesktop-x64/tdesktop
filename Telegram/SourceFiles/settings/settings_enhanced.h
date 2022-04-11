/*
This file is part of 64Gram Desktop,
the unofficial app based on Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/TDesktop-x64/tdesktop/blob/dev/LEGAL
*/
#pragma once

#include "settings/settings_common.h"

class BoxContent;

namespace Window {
class Controller;
class SessionController;
} // namespace Window

namespace Settings {
	class Enhanced : public Section<Enhanced> {
	public:
		Enhanced(
				QWidget *parent,
				not_null<Window::SessionController *> controller);
		[[nodiscard]] static rpl::producer<QString> Title();

	private:
		void setupContent(not_null<Window::SessionController *> controller);
		void SetupEnhancedNetwork(not_null<Ui::VerticalLayout *> container);
		void SetupEnhancedMessages(not_null<Ui::VerticalLayout *> container);
		void SetupEnhancedButton(not_null<Ui::VerticalLayout *> container);
		void SetupEnhancedVoiceChat(not_null<Ui::VerticalLayout *> container);
		void SetupEnhancedOthers(not_null<Window::SessionController*> controller, not_null<Ui::VerticalLayout *> container);
		void reqBlocked(int offset);
		void writeBlocklistFile();

		rpl::event_stream<QString> _AlwaysDeleteChanged;
		rpl::event_stream<QString> _BitrateChanged;

		mtpRequestId _requestId = 0;
		QList<int64> blockList;
		int32 blockCount = 0;
	};

} // namespace Settings
