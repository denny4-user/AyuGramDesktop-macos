// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "ui/rp_widget.h"
#include "ui/effects/animations.h"

class ImageView : public Ui::RpWidget
{
public:
	ImageView(QWidget *parent);

	void setImage(const QImage &image);
	QImage getImage() const;

	// Where the current image is drawn, in the widget coordinates.
	[[nodiscard]] QRect imageRect() const;
	// Extra space on both sides, for controls drawn next to the image.
	void setSideReserve(int reserve);
	[[nodiscard]] rpl::producer<> imageChanges() const;

protected:
	void paintEvent(QPaintEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;

	void computeDiffImages(const QImage &prev, const QImage &curr);

private:
	QImage image;
	QImage prevImage;
	QImage baseImage;
	QImage prevDiffImage;
	QImage newDiffImage;

	Ui::Animations::Simple animation;
	int sideReserve = 0;
	rpl::event_stream<> imageChanged;

};
