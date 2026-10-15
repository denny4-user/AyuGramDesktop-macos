// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/boxes/online_history_box.h"

#include "ayu/features/online_tracker/online_tracker.h"
#include "base/unixtime.h"
#include "data/data_user.h"
#include "lang/lang_keys.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/popup_menu.h"
#include "ui/vertical_list.h"
#include "window/window_session_controller.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"

namespace {

using namespace AyuFeatures::OnlineTracker;

[[nodiscard]] QString Tr(const char *en, const char *ru) {
	return QString::fromUtf8(Lang::Id().startsWith(u"ru"_q) ? ru : en);
}

[[nodiscard]] QString FormatTime(TimeId value) {
	return QLocale().toString(
		base::unixtime::parse(value).time(),
		QLocale::ShortFormat);
}

[[nodiscard]] QString FormatDuration(TimeId seconds) {
	const auto minutes = seconds / 60;
	const auto hours = minutes / 60;
	if (minutes < 1) {
		return Tr("<1 min", "<1 мин");
	} else if (hours < 1) {
		return Tr("%1 min", "%1 мин").arg(minutes);
	} else if (minutes % 60) {
		return Tr("%1 h %2 min", "%1 ч %2 мин").arg(hours).arg(minutes % 60);
	}
	return Tr("%1 h", "%1 ч").arg(hours);
}

// Newest days first, every day lists its intervals from the latest.
[[nodiscard]] QString FormatHistory(const std::vector<Interval> &list) {
	if (list.empty()) {
		return Tr(
			"Nothing recorded yet. Intervals appear here when the user "
			"comes online while the tracking is on.",
			"Пока ничего не записано. Интервалы появятся, когда "
			"пользователь будет в сети при включённом отслеживании.");
	}
	const auto now = base::unixtime::now();
	auto result = QStringList();
	auto day = QDate();
	for (auto i = list.rbegin(); i != list.rend(); ++i) {
		const auto start = base::unixtime::parse(i->start);
		if (start.date() != day) {
			day = start.date();
			if (!result.isEmpty()) {
				result.push_back(QString());
			}
			result.push_back(langDayOfMonthFull(day));
		}
		const auto end = i->end
			? FormatTime(i->end)
			: Tr("now", "сейчас");
		const auto duration = FormatDuration(
			(i->end ? i->end : now) - i->start);
		result.push_back(u"%1%2 – %3  ·  %4"_q
			.arg(i->approximate ? u"≈ "_q : QString())
			.arg(FormatTime(i->start))
			.arg(end)
			.arg(duration));
	}
	return result.join('\n');
}

} // namespace

void OnlineHistoryBox(
		not_null<Ui::GenericBox*> box,
		not_null<UserData*> user) {
	box->setTitle(Tr("Online history", "История онлайна"));
	box->setWidth(st::boxWideWidth);

	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		Tr(
			"Each line is {came online} – {went offline}. Lines with ≈ are "
			"estimated from the user's activity: their last seen time is "
			"hidden, so the exact time is not available.",
			"Каждая строка — {появился в сети} – {вышел из сети}. Строки "
			"с ≈ оценены по активности пользователя: время захода у него "
			"скрыто, поэтому точного времени нет."),
		st::boxDividerLabel));
	Ui::AddSkip(box->verticalLayout());

	const auto label = box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		QString(),
		st::boxLabel));
	label->setSelectable(true);
	const auto refresh = [=] {
		label->setText(FormatHistory(History(user)));
	};
	refresh();
	Changes(user) | rpl::on_next(refresh, label->lifetime());

	box->addLeftButton(
		rpl::single(Tr("Clear", "Очистить")),
		[=] { ClearHistory(user); },
		st::attentionBoxButton);
	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
}

void FillOnlineTrackerMenu(
		not_null<Ui::PopupMenu*> menu,
		not_null<Window::SessionController*> controller,
		not_null<UserData*> user) {
	const auto tracked = IsTracked(user);
	menu->addAction(
		(tracked
			? Tr("Stop tracking online", "Выключить отслеживание онлайна")
			: Tr("Track online", "Отслеживать онлайн")),
		[=] { SetTracked(user, !tracked); },
		&st::menuIconTimer);
	if (tracked || !History(user).empty()) {
		menu->addAction(
			Tr("Online history", "История онлайна"),
			[=] { controller->show(Box(OnlineHistoryBox, user)); },
			&st::menuIconStats);
	}
}
