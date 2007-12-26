/***********************************************************************
 * Copyright 2005  Max Howell <max.howell@methylblue.com>
 *           2007  Ian Monroe <ian@monroe.nu>
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

#ifndef CODEINE_PART_H
#define CODEINE_PART_H

#include <KParts/StatusBarExtension>
#include <KParts/Part>
#include <KUrl>

class KAboutData;
class QSlider;


namespace Codeine
{
   class Part : public KParts::ReadOnlyPart
   {
   public:
      Part(QWidget* parentWidget, QObject* parent, const QStringList& /*args*/ );

      virtual bool openFile() { return false; } //pure virtual in base class
      virtual bool openURL( const KUrl& );
      virtual bool closeURL();

      static KAboutData *createAboutData();

   private:
      KUrl m_url;
      KParts::StatusBarExtension *m_statusBarExtension;
      QSlider *m_slider;

      KStatusBar *statusBar() { return m_statusBarExtension->statusBar(); }

   };
}

#endif
