// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

namespace Main {
class Account;
class Domain;
} // namespace Main

namespace AyuFeatures::AccountCodes {

// An account code is an extra local passcode bound to one account.
// It unlocks the same local key as the main passcode (so it only works
// while the main passcode is set) and then opens that account.
//
// A "restricted" code opens the account alone: other accounts are hidden
// until the app is locked and unlocked with another code or the main
// passcode.

struct Info {
	int index = 0;
	bool restricted = false;
};

[[nodiscard]] std::vector<Info> List(not_null<Main::Domain*> domain);
[[nodiscard]] std::optional<Info> Lookup(
	not_null<Main::Domain*> domain,
	not_null<const Main::Account*> account);

enum class SetResult {
	Success,
	SameAsPasscode,
	AlreadyUsed,
	NoPasscode,
};
[[nodiscard]] SetResult Set(
	not_null<Main::Domain*> domain,
	not_null<const Main::Account*> account,
	const QByteArray &code,
	bool restricted);
void SetRestricted(
	not_null<Main::Domain*> domain,
	not_null<const Main::Account*> account,
	bool restricted);
void Remove(
	not_null<Main::Domain*> domain,
	not_null<const Main::Account*> account);
void Clear(const QString &dataName);

// The main passcode must differ from every account code.
[[nodiscard]] bool IsUsed(
	not_null<Main::Domain*> domain,
	const QByteArray &code);

// Called when the main passcode did not match. Starts the domain if it
// was not started yet, opens the account and applies the restriction.
[[nodiscard]] bool TryUnlock(const QByteArray &code);

// Drops the code of an account that was logged out and leaves
// the restricted mode if that account was the restricted one.
void AccountLoggedOut(not_null<Main::Account*> account);

[[nodiscard]] QString Tr(const char *en, const char *ru);

} // namespace AyuFeatures::AccountCodes
