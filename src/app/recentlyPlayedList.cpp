/***********************************************************************
 * Copyright 2008 David Edmundson <kde@davidedmundson.co.uk>
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

#include "recentlyPlayedList.h"
#include <KApplication>
#include <KConfig>
#include <KDebug>
#include <KMenu>
#include <KDialog>
#include <KLocale>
#include <KMessageBox>

#include <QFile>
#include <QFileInfo>
#include <QContextMenuEvent>

#ifdef HAVE_ZEITGEIST
#include <QZeitgeist/LogModel>
#include <QZeitgeist/Interpretation>
#endif

//this is a widget for dispaying the rcently played items in a list. It is subclassed so that we can hook up a context menu
RecentlyPlayedList::RecentlyPlayedList(QWidget *parent)
		:QListView(parent)
{
  setAlternatingRowColors( true );
  setSelectionMode(QAbstractItemView::SingleSelection);
  //configGroup = new KConfigGroup( KGlobal::config(), "General" );
#ifdef HAVE_ZEITGEIST
  m_model = new QZeitgeist::LogModel(this);
  setModel(m_model);
  QZeitgeist::DataModel::Event audioTemplate;
  QZeitgeist::DataModel::Event videoTemplate;
  QZeitgeist::DataModel::Subject audioSubjectTemplate;
  QZeitgeist::DataModel::Subject videoSubjectTemplate;
  audioSubjectTemplate.setInterpretation(QZeitgeist::Interpretation::Subject::NFOAudio);
  videoSubjectTemplate.setInterpretation(QZeitgeist::Interpretation::Subject::NFOVideo);
  audioTemplate.setSubjects(QZeitgeist::DataModel::SubjectList() << audioSubjectTemplate);
  videoTemplate.setSubjects(QZeitgeist::DataModel::SubjectList() << videoSubjectTemplate);
  m_model->setEventTemplates(QZeitgeist::DataModel::EventList() << audioTemplate << videoTemplate);
  m_model->setResultType(QZeitgeist::Log::MostRecentSubjects);
  m_model->refresh();
#endif
}

RecentlyPlayedList::~RecentlyPlayedList()
{
}

/*void
RecentlyPlayedList::loadEntries()
{
  clear();
  const QStringList entries = configGroup->readPathEntry( "Recent Urls", QStringList() );

  QListIterator<QString> i(entries);
  i.toBack();
  while(i.hasPrevious())
  {
	KUrl url = KUrl(i.previous());
	QListWidgetItem* listItem = new QListWidgetItem(  url.fileName().isEmpty() ? url.prettyUrl() : url.fileName() );
	listItem->setData( 0xdecade, QVariant::fromValue( url ) );

	if(KConfigGroup( KGlobal::config(), url.prettyUrl()).readPathEntry( "IsVideo", QString() )==QLatin1String( "false" ))
	  listItem->setIcon( KIcon( QLatin1String(  "audio-x-generic" ) ) );
	else
	  listItem->setIcon( KIcon( QLatin1String(  "video-x-generic" ) ) );
	addItem( listItem );
  }
}*/

void
RecentlyPlayedList::contextMenuEvent(QContextMenuEvent * event )
{
  KMenu menu;
  kDebug() << "Loading Menu";
  menu.addAction(KIcon(QLatin1String( "list-remove" )),i18n("Remove Entry"),this,SLOT(removeEntry()));
  menu.addAction(KIcon(QLatin1String( "list-remove" )),i18n("Clear List"),this,SLOT(clearList()));
  menu.exec( event->globalPos() );
}

/*void
RecentlyPlayedList::removeEntry()
{
  QStringList list = configGroup->readPathEntry( "Recent Urls", QStringList() );
  KUrl toRemove = currentItem()->data(0xdecade).value<KUrl>();
  list.removeAll(toRemove.prettyUrl());
  configGroup->writePathEntry("Recent Urls",list.join( QLatin1String( "," )));
  loadEntries();
}

void
RecentlyPlayedList::clearList()
{
  configGroup->writePathEntry("Recent Urls",QString());
  loadEntries();
}

//send the url for the item clicked, not the item
void
RecentlyPlayedList::itemDoubleClicked(QListWidgetItem* item)
{
  KUrl url = item->data(0xdecade).value<KUrl>();

  if( url.isLocalFile() )
  {
    QFileInfo fileInfo( url.toLocalFile() );

    if( !fileInfo.exists() )
    {
      if( KMessageBox::questionYesNo( 0,
                                      i18n( "This file could not be found. Would you like to remove it from the playlist?" ),
                                      i18n( "File not found" ) ) == KMessageBox::Yes )
      {
        removeEntry();
      }

      return;
    }
  }

  emit(itemDoubleClicked(url));
}*/

#include "recentlyPlayedList.moc"
