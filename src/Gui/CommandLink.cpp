/****************************************************************************
 *   Copyright (c) 2017 Zheng, Lei (realthunder) <realthunder.dev@gmail.com>*
 *                                                                          *
 *   This file is part of the FreeCAD CAx development system.               *
 *                                                                          *
 *   This library is free software; you can redistribute it and/or          *
 *   modify it under the terms of the GNU Library General Public            *
 *   License as published by the Free Software Foundation; either           *
 *   version 2 of the License, or (at your option) any later version.       *
 *                                                                          *
 *   This library  is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *   GNU Library General Public License for more details.                   *
 *                                                                          *
 *   You should have received a copy of the GNU Library General Public      *
 *   License along with this library; see the file COPYING.LIB. If not,     *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,          *
 *   Suite 330, Boston, MA  02111-1307, USA                                 *
 *                                                                          *
 ****************************************************************************/

#include "PreCompiled.h"

#ifndef _PreComp_
# include <sstream>
# include <QMessageBox>
#endif

#include <boost/algorithm/string/predicate.hpp>
#include "Command.h"
#include "Action.h"
#include "Application.h"
#include "MainWindow.h"
#include "Tree.h"
#include "Document.h"
#include "Selection.h"
#include "ViewProviderDocumentObject.h"

#include <Base/Console.h>
#include <Base/Exception.h>
#include <Base/Parameter.h>
#include <App/Application.h>
#include <App/Document.h>
#include <App/DocumentObject.h>

FC_LOG_LEVEL_INIT("CommandLink",true,true);

using namespace Gui;

static void setLinkLabel(App::DocumentObject *obj, const char *doc, const char *name) {
    auto linked = obj->getLinkedObject(true);
    if(!linked) linked = obj;
    const char *label = linked->Label.getValue();
    Command::doCommand(Command::Doc,"App.getDocument('%s').getObject('%s').Label='%s_%s'",doc,name,name,label);
}

////////////////////////////////////////////////////////////////////////////////////////////

DEF_STD_CMD_A(StdCmdLinkMakeGroup)

StdCmdLinkMakeGroup::StdCmdLinkMakeGroup()
  : Command("Std_LinkMakeGroup")
{
    sGroup        = QT_TR_NOOP("Link");
    sMenuText     = QT_TR_NOOP("Make link group");
    sToolTipText  = QT_TR_NOOP("Create a group of links");
    sWhatsThis    = "Std_LinkMakeGroup";
    sStatusTip    = sToolTipText;
    eType         = AlterDoc;
    sPixmap       = "LinkGroup";
}

bool StdCmdLinkMakeGroup::isActive() {
    return App::GetApplication().getActiveDocument() && Selection().hasSelection();
}

void StdCmdLinkMakeGroup::activated(int) {
    std::vector<App::DocumentObject*> objs;
    std::set<App::DocumentObject*> objset;

    auto doc = App::GetApplication().getActiveDocument();
    if(!doc) {
        FC_ERR("no active document");
        return;
    }

    for(auto &sel : Selection().getCompleteSelection()) {
        if(sel.pObject && sel.pObject->getNameInDocument() &&
           objset.insert(sel.pObject).second)
            objs.push_back(sel.pObject);
    }

    doc->openTransaction("Make link group");
    try {
        std::string groupName = doc->getUniqueObjectName("LinkGroup");
        Command::doCommand(Command::Doc,
            "App.getDocument('%s').addObject('App::LinkGroup','%s')",doc->getName(),groupName.c_str());
        if(objs.size()) {
            Command::doCommand(Command::Doc,"__objs__ = []");
            for(auto obj : objs) {
                std::string name;
                if(doc != obj->getDocument()) {
                    name = doc->getUniqueObjectName("Link");
                    Command::doCommand(Command::Doc,
                        "App.getDocument('%s').addObject('App::Link','%s').setLink(App.getDocument('%s').%s)",
                        doc->getName(),name.c_str(),obj->getDocument()->getName(),obj->getNameInDocument());
                    setLinkLabel(obj,doc->getName(),name.c_str());
                }else
                    name = obj->getNameInDocument();
                Command::doCommand(Command::Doc,"__objs__.append(App.getDocument('%s').%s)",
                        doc->getName(),name.c_str());
                Command::doCommand(Command::Doc,"App.getDocument('%s').%s.ViewObject.Visibility=False",
                        doc->getName(),name.c_str());
            }
            Command::doCommand(Command::Doc,"App.getDocument('%s').%s.setLink(__objs__)",
                    doc->getName(),groupName.c_str());
            Command::doCommand(Command::Doc,"del __objs__");
        }
    } catch (const Base::Exception& e) {
        QMessageBox::critical(getMainWindow(), QObject::tr("Create link group failed"),
            QString::fromLatin1(e.what()));
        doc->abortTransaction();
        e.ReportException();
    }
    doc->commitTransaction();
}

