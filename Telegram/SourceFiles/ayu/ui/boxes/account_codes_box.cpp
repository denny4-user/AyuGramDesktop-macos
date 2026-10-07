// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/boxes/account_codes_box.h"

#include "ayu/features/account_codes/account_codes.h"
#include "data/data_user.h"
#include "lang/lang_keys.h"
#include "main/main_account.h"
#include "main/main_domain.h"
#include "main/main_session.h"
#include "settings/cloud_password/settings_cloud_password_common.h"
#include "settings/settings_common.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/fields/password_input.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/vertical_list.h"
#include "window/window_session_controller.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"

namespace {

using namespace AyuFeatures::AccountCodes;

[[nodiscard]] QString StatusText(const std::optional<Info> &info) {
	return !info
		? Tr("Not set", "Не задан")
		: info->restricted
		? Tr("Only this account", "Только этот аккаунт")
		: Tr("Set", "Задан");
}

void AccountCodeEditBox(
		not_null<Ui::GenericBox*> box,
		not_null<Main::Account*> account,
		Fn<void()> changed) {
	const auto domain = &account->domain();
	const auto existing = Lookup(domain, account);
	const auto name = account->sessionExists()
		? account->session().user()->name()
		: QString();

	box->setTitle(Tr("Code for %1", "Код для %1").arg(name));
	box->setWidth(st::boxWideWidth);

	const auto content = box->verticalLayout();
	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		existing
			? Tr(
				"Enter a new code to change it, or leave the fields empty "
				"to keep the current one.",
				"Введите новый код, чтобы сменить его, или оставьте поля "
				"пустыми, чтобы сохранить текущий.")
			: Tr(
				"Enter this code on the lock screen instead of the main "
				"passcode to open this account right away.",
				"Введите этот код на экране блокировки вместо основного "
				"код-пароля, чтобы сразу открыть этот аккаунт."),
		st::boxLabel));
	Ui::AddSkip(content);

	const auto code = Settings::CloudPassword::AddPasswordField(
		content,
		rpl::single(Tr("Code", "Код")),
		QString());
	const auto confirm = Settings::CloudPassword::AddPasswordField(
		content,
		rpl::single(Tr("Repeat the code", "Повторите код")),
		QString());
	const auto error = Settings::CloudPassword::AddError(content, confirm);
	QObject::connect(code, &Ui::MaskedInputField::changed, [=] {
		error->hide();
	});

	Ui::AddSkip(content);
	const auto restricted = box->addRow(
		object_ptr<Ui::Checkbox>(
			box,
			Tr("Only this account", "Только этот аккаунт"),
			existing && existing->restricted,
			st::defaultBoxCheckbox));
	Ui::AddSkip(content);
	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		Tr(
			"Other accounts are hidden after unlocking with this code. "
			"To see them, lock the app and enter another code or the main "
			"passcode.",
			"После входа по этому коду остальные аккаунты скрыты. "
			"Чтобы увидеть их, заблокируйте приложение и введите другой код "
			"или основной код-пароль."),
		st::boxDividerLabel));

	const auto showError = [=](const QString &text) {
		error->setText(text);
		error->show();
	};
	const auto save = [=] {
		const auto value = code->text();
		if (value.isEmpty() && confirm->text().isEmpty()) {
			if (!existing) {
				code->showError();
				return;
			}
			SetRestricted(domain, account, restricted->checked());
			changed();
			box->closeBox();
			return;
		} else if (value != confirm->text()) {
			confirm->selectAll();
			confirm->showError();
			showError(tr::lng_passcode_differ(tr::now));
			return;
		}
		switch (Set(domain, account, value.toUtf8(), restricted->checked())) {
		case SetResult::Success:
			changed();
			box->closeBox();
			return;
		case SetResult::SameAsPasscode:
			code->showError();
			showError(Tr(
				"This is the main passcode, choose another code.",
				"Это основной код-пароль, выберите другой код."));
			return;
		case SetResult::AlreadyUsed:
			code->showError();
			showError(Tr(
				"This code is already set for another account.",
				"Этот код уже задан для другого аккаунта."));
			return;
		case SetResult::NoPasscode:
			showError(Tr(
				"Set the main passcode first.",
				"Сначала включите основной код-пароль."));
			return;
		}
	};

	QObject::connect(code, &Ui::MaskedInputField::submitted, [=] {
		confirm->setFocusFast();
	});
	QObject::connect(confirm, &Ui::MaskedInputField::submitted, save);

	box->addButton(tr::lng_settings_save(), save);
	box->addButton(tr::lng_cancel(), [=] { box->closeBox(); });
	if (existing) {
		box->addLeftButton(
			rpl::single(Tr("Remove", "Удалить")),
			[=] {
				Remove(domain, account);
				changed();
				box->closeBox();
			},
			st::attentionBoxButton);
	}
	box->setFocusCallback([=] { code->setFocusFast(); });
}

} // namespace

void AccountCodesBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller) {
	const auto domain = &controller->session().domain();

	box->setTitle(Tr("Account codes", "Коды аккаунтов"));
	box->setWidth(st::boxWideWidth);

	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		Tr(
			"Each account can have its own code. Enter it on the lock "
			"screen to open that account right away. The main passcode "
			"still opens all accounts.",
			"У каждого аккаунта может быть свой код. Введите его на экране "
			"блокировки, чтобы сразу открыть этот аккаунт. Основной "
			"код-пароль по-прежнему открывает все аккаунты."),
		st::boxLabel));
	Ui::AddSkip(box->verticalLayout());

	const auto list = box->addRow(
		object_ptr<Ui::VerticalLayout>(box),
		style::margins());
	const auto rebuild = std::make_shared<Fn<void()>>();
	*rebuild = [=] {
		list->clear();
		const auto weak = std::weak_ptr<Fn<void()>>(rebuild);
		for (const auto account : domain->orderedAccounts()) {
			if (!account->sessionExists()) {
				continue;
			}
			const auto button = Settings::AddButtonWithLabel(
				list,
				rpl::single(account->session().user()->name()),
				rpl::single(StatusText(Lookup(domain, account))),
				st::settingsButtonNoIcon);
			button->setClickedCallback([=] {
				controller->show(Box(AccountCodeEditBox, account, [=] {
					if (const auto strong = weak.lock()) {
						(*strong)();
					}
				}));
			});
		}
	};
	(*rebuild)();
	box->lifetime().add([rebuild] { *rebuild = nullptr; });

	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
}
