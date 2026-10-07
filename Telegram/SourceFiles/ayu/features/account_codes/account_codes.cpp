// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/features/account_codes/account_codes.h"

#include "base/random.h"
#include "core/application.h"
#include "lang/lang_keys.h"
#include "main/main_account.h"
#include "main/main_domain.h"
#include "mtproto/mtproto_auth_key.h"
#include "storage/details/storage_file_utilities.h"
#include "storage/serialize_common.h"
#include "storage/storage_domain.h"

#include <QtCore/QDir>
#include <QtCore/QFile>

namespace AyuFeatures::AccountCodes {
namespace {

using namespace Storage::details;

// File layout (tdata/ayucodes_<dataName>s):
// - public part: count, then (salt, sealed) for every code, where sealed
//   is the local key and the account index encrypted by the code;
// - private part, encrypted by the local key: (index, restricted) for
//   every code in the same order, readable for the settings UI.
struct Entry {
	QByteArray salt;
	QByteArray sealed;
	int index = 0;
	bool restricted = false;
};

[[nodiscard]] QString BasePath() {
	return cWorkingDir() + u"tdata/"_q;
}

[[nodiscard]] QString FileName(const QString &dataName) {
	return u"ayucodes_"_q + dataName;
}

struct FileData {
	std::vector<Entry> entries;
	QByteArray privateEncrypted;
};

[[nodiscard]] std::optional<FileData> ReadPublic(const QString &dataName) {
	FileReadDescriptor file;
	if (!ReadFile(file, FileName(dataName), BasePath())) {
		return std::nullopt;
	}
	auto publicPart = QByteArray();
	auto result = FileData();
	file.stream >> publicPart >> result.privateEncrypted;
	if (!CheckStreamStatus(file.stream)) {
		return std::nullopt;
	}
	QDataStream stream(publicPart);
	stream.setVersion(QDataStream::Qt_5_1);
	auto count = qint32();
	stream >> count;
	if (count < 0 || count > Main::Domain::kPremiumMaxAccounts) {
		return std::nullopt;
	}
	for (auto i = 0; i != count; ++i) {
		auto entry = Entry();
		stream >> entry.salt >> entry.sealed;
		result.entries.push_back(std::move(entry));
	}
	if (stream.status() != QDataStream::Ok) {
		return std::nullopt;
	}
	return result;
}

[[nodiscard]] bool ReadPrivate(
		FileData &data,
		const MTP::AuthKeyPtr &localKey) {
	EncryptedDescriptor info;
	if (!DecryptLocal(info, data.privateEncrypted, localKey)) {
		return false;
	}
	auto count = qint32();
	info.stream >> count;
	if (count != int(data.entries.size())) {
		return false;
	}
	for (auto &entry : data.entries) {
		auto index = qint32();
		auto restricted = qint32();
		info.stream >> index >> restricted;
		entry.index = index;
		entry.restricted = (restricted != 0);
	}
	return (info.stream.status() == QDataStream::Ok);
}

[[nodiscard]] std::vector<Entry> Load(not_null<Main::Domain*> domain) {
	const auto &local = domain->local();
	auto data = ReadPublic(local.dataName());
	if (!data || !local.localKey() || !ReadPrivate(*data, local.localKey())) {
		return {};
	}
	return std::move(data->entries);
}

void Write(not_null<Main::Domain*> domain, const std::vector<Entry> &list) {
	const auto &local = domain->local();
	if (list.empty()) {
		Clear(local.dataName());
		return;
	}
	auto publicPart = QByteArray();
	{
		QDataStream stream(&publicPart, QIODevice::WriteOnly);
		stream.setVersion(QDataStream::Qt_5_1);
		stream << qint32(list.size());
		for (const auto &entry : list) {
			stream << entry.salt << entry.sealed;
		}
	}
	EncryptedDescriptor privatePart(
		sizeof(qint32) + list.size() * 2 * sizeof(qint32));
	privatePart.stream << qint32(list.size());
	for (const auto &entry : list) {
		privatePart.stream
			<< qint32(entry.index)
			<< qint32(entry.restricted ? 1 : 0);
	}

	const auto path = BasePath();
	if (!QDir().exists(path)) {
		QDir().mkpath(path);
	}
	FileWriteDescriptor file(FileName(local.dataName()), path);
	file.writeData(publicPart);
	file.writeEncrypted(privatePart, local.localKey());
}

// Returns the local key and the account index if the code opens the entry.
[[nodiscard]] std::optional<std::pair<MTP::AuthKeyPtr, int>> Open(
		const Entry &entry,
		const QByteArray &code) {
	if (entry.salt.size() != LocalEncryptSaltSize) {
		return std::nullopt;
	}
	const auto codeKey = CreateLocalKey(code, entry.salt);
	EncryptedDescriptor sealed;
	if (!DecryptLocal(sealed, entry.sealed, codeKey)) {
		return std::nullopt;
	}
	const auto key = Serialize::read<MTP::AuthKey::Data>(sealed.stream);
	auto index = qint32();
	sealed.stream >> index;
	if (sealed.stream.status() != QDataStream::Ok) {
		return std::nullopt;
	}
	return std::make_pair(std::make_shared<MTP::AuthKey>(key), int(index));
}

[[nodiscard]] Entry Seal(
		const MTP::AuthKeyPtr &localKey,
		int index,
		const QByteArray &code,
		bool restricted) {
	auto result = Entry{ .index = index, .restricted = restricted };
	result.salt.resize(LocalEncryptSaltSize);
	base::RandomFill(result.salt.data(), result.salt.size());
	const auto codeKey = CreateLocalKey(code, result.salt);

	EncryptedDescriptor sealed(MTP::AuthKey::kSize + sizeof(qint32));
	localKey->write(sealed.stream);
	sealed.stream << qint32(index);
	result.sealed = PrepareEncrypted(sealed, codeKey);
	return result;
}

} // namespace

std::vector<Info> List(not_null<Main::Domain*> domain) {
	auto result = std::vector<Info>();
	for (const auto &entry : Load(domain)) {
		result.push_back({ entry.index, entry.restricted });
	}
	return result;
}

std::optional<Info> Lookup(
		not_null<Main::Domain*> domain,
		not_null<const Main::Account*> account) {
	const auto index = domain->indexOf(account);
	if (!index) {
		return std::nullopt;
	}
	for (const auto &info : List(domain)) {
		if (info.index == *index) {
			return info;
		}
	}
	return std::nullopt;
}

SetResult Set(
		not_null<Main::Domain*> domain,
		not_null<const Main::Account*> account,
		const QByteArray &code,
		bool restricted) {
	auto &local = domain->local();
	const auto index = domain->indexOf(account);
	if (!local.hasLocalPasscode() || !local.localKey() || !index) {
		return SetResult::NoPasscode;
	} else if (local.checkPasscode(code)) {
		return SetResult::SameAsPasscode;
	}
	auto list = Load(domain);
	for (const auto &entry : list) {
		if (entry.index != *index && Open(entry, code)) {
			return SetResult::AlreadyUsed;
		}
	}
	list.erase(ranges::remove(list, *index, &Entry::index), end(list));
	list.push_back(Seal(local.localKey(), *index, code, restricted));
	Write(domain, list);
	return SetResult::Success;
}

void SetRestricted(
		not_null<Main::Domain*> domain,
		not_null<const Main::Account*> account,
		bool restricted) {
	const auto index = domain->indexOf(account);
	if (!index) {
		return;
	}
	auto list = Load(domain);
	for (auto &entry : list) {
		if (entry.index == *index) {
			entry.restricted = restricted;
			Write(domain, list);
			return;
		}
	}
}

void Remove(
		not_null<Main::Domain*> domain,
		not_null<const Main::Account*> account) {
	const auto index = domain->indexOf(account);
	if (!index) {
		return;
	}
	auto list = Load(domain);
	const auto was = list.size();
	list.erase(ranges::remove(list, *index, &Entry::index), end(list));
	if (list.size() != was) {
		Write(domain, list);
	}
}

void Clear(const QString &dataName) {
	const auto base = BasePath() + FileName(dataName);
	for (const auto suffix : { u"s"_q, u"0"_q, u"1"_q }) {
		QFile::remove(base + suffix);
	}
}

bool IsUsed(not_null<Main::Domain*> domain, const QByteArray &code) {
	for (const auto &entry : Load(domain)) {
		if (Open(entry, code)) {
			return true;
		}
	}
	return false;
}

bool TryUnlock(const QByteArray &code) {
	auto &domain = Core::App().domain();
	auto data = ReadPublic(domain.local().dataName());
	if (!data) {
		return false;
	}
	for (auto i = 0, count = int(data->entries.size()); i != count; ++i) {
		const auto opened = Open(data->entries[i], code);
		if (!opened) {
			continue;
		}
		const auto &[localKey, index] = *opened;
		if (!ReadPrivate(*data, localKey)
			|| data->entries[i].index != index) {
			return false;
		}
		const auto restricted = data->entries[i].restricted;
		if (domain.started()) {
			const auto current = domain.local().localKey();
			if (!current || !current->equals(localKey)) {
				return false;
			}
		} else if (domain.startWithAccountCode(localKey, index)
				!= Storage::StartResult::Success) {
			return false;
		}
		const auto account = domain.accountByIndex(index);
		if (!account) {
			return false;
		}
		domain.setRestrictedTo(nullptr);
		domain.activate(account);
		if (restricted) {
			domain.setRestrictedTo(account);
		}
		return true;
	}
	return false;
}

void AccountLoggedOut(not_null<Main::Account*> account) {
	auto &domain = account->domain();
	if (!domain.started()) {
		return;
	}
	Remove(&domain, account);
	if (domain.restrictedTo() == account.get()) {
		domain.setRestrictedTo(nullptr);
		if (domain.local().hasLocalPasscode()) {
			Core::App().lockByPasscode();
		}
	}
}

QString Tr(const char *en, const char *ru) {
	return QString::fromUtf8(Lang::Id().startsWith(u"ru"_q) ? ru : en);
}

} // namespace AyuFeatures::AccountCodes
