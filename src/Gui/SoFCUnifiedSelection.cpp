/***************************************************************************
 *   Copyright (c) 2005 Jürgen Riegel <juergen.riegel@web.de>              *
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
# include <qstatusbar.h>
# include <qstring.h>
# include <Inventor/details/SoFaceDetail.h>
# include <Inventor/details/SoLineDetail.h>
#endif

#include <Inventor/elements/SoOverrideElement.h>
#include <Inventor/elements/SoLazyElement.h>
#include <Inventor/elements/SoCacheElement.h>
#include <Inventor/elements/SoOverrideElement.h>
#include <Inventor/elements/SoWindowElement.h>

#include <Inventor/SoFullPath.h>
#include <Inventor/actions/SoGLRenderAction.h>
#include <Inventor/actions/SoHandleEventAction.h>
#include <Inventor/events/SoKeyboardEvent.h>
#include <Inventor/elements/SoComplexityElement.h>
#include <Inventor/elements/SoComplexityTypeElement.h>
#include <Inventor/elements/SoCoordinateElement.h>
#include <Inventor/elements/SoElements.h>
#include <Inventor/elements/SoFontNameElement.h>
#include <Inventor/elements/SoFontSizeElement.h>
#include <Inventor/elements/SoMaterialBindingElement.h>
#include <Inventor/elements/SoModelMatrixElement.h>
#include <Inventor/elements/SoShapeStyleElement.h>
#include <Inventor/elements/SoProfileCoordinateElement.h>
#include <Inventor/elements/SoProfileElement.h>
#include <Inventor/elements/SoSwitchElement.h>
#include <Inventor/elements/SoUnitsElement.h>
#include <Inventor/elements/SoViewVolumeElement.h>
#include <Inventor/elements/SoViewingMatrixElement.h>
#include <Inventor/elements/SoViewportRegionElement.h>
#include <Inventor/elements/SoGLCacheContextElement.h>
#include <Inventor/events/SoMouseButtonEvent.h>
#include <Inventor/misc/SoState.h>
#include <Inventor/misc/SoChildList.h>
#include <Inventor/nodes/SoMaterial.h>
#include <Inventor/nodes/SoMaterialBinding.h>
#include <Inventor/nodes/SoNormalBinding.h>
#include <Inventor/events/SoLocation2Event.h>
#include <Inventor/SoPickedPoint.h>

#ifdef FC_OS_MACOSX
# include <OpenGL/gl.h>
#else
# ifdef FC_OS_WIN32
#  include <windows.h>
# endif
# include <GL/gl.h>
#endif

#include <QtOpenGL.h>

#include <Base/Console.h>
#include <App/Application.h>
#include <App/Document.h>
#include <Gui/Document.h>
#include <App/DocumentObject.h>

#include "SoFCUnifiedSelection.h"
#include "Application.h"
#include "MainWindow.h"
#include "Selection.h"
#include "ViewProvider.h"
#include "SoFCInteractiveElement.h"
#include "SoFCSelectionAction.h"
#include "ViewProviderDocumentObject.h"
#include "ViewProviderGeometryObject.h"

FC_LOG_LEVEL_INIT("SoFCUnifiedSelection",false,true,true);

using namespace Gui;

SoFullPath * Gui::SoFCUnifiedSelection::currenthighlight = NULL;

// *************************************************************************

SO_NODE_SOURCE(SoFCUnifiedSelection);

/*!
  Constructor.
*/
SoFCUnifiedSelection::SoFCUnifiedSelection() : pcDocument(0)
{
    SO_NODE_CONSTRUCTOR(SoFCUnifiedSelection);

    SO_NODE_ADD_FIELD(colorHighlight, (SbColor(1.0f, 0.6f, 0.0f)));
    SO_NODE_ADD_FIELD(colorSelection, (SbColor(0.1f, 0.8f, 0.1f)));
    SO_NODE_ADD_FIELD(highlightMode,  (AUTO));
    SO_NODE_ADD_FIELD(selectionMode,  (ON));
    SO_NODE_ADD_FIELD(selectionRole,  (true));

    SO_NODE_DEFINE_ENUM_VALUE(HighlightModes, AUTO);
    SO_NODE_DEFINE_ENUM_VALUE(HighlightModes, ON);
    SO_NODE_DEFINE_ENUM_VALUE(HighlightModes, OFF);
    SO_NODE_SET_SF_ENUM_TYPE (highlightMode, HighlightModes);

    detailPath = static_cast<SoFullPath*>(new SoPath(20));
    detailPath->ref();

    setPreSelection = false;
    preSelection = -1;
}

/*!
  Destructor.
*/
SoFCUnifiedSelection::~SoFCUnifiedSelection()
{
    // If we're being deleted and we're the current highlight,
    // NULL out that variable
    if (currenthighlight != NULL) {
        currenthighlight->unref();
        currenthighlight = NULL;
    }
    if (detailPath) {
        detailPath->unref();
        detailPath = NULL;
    }
}

// doc from parent
void
SoFCUnifiedSelection::initClass(void)
{
    SO_NODE_INIT_CLASS(SoFCUnifiedSelection,SoSeparator,"Separator");
}

void SoFCUnifiedSelection::finish()
{
    atexit_cleanup();
}

bool SoFCUnifiedSelection::hasHighlight() {
    return currenthighlight != NULL;
}

