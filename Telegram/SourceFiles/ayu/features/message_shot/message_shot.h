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

// Hiding parts of chosen messages: a click on a part in the preview
// replaces it with a solid placeholder. Only applied while rendering.
enum class HidePart
{
	Name,
	Text,
	ReplyName,
	ReplyText,
	Avatar,
	Media,
};

struct HideTarget
{
	FullMsgId id;
	HidePart part = HidePart::Text;
	QRect rect; // In the logical pixels of the rendered image.
};

[[nodiscard]] bool isHidden(not_null<const HistoryItem*> item, HidePart part);
[[nodiscard]] bool isHidden(FullMsgId id, HidePart part);
void toggleHidden(FullMsgId id, HidePart part);
void clearHidden();
[[nodiscard]] std::vector<HideTarget> hideTargets();

// Called by the painting code to remember where a part was drawn.
void recordPart(
	QPainter &p,
	not_null<const HistoryItem*> item,
	HidePart part,
	QRect rect);

// An opaque placeholder color, the text color mixed into the background.
[[nodiscard]] QColor hiddenColor(QColor background, QColor text);
// Rounded bars in place of hidden text lines.
void paintHiddenLines(
	QPainter &p,
	QPoint topLeft,
	const std::vector<int> &widths,
	int lineHeight,
	QColor color);
// A solid rounded block in place of a hidden avatar or attachment.
void paintHiddenRect(QPainter &p, QRect rect, int radius, QColor color);

bool isChoosingTheme();
bool setChoosingTheme(bool val);

// util
QColor makeDefaultBackgroundColor();

void Make(not_null<QWidget*> box, const ShotConfig &config, const Fn<void(QImage&,bool)>& callback);

void Wrapper(not_null<HistoryView::ListWidget*> widget, Fn<void()> clearSelected);
void Wrapper(not_null<HistoryInner*> widget, Fn<void()> clearSelected);

}
