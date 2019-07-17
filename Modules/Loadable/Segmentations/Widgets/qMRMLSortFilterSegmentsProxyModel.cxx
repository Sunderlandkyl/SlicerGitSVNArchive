/*==============================================================================

  Copyright (c) Laboratory for Percutaneous Surgery (PerkLab)
  Queen's University, Kingston, ON, Canada. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Kyle Sunderland, PerkLab, Queen's University
  and was supported through CANARIE's Research Software Program, and Cancer
  Care Ontario.

==============================================================================*/

#include "qMRMLSortFilterSegmentsProxyModel.h"

// MRML include
#include "vtkMRMLSegmentationNode.h"

// Subject Hierarchy includes
#include "qSlicerSubjectHierarchyPluginHandler.h"
#include "qSlicerSubjectHierarchyAbstractPlugin.h"
#include "qMRMLSegmentsModel.h"

// Qt includes
#include <QDebug>
#include <QStandardItem>

// -----------------------------------------------------------------------------
// qMRMLSortFilterSegmentsProxyModelPrivate

// -----------------------------------------------------------------------------
/// \ingroup Slicer_MRMLWidgets
class qMRMLSortFilterSegmentsProxyModelPrivate
{
public:
  qMRMLSortFilterSegmentsProxyModelPrivate();

  QString NameFilter;
  QString AttributeNameFilter;
  QString AttributeValueFilter;
  bool ShowNotStarted;
  bool ShowInProgress;
  bool ShowCompleted;
  bool ShowFlagged;
};

// -----------------------------------------------------------------------------
qMRMLSortFilterSegmentsProxyModelPrivate::qMRMLSortFilterSegmentsProxyModelPrivate()
  : NameFilter(QString())
  , AttributeNameFilter(QString())
  , AttributeValueFilter(QString())
  , ShowNotStarted(false)
  , ShowInProgress(false)
  , ShowCompleted(false)
  , ShowFlagged(false)
{
}

// -----------------------------------------------------------------------------
// qMRMLSortFilterSegmentsProxyModel

// -----------------------------------------------------------------------------
CTK_GET_CPP(qMRMLSortFilterSegmentsProxyModel, QString, nameFilter, NameFilter);
CTK_GET_CPP(qMRMLSortFilterSegmentsProxyModel, QString, attributeNameFilter, AttributeNameFilter);
CTK_GET_CPP(qMRMLSortFilterSegmentsProxyModel, QString, attributeValueFilter, AttributeValueFilter);
CTK_GET_CPP(qMRMLSortFilterSegmentsProxyModel, bool, showNotStarted, ShowNotStarted);
CTK_GET_CPP(qMRMLSortFilterSegmentsProxyModel, bool, showInProgress, ShowInProgress);
CTK_GET_CPP(qMRMLSortFilterSegmentsProxyModel, bool, showCompleted, ShowCompleted);
CTK_GET_CPP(qMRMLSortFilterSegmentsProxyModel, bool, showFlagged, ShowFlagged);

//------------------------------------------------------------------------------
qMRMLSortFilterSegmentsProxyModel::qMRMLSortFilterSegmentsProxyModel(QObject *vparent)
 : QSortFilterProxyModel(vparent)
 , d_ptr(new qMRMLSortFilterSegmentsProxyModelPrivate)
{
  // For speed issue, we might want to disable the dynamic sorting however
  // when having source models using QStandardItemModel, drag&drop is handled
  // in 2 steps, first a new row is created (which automatically calls
  // filterAcceptsRow() that returns false) and then set the row with the
  // correct values (which doesn't call filterAcceptsRow() on the up to date
  // value unless DynamicSortFilter is true).
  this->setDynamicSortFilter(true);
}

//------------------------------------------------------------------------------
qMRMLSortFilterSegmentsProxyModel::~qMRMLSortFilterSegmentsProxyModel()
= default;