void SoFCUnifiedSelection::applySettings()
{
    float transparency;
    ParameterGrp::handle hGrp = Gui::WindowParameter::getDefaultParameter()->GetGroup("View");
    bool enablePre = hGrp->GetBool("EnablePreselection", true);
    bool enableSel = hGrp->GetBool("EnableSelection", true);
    if (!enablePre) {
        this->highlightMode = SoFCUnifiedSelection::OFF;
    }
    else {
        // Search for a user defined value with the current color as default
        SbColor highlightColor = this->colorHighlight.getValue();
        unsigned long highlight = (unsigned long)(highlightColor.getPackedValue());
        highlight = hGrp->GetUnsigned("HighlightColor", highlight);
        highlightColor.setPackedValue((uint32_t)highlight, transparency);
        this->colorHighlight.setValue(highlightColor);
    }
    if (!enableSel) {
        this->selectionMode = SoFCUnifiedSelection::OFF;
    }
    else {
        // Do the same with the selection color
        SbColor selectionColor = this->colorSelection.getValue();
        unsigned long selection = (unsigned long)(selectionColor.getPackedValue());
        selection = hGrp->GetUnsigned("SelectionColor", selection);
        selectionColor.setPackedValue((uint32_t)selection, transparency);
        this->colorSelection.setValue(selectionColor);
    }
}

const char* SoFCUnifiedSelection::getFileFormatName(void) const
{
    return "Separator";
}

void SoFCUnifiedSelection::write(SoWriteAction * action)
{
    SoOutput * out = action->getOutput();
    if (out->getStage() == SoOutput::WRITE) {
        // Do not write out the fields of this class
        if (this->writeHeader(out, true, false)) return;
        SoGroup::doAction((SoAction *)action);
        this->writeFooter(out);
    }
    else {
        inherited::write(action);
    }
}

int SoFCUnifiedSelection::getPriority(const SoPickedPoint* p)
{
    const SoDetail* detail = p->getDetail();
    if (!detail)                                           return 0;
    if (detail->isOfType(SoFaceDetail::getClassTypeId()))  return 1;
    if (detail->isOfType(SoLineDetail::getClassTypeId()))  return 2;
    if (detail->isOfType(SoPointDetail::getClassTypeId())) return 3;
    return 0;
}

std::vector<SoFCUnifiedSelection::PickedInfo> 
SoFCUnifiedSelection::getPickedList(SoHandleEventAction* action, bool singlePick) const
{
    ViewProvider *last_vp = 0;
    std::vector<PickedInfo> ret;
    const SoPickedPointList & points = action->getPickedPointList();
    for(int i=0,count=points.getLength();i<count;++i) {
        PickedInfo info;
        info.pp = points[i];
        info.vpd = 0;
        ViewProvider *vp = 0;
        SoFullPath *path = static_cast<SoFullPath *>(info.pp->getPath());
        if (this->pcDocument && path && path->containsPath(action->getCurPath())) {
            vp = this->pcDocument->getViewProviderByPathFromHead(path);
            if(singlePick && last_vp && last_vp!=vp)
                return ret;
        }
        if(!vp || !vp->isDerivedFrom(ViewProviderDocumentObject::getClassTypeId())) {
            if(!singlePick) continue;
            if(ret.empty())
                ret.push_back(info);
            break;
        }
        info.vpd = static_cast<ViewProviderDocumentObject*>(vp);
        if(!info.vpd->useNewSelectionModel() || !info.vpd->isSelectable()) {
            if(!singlePick) continue;
            if(ret.empty()) {
                info.vpd = 0;
                ret.push_back(info);
            }
            break;
        }
        if(!info.vpd->getElementPicked(info.pp,info.element))
            continue;

        if(singlePick) 
            last_vp = vp;
        ret.push_back(info);
    }

    if(ret.size()<=1) return ret;

    // To identify the picking of lines in a concave area we have to 
    // get all intersection points. If we have two or more intersection
    // points where the first is of a face and the second of a line with
    // almost similar coordinates we use the second point, instead.

    int picked_prio = getPriority(ret[0].pp);
    auto last_vpd = ret[0].vpd;
    const SbVec3f& picked_pt = ret.front().pp->getPoint();
    auto itPicked = ret.begin();
    for(auto it=ret.begin()+1;it!=ret.end();++it) {
        auto &info = *it;
        if(last_vpd != info.vpd) 
            break;

        int cur_prio = getPriority(info.pp);
        const SbVec3f& cur_pt = info.pp->getPoint();

        if ((cur_prio > picked_prio) && picked_pt.equals(cur_pt, 0.01f)) {
            itPicked = it;
            picked_prio = cur_prio;
        }
    }

    if(singlePick) {
        std::vector<PickedInfo> sret(itPicked,itPicked+1);
        return sret;
    }
    if(itPicked != ret.begin())
        std::swap(*itPicked, *ret.begin());
    return ret;
}

