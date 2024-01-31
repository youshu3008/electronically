/**
 * This file is a part of LuminanceHDR package.
 * ----------------------------------------------------------------------
 * Copyright (C) 2006,2007 Giuseppe Rota
 * Copyright (C) 2011 Davide Anastasia
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * ----------------------------------------------------------------------
 *
 * Original Work
 * @author Giuseppe Rota <grota@users.sourceforge.net>
 * Improvements, bugfixing
 * @author Franco Comida <fcomida@users.sourceforge.net>
 * Improvements, new functionalities
 * @author Davide Anastasia <davideanastasia@users.sourceforge.net>
 *
 */

#ifndef IMAGEHDRVIEWER_H
#define IMAGEHDRVIEWER_H

#include <QComboBox>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QScopedPointer>
#include <iostream>

#include "GenericViewer.h"

// Forward declaration
namespace pfs {
class Frame;
}

class LuminanceRangeWidget;

class HdrViewer : public GenericViewer {
    Q_OBJECT

   public:
    explicit HdrViewer(pfs::Frame *frame, QWidget *parent = 0, bool ns = false);
    virtual ~HdrViewer();

    LuminanceRangeWidget *lumRange();

    QString getFileNamePostFix() override;
    QString getExifComment() override;

    //! \brief virtual function
    //! \return always return TRUE
    bool isHDR() override;

    //! \brief returns max value of the handled frame
    float getMaxLuminanceValue() override;

    //! \brief returns min value of the handled frame
    float getMinLuminanceValue() override;

    RGBMappingType getLuminanceMappingMethod() override;

   public Q_SLOTS:
    void updateRangeWindow();
    int getLumMappingMethod();
    void setLumMappingMethod(int method);

   protected Q_SLOTS:
    virtual void updatePixmap() override;

   protected:
    // Methods
    virtual void retranslateUi() override;
    void setRangeWindow(float min, float max);
    void keyPressEvent(QKeyEvent *event) override;

    // UI
    LuminanceRangeWidget *m_lumRange;
    QComboBox *m_mappingMethodCB;
    QLabel *m_mappingMethodLabel;
    QLabel *m_histLabel;

   private:
    void initUi();
    void refreshPixmap();

    RGBMappingType m_mappingMethod;
    float m_minValue;
    float m_maxValue;

    QImage *mapFrameToImage(pfs::Frame *in_frame);
};

inline bool HdrViewer::isHDR() { return true; }

#endif