//-----------------------------------------------------------------------------
vtkMRMLScene* qMRMLSortFilterSegmentsProxyModel::mrmlScene()const
{
  qMRMLSegmentsModel* model = qobject_cast<qMRMLSegmentsModel*>(this->sourceModel());
  if (!model)
    {
    return nullptr;
    }
  return model->mrmlScene();
}

//-----------------------------------------------------------------------------
vtkMRMLSegmentationNode* qMRMLSortFilterSegmentsProxyModel::subjectHierarchyNode()const
{
  qMRMLSegmentsModel* model = qobject_cast<qMRMLSegmentsModel*>(this->sourceModel());
  if (!model)
    {
    return nullptr;
    }
  return model->segmentationNode();
}

//-----------------------------------------------------------------------------
void qMRMLSortFilterSegmentsProxyModel::setNameFilter(QString filter)
{
  Q_D(qMRMLSortFilterSegmentsProxyModel);
  if (d->NameFilter == filter)
    {
    return;
    }
  d->NameFilter = filter;
  this->invalidateFilter();
}

//-----------------------------------------------------------------------------
void qMRMLSortFilterSegmentsProxyModel::setAttributeNameFilter(QString filter)
{
  Q_D(qMRMLSortFilterSegmentsProxyModel);
  if (d->AttributeNameFilter == filter)
    {
    return;
    }
  d->AttributeNameFilter = filter;
  this->invalidateFilter();
}

//-----------------------------------------------------------------------------
void qMRMLSortFilterSegmentsProxyModel::setAttributeValueFilter(QString filter)
{
  Q_D(qMRMLSortFilterSegmentsProxyModel);
  if (d->AttributeValueFilter == filter)
    {
    return;
    }
  d->AttributeValueFilter = filter;
  this->invalidateFilter();
}

//-----------------------------------------------------------------------------
void qMRMLSortFilterSegmentsProxyModel::setShowNotStarted(bool show)
{
  Q_D(qMRMLSortFilterSegmentsProxyModel);
  if (d->ShowNotStarted == show)
  {
    return;
  }
  d->ShowNotStarted = show;
  this->invalidateFilter();
}

//-----------------------------------------------------------------------------
void qMRMLSortFilterSegmentsProxyModel::setShowInProgress(bool show)
{
  Q_D(qMRMLSortFilterSegmentsProxyModel);
  if (d->ShowInProgress == show)
  {
    return;
  }
  d->ShowInProgress = show;
  this->invalidateFilter();
}

//-----------------------------------------------------------------------------
void qMRMLSortFilterSegmentsProxyModel::setShowCompleted(bool show)
{
  Q_D(qMRMLSortFilterSegmentsProxyModel);
  if (d->ShowCompleted == show)
  {
    return;
  }
  d->ShowCompleted = show;
  this->invalidateFilter();
}

//-----------------------------------------------------------------------------
void qMRMLSortFilterSegmentsProxyModel::setShowFlagged(bool show)
{
  Q_D(qMRMLSortFilterSegmentsProxyModel);
  if (d->ShowFlagged == show)
  {
    return;
  }
  d->ShowFlagged = show;
  this->invalidateFilter();
}

//-----------------------------------------------------------------------------
std::string qMRMLSortFilterSegmentsProxyModel::segmentIDFromIndex(const QModelIndex& index)const
{
  qMRMLSegmentsModel* sceneModel = qobject_cast<qMRMLSegmentsModel*>(this->sourceModel());
  return sceneModel->segmentIDFromIndex( this->mapToSource(index) );
}

//-----------------------------------------------------------------------------
QModelIndex qMRMLSortFilterSegmentsProxyModel::indexFromSegmentID(std::string itemID, int column)const
{
  qMRMLSegmentsModel* sceneModel = qobject_cast<qMRMLSegmentsModel*>(this->sourceModel());
  return this->mapFromSource(sceneModel->indexFromSegmentID(itemID, column));
}