void SoFCUnifiedSelection::doAction(SoAction *action)
{
    if (action->getTypeId() == SoFCEnableHighlightAction::getClassTypeId()) {
        SoFCEnableHighlightAction *preaction = (SoFCEnableHighlightAction*)action;
        if (preaction->highlight) {
            this->highlightMode = SoFCUnifiedSelection::AUTO;
        }
        else {
            this->highlightMode = SoFCUnifiedSelection::OFF;
        }
    }

    if (action->getTypeId() == SoFCEnableSelectionAction::getClassTypeId()) {
        SoFCEnableSelectionAction *selaction = (SoFCEnableSelectionAction*)action;
        if (selaction->selection) {
            this->selectionMode = SoFCUnifiedSelection::ON;
        }
        else {
            this->selectionMode = SoFCUnifiedSelection::OFF;
        }
    }

    if (action->getTypeId() == SoFCSelectionColorAction::getClassTypeId()) {
        SoFCSelectionColorAction *colaction = (SoFCSelectionColorAction*)action;
        this->colorSelection = colaction->selectionColor;
    }

    if (action->getTypeId() == SoFCHighlightColorAction::getClassTypeId()) {
        SoFCHighlightColorAction *colaction = (SoFCHighlightColorAction*)action;
        this->colorHighlight = colaction->highlightColor;
    }

    if (highlightMode.getValue() != OFF && action->getTypeId() == SoFCHighlightAction::getClassTypeId()) {
        SoFCHighlightAction *hilaction = static_cast<SoFCHighlightAction*>(action);
        // Do not clear currently highlighted object when setting new pre-selection
        if (!setPreSelection && hilaction->SelChange.Type == SelectionChanges::RmvPreselect) {
            if (currenthighlight) {
                SoHighlightElementAction action;
                action.apply(currenthighlight);
                currenthighlight->unref();
                currenthighlight = 0;
            }
        }
    }

    if (selectionMode.getValue() == ON && action->getTypeId() == SoFCSelectionAction::getClassTypeId()) {
        SoFCSelectionAction *selaction = static_cast<SoFCSelectionAction*>(action);
        if (selaction->SelChange.Type == SelectionChanges::AddSelection || 
            selaction->SelChange.Type == SelectionChanges::RmvSelection) {
            // selection changes inside the 3d view are handled in handleEvent()
            App::Document* doc = App::GetApplication().getDocument(selaction->SelChange.pDocName);
            App::DocumentObject* obj = doc->getObject(selaction->SelChange.pObjectName);
            ViewProvider*vp = Application::Instance->getViewProvider(obj);
            if (vp && vp->useNewSelectionModel() && vp->isSelectable()) {
                SoDetail *detail = nullptr;
                detailPath->truncate(0);
                if(selaction->SelChange.pSubName && selaction->SelChange.pSubName[0])
                    detail = vp->getDetailPath(selaction->SelChange.pSubName,detailPath,true);
                SoSelectionElementAction::Type type = SoSelectionElementAction::None;
                if (selaction->SelChange.Type == SelectionChanges::AddSelection) {
                    if (detail)
                        type = SoSelectionElementAction::Append;
                    else
                        type = SoSelectionElementAction::All;
                }
                else {
                    if (detail)
                        type = SoSelectionElementAction::Remove;
                    else
                        type = SoSelectionElementAction::None;
                }

                if(checkSelectionStyle(type,vp)) {
                    SoSelectionElementAction action(type);
                    action.setColor(this->colorSelection.getValue());
                    action.setElement(detail);
                    if(detailPath->getLength())
                        action.apply(detailPath);
                    else
                        action.apply(vp->getRoot());
                }
                detailPath->truncate(0);
                delete detail;
            }
        }
        else if (selaction->SelChange.Type == SelectionChanges::ClrSelection ||
                 selaction->SelChange.Type == SelectionChanges::SetSelection) {
            std::vector<ViewProvider*> vps;
            if (this->pcDocument)
                vps = this->pcDocument->getViewProvidersOfType(ViewProviderDocumentObject::getClassTypeId());
            for (std::vector<ViewProvider*>::iterator it = vps.begin(); it != vps.end(); ++it) {
                ViewProviderDocumentObject* vpd = static_cast<ViewProviderDocumentObject*>(*it);
                if (vpd->useNewSelectionModel()) {
                    SoSelectionElementAction::Type type;
                    if(Selection().isSelected(vpd->getObject()) && vpd->isSelectable())
                        type = SoSelectionElementAction::All;
                    else
                        type = SoSelectionElementAction::None;
                    if(checkSelectionStyle(type,vpd)) {
                        SoSelectionElementAction action(type);
                        action.setColor(this->colorSelection.getValue());
                        action.apply(vpd->getRoot());
                    }
                }
            }
        } else if (selaction->SelChange.Type == SelectionChanges::SetPreselectSignal) {
            // selection changes inside the 3d view are handled in handleEvent()
            App::Document* doc = App::GetApplication().getDocument(selaction->SelChange.pDocName);
            App::DocumentObject* obj = doc->getObject(selaction->SelChange.pObjectName);
            ViewProvider*vp = Application::Instance->getViewProvider(obj);
            if (vp && vp->isDerivedFrom(ViewProviderDocumentObject::getClassTypeId()) &&
                vp->useNewSelectionModel() && vp->isSelectable()) 
            {
                detailPath->truncate(0);
                SoDetail* detail = vp->getDetailPath(selaction->SelChange.pSubName,detailPath,true);
                setHighlight(detailPath,detail,static_cast<ViewProviderDocumentObject*>(vp),
                        selaction->SelChange.pSubName, 
                        selaction->SelChange.x,
                        selaction->SelChange.y,
                        selaction->SelChange.z);
                delete detail;
            }
        }
    }

    inherited::doAction( action );
}

