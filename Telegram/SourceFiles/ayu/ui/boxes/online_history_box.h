// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

class UserData;

namespace Ui {
class GenericBox;
class PopupMenu;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

void OnlineHistoryBox(
	not_null<Ui::GenericBox*> box,
	not_null<UserData*> user);

// The menu shown by a click on the last seen line of a user profile.
void FillOnlineTrackerMenu(
	not_null<Ui::PopupMenu*> menu,
	not_null<Window::SessionController*> controller,
	not_null<UserData*> user);