//-----------------------------------------------------------------------------
QStandardItem* qMRMLSortFilterSegmentsProxyModel::sourceItem(const QModelIndex& sourceIndex)const
{
  qMRMLSegmentsModel* model = qobject_cast<qMRMLSegmentsModel*>(this->sourceModel());
  if (!model)
    {
    return nullptr;
    }
  return sourceIndex.isValid() ? model->itemFromIndex(sourceIndex) : model->invisibleRootItem();
}

//------------------------------------------------------------------------------
bool qMRMLSortFilterSegmentsProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent)const
{
  QStandardItem* parentItem = this->sourceItem(sourceParent);
  if (!parentItem)
    {
    return false;
    }
  QStandardItem* item = nullptr;

  // Sometimes the row is not complete (DnD), search for a non null item
  for (int childIndex=0; childIndex < parentItem->columnCount(); ++childIndex)
    {
    item = parentItem->child(sourceRow, childIndex);
    if (item)
      {
      break;
      }
    }
  if (item == nullptr)
    {
    return false;
    }

  qMRMLSegmentsModel* model = qobject_cast<qMRMLSegmentsModel*>(this->sourceModel());
  std::string segmentID = model->segmentIDFromItem(item);
  return this->filterAcceptsItem(segmentID);
}

//------------------------------------------------------------------------------
bool qMRMLSortFilterSegmentsProxyModel::filterAcceptsItem(std::string segmentID)const
{
  Q_D(const qMRMLSortFilterSegmentsProxyModel);

  qMRMLSegmentsModel* model = qobject_cast<qMRMLSegmentsModel*>(this->sourceModel());

  vtkMRMLSegmentationNode* segmentationNode = model->segmentationNode();
  vtkSegmentation* segmentation;
  if (segmentationNode)
    {
    segmentation = segmentationNode->GetSegmentation();
    }
  vtkSegment* segment;
  if (segmentation)
    {
    segment = segmentation->GetSegment(segmentID);
    }

  if (!segment)
    {
    qCritical() << Q_FUNC_INFO << "Invalid segment";
    return false;
    }

  // Filter by item name
  if (!d->NameFilter.isEmpty())
    {
    QString segmentName(segment->GetName());
    if (!segmentName.contains(d->NameFilter, Qt::CaseInsensitive))
      {
        return false;
      }
    }

  if (d->ShowNotStarted || d->ShowInProgress || d->ShowCompleted || d->ShowFlagged)
    {
    int status = model->getStatus(segment);
    switch (status)
      {
      case qMRMLSegmentsModel::NotStarted:
        if (!d->ShowNotStarted)
          {
          return false;
          }
        break;
      case qMRMLSegmentsModel::InProgress:
        if (!d->ShowInProgress)
          {
          return false;
          }
        break;
      case qMRMLSegmentsModel::Completed:
        if (!d->ShowCompleted)
          {
          return false;
          }
        break;
      case qMRMLSegmentsModel::Flagged:
        if (!d->ShowFlagged)
          {
          return false;
          }
        break;
      }
    }

  // All criteria were met
  return true;
}

//------------------------------------------------------------------------------
Qt::ItemFlags qMRMLSortFilterSegmentsProxyModel::flags(const QModelIndex & index)const
{
  std::string segmentID = this->segmentIDFromIndex(index);
  bool isSelectable = this->filterAcceptsItem(segmentID);
  qMRMLSegmentsModel* sceneModel = qobject_cast<qMRMLSegmentsModel*>(this->sourceModel());
  QStandardItem* item = sceneModel->itemFromSegmentID(segmentID, index.column());
  if (!item)
    {
    return Qt::ItemFlags();
    }

  QFlags<Qt::ItemFlag> flags = item->flags();
  if (isSelectable)
    {
    return flags | Qt::ItemIsSelectable;
    }
  else
    {
    return flags & ~Qt::ItemIsSelectable;
    }
}