bool SoFCUnifiedSelection::setHighlight(const PickedInfo &info) {
    if(!info.pp)
        return setHighlight(0,0,0,0,0.0,0.0,0.0);
    const auto &pt = info.pp->getPoint();
    return setHighlight(static_cast<SoFullPath*>(info.pp->getPath()), 
            info.pp->getDetail(), info.vpd, info.element.c_str(), pt[0],pt[1],pt[2]);
}

bool SoFCUnifiedSelection::setHighlight(SoFullPath *path, const SoDetail *det, 
        ViewProviderDocumentObject *vpd, const char *element, float x, float y, float z) 
{
    setPreSelection = true;
    bool highlighted = false;
    if(path && path->getLength() && 
       vpd && vpd->getObject() && vpd->getObject()->getNameInDocument()) 
    {
        const char *docname = vpd->getObject()->getDocument()->getName();
        const char *objname = vpd->getObject()->getNameInDocument();

        this->preSelection = 1;
        static char buf[513];
        snprintf(buf,512,"Preselected: %s.%s.%s (%g, %g, %g)"
                ,docname,objname,element
                ,fabs(x)>1e-7?x:0.0
                ,fabs(y)>1e-7?y:0.0
                ,fabs(z)>1e-7?z:0.0);

        getMainWindow()->showMessage(QString::fromLatin1(buf));

        if (Gui::Selection().setPreselect(docname,objname,element,x,y,z)) {
            if (currenthighlight && *currenthighlight!=*path) {
                SoHighlightElementAction action;
                action.setHighlighted(false);
                action.apply(currenthighlight);
                currenthighlight->unref();
                currenthighlight = 0;
            }
            currenthighlight = static_cast<SoFullPath*>(path->copy());
            currenthighlight->ref();
            highlighted = true;
        }
    }

    if(currenthighlight) {
        SoHighlightElementAction action;
        action.setHighlighted(highlighted);
        action.setColor(this->colorHighlight.getValue());
        action.setElement(det);
        action.apply(currenthighlight);
        if(!highlighted) {
            currenthighlight->unref();
            currenthighlight = 0;
        }
        this->touch();
    }
    setPreSelection = false;
    return highlighted;
}

