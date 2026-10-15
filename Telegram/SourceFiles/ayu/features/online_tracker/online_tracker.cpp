// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/features/online_tracker/online_tracker.h"

#include "base/timer.h"
#include "base/unixtime.h"
#include "data/data_changes.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "main/main_session.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

namespace AyuFeatures::OnlineTracker {
namespace {

constexpr auto kMaxIntervals = 3000;
constexpr auto kCheckTimeout = 10 * crl::time(1000);
constexpr auto kSaveDelay = 3 * crl::time(1000);

struct UserState {
	bool tracked = false;
	std::vector<Interval> intervals;
	rpl::event_stream<> changes;
};

class SessionTracker final {
public:
	explicit SessionTracker(not_null<Main::Session*> session);
	~SessionTracker();

	[[nodiscard]] bool tracked(UserId id) const;
	void setTracked(not_null<UserData*> user, bool tracked);
	[[nodiscard]] std::vector<Interval> history(UserId id) const;
	void clearHistory(UserId id);
	[[nodiscard]] rpl::producer<> changes(UserId id);

private:
	void load();
	void save();
	void saveDelayed();
	void check(not_null<UserData*> user);
	void checkAll();
	void fireChanged(UserId id);

	const not_null<Main::Session*> _session;
	// Computed once: save() may run while the session is being destroyed.
	const QString _path;
	std::map<UserId, UserState> _users;
	base::Timer _checkTimer;
	base::Timer _saveTimer;
	rpl::lifetime _lifetime;

};

std::map<not_null<Main::Session*>, std::unique_ptr<SessionTracker>> Trackers;

[[nodiscard]] SessionTracker *TrackerFor(not_null<UserData*> user) {
	const auto i = Trackers.find(&user->session());
	return (i != end(Trackers)) ? i->second.get() : nullptr;
}

[[nodiscard]] UserId IdOf(not_null<UserData*> user) {
	return peerToUser(user->id);
}

SessionTracker::SessionTracker(not_null<Main::Session*> session)
: _session(session)
, _path(cWorkingDir()
	+ u"tdata/ayu/online_tracker_%1.json"_q.arg(session->userId().bare))
, _checkTimer([=] { checkAll(); })
, _saveTimer([=] { save(); }) {
	load();

	_session->changes().peerUpdates(
		Data::PeerUpdate::Flag::OnlineStatus
	) | rpl::on_next([=](const Data::PeerUpdate &update) {
		if (const auto user = update.peer->asUser()) {
			if (tracked(IdOf(user))) {
				check(user);
			}
		}
	}, _lifetime);

	// Users hidden by their privacy settings go offline locally, without
	// any update, so open intervals are checked on a timer as well.
	_checkTimer.callEach(kCheckTimeout);
	checkAll();
}

SessionTracker::~SessionTracker() {
	if (_saveTimer.isActive()) {
		save();
	}
}

void SessionTracker::load() {
	QFile file(_path);
	if (!file.open(QIODevice::ReadOnly)) {
		return;
	}
	const auto document = QJsonDocument::fromJson(file.readAll());
	const auto root = document.object();
	const auto savedAt = TimeId(root.value(u"savedAt"_q).toInteger());
	const auto users = root.value(u"users"_q).toObject();
	for (auto i = users.begin(); i != users.end(); ++i) {
		const auto id = UserId(BareId(i.key().toULongLong()));
		if (!id) {
			continue;
		}
		const auto data = i.value().toObject();
		auto &state = _users[id];
		state.tracked = data.value(u"tracked"_q).toBool();
		for (const auto value : data.value(u"intervals"_q).toArray()) {
			const auto list = value.toArray();
			auto interval = Interval{
				.start = TimeId(list.at(0).toInteger()),
				.end = TimeId(list.at(1).toInteger()),
				.approximate = list.at(2).toBool(),
			};
			if (!interval.start) {
				continue;
			} else if (!interval.end) {
				// The app was closed while the user was online.
				interval.end = std::max(savedAt, interval.start);
				interval.approximate = true;
			}
			state.intervals.push_back(interval);
		}
	}
}

void SessionTracker::save() {
	_saveTimer.cancel();

	auto users = QJsonObject();
	for (const auto &[id, state] : _users) {
		if (!state.tracked && state.intervals.empty()) {
			continue;
		}
		auto intervals = QJsonArray();
		for (const auto &interval : state.intervals) {
			intervals.append(QJsonArray{
				qint64(interval.start),
				qint64(interval.end),
				interval.approximate,
			});
		}
		users.insert(QString::number(id.bare), QJsonObject{
			{ u"tracked"_q, state.tracked },
			{ u"intervals"_q, intervals },
		});
	}
	const auto root = QJsonObject{
		{ u"savedAt"_q, qint64(base::unixtime::now()) },
		{ u"users"_q, users },
	};
	QDir().mkpath(QFileInfo(_path).absolutePath());
	QFile file(_path);
	if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
	}
}

void SessionTracker::saveDelayed() {
	if (!_saveTimer.isActive()) {
		_saveTimer.callOnce(kSaveDelay);
	}
}

bool SessionTracker::tracked(UserId id) const {
	const auto i = _users.find(id);
	return (i != end(_users)) && i->second.tracked;
}

void SessionTracker::setTracked(not_null<UserData*> user, bool tracked) {
	const auto id = IdOf(user);
	auto &state = _users[id];
	if (state.tracked == tracked) {
		return;
	}
	state.tracked = tracked;
	if (tracked) {
		check(user);
	} else if (!state.intervals.empty() && !state.intervals.back().end) {
		state.intervals.back().end = base::unixtime::now();
	}
	saveDelayed();
	fireChanged(id);
}

std::vector<Interval> SessionTracker::history(UserId id) const {
	const auto i = _users.find(id);
	return (i != end(_users)) ? i->second.intervals : std::vector<Interval>();
}

void SessionTracker::clearHistory(UserId id) {
	const auto i = _users.find(id);
	if (i == end(_users) || i->second.intervals.empty()) {
		return;
	}
	auto &intervals = i->second.intervals;
	// Keep the interval that is still going on.
	const auto open = !intervals.back().end
		? std::make_optional(intervals.back())
		: std::nullopt;
	intervals.clear();
	if (open) {
		intervals.push_back(*open);
	}
	saveDelayed();
	fireChanged(id);
}

rpl::producer<> SessionTracker::changes(UserId id) {
	return _users[id].changes.events();
}

void SessionTracker::fireChanged(UserId id) {
	const auto i = _users.find(id);
	if (i != end(_users)) {
		i->second.changes.fire({});
	}
}

void SessionTracker::check(not_null<UserData*> user) {
	const auto id = IdOf(user);
	auto &state = _users[id];
	if (!state.tracked) {
		return;
	}
	const auto now = base::unixtime::now();
	const auto status = user->lastseen();
	const auto online = status.isOnline(now);
	auto &intervals = state.intervals;
	const auto isOpen = !intervals.empty() && !intervals.back().end;
	if (online && !isOpen) {
		intervals.push_back({
			.start = now,
			.approximate = status.isLocalOnlineValue(),
		});
		if (int(intervals.size()) > kMaxIntervals) {
			intervals.erase(
				begin(intervals),
				begin(intervals) + (intervals.size() - kMaxIntervals));
		}
	} else if (online && isOpen) {
		// The server confirmed a locally estimated online.
		if (intervals.back().approximate && !status.isLocalOnlineValue()) {
			intervals.back().approximate = false;
		} else {
			return;
		}
	} else if (!online && isOpen) {
		auto &last = intervals.back();
		// For a visible status onlineTill() is the server "was online"
		// time, for a local estimate it is when the estimate ran out.
		const auto till = status.onlineTill();
		last.end = (till > 0 && till <= now)
			? std::max(till, last.start)
			: now;
		if (status.isHidden()) {
			last.approximate = true;
		}
	} else {
		return;
	}
	saveDelayed();
	state.changes.fire({});
}

void SessionTracker::checkAll() {
	for (const auto &[id, state] : _users) {
		if (state.tracked) {
			if (const auto user = _session->data().userLoaded(id)) {
				check(user);
			}
		}
	}
}

} // namespace

void Start(not_null<Main::Session*> session) {
	if (Trackers.contains(session)) {
		return;
	}
	Trackers.emplace(session, std::make_unique<SessionTracker>(session));
	session->lifetime().add([=] {
		Trackers.erase(session);
	});
}

bool IsTracked(not_null<UserData*> user) {
	const auto tracker = TrackerFor(user);
	return tracker && tracker->tracked(IdOf(user));
}

void SetTracked(not_null<UserData*> user, bool tracked) {
	if (const auto tracker = TrackerFor(user)) {
		tracker->setTracked(user, tracked);
	}
}

std::vector<Interval> UserHistory(not_null<UserData*> user) {
	const auto tracker = TrackerFor(user);
	return tracker ? tracker->history(IdOf(user)) : std::vector<Interval>();
}

void ClearHistory(not_null<UserData*> user) {
	if (const auto tracker = TrackerFor(user)) {
		tracker->clearHistory(IdOf(user));
	}
}

rpl::producer<> Changes(not_null<UserData*> user) {
	const auto tracker = TrackerFor(user);
	return tracker ? tracker->changes(IdOf(user)) : rpl::never<>();
}

} // namespace AyuFeatures::OnlineTracker
