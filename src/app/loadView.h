/***********************************************************************
 * Copyright 2009 David Edmundson <kde@davidedmundson.co.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ***********************************************************************/
#ifndef LOADVIEW_H
#define LOADVIEW_H
#include <QWidget>

#include <QUrl>
#include "ui_loadView.h"

namespace Dragon
{

class LoadView : public QWidget, private Ui_LoadView
{
    Q_OBJECT
public:
    explicit LoadView(QWidget *parent);
    void setThumbnail(QWidget *object);
signals:
    void loadUrl(QUrl);
    void openFilePressed();
    void openDVDPressed();
    void openStreamPressed();
    void reloadRecentlyList();
};

}
#endif 
