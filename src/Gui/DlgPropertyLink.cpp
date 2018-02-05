/***************************************************************************
 *   Copyright (c) 2014 Werner Mayer <wmayer[at]users.sourceforge.net>     *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/


#include "PreCompiled.h"
#ifndef _PreComp_
# include <algorithm>
# include <sstream>
# include <QTreeWidgetItem>
# include <QMessageBox>
#endif

#include <App/Application.h>
#include <App/Document.h>
#include <App/DocumentObject.h>
#include <App/GeoFeature.h>

#include "DlgPropertyLink.h"
#include "Application.h"
#include "ViewProviderDocumentObject.h"
#include "ui_DlgPropertyLink.h"

using namespace Gui::Dialog;

/* TRANSLATOR Gui::Dialog::DlgPropertyLink */

DlgPropertyLink::DlgPropertyLink(const QStringList& list, QWidget* parent, Qt::WindowFlags fl, bool xlink)
  : QDialog(parent, fl), link(list), ui(new Ui_DlgPropertyLink)
{
#ifdef FC_DEBUG
    assert(list.size() >= 4);
#endif

    // populate inList to filter out any objects that contains the owner object
    // of the editing link property
    auto doc = App::GetApplication().getDocument(qPrintable(link[0]));
    if(doc) {
        auto obj = doc->getObject(qPrintable(link[3]));
        if(obj && obj->getNameInDocument()) {
            inList = obj->getInListEx(true);
            inList.insert(obj);
        }
    }

    ui->setupUi(this);
    if(!xlink) 
        ui->comboBox->hide();
    else {
        std::string linkDoc = qPrintable(link[0]);
        for(auto doc : App::GetApplication().getDocuments()) {
            QString name(QString::fromUtf8(doc->getName()));
            ui->comboBox->addItem(name);
            if(linkDoc == doc->getName())
                ui->comboBox->setCurrentIndex(ui->comboBox->count()-1);
        }
    }
    findObjects(ui->checkObjectType->isChecked());

    connect(ui->treeWidget, SIGNAL(itemExpanded(QTreeWidgetItem*)),
            this, SLOT(onItemExpanded(QTreeWidgetItem*)));
}

/**
 *  Destroys the object and frees any allocated resources
 */
DlgPropertyLink::~DlgPropertyLink()
{
    // no need to delete child widgets, Qt does it all for us
    delete ui;
}

void DlgPropertyLink::setSelectionMode(QAbstractItemView::SelectionMode mode)
{
    ui->treeWidget->setSelectionMode(mode);
    findObjects(ui->checkObjectType->isChecked());
}

void DlgPropertyLink::accept()
{
    if (ui->treeWidget->selectionMode() == QAbstractItemView::SingleSelection) {
        QList<QTreeWidgetItem*> items = ui->treeWidget->selectedItems();
        if (items.isEmpty()) {
            QMessageBox::warning(this, tr("No selection"), tr("Please select an object from the list"));
            return;
        }
    }

    QDialog::accept();
}

static QStringList getLinkFromItem(const QStringList &link, QTreeWidgetItem *selItem) {
    QStringList list = link;
    if(link.size()>=5) {
        QString subname;
        auto parent = selItem;
        for(auto item=parent;;item=parent) {
            parent = item->parent();
            if(!parent) {
                list[1] = item->data(0,Qt::UserRole).toString();
                break;
            }
            subname = QString::fromLatin1("%1.%2").
                arg(item->data(0,Qt::UserRole).toString()).arg(subname);
        }
        list[4] = subname;
        if(subname.size())
            list[2] = QString::fromLatin1("%1 (%2.%3)").
                arg(selItem->text(0)).arg(list[1]).arg(subname);
        else
            list[2] = selItem->text(0);
        QString docName(selItem->data(0, Qt::UserRole+1).toString());
        if(list.size()>5)
            list[5] = docName;
        else
            list << docName;
    }else{
        list[1] = selItem->data(0,Qt::UserRole).toString();
        list[2] = selItem->text(0);
        if (list[1].isEmpty())
            list[2] = QString::fromUtf8("");
    }
    return list;
}

QStringList DlgPropertyLink::propertyLink() const
{
    auto items = ui->treeWidget->selectedItems();
    if (items.isEmpty()) {
        return link;
    }
    return getLinkFromItem(link,items[0]);
}