////////////////////////////////////////////////////////////////////////////////////////////

DEF_STD_CMD(StdCmdLinkMake)

StdCmdLinkMake::StdCmdLinkMake()
  : Command("Std_LinkMake")
{
    sGroup        = QT_TR_NOOP("Link");
    sMenuText     = QT_TR_NOOP("Make link");
    sToolTipText  = QT_TR_NOOP("Create a link to the selected object(s)");
    sWhatsThis    = "Std_LinkMake";
    sStatusTip    = sToolTipText;
    eType         = AlterDoc;
    sPixmap       = "Link";
}

void StdCmdLinkMake::activated(int) {
    auto doc = App::GetApplication().getActiveDocument();
    if(!doc) {
        FC_ERR("no active document");
        return;
    }

    std::set<App::DocumentObject*> objs;
    for(auto &sel : Selection().getCompleteSelection()) {
        if(sel.pObject && sel.pObject->getNameInDocument())
           objs.insert(sel.pObject);
    }

    doc->openTransaction("Make link");
    try {
        if(objs.empty()) {
            std::string name = doc->getUniqueObjectName("Link");
            Command::doCommand(Command::Doc, "App.getDocument('%s').addObject('App::Link','%s')",
                doc->getName(),name.c_str());
        }else{
            for(auto obj : objs) {
                std::string name = doc->getUniqueObjectName("Link");
                Command::doCommand(Command::Doc,
                    "App.getDocument('%s').addObject('App::Link','%s').setLink(App.getDocument('%s').%s)",
                    doc->getName(),name.c_str(),obj->getDocument()->getName(),obj->getNameInDocument());
                setLinkLabel(obj,doc->getName(),name.c_str());
            }
        }
        doc->commitTransaction();
    } catch (const Base::Exception& e) {
        QMessageBox::critical(getMainWindow(), QObject::tr("Create link failed"),
            QString::fromLatin1(e.what()));
        doc->abortTransaction();
        e.ReportException();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////

DEF_STD_CMD_A(StdCmdLinkMakeRelative)

StdCmdLinkMakeRelative::StdCmdLinkMakeRelative()
  : Command("Std_LinkMakeRelative")
{
    sGroup        = QT_TR_NOOP("Link");
    sMenuText     = QT_TR_NOOP("Make relative link");
    sToolTipText  = QT_TR_NOOP("Create a relative link of two selected objects");
    sWhatsThis    = "Std_LinkMakeRelative";
    sStatusTip    = sToolTipText;
    eType         = AlterDoc;
    sPixmap       = "LinkSub";
}

static App::DocumentObject *resolveLinkRelative(std::string *subname=0) {
    const auto &sels = Selection().getCompleteSelection(false);
    if(sels.size()!=2 || 
       !sels[0].pObject || !sels[0].pObject->getNameInDocument() ||
       !sels[1].pObject || !sels[1].pObject->getNameInDocument())
        return 0;
    auto len1 = strlen(sels[0].SubName);
    auto len2 = strlen(sels[1].SubName);
    if(len1>len2) {
        if(strncmp(sels[0].SubName,sels[1].SubName,len2)==0) {
            if(subname)
                *subname = sels[0].SubName+len2;
            return sels[1].pObject;
        }
    }else if(len1<len2) {
        if(strncmp(sels[1].SubName,sels[0].SubName,len1)==0) {
            if(subname)
                *subname = sels[1].SubName+len1;
            return sels[0].pObject;
        }
    }
    return 0;
}

bool StdCmdLinkMakeRelative::isActive() {
    return resolveLinkRelative()!=0;
}

void StdCmdLinkMakeRelative::activated(int) {
    auto doc = App::GetApplication().getActiveDocument();
    if(!doc) {
        FC_ERR("no active document");
        return;
    }
    std::string subname;
    auto owner = resolveLinkRelative(&subname);
    if(!owner) {
        FC_ERR("invalid selection");
        return;
    }
    auto obj = owner->getSubObject(subname.c_str());
    if(!obj) {
        FC_ERR("invalid sub-object " << owner->getNameInDocument() << '.' << subname);
        return;
    }

    std::string name = doc->getUniqueObjectName("Link");
    doc->openTransaction("Make link sub");
    try {
        Command::doCommand(Command::Doc, 
            "App.getDocument('%s').addObject('App::Link','%s').setLink(App.getDocument('%s').%s,'%s')", 
            doc->getName(),name.c_str(),
            owner->getDocument()->getName(),owner->getNameInDocument(), subname.c_str());
        setLinkLabel(obj,doc->getName(),name.c_str());
        doc->commitTransaction();
    } catch (const Base::Exception& e) {
        QMessageBox::critical(getMainWindow(), QObject::tr("Create link sub failed"),
            QString::fromLatin1(e.what()));
        doc->abortTransaction();
        e.ReportException();
    }
    return;
}

/////////////////////////////////////////////////////////////////////////////////////

static void linkConvert(bool unlink) {
    // We are trying to replace an object with a link (App::Link), or replace a
    // link back to its linked object (i.e. unlink). This is a very complex
    // operation. It works by reassign the link property of the parent of the
    // selected object(s) to a newly created link to the original object.
    // Everything should remain the same. To simplify the operation a bit, we
    // restrict ourself to replace selected objects of the active document only.
    //
    // But still, the complication arises because there may have relative links
    // that are affected by this operation. Currently, the only supported
    // relative link are any objects containing a PropertyXLink with a non-empty
    // 'Subname'. The ProeprtyXLink is pointed to the parent object, while the
    // subname references some child object inside. The subname may contain
    // multiple levels of indirection. So, if the subname refers to an object
    // being replace through the exact same parent object, the relative link
    // will be broken. We shall take care of this situation, and make the
    // appropriate adjustment.

    App::Document *doc = App::GetApplication().getActiveDocument();
    if(!doc) return;

    // first, generate partial (link name not known yet) command for
    // objects that are replacable. The commands are keyed by a
    // pair(parent, object)
    std::map<std::pair<App::DocumentObject*,App::DocumentObject*>, std::vector<std::string> > replaceCmds;
    for(auto sel : TreeWidget::getSelection(doc)) {
        auto obj = sel.second->getObject();
        auto parent = sel.first;
        if(!parent) {
            FC_WARN("skip '" << obj->getNameInDocument() << "' with no parent");
            continue;
        }
        auto parentObj = parent->getObject();
        if(unlink) {
            auto linked = obj->getLinkedObject(false);
            if(!linked || linked == obj) {
                FC_WARN("skip non link");
                continue;
            }
        }
        if(parentObj->getDocument()!=obj->getDocument()) {
            FC_WARN("cannot convert link for external object '" << obj->getNameInDocument() << "'");
            continue;
        }

        std::map<std::string, App::Property*> props;
        parentObj->getPropertyMap(props);

        std::vector<std::string> cmds;
        for(auto &v : props) {
            auto &propName = v.first;
            auto prop = v.second;
            auto linkProp = dynamic_cast<App::PropertyLink*>(prop);
            if(linkProp) {
                if(prop->testStatus(App::Property::Immutable) || parentObj->isReadOnly(prop))
                    continue;
                if(linkProp->getValue()==obj) {
                    std::ostringstream str;
                    str << "App.ActiveDocument."<< parentObj->getNameInDocument()<<'.'
                        << propName << '=' << "App.ActiveDocument.getObject('%s')";
                    cmds.push_back(str.str());
                }
                continue;
            }
            auto linksProp = dynamic_cast<App::PropertyLinkList*>(prop);
            if(linksProp) {
                if(prop->testStatus(App::Property::Immutable) || parentObj->isReadOnly(prop))
                    continue;
                int i;
                if(linksProp->find(obj->getNameInDocument(),&i)) {
                    std::ostringstream str;
                    str << "App.ActiveDocument."<< parentObj->getNameInDocument()<<'.'
                        << propName << "={"<<i<<":App.ActiveDocument.getObject('%s')}";
                    cmds.push_back(str.str());
                }
                continue;
            }
        }
        if(cmds.size())
            replaceCmds[std::make_pair(parentObj,obj)].swap(cmds);
        else 
            FC_WARN("skip '" << obj->getNameInDocument() << "': no link property found");
    }

    // Collect all realtive links in all documents, so that we can make
    // corrections for those that are affected by this operation.
    std::map<App::Document*, std::vector<std::pair<std::string,App::PropertyXLink*> > > links;
    for(auto pDoc : App::GetApplication().getDocuments()) {
        for(auto pObj : pDoc->getObjects()) {
            if(!pObj->getNameInDocument())
                continue;
            std::map<std::string, App::Property*> props;
            pObj->getPropertyMap(props);
            for(auto &v : props) {
                auto &propName = v.first;
                auto prop = v.second;
                auto link = dynamic_cast<App::PropertyXLink*>(prop);
                if(!link || link->testStatus(App::Property::Immutable) || pObj->isReadOnly(link))
                    continue;
                auto linked = link->getValue();
                if(!linked || !linked->getNameInDocument())
                    continue;
                auto sub = link->getSubName();
                if(!sub || !sub[0])
                    continue;
                links[pDoc].push_back(std::make_pair(propName,link));
            }
        }
    }

    std::map<App::DocumentObject*,std::string> replacedObjs;
    std::map<App::Document*,std::vector<std::string> > changeCmds;

    // now, do actual operation
    const char *transactionName = unlink?"Unlink":"Replace with link";
    doc->openTransaction(transactionName);
    try {
        for(auto &v : replaceCmds) {
            auto parent = v.first.first;
            auto obj =  v.first.second;
            auto &rcmds = v.second;
            auto it  = replacedObjs.find(obj);
            std::string name;
            // create the link if not done yet
            if(it!=replacedObjs.end())
                name = it->second;
            else if(unlink) {
                auto linked = obj->getLinkedObject(false);
                if(!linked || linked == obj || !linked->getNameInDocument())
                    continue;
                name = linked->getNameInDocument();
                it = replacedObjs.insert(std::make_pair(obj,name)).first;
            }else{
                name = doc->getUniqueObjectName("Link");
                Command::doCommand(Command::Doc,
                    "App.ActiveDocument.addObject('App::Link','%s')."
                    "setLink(App.ActiveDocument.getObject('%s'))",
                    name.c_str(),obj->getNameInDocument());
                setLinkLabel(obj,doc->getName(),name.c_str());
                if(obj->getPropertyByName("Placement"))
                    Command::doCommand(Command::Doc,
                        "App.ActiveDocument.getObject('%s').Placement = "
                        "App.ActiveDocument.getObject('%s').Placement",
                        name.c_str(),obj->getNameInDocument());
                else
                    Command::doCommand(Command::Doc,
                        "App.ActiveDocument.getObject('%s').LinkTransform = True", name.c_str());
                it = replacedObjs.insert(std::make_pair(obj,name)).first;
            }

            // do the replacement operation
            for(auto &cmd : rcmds)
                Command::doCommand(Command::Doc,cmd.c_str(),name.c_str());

            // generate command for relative link correction

            std::string objName(obj->getNameInDocument());
            objName += '.';
            std::string subName(parent->getNameInDocument());
            subName += '.';
            auto offset = subName.size();
            subName += objName;
            for(auto &v : links) {
                auto pDoc = v.first;
                auto &ccmds = changeCmds[pDoc];
                for(auto &vv : v.second) {
                    auto &propName = vv.first;
                    auto link = vv.second;
                    auto pObj = static_cast<App::DocumentObject*>(link->getContainer());
                    auto linked = link->getValue();
                    if(!linked || !linked->getNameInDocument())
                        continue;
                    auto sub = link->getSubName();
                    if(linked == parent) {
                        if(boost::algorithm::starts_with(sub,objName.c_str())) {
                            std::ostringstream str;
                            str << "App.getDocument('"<<pDoc->getName()<<"').getObject('"
                                << pObj->getNameInDocument() << "')." << propName
                                << "=(App.ActiveDocument.getObject('"
                                << parent->getNameInDocument() << "'),'" 
                                << name << '.' << (sub+objName.size()) << "')";
                            ccmds.push_back(str.str());
                        }
                        continue;
                    }
                    auto pos = strstr(sub,subName.c_str());
                    if(!pos) continue;
                    if(pos!=sub && pos[-1]!='.') {
                        FC_LOG("subname mismatch " << pDoc->getName() << '.' << 
                                linked->getNameInDocument() << '.' << sub);
                        continue;
                    }
                    std::string tmpSub(sub,pos-sub+offset);
                    auto subObj = linked->getSubObject(tmpSub.c_str());
                    if(subObj != parent) {
                        FC_LOG("sub object mismatch " << pDoc->getName() << '.' << 
                                linked->getNameInDocument() << '.' << sub);
                        continue;
                    }
                    std::ostringstream str;
                    str << "App.getDocument('" << pDoc->getName() << "').getObject('"
                        << pObj->getNameInDocument() << "')." << propName
                        << "=(App.ActiveDocument.getObject('"<< linked->getNameInDocument() 
                        << "'),'" << tmpSub << '.' << name << '.' << (pos+offset) << "')";
                    ccmds.push_back(str.str());
                }
            }
        }

        // run the command for realtive link correction of the current document
        auto it = changeCmds.find(doc);
        if(it!=changeCmds.end()) {
            for(auto &cmd : it->second)
                Command::runCommand(Gui::Command::Doc, cmd.c_str());
            changeCmds.erase(it);
        }

        // commit any changes to the current active document
        doc->commitTransaction();

    } catch (const Base::Exception& e) {
        auto title = unlink?QObject::tr("Unlink failed"):QObject::tr("Replace link failed");
        QMessageBox::critical(getMainWindow(), title, QString::fromLatin1(e.what()));
        doc->abortTransaction();
        e.ReportException();
        return;
    }

    // In case the replaced object affect realtive links in other documents, we
    // do the correction here. Note, twe don't roll back if there is any error.
    for(auto &v : changeCmds) {
        auto doc = v.first;
        doc->openTransaction(transactionName);
        for(auto &cmd : v.second) {
            try {
                Command::runCommand(Gui::Command::Doc, cmd.c_str());
            }catch(const Base::Exception &e) {
                e.ReportException();
            }
        }
        doc->commitTransaction();
    }
}

static bool linkConvertible(bool unlink) {
    App::Document *doc = App::GetApplication().getActiveDocument();
    if(!doc) return false;

    int count = 0;
    for(auto sel : TreeWidget::getSelection(doc)) {
        auto parent = sel.first;
        if(!parent) return false;
        App::DocumentObject *parentObj = parent->getObject();
        auto obj = sel.second->getObject();
        if(obj->getDocument()!=parentObj->getDocument()) 
            return false;
        if(unlink) {
            auto linked = obj->getLinkedObject(false);
            if(!linked || linked == obj)
                return false;
        }
        std::map<std::string, App::Property*> props;
        parentObj->getPropertyMap(props);
        bool found = false;
        for(auto &v : props) {
            auto linkProp = dynamic_cast<App::PropertyLink*>(v.second);
            if(linkProp) {
                if(v.second->testStatus(App::Property::Immutable) || parentObj->isReadOnly(v.second))
                    continue;
                if(linkProp->getValue()==obj) {
                    found = true;
                    break;
                }
                continue;
            }
            auto linksProp = dynamic_cast<App::PropertyLinkList*>(v.second);
            if(linksProp) {
                if(v.second->testStatus(App::Property::Immutable) || parentObj->isReadOnly(v.second))
                    continue;
                if(linksProp->find(obj->getNameInDocument())) {
                    found = true;
                    break;
                }
                continue;
            }
        }
        if(!found) return false;
        ++count;
    }
    return count!=0;
}

////////////////////////////////////////////////////////////////////////////////////////////

DEF_STD_CMD_A(StdCmdLinkReplace)

StdCmdLinkReplace::StdCmdLinkReplace()
  : Command("Std_LinkReplace")
{
    sGroup        = QT_TR_NOOP("Link");
    sMenuText     = QT_TR_NOOP("Replace with link");
    sToolTipText  = QT_TR_NOOP("Replace the selected object(s) with link");
    sWhatsThis    = "Std_LinkReplace";
    sStatusTip    = sToolTipText;
    eType         = AlterDoc;
    sPixmap       = "LinkReplace";
}

bool StdCmdLinkReplace::isActive() {
    return linkConvertible(false);
}

void StdCmdLinkReplace::activated(int) {
    linkConvert(false);
}

////////////////////////////////////////////////////////////////////////////////////////////

DEF_STD_CMD_A(StdCmdLinkUnlink)

StdCmdLinkUnlink::StdCmdLinkUnlink()
  : Command("Std_LinkUnlink")
{
    sGroup        = QT_TR_NOOP("Link");
    sMenuText     = QT_TR_NOOP("Unlink");
    sToolTipText  = QT_TR_NOOP("Strip on level of link");
    sWhatsThis    = "Std_LinkUnlink";
    sStatusTip    = sToolTipText;
    eType         = AlterDoc;
    sPixmap       = "Unlink";
}

bool StdCmdLinkUnlink::isActive() {
    return linkConvertible(true);
}

void StdCmdLinkUnlink::activated(int) {
    linkConvert(true);
}

////////////////////////////////////////////////////////////////////////////////////////////

DEF_STD_CMD_A(StdCmdLinkImport)

StdCmdLinkImport::StdCmdLinkImport()
  : Command("Std_LinkImport")
{
    sGroup        = QT_TR_NOOP("Link");
    sMenuText     = QT_TR_NOOP("Import links");
    sToolTipText  = QT_TR_NOOP("Import selected external link(s)");
    sWhatsThis    = "Std_LinkImport";
    sStatusTip    = sToolTipText;
    eType         = AlterDoc;
    sPixmap       = "LinkImport";
}

static std::map<App::Document*, std::vector<App::DocumentObject*> > getLinkImportSelections(bool checking) 
{
    std::map<App::Document*, std::vector<App::DocumentObject*> > objMap;
    for(auto &sel : Selection().getCompleteSelection(false)) {
        App::DocumentObject *parent = 0;
        auto obj = Selection().resolveObject(sel.pObject,sel.SubName,&parent);
        if(!parent || parent->getDocument()==obj->getDocument()) {
            if(!checking) 
                FC_WARN("skip invalid parent of " << 
                    sel.DocName << '.' << sel.FeatName << '.' << sel.SubName);
        }else{
            objMap[parent->getDocument()].push_back(obj);
            if(checking)
                break;
        }
    }
    return objMap;
}

bool StdCmdLinkImport::isActive() {
    return !getLinkImportSelections(true).empty();
}

void StdCmdLinkImport::activated(int) {
    for(auto &v : getLinkImportSelections(false)) {
        auto doc = v.first;
        doc->openTransaction("Import links");
        try {
            // TODO: Is it possible to do this using interpreter?
            for(auto obj : doc->importLinks(v.second))
                obj->Visibility.setValue(false);
            doc->commitTransaction();
        } catch (const Base::Exception& e) {
            QMessageBox::critical(getMainWindow(), QObject::tr("Failed to import links"),
                QString::fromLatin1(e.what()));
            doc->abortTransaction();
            e.ReportException();
            break;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////

DEF_STD_CMD_A(StdCmdLinkImportAll)

StdCmdLinkImportAll::StdCmdLinkImportAll()
  : Command("Std_LinkImportAll")
{
    sGroup        = QT_TR_NOOP("Link");
    sMenuText     = QT_TR_NOOP("Import all links");
    sToolTipText  = QT_TR_NOOP("Import all links of the active document");
    sWhatsThis    = "Std_LinkImportAll";
    sStatusTip    = sToolTipText;
    eType         = AlterDoc;
    sPixmap       = "LinkImportAll";
}

bool StdCmdLinkImportAll::isActive() {
    auto doc = App::GetApplication().getActiveDocument();
    return doc && App::PropertyXLink::hasXLink(doc);
}

void StdCmdLinkImportAll::activated(int) {
    auto doc = App::GetApplication().getActiveDocument();
    doc->openTransaction("Import all links");
    try {
        std::ostringstream str;
        str << "for _o in App.ActiveDocument.importLinks():" << std::endl
                << "  _o.ViewObject.Visibility=False" << std::endl
            << "_o = None";
        Command::runCommand(Command::Doc,str.str().c_str());
        doc->commitTransaction();
    } catch (const Base::Exception& e) {
        QMessageBox::critical(getMainWindow(), QObject::tr("Failed to import all links"),
            QString::fromLatin1(e.what()));
        doc->abortTransaction();
        e.ReportException();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////

DEF_STD_CMD_A(StdCmdLinkSelectLinked)

StdCmdLinkSelectLinked::StdCmdLinkSelectLinked()
  : Command("Std_LinkSelectLinked")
{
    sGroup        = QT_TR_NOOP("Link");
    sMenuText     = QT_TR_NOOP("Select linked object");
    sToolTipText  = QT_TR_NOOP("Select the linked object");
    sWhatsThis    = "Std_LinkSelectLinked";
    sStatusTip    = sToolTipText;
    eType         = AlterSelection;
    sPixmap       = "LinkSelect";
}

static App::DocumentObject *getSelectedLink(bool finalLink) {
    const auto &sels = Selection().getSelection("*",true,true);
    if(sels.empty())
        return 0;
    auto linked = sels[0].pObject->getLinkedObject(false);
    if(!linked || linked==sels[0].pObject)
        return 0;

    if(finalLink) {
        auto linkedFinal = sels[0].pObject->getLinkedObject(true);
        if(linkedFinal == linked)
            return 0;
    }
    return sels[0].pObject;
}

bool StdCmdLinkSelectLinked::isActive() {
    return getSelectedLink(false)!=0;
}

void StdCmdLinkSelectLinked::activated(int)
{
    auto obj = getSelectedLink(false);
    if(!obj){
        FC_WARN("invalid selection");
        return;
    }
    for(auto tree : getMainWindow()->findChildren<TreeWidget*>())
        tree->selectLinkedObject(obj,false);
}

////////////////////////////////////////////////////////////////////////////////////////////

DEF_STD_CMD_A(StdCmdLinkSelectLinkedFinal)

StdCmdLinkSelectLinkedFinal::StdCmdLinkSelectLinkedFinal()
  : Command("Std_LinkSelectLinkedFinal")
{
    sGroup        = QT_TR_NOOP("Link");
    sMenuText     = QT_TR_NOOP("Select final linked object");
    sToolTipText  = QT_TR_NOOP("Select the deepest linked object");
    sWhatsThis    = "Std_LinkSelectLinkedFinal";
    sStatusTip    = sToolTipText;
    eType         = AlterSelection;
    sPixmap       = "LinkSelectFinal";
}

bool StdCmdLinkSelectLinkedFinal::isActive() {
    return getSelectedLink(true)!=0;
}

void StdCmdLinkSelectLinkedFinal::activated(int) {
    auto obj = getSelectedLink(true);
    if(!obj){
        FC_WARN("invalid selection");
        return;
    }
    for(auto tree : getMainWindow()->findChildren<TreeWidget*>())
        tree->selectLinkedObject(obj,true);
}

////////////////////////////////////////////////////////////////////////////////////////////

DEF_STD_CMD_A(StdCmdLinkSelectAllLinks)

StdCmdLinkSelectAllLinks::StdCmdLinkSelectAllLinks()
  : Command("Std_LinkSelectAllLinks")
{
    sGroup        = QT_TR_NOOP("Link");
    sMenuText     = QT_TR_NOOP("Select all links");
    sToolTipText  = QT_TR_NOOP("Select all links to the current selected object");
    sWhatsThis    = "Std_LinkSelectAllLinks";
    sStatusTip    = sToolTipText;
    eType         = AlterSelection;
    sPixmap       = "LinkSelectAll";
}

bool StdCmdLinkSelectAllLinks::isActive() {
    const auto &sels = Selection().getSelection("*",true,true);
    if(sels.empty())
        return false;
    return App::GetApplication().hasLinksTo(sels[0].pObject);
}

void StdCmdLinkSelectAllLinks::activated(int)
{
    const auto &sels = Selection().getSelection("*",true,true);
    if(sels.empty()) {
        FC_ERR("invalid selection");
        return;
    }
    for(auto tree : getMainWindow()->findChildren<TreeWidget*>())
        tree->selectAllLinks(sels[0].pObject);
}

//===========================================================================
// Instantiation
//===========================================================================


namespace Gui {

void CreateLinkCommands(void)
{
    CommandManager &rcCmdMgr = Application::Instance->commandManager();
    rcCmdMgr.addCommand(new StdCmdLinkSelectLinked());
    rcCmdMgr.addCommand(new StdCmdLinkSelectLinkedFinal());
    rcCmdMgr.addCommand(new StdCmdLinkSelectAllLinks());
    rcCmdMgr.addCommand(new StdCmdLinkMake());
    rcCmdMgr.addCommand(new StdCmdLinkMakeRelative());
    rcCmdMgr.addCommand(new StdCmdLinkMakeGroup());
    rcCmdMgr.addCommand(new StdCmdLinkReplace());
    rcCmdMgr.addCommand(new StdCmdLinkUnlink());
    rcCmdMgr.addCommand(new StdCmdLinkImport());
    rcCmdMgr.addCommand(new StdCmdLinkImportAll());
}

} // namespace Gui

