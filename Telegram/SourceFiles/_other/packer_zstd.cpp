/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "packer_zstd.h"

#include "core/update_zstd_pack.h"

#include <QtCore/QCommandLineParser>
#include <QtCore/QDirIterator>
#include <QtCore/QSet>
#include <QtCore/QThread>

#include <algorithm>

#ifndef PACKER_DISABLE_PRIVATE
extern const char *PrivateKey;
extern const char *PrivateBetaKey;
#include "../../../../DesktopPrivate/packer_private.h"
#else
const char *PrivateKey = "";
const char *PrivateBetaKey = "";
#endif

namespace {

QByteArray ReadKey(const QString &path) {
	auto file = QFile(path);
	return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

} // namespace

int main(int argc, char *argv[]) {
	using namespace Core::Updates::Zstd;
	auto app = QCoreApplication(argc, argv);
	auto parser = QCommandLineParser();
	parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
	parser.setApplicationDescription(QStringLiteral("64Gram zstd update packer"));
	parser.addHelpOption();
	parser.addOptions({
		{ "path", "File or directory to include (repeatable).", "path" },
		{ "version", "Numeric application version.", "version" },
		{ "target", "Windows target: win, win64, winarm.", "target" },
		{ "arch", "macOS architecture: x86_64 or arm64.", "arch" },
		{ "beta", "Pack a beta update using the beta key." },
		{ "channel", "Release channel: stable or beta.", "channel" },
		{ "compression-level", "Zstd compression level (1-19).", "level", "17" },
		{ "threads", "Compression workers (positive count; defaults to all logical cores).", "count",
			QString::number(std::max(QThread::idealThreadCount(), 1)) },
		{ "private-key", "Override the compiled-in private key with a PEM file.", "pem" },
		{ "public-key", "Public key PEM for checking an overridden private key.", "pem" },
	});
	parser.process(app);
	const auto fail = [](const QString &error) {
		std::cerr << error.toStdString() << std::endl;
		return 1;
	};
	const auto paths = parser.values("path");
	const auto version = parser.value("version").toUInt();
	const auto channel = parser.value("channel");
	if (paths.isEmpty() || version <= 1016 || version > 999999999
		|| (!channel.isEmpty() && channel != "stable" && channel != "beta")
		|| (parser.isSet("beta") && channel == "stable")
		|| !parser.positionalArguments().isEmpty()) {
		parser.showHelp(1);
	}
	const auto beta = parser.isSet("beta") || channel == "beta";
	auto prefix = QString();
#ifdef Q_OS_WIN
	const auto name = parser.value("target");
	if (!name.isEmpty() && name != "win" && name != "win64" && name != "winarm") {
		return fail(QStringLiteral("Invalid Windows target."));
	}
	prefix = name == "winarm" ? "tarm64upd"
		: name == "win64" ? "tx64upd" : "tupdate";
#elif defined Q_OS_MAC
	const auto name = parser.value("arch");
	if (!name.isEmpty() && name != "x86_64" && name != "arm64") {
		return fail(QStringLiteral("Invalid macOS architecture."));
	}
	prefix = name == "arm64" ? "tarmacupd" : "tmacupd";
#else
	prefix = "tlinuxupd";
#endif
	if (parser.isSet("private-key") != parser.isSet("public-key")) {
		return fail(QStringLiteral("Pass both -private-key and -public-key."));
	}
	const auto privateKey = parser.isSet("private-key")
		? ReadKey(parser.value("private-key"))
		: QByteArray(beta ? PrivateBetaKey : PrivateKey);
	const auto publicKey = parser.isSet("public-key")
		? ReadKey(parser.value("public-key"))
		: beta ? PublicBetaKey() : PublicKey();
	if (privateKey.isEmpty() || publicKey.isEmpty()) {
		return fail(QStringLiteral("Update signing keys are missing."));
	}
	const auto root = QFileInfo(paths.front()).canonicalPath() + '/';
	auto files = QFileInfoList();
	for (const auto &path : paths) {
		const auto info = QFileInfo(path);
		if (!info.exists() || info.isSymLink()) {
			return fail(QStringLiteral("Invalid input: ") + path);
		} else if (info.isDir()) {
			auto iterator = QDirIterator(
				path,
				QDir::Files | QDir::NoSymLinks,
				QDirIterator::Subdirectories);
			while (iterator.hasNext()) {
				iterator.next();
				files.push_back(iterator.fileInfo());
			}
		} else {
			files.push_back(info);
		}
	}
	std::sort(files.begin(), files.end(), [](const auto &a, const auto &b) {
		return a.canonicalFilePath() < b.canonicalFilePath();
	});
	if (files.empty()) {
		return fail(QStringLiteral("No files to pack."));
	}
	auto data = QByteArray();
	auto stream = QDataStream(&data, QIODevice::WriteOnly);
	stream.setVersion(QDataStream::Qt_5_1);
	stream << version << quint32(files.size());
	auto names = QSet<QString>();
	for (const auto &info : files) {
		const auto full = info.canonicalFilePath();
		if (!full.startsWith(root) || !info.isFile() || info.size() < 0
			|| info.size() > kMaxUncompressedSize - data.size()) {
			return fail(QStringLiteral("Invalid input path or size: ") + full);
		}
		const auto name = full.mid(root.size());
		const auto normalized = name.normalized(QString::NormalizationForm_C).toCaseFolded();
		if (names.contains(normalized)) {
			return fail(QStringLiteral("Duplicate input: ") + name);
		}
		names.insert(normalized);
		auto file = QFile(full);
		if (!file.open(QIODevice::ReadOnly)) {
			return fail(QStringLiteral("Cannot read: ") + full);
		}
		const auto bytes = file.readAll();
		if (file.error() != QFileDevice::NoError || bytes.size() != info.size()) {
			return fail(QStringLiteral("Incomplete read: ") + full);
		}
		stream << name << quint32(bytes.size()) << bytes;
#ifndef Q_OS_WIN
		stream << info.isExecutable();
#endif
		if (stream.status() != QDataStream::Ok || data.size() > kMaxUncompressedSize) {
			return fail(QStringLiteral("Serialized update is too large."));
		}
	}
	auto error = QString();
	const auto package = PackUpdate(
		data,
		privateKey,
		parser.value("compression-level").toInt(),
		parser.value("threads").toInt(),
		&error);
	if (package.isEmpty()) {
		return fail(error);
	}
	const auto verified = VerifyUpdate(
		package,
		0,
		publicKey,
		publicKey,
		&error);
	if (!verified) {
		return fail(error);
	}
	if (verified->files != data) {
		return fail(QStringLiteral("Compression round trip failed."));
	}
	const auto outputName = prefix + QString::number(version);
	auto output = QFile(outputName);
	if (!output.open(QIODevice::WriteOnly | QIODevice::NewOnly)
		|| output.write(package) != package.size()
		|| !output.flush()) {
		return fail(QStringLiteral("Cannot write update: ") + outputName);
	}
	std::cout << outputName.toStdString() << ": " << data.size()
		<< " -> " << package.size() << " bytes, signed and verified." << std::endl;
	return 0;
}
