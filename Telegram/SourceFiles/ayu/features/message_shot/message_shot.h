// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "ayu/features/message_shot/message_shot_theme_state.h"
#include "history/view/history_view_list_widget.h"
#include "ui/chat/chat_style.h"
#include "window/window_session_controller.h"

class HistoryInner;

namespace AyuFeatures::MessageShot {

struct ShotConfig
{
	not_null<Window::SessionController*> controller;
	std::shared_ptr<Ui::ChatStyle> st;
	std::vector<not_null<HistoryItem*>> messages;
};

enum RenderPart
{
	Date,
	Reactions,
	HeaderDecorations,
};

void setShotConfig(ShotConfig &config);
void resetShotConfig();
ShotConfig getShotConfig();

bool ignoreRender(RenderPart part);
bool isTakingShot();

// Hiding the text of chosen messages and of their reply quotes.
// Only applied while the shot is being rendered.
struct HideTarget
{
	FullMsgId id;
	QRect bubble; // In the logical pixels of the rendered image.
	QRect reply; // Empty if the message has no reply quote.
	bool hasText = false;
	bool out = false;
};

[[nodiscard]] bool isTextHidden(not_null<const HistoryItem*> item);
[[nodiscard]] bool isReplyHidden(not_null<const HistoryItem*> item);
[[nodiscard]] bool isTextHidden(FullMsgId id);
[[nodiscard]] bool isReplyHidden(FullMsgId id);
void toggleTextHidden(FullMsgId id);
void toggleReplyHidden(FullMsgId id);
void clearHidden();
[[nodiscard]] std::vector<HideTarget> hideTargets();

// Called by the reply painting code to remember where the quote is.
void recordReplyRect(not_null<const HistoryItem*> item, QRect rect);

// Draws rounded bars in place of hidden text lines.
void paintHiddenLines(
	QPainter &p,
	QPoint topLeft,
	const std::vector<int> &widths,
	int lineHeight,
	QColor color);

bool isChoosingTheme();
bool setChoosingTheme(bool val);

// util
QColor makeDefaultBackgroundColor();

void Make(not_null<QWidget*> box, const ShotConfig &config, const Fn<void(QImage&,bool)>& callback);

void Wrapper(not_null<HistoryView::ListWidget*> widget, Fn<void()> clearSelected);
void Wrapper(not_null<HistoryInner*> widget, Fn<void()> clearSelected);

}