bool SoFCUnifiedSelection::setSelection(const std::vector<PickedInfo> &infos, bool ctrlDown) {
    if(infos.empty() || !infos[0].vpd) return false;

    std::vector<SelectionSingleton::SelObj> sels;
    if(infos.size()>1) {
        for(auto &info : infos) {
            if(!info.vpd) continue;
            SelectionSingleton::SelObj sel;
            sel.pObject = info.vpd->getObject();
            sel.pDoc = sel.pObject->getDocument();
            sel.DocName = sel.pDoc->getName();
            sel.FeatName = sel.pObject->getNameInDocument();
            sel.TypeName = sel.pObject->getTypeId().getName();
            sel.SubName = info.element.c_str();
            const auto &pt = info.pp->getPoint();
            sel.x = pt[0];
            sel.y = pt[1];
            sel.z = pt[2];
            sels.push_back(sel);
        }
    }

    const auto &info = infos[0];
    auto vpd = info.vpd;
    if(!vpd) return false;
    const char *objname = vpd->getObject()->getNameInDocument();
    if(!objname) return false;
    const char *docname = vpd->getObject()->getDocument()->getName();

    bool hasNext = false;
    const SoPickedPoint * pp = info.pp;
    const SoDetail *det = pp->getDetail();
    SoDetail *detNext = 0;
    SoFullPath *pPath = static_cast<SoFullPath*>(pp->getPath());
    const auto &pt = pp->getPoint();
    SoSelectionElementAction::Type type = SoSelectionElementAction::None;
    HighlightModes mymode = (HighlightModes) this->highlightMode.getValue();
    static char buf[513];

    if (ctrlDown) {
        if(Gui::Selection().isSelected(docname,objname,info.element.c_str()))
            Gui::Selection().rmvSelection(docname,objname,info.element.c_str(),&sels);
        else {
            bool ok = Gui::Selection().addSelection(docname,objname,
                    info.element.c_str(), pt[0] ,pt[1] ,pt[2], &sels);
            if (ok && mymode == OFF) {
                snprintf(buf,512,"Selected: %s.%s.%s (%g, %g, %g)",
                        docname,objname,info.element.c_str()
                        ,fabs(pt[0])>1e-7?pt[0]:0.0
                        ,fabs(pt[1])>1e-7?pt[1]:0.0
                        ,fabs(pt[2])>1e-7?pt[2]:0.0);

                getMainWindow()->showMessage(QString::fromLatin1(buf));
            }
        }
        return true;
    }

    // Hierarchy acending
    //
    // If the clicked subelement is already selected, check if there is an
    // upper hierarchy, and select that hierarchy instead. 
    //
    // For example, let's suppose PickedInfo above reports
    // 'link.link2.box.Face1', and below Selection().getSelectedElement returns
    // 'link.link2.box.', meaning that 'box' is the current selected hierarchy,
    // and the user is clicking the box again.  So we shall go up one level,
    // and select 'link.link2.'
    //

    std::string subName = info.element;
    std::string objectName = objname;

    const char *subSelected = Gui::Selection().getSelectedElement(
                                vpd->getObject(),subName.c_str());

    FC_TRACE("select " << (subSelected?subSelected:"'null'") << ", " << 
            objectName << ", " << subName);
    if(subSelected) {
        std::string nextsub;
        const char *next = strrchr(subSelected,'.');
        if(next && next!=subSelected) {
            if(next[1]==0) {
                // The convention of dot separated SubName demands a mandatory
                // ending dot for every object name reference inside SubName.
                // The non-object sub-element, however, must not end with a dot.
                // So, next[1]==0 here means current selection is a whole object
                // selection (because no sub-element), so we shall search
                // upwards for the second last dot, which is the end of the
                // parent name of the current selected object
                for(--next;next!=subSelected;--next) {
                    if(*next == '.') break;
                }
            }
            if(*next == '.')
                nextsub = std::string(subSelected,next-subSelected+1);
        }
        if(nextsub.length() || *subSelected!=0) {
            hasNext = true;
            subName = nextsub;
            detailPath->truncate(0);
            detNext = vpd->getDetailPath(subName.c_str(),detailPath,true);
            if(detailPath->getLength()) {
                pPath = detailPath;
                det = detNext;
                FC_TRACE("select next " << objectName << ", " << subName);
            }
        }
    }

#if 0 // geo feature group now implements getElementPicked

    // If no next hierarchy is found, do another try on view provider hierarchies, 
    // which is used by geo feature group.
    if(!hasNext) {
        bool found = false;
        auto vps = this->pcDocument->getViewProvidersByPath(pPath);
        for(auto it=vps.begin();it!=vps.end();++it) {
            auto vpdNext = it->first;
            if(Gui::Selection().isSelected(vpdNext->getObject(),"")) {
                found = true;
                continue;
            }
            if(!found || !vpdNext->useNewSelectionModel() || !vpdNext->isSelectable())
                continue;
            hasNext = true;
            vpd = vpdNext;
            det = 0;
            pPath->truncate(it->second+1);
            objectName = vpd->getObject()->getNameInDocument();
            subName = "";
            break;
        }
    }
#endif

    FC_TRACE("clearing selection");
    Gui::Selection().clearSelection(docname);
    FC_TRACE("add selection");
    bool ok = Gui::Selection().addSelection(docname, objectName.c_str() ,subName.c_str(), 
            pt[0] ,pt[1] ,pt[2], &sels);
    if (ok)
        type = hasNext?SoSelectionElementAction::All:SoSelectionElementAction::Append;

    if (mymode == OFF) {
        snprintf(buf,512,"Selected: %s.%s.%s (%g, %g, %g)",
                docname, objectName.c_str() ,subName.c_str()
                ,fabs(pt[0])>1e-7?pt[0]:0.0
                ,fabs(pt[1])>1e-7?pt[1]:0.0
                ,fabs(pt[2])>1e-7?pt[2]:0.0);

        getMainWindow()->showMessage(QString::fromLatin1(buf));
    }

    if (pPath && checkSelectionStyle(type,vpd)) {
        FC_TRACE("applying action");
        SoSelectionElementAction action(type);
        action.setColor(this->colorSelection.getValue());
        action.setElement(det);
        action.apply(pPath);
        FC_TRACE("applied action");
        this->touch();
    }

    if(detNext) delete detNext;
    return true;
}

// doc from parent
void
SoFCUnifiedSelection::handleEvent(SoHandleEventAction * action)
{
    // If off then don't handle this event
    if (!selectionRole.getValue()) {
        inherited::handleEvent(action);
        return;
    }

    HighlightModes mymode = (HighlightModes) this->highlightMode.getValue();
    const SoEvent * event = action->getEvent();

    // If we don't need to pick for locate highlighting,
    // then just behave as separator and return.
    // NOTE: we still have to pick for ON even though we don't have
    // to re-render, because the app needs to be notified as the mouse
    // goes over locate highlight nodes.
    //if (highlightMode.getValue() == OFF) {
    //    inherited::handleEvent( action );
    //    return;
    //}

    //
    // If this is a mouseMotion event, then check for locate highlighting
    //
    if (event->isOfType(SoLocation2Event::getClassTypeId())) {
        // NOTE: If preselection is off then we do not check for a picked point because otherwise this search may slow
        // down extremely the system on really big data sets. In this case we just check for a picked point if the data
        // set has been selected.
        if (mymode == AUTO || mymode == ON) {
            // check to see if the mouse is over our geometry...
            auto infos = this->getPickedList(action,true);
            if(infos.size()) 
                setHighlight(infos[0]);
            else {
                setHighlight(PickedInfo());
                if (this->preSelection > 0) {
                    this->preSelection = 0;
                    // touch() makes sure to call GLRenderBelowPath so that the cursor can be updated
                    // because only from there the SoGLWidgetElement delivers the OpenGL window
                    this->touch();
                }
            }
        }
    }
    // mouse press events for (de)selection
    else if (event->isOfType(SoMouseButtonEvent::getClassTypeId()) && 
             selectionMode.getValue() == SoFCUnifiedSelection::ON) {
        const SoMouseButtonEvent* e = static_cast<const SoMouseButtonEvent *>(event);
        if (SoMouseButtonEvent::isButtonReleaseEvent(e,SoMouseButtonEvent::BUTTON1)) {
            // check to see if the mouse is over a geometry...
            auto infos = this->getPickedList(action,!Selection().needPickedList());
            if(setSelection(infos,event->wasCtrlDown()))
                action->setHandled();
        } // mouse release
    }

    inherited::handleEvent(action);
}

