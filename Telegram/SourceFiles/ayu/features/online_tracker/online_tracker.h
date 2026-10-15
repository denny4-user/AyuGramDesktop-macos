// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

class UserData;

namespace Main {
class Session;
} // namespace Main

namespace AyuFeatures::OnlineTracker {

// Records when a tracked user was online as {came online}-{went offline}.
//
// For users who show their last seen time the boundaries come from
// the server. For users who hide it ("last seen recently") there is no
// such data, so the client's own estimate is used: Telegram Desktop marks
// a user online for a short time after it sees their activity (typing,
// a new message, a reaction). Such intervals are marked approximate.
struct Interval {
	TimeId start = 0;
	TimeId end = 0; // 0 while the user is still online.
	bool approximate = false;
};

void Start(not_null<Main::Session*> session);

[[nodiscard]] bool IsTracked(not_null<UserData*> user);
void SetTracked(not_null<UserData*> user, bool tracked);

[[nodiscard]] std::vector<Interval> History(not_null<UserData*> user);
void ClearHistory(not_null<UserData*> user);
[[nodiscard]] rpl::producer<> Changes(not_null<UserData*> user);

} // namespace AyuFeatures::OnlineTracker