QVariantList DlgPropertyLink::propertyLinkList() const
{
    QVariantList varList;
    QList<QTreeWidgetItem*> items = ui->treeWidget->selectedItems();
    if (items.isEmpty()) {
        varList << link;
    }
    else {
        for (QList<QTreeWidgetItem*>::iterator it = items.begin(); it != items.end(); ++it)
            varList << getLinkFromItem(link,*it);
    }

    return varList;
}

void DlgPropertyLink::findObjects(bool on)
{
    ui->treeWidget->clear();

    QString docName = link[0]; // document name of the owner object of this editing property
    QString objName = link[1]; // linked object name

    bool isSingleSelection = (ui->treeWidget->selectionMode() == QAbstractItemView::SingleSelection);
    App::Document* doc = App::GetApplication().getDocument((const char*)docName.toLatin1());
    if (doc) {
        Base::Type baseType = App::DocumentObject::getClassTypeId();
        if (!on) {
            App::Document *linkedDoc = doc;
            if (link.size()>=6) 
                linkedDoc = App::GetApplication().getDocument(qPrintable(link[5]));
            if(linkedDoc) {    
                App::DocumentObject* obj = linkedDoc->getObject((const char*)objName.toLatin1());
                if (obj && inList.find(obj)==inList.end()) {
                    Base::Type objType = obj->getTypeId();
                    // get only geometric types
                    if (objType.isDerivedFrom(App::GeoFeature::getClassTypeId()))
                        baseType = App::GeoFeature::getClassTypeId();

                    // get the direct base class of App::DocumentObject which 'obj' is derived from
                    while (!objType.isBad()) {
                        std::string name = objType.getName();
                        Base::Type parType = objType.getParent();
                        if (parType == baseType) {
                            baseType = objType;
                            break;
                        }
                        objType = parType;
                    }
                }
            }
        }

        // Add a "None" entry on top
        if (isSingleSelection) {
            auto* item = new QTreeWidgetItem(ui->treeWidget);
            item->setText(0,tr("None (Remove link)"));
            QByteArray ba("");
            item->setData(0,Qt::UserRole, ba);
        }

        for(auto obj : doc->getObjectsOfType(baseType))
            createItem(obj,0);
    }
}

void DlgPropertyLink::createItem(App::DocumentObject *obj, QTreeWidgetItem *parent) {
    if(!obj || !obj->getNameInDocument())
        return;

    if(inList.find(obj)!=inList.end())
        return;

    auto vp = Gui::Application::Instance->getViewProvider(obj);
    if(!vp) 
        return;
    QString searchText = ui->searchBox->text();
    if (!searchText.isEmpty()) { 
        QString label = QString::fromUtf8((obj)->Label.getValue());
        if (!label.contains(searchText,Qt::CaseInsensitive))
            return;
    }
    QTreeWidgetItem* item;
    if(parent)
        item = new QTreeWidgetItem(parent);
    else
        item = new QTreeWidgetItem(ui->treeWidget);
    item->setIcon(0, vp->getIcon());
    item->setText(0, QString::fromUtf8((obj)->Label.getValue()));
    item->setData(0, Qt::UserRole, QByteArray(obj->getNameInDocument()));
    item->setData(0, Qt::UserRole+1, QByteArray(obj->getDocument()->getName()));
    if(link.size()>=5) {
        item->setChildIndicatorPolicy(obj->hasChildElement()||vp->getChildRoot()?
                QTreeWidgetItem::ShowIndicator:QTreeWidgetItem::DontShowIndicator);
    }
}

void DlgPropertyLink::onItemExpanded(QTreeWidgetItem * item) {
    if(link.size()<5 || item->childCount()) 
        return;

    std::string name(qPrintable(item->data(0, Qt::UserRole).toString()));
    std::string docName(qPrintable(item->data(0, Qt::UserRole+1).toString()));
    auto doc = App::GetApplication().getDocument(docName.c_str());
    if(doc) {
        auto obj = doc->getObject(name.c_str());
        if(!obj) return;
        auto vp = Application::Instance->getViewProvider(obj);
        if(!vp) return;
        for(auto obj : vp->claimChildren())
            createItem(obj,item);
    }
}

void DlgPropertyLink::on_checkObjectType_toggled(bool on)
{
    findObjects(on);
}

void DlgPropertyLink::on_searchBox_textChanged(const QString& /*search*/)
{
    bool on = ui->checkObjectType->isChecked();
    findObjects(on);
}

void DlgPropertyLink::on_comboBox_currentIndexChanged(const QString& text)
{
    link[0] = text;
    bool on = ui->checkObjectType->isChecked();
    findObjects(on);
}


#include "moc_DlgPropertyLink.cpp"
