/***************************************************************************
 *   Copyright (C) 2007 by Andreas Eckstein   *
 *   andreas.eckstein@gmx.net   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/


#include <qlayout.h>
#include <kglobal.h>
#include <klocale.h>
#include <kconfig.h>
#include <klineedit.h>
#include <kapplication.h>
#include <kmessagebox.h>
#include <kprocess.h>

#include "recoll_applet.h"


recoll_applet::recoll_applet(const QString& configFile, Type type, int actions, QWidget *parent, const char *name)
    : KPanelApplet(configFile, type, actions, parent, name)
{
    ksConfig = config();

    //m_combo = new KHistoryCombo(this);
    m_popup = new QPopupMenu();
    
    m_frame = new QFrame(this, "applet frame");
    m_combo = new KPixmapCombo(m_frame, "find applet");
    
    m_combo->setLineEdit(new KLineEdit(m_combo));
    
    m_combo->setMaxCount(7);
    m_combo->setEditable(true);
    m_combo->setFocusPolicy(ClickFocus);
    m_combo->setFocus();
    
    watchForFocus(m_combo->lineEdit());

    addServiceType("", "All Files", "kfind");
    addServiceType("media", "Media", "cdaudio_unmount");
    addServiceType("message", "E-Mail", "kmail");
    addServiceType("presentation", "Presentations", "kpresenter");
    addServiceType("spreadsheet", "Spreadsheets", "kcalc");
    addServiceType("text", "Text Files", "kate");
    addServiceType("other", "Other", "konqueror");
    
    initCombo();

    connect(m_combo, SIGNAL(returnPressed(const QString&)), SLOT(evaluate(const QString&)));
    connect(m_combo, SIGNAL(activated(int)), SLOT(selectFromHistory(int)));
    connect(m_combo, SIGNAL(iconClicked(QMouseEvent*)), SLOT(showPopup(QMouseEvent*)));
    connect(m_popup, SIGNAL(activated(int)), SLOT(changeServiceType(int)));
    connect(m_combo, SIGNAL(cleared()), SLOT(initCombo()));

    m_combo->setFixedSize(150,26);
    m_combo->move(0,2);
    m_frame->setFixedSize(150,29);
    mainView = m_frame;
    mainView->show();
}

recoll_applet::~recoll_applet()
{
    delete m_combo;
    delete m_popup;
    delete m_frame;
}

void recoll_applet::initCombo()
{
    currentServiceType=0;
    if(m_serviceTypes.count()==0)
        return;
    m_combo->setCurrentIcon(m_serviceTypes.at(currentServiceType)->icon);
}

void recoll_applet::addServiceType(const char *str, const char *label, const char *icon)
{
    addServiceType(QString(str), QString(label), QString(icon));
}

void recoll_applet::addServiceType(QString str, QString label, QString icon)
{
    ServiceType *st = new ServiceType(str,label,icon);
    m_serviceTypes.append(st);
    m_popup->insertItem(st->icon, label, m_serviceTypes.count()-1);
}

void recoll_applet::about()
{
    KMessageBox::information(0, i18n("This is an about box"));
}


void recoll_applet::help()
{
    KMessageBox::information(0, i18n("This is a help box"));
}

void recoll_applet::preferences()
{
    KMessageBox::information(0, i18n("This is a preferences box"));
}

int recoll_applet::widthForHeight(int) const
{
    //return width();
    return 150;
}

int recoll_applet::heightForWidth(int) const
{
    return height();
}

void recoll_applet::resizeEvent(QResizeEvent *)
{
}

void recoll_applet::selectFromHistory(int i)
{
    int st = m_serviceTypes.getServiceType(m_combo->pixmap(i));
    
   if(m_combo->text(i).length()!=0)
        m_combo->addToHistory2(m_combo->text(i));
    
   performAction(m_combo->text(i), st);
}

void recoll_applet::evaluate(const QString& text)
{
    if(text.length()==0)
        return;

    m_combo->addToHistory2(text);
    performAction(text, currentServiceType);
}

void recoll_applet::performAction(QString text, int serviceType)
{
    QString urlstr = text;
    if(m_serviceTypes.at(serviceType)->cmd!=QString::null)
        if(m_serviceTypes.at(serviceType)->cmd.length()>0)
            urlstr += (" rclcat:" + m_serviceTypes.at(serviceType)->cmd);

    KProcess proc;
    proc << "recoll";
    proc << "-l";
    proc << "-q";
    proc << urlstr;
    proc.start(KProcess::DontCare);
}


void recoll_applet::showPopup(QMouseEvent *e)
{
    m_popup->popup(QPoint(e->globalX(), e->globalY()));
}

void recoll_applet::changeServiceType(int i)
{
    currentServiceType=i;
    m_combo->setCurrentIcon(m_serviceTypes.at(i)->icon);
}

extern "C"
{
    KPanelApplet* init( QWidget *parent, const QString& configFile)
    {
        KGlobal::locale()->insertCatalogue("recoll_applet");
        return new recoll_applet(configFile, KPanelApplet::Normal,
                             KPanelApplet::About | KPanelApplet::Help | KPanelApplet::Preferences,
                             parent, "recoll_applet");
    }
}