bool SoFCUnifiedSelection::checkSelectionStyle(int type, ViewProvider *vp) {
    if((type == SoSelectionElementAction::All ||
        type == SoSelectionElementAction::None) &&
        vp->isDerivedFrom(ViewProviderGeometryObject::getClassTypeId()) &&
        static_cast<ViewProviderGeometryObject*>(vp)->SelectionStyle.getValue()==1)
    {
        bool selected = type==SoSelectionElementAction::All;
        static_cast<ViewProviderGeometryObject*>(vp)->showBoundingBox(selected);
        if(selected) return false;
    }
    return true;
}

void SoFCUnifiedSelection::GLRenderBelowPath(SoGLRenderAction * action)
{
    inherited::GLRenderBelowPath(action);

    // nothing picked, so restore the arrow cursor if needed
    if (this->preSelection == 0) {
        // this is called when a selection gate forbade to select an object
        // and the user moved the mouse to an empty area
        this->preSelection = -1;
        QtGLWidget* window;
        SoState *state = action->getState();
        SoGLWidgetElement::get(state, window);
        QWidget* parent = window ? window->parentWidget() : 0;
        if (parent) {
            QCursor c = parent->cursor();
            if (c.shape() == Qt::ForbiddenCursor) {
                c.setShape(Qt::ArrowCursor);
                parent->setCursor(c);
            }
        }
    }
}

// ---------------------------------------------------------------

SO_ACTION_SOURCE(SoHighlightElementAction);

void SoHighlightElementAction::initClass()
{
    SO_ACTION_INIT_CLASS(SoHighlightElementAction,SoAction);

    SO_ENABLE(SoHighlightElementAction, SoSwitchElement);

    SO_ACTION_ADD_METHOD(SoNode,nullAction);

    SO_ENABLE(SoHighlightElementAction, SoCoordinateElement);

    SO_ACTION_ADD_METHOD(SoGroup,callDoAction);
    SO_ACTION_ADD_METHOD(SoIndexedLineSet,callDoAction);
    SO_ACTION_ADD_METHOD(SoIndexedFaceSet,callDoAction);
    SO_ACTION_ADD_METHOD(SoPointSet,callDoAction);
}

SoHighlightElementAction::SoHighlightElementAction () : _highlight(false), _det(0)
{
    SO_ACTION_CONSTRUCTOR(SoHighlightElementAction);
}

SoHighlightElementAction::~SoHighlightElementAction()
{
}

void SoHighlightElementAction::beginTraversal(SoNode *node)
{
    traverse(node);
}

void SoHighlightElementAction::callDoAction(SoAction *action,SoNode *node)
{
    node->doAction(action);
}

void SoHighlightElementAction::setHighlighted(SbBool ok)
{
    this->_highlight = ok;
}

SbBool SoHighlightElementAction::isHighlighted() const
{
    return this->_highlight;
}

void SoHighlightElementAction::setColor(const SbColor& c)
{
    this->_color = c;
}

const SbColor& SoHighlightElementAction::getColor() const
{
    return this->_color;
}

void SoHighlightElementAction::setElement(const SoDetail* det)
{
    this->_det = det;
}

const SoDetail* SoHighlightElementAction::getElement() const
{
    return this->_det;
}

// ---------------------------------------------------------------

SO_ACTION_SOURCE(SoSelectionElementAction);

void SoSelectionElementAction::initClass()
{
    SO_ACTION_INIT_CLASS(SoSelectionElementAction,SoAction);

    SO_ENABLE(SoSelectionElementAction, SoSwitchElement);

    SO_ACTION_ADD_METHOD(SoNode,nullAction);

    SO_ENABLE(SoSelectionElementAction, SoCoordinateElement);

    SO_ACTION_ADD_METHOD(SoCoordinate3,callDoAction);
    SO_ACTION_ADD_METHOD(SoGroup,callDoAction);
    SO_ACTION_ADD_METHOD(SoIndexedLineSet,callDoAction);
    SO_ACTION_ADD_METHOD(SoIndexedFaceSet,callDoAction);
    SO_ACTION_ADD_METHOD(SoPointSet,callDoAction);
}

SoSelectionElementAction::SoSelectionElementAction (Type t, bool secondary)
    : _type(t), _det(0), _secondary(secondary)
{
    SO_ACTION_CONSTRUCTOR(SoSelectionElementAction);
}

SoSelectionElementAction::~SoSelectionElementAction()
{
}

void SoSelectionElementAction::beginTraversal(SoNode *node)
{
    traverse(node);
}

void SoSelectionElementAction::callDoAction(SoAction *action,SoNode *node)
{
    node->doAction(action);
}

SoSelectionElementAction::Type
SoSelectionElementAction::getType() const
{
    return this->_type;
}

void SoSelectionElementAction::setColor(const SbColor& c)
{
    this->_color = c;
}

const SbColor& SoSelectionElementAction::getColor() const
{
    return this->_color;
}

void SoSelectionElementAction::setElement(const SoDetail* det)
{
    this->_det = det;
}

const SoDetail* SoSelectionElementAction::getElement() const
{
    return this->_det;
}

// ---------------------------------------------------------------

SO_ACTION_SOURCE(SoVRMLAction);

void SoVRMLAction::initClass()
{
    SO_ACTION_INIT_CLASS(SoVRMLAction,SoAction);

    SO_ENABLE(SoVRMLAction, SoSwitchElement);

    SO_ACTION_ADD_METHOD(SoNode,nullAction);

    SO_ENABLE(SoVRMLAction, SoCoordinateElement);
    SO_ENABLE(SoVRMLAction, SoMaterialBindingElement);
    SO_ENABLE(SoVRMLAction, SoLazyElement);
    SO_ENABLE(SoVRMLAction, SoShapeStyleElement);

    SO_ACTION_ADD_METHOD(SoCoordinate3,callDoAction);
    SO_ACTION_ADD_METHOD(SoMaterialBinding,callDoAction);
    SO_ACTION_ADD_METHOD(SoMaterial,callDoAction);
    SO_ACTION_ADD_METHOD(SoNormalBinding,callDoAction);
    SO_ACTION_ADD_METHOD(SoGroup,callDoAction);
    SO_ACTION_ADD_METHOD(SoIndexedLineSet,callDoAction);
    SO_ACTION_ADD_METHOD(SoIndexedFaceSet,callDoAction);
    SO_ACTION_ADD_METHOD(SoPointSet,callDoAction);
}

SoVRMLAction::SoVRMLAction() : overrideMode(true)
{
    SO_ACTION_CONSTRUCTOR(SoVRMLAction);
}

SoVRMLAction::~SoVRMLAction()
{
}

void SoVRMLAction::setOverrideMode(SbBool on)
{
    overrideMode = on;
}

SbBool SoVRMLAction::isOverrideMode() const
{
    return overrideMode;
}

void SoVRMLAction::callDoAction(SoAction *action, SoNode *node)
{
    if (node->getTypeId().isDerivedFrom(SoNormalBinding::getClassTypeId()) && action->isOfType(SoVRMLAction::getClassTypeId())) {
        SoVRMLAction* vrmlAction = static_cast<SoVRMLAction*>(action);
        if (vrmlAction->overrideMode) {
            SoNormalBinding* bind = static_cast<SoNormalBinding*>(node);
            vrmlAction->bindList.push_back(bind->value.getValue());
            // this normal binding causes some problems for the part view provider
            // See also #0002222: Number of normals in exported VRML is wrong
            if (bind->value.getValue() == static_cast<int>(SoNormalBinding::PER_VERTEX_INDEXED))
                bind->value = SoNormalBinding::OVERALL;
        }
        else if (!vrmlAction->bindList.empty()) {
            static_cast<SoNormalBinding*>(node)->value = static_cast<SoNormalBinding::Binding>(vrmlAction->bindList.front());
            vrmlAction->bindList.pop_front();
        }
    }

    node->doAction(action);
}

// ---------------------------------------------------------------------------------

SoFCSelectionRoot::Stack SoFCSelectionRoot::SelStack;
SoFCSelectionRoot::Stack SoFCSelectionRoot::SelStack2;

SO_NODE_SOURCE(SoFCSelectionRoot);

SoFCSelectionRoot::SoFCSelectionRoot(bool secondary)
    :pushed(false)
    ,secondary(secondary)
{
    SO_NODE_CONSTRUCTOR(SoFCSelectionRoot);
}

SoFCSelectionRoot::~SoFCSelectionRoot()
{
}

void SoFCSelectionRoot::initClass(void)
{
    SO_NODE_INIT_CLASS(SoFCSelectionRoot,SoSeparator,"Separator");
}

void SoFCSelectionRoot::finish()
{
    atexit_cleanup();
}

SoNode *SoFCSelectionRoot::getCurrentRoot(bool front, SoNode *def) {
    if(SelStack.size()) 
        return front?SelStack.front():SelStack.back();
    return def;
}

SoFCSelectionRoot::ContextPtr SoFCSelectionRoot::getContext(SoNode *_node, ContextPtr def, ContextPtr *ctx2)
{
    // NOTE: _node is not necssary of type SoFCSelectionRoot, but it is safe
    // here since we only use it as searching key, although it is probably not
    // a best practice.
    auto node = static_cast<SoFCSelectionRoot*>(_node);

    if(ctx2 && SelStack2.size() && SelStack2.back()->contextMap2.size()) {
        auto &map = SelStack2.back()->contextMap2;
        Stack key;
        key.resize(2);
        key[0] = node;
        for(size_t i=0;i<SelStack2.size();++i) {
            if(i+1<SelStack2.size())
                key[1] = SelStack2[i];
            else 
                key.resize(1);
            auto it = map.find(key);
            if(it == map.end()) continue;
            *ctx2 = it->second;
            break;
        }
    }

    if(SelStack.empty())
        return def;

    SoFCSelectionRoot *front = SelStack.front();
    SelStack.front() = node;
    auto it = front->contextMap.find(SelStack);
    SelStack.front() = front;
    if(it!=front->contextMap.end()) 
        return it->second;
    return ContextPtr();
}

SoFCSelectionRoot::ContextPtr *SoFCSelectionRoot::getContext(
        SoAction *action, SoNode *_node, ContextPtr *pdef) 
{
    // NOTE: _node is not necssary of type SoFCSelectionRoot, but it is safe
    // here since we only use it as searching key. But it is probably not a
    // best practice.
    auto node = static_cast<SoFCSelectionRoot*>(_node);

    const SoFullPath *path = (const SoFullPath*)action->getCurPath();
    bool secondary = false;
    if(action->isOfType(SoSelectionElementAction::getClassTypeId()))
        secondary = static_cast<SoSelectionElementAction*>(action)->isSecondary();
    Stack stack;
    for(int i=0,count=path->getLength();i<count;++i) {
        SoNode *n = path->getNode(i);
        if(n->getTypeId().isDerivedFrom(SoFCSelectionRoot::getClassTypeId())) {
            auto sel = static_cast<SoFCSelectionRoot*>(n);
            if(secondary || !sel->secondary)
                stack.push_back(sel);
        }
    }
    if(stack.empty())
        return pdef;

    if(secondary) {
        Stack key;
        key.reserve(2);
        key.push_back(node);
        if(stack.size()>1)
            key.push_back(stack.front());
        SoFCSelectionRoot *back = stack.back();
        if(pdef)
            return &back->contextMap2[key];
        back->contextMap2.erase(key);
        return 0;
    }

    SoFCSelectionRoot *front = stack.front();
    stack.front() = node;
    if(pdef) 
        return &front->contextMap[stack];
    front->contextMap.erase(stack);
    return 0;
}

#define DEFINE_RENDER(_name) \
void SoFCSelectionRoot::_name(SoGLRenderAction * action) {\
    if(pushed)\
        inherited::_name(action);\
    else {\
        pushed = true;\
        if(!secondary)\
            SelStack.push_back(this);\
        SelStack2.push_back(this);\
        inherited::_name(action);\
        SelStack2.pop_back();\
        if(!secondary)\
            SelStack.pop_back();\
        pushed = false;\
    }\
}

DEFINE_RENDER(GLRender)
DEFINE_RENDER(GLRenderBelowPath)
DEFINE_RENDER(GLRenderInPath)
DEFINE_RENDER(GLRenderOffPath)

void SoFCSelectionRoot::resetContext() {
    contextMap.clear();
}

void SoFCSelectionRoot::doAction(SoAction *action) {
    // The idea here is that if the 'select none' action is applied to a node,
    // and we are the first SoFCSelectionRoot encounted (which means all
    // children stores selection context here), then we can simply perform the
    // action by clearing the selection context here, and save the time for
    // traversing a potentially large amount of children nodes. The time saving
    // is very noticable for large groups.
    if(action->getWhatAppliedTo() == SoAction::NODE && 
       action->isOfType(SoSelectionElementAction::getClassTypeId())) {
        auto selAction = static_cast<SoSelectionElementAction*>(action);
        if(selAction->getType() == SoSelectionElementAction::None &&
           !selAction->isSecondary())
        {
            resetContext();
            touch();
            return;
        }
    }
    inherited::doAction(action);
}

/////////////////////////////////////////////////////////////////////////////

SO_NODE_SOURCE(SoFCPathAnnotation);

SoFCPathAnnotation::SoFCPathAnnotation()
{
    SO_NODE_CONSTRUCTOR(SoFCPathAnnotation);
    path = 0;
}

SoFCPathAnnotation::~SoFCPathAnnotation()
{
    if(path) path->unref();
}

void SoFCPathAnnotation::finish() 
{
    atexit_cleanup();
}

void SoFCPathAnnotation::initClass(void)
{
    SO_NODE_INIT_CLASS(SoFCPathAnnotation,SoSeparator,"Separator");
}

void SoFCPathAnnotation::GLRender(SoGLRenderAction * action)
{
    switch (action->getCurPathCode()) {
    case SoAction::NO_PATH:
    case SoAction::BELOW_PATH:
        this->GLRenderBelowPath(action);
        break;
    case SoAction::OFF_PATH:
        break;
    case SoAction::IN_PATH:
        this->GLRenderInPath(action);
        break;
    }
}

void SoFCPathAnnotation::GLRenderBelowPath(SoGLRenderAction * action)
{
    if(!path)
        return;

    SoState * state = action->getState();
    SoGLCacheContextElement::shouldAutoCache(state, SoGLCacheContextElement::DONT_AUTO_CACHE);

    if (action->isRenderingDelayedPaths()) {
        SbBool zbenabled = glIsEnabled(GL_DEPTH_TEST);
        if (zbenabled) glDisable(GL_DEPTH_TEST);
        inherited::GLRenderInPath(action);
        if (zbenabled) glEnable(GL_DEPTH_TEST);
    }
    else {
        SoCacheElement::invalidate(action->getState());
        auto curPath = action->getCurPath();
        SoPath *newPath = new SoPath(curPath->getLength()+path->getLength());
        newPath->append(curPath);
        newPath->append(path);
        action->addDelayedPath(newPath);
    }
}

void SoFCPathAnnotation::GLRenderInPath(SoGLRenderAction * action)
{
    GLRenderBelowPath(action);
}

void SoFCPathAnnotation::setPath(SoPath *newPath) {
    if(path) {
        path->unref();
        removeAllChildren();
    }
    if(!newPath || !newPath->getLength())
        return;

    path = newPath->copy();
    path->ref();
    addChild(path->getNode(0));
}

