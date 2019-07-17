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

// Qt includes
#include <QApplication>
#include <QColor>
#include <QDebug>
#include <QMessageBox>
#include <QMimeData>
#include <QTimer>

// qMRML includes
#include "qMRMLSegmentsModel_p.h"

// SlicerQt includes
#include <qSlicerApplication.h>
#include <qSlicerCoreApplication.h>
#include <qSlicerModuleManager.h>
#include <qSlicerAbstractCoreModule.h>

// MRML includes
#include <vtkMRMLSegmentationDisplayNode.h>
#include <vtkMRMLSegmentationNode.h>

// Terminology includes
#include "qSlicerTerminologyItemDelegate.h"

// Polyseg includes
#include "vtkSegment.h"

// Segmentations includes
#include "qMRMLSegmentsTableView.h"

const std::string STATUS_TAG_NAME = "Segmentation.Status";

//------------------------------------------------------------------------------
qMRMLSegmentsModelPrivate::qMRMLSegmentsModelPrivate(qMRMLSegmentsModel& object)
  : q_ptr(&object)
  , NameColumn(-1)
  , VisibilityColumn(-1)
  , ColorColumn(-1)
  , OpacityColumn(-1)
  , StatusColumn(-1)
  , SegmentationNode(nullptr)
  , MRMLScene(nullptr)
{
  this->CallBack = vtkSmartPointer<vtkCallbackCommand>::New();
  this->PendingItemModified = -1; // -1 means not updating

  this->HiddenIcon = QIcon(":Icons/VisibleOff.png");
  this->VisibleIcon = QIcon(":Icons/VisibleOn.png");

  this->NotStartedIcon = QIcon(":Icons/Dot.png");
  this->InProgressIcon = QIcon(":Icons/Edit.png");
  this->FlaggedIcon = QIcon(":Icons/Flag.png");
  this->CompletedIcon = QIcon(":Icons/Present.png");

  qRegisterMetaType<QStandardItem*>("QStandardItem*");
}

//------------------------------------------------------------------------------
qMRMLSegmentsModelPrivate::~qMRMLSegmentsModelPrivate()
{
  if (this->SegmentationNode)
    {
    this->SegmentationNode->RemoveObserver(this->CallBack);
    }
  if (this->MRMLScene)
    {
    this->MRMLScene->RemoveObserver(this->CallBack);
    }
}

//------------------------------------------------------------------------------
void qMRMLSegmentsModelPrivate::init()
{
  Q_Q(qMRMLSegmentsModel);
  this->CallBack->SetClientData(q);
  this->CallBack->SetCallback(qMRMLSegmentsModel::onEvent);

  QObject::connect(q, SIGNAL(itemChanged(QStandardItem*)), q, SLOT(onItemChanged(QStandardItem*)));

  q->setVisibilityColumn(0);
  q->setColorColumn(1);
  q->setOpacityColumn(2);
  q->setNameColumn(3);
  q->setStatusColumn(4);

  q->setHorizontalHeaderLabels(
    QStringList() << "" /*Visibility*/ << "" /*Color*/ << "Opacity" << "Name" << "" /*"Status"*/ );

  q->horizontalHeaderItem(q->nameColumn())->setToolTip(qMRMLSegmentsModel::tr("Segment name")); //TODO
  q->horizontalHeaderItem(q->visibilityColumn())->setToolTip(qMRMLSegmentsModel::tr("Segment visibility")); //TODO
  q->horizontalHeaderItem(q->colorColumn())->setToolTip(qMRMLSegmentsModel::tr("Segment color"));
  q->horizontalHeaderItem(q->opacityColumn())->setToolTip(qMRMLSegmentsModel::tr("Segment opacity (all views)"));
  q->horizontalHeaderItem(q->statusColumn())->setToolTip(qMRMLSegmentsModel::tr("Segment status"));

  q->horizontalHeaderItem(q->visibilityColumn())->setIcon(QIcon(":/Icons/Small/SlicerVisibleInvisible.png"));
  q->horizontalHeaderItem(q->colorColumn())->setIcon(QIcon(":/Icons/Colors.png"));
  q->horizontalHeaderItem(q->statusColumn())->setIcon(QIcon(":/Icons/Flag.png"));
}

//------------------------------------------------------------------------------
QStandardItem* qMRMLSegmentsModelPrivate::insertSegment(std::string segmentID, int index)
{
  Q_Q(qMRMLSegmentsModel);
  QStandardItem* item = q->itemFromSegmentID(segmentID);
  if (item)
  {
    // It is possible that the item has been already added if it is the parent of a child item already inserted
    return item;
  }

  item = q->insertSegment(segmentID, index);// TODO
  if (q->itemFromSegmentID(segmentID) != item)
  {
    qCritical() << Q_FUNC_INFO << ": Item mismatch when inserting segment item with ID " << segmentID.c_str();
    return nullptr;
  }
  return item;
}

//------------------------------------------------------------------------------
QString qMRMLSegmentsModelPrivate::getTerminologyUserDataForSegment(vtkSegment* segment)
{
  if (!segment)
  {
    qCritical() << Q_FUNC_INFO << ": Invalid segment given";
    return QString();
  }

  std::string tagValue;
  return (segment->GetTag(vtkSegment::GetTerminologyEntryTagName(), tagValue) ? QString(tagValue.c_str()) : QString());
}


//------------------------------------------------------------------------------
// qMRMLSegmentsModel
//------------------------------------------------------------------------------
qMRMLSegmentsModel::qMRMLSegmentsModel(QObject *_parent)
  :QStandardItemModel(_parent)
  , d_ptr(new qMRMLSegmentsModelPrivate(*this))
{
  Q_D(qMRMLSegmentsModel);
  d->init();
}

//------------------------------------------------------------------------------
qMRMLSegmentsModel::qMRMLSegmentsModel(qMRMLSegmentsModelPrivate* pimpl, QObject* parent)
  : QStandardItemModel(parent)
  , d_ptr(pimpl)
{
  Q_D(qMRMLSegmentsModel);
  d->init();
}

//------------------------------------------------------------------------------
qMRMLSegmentsModel::~qMRMLSegmentsModel()
= default;

//------------------------------------------------------------------------------
void qMRMLSegmentsModel::setMRMLScene(vtkMRMLScene* scene)
{
  Q_D(qMRMLSegmentsModel);
  if (scene == d->MRMLScene)
    {
    return;
    }

  if (d->MRMLScene)
    {
    d->MRMLScene->RemoveObserver(d->CallBack);
    }

  d->MRMLScene = scene;
  if (scene)
    {
    scene->AddObserver(vtkMRMLScene::EndCloseEvent, d->CallBack);
    scene->AddObserver(vtkMRMLScene::EndImportEvent, d->CallBack);
    scene->AddObserver(vtkMRMLScene::StartBatchProcessEvent, d->CallBack);
    scene->AddObserver(vtkMRMLScene::EndBatchProcessEvent, d->CallBack);
    scene->AddObserver(vtkMRMLScene::NodeRemovedEvent, d->CallBack);
    }
}

//------------------------------------------------------------------------------
vtkMRMLScene* qMRMLSegmentsModel::mrmlScene()const
{
  Q_D(const qMRMLSegmentsModel);
  return d->MRMLScene;
}

//------------------------------------------------------------------------------
void qMRMLSegmentsModel::setSegmentationNode(vtkMRMLSegmentationNode* segmentationNode)
{
  Q_D(qMRMLSegmentsModel);
  if (segmentationNode == d->SegmentationNode)
    {
    return;
    }

  if (d->SegmentationNode)
    {
    d->SegmentationNode->RemoveObserver(d->CallBack);
    }
  d->SegmentationNode = segmentationNode;

  // Remove all items
  const int oldColumnCount = this->columnCount();
  this->removeRows(0, this->rowCount());
  this->setColumnCount(oldColumnCount);

  // Update whole subject hierarchy
  this->updateFromSegments();

  if (segmentationNode && segmentationNode->GetSegmentation())
    {
    // Using priority value of -10 in certain observations results in those callbacks being called after
    // those with neutral priorities. Useful to have the plugin handler deal with new items before allowing
    // them to be handled by the model.
    // Same idea for +10, in which case the callback is called first.
    segmentationNode->AddObserver(vtkSegmentation::SegmentAdded, d->CallBack);
    segmentationNode->AddObserver(vtkSegmentation::SegmentRemoved, d->CallBack);
    segmentationNode->AddObserver(vtkSegmentation::SegmentModified, d->CallBack);
    segmentationNode->AddObserver(vtkSegmentation::MasterRepresentationModified, d->CallBack);
    }
}

//------------------------------------------------------------------------------
vtkMRMLSegmentationNode* qMRMLSegmentsModel::segmentationNode()const
{
  Q_D(const qMRMLSegmentsModel);
  return d->SegmentationNode;
}

// -----------------------------------------------------------------------------
std::string qMRMLSegmentsModel::getStatusTagName()
{
  return STATUS_TAG_NAME;
}

// -----------------------------------------------------------------------------
std::string qMRMLSegmentsModel::segmentIDFromIndex(const QModelIndex &index)const
{
  return this->segmentIDFromItem(this->itemFromIndex(index));
}

//------------------------------------------------------------------------------
std::string qMRMLSegmentsModel::segmentIDFromItem(QStandardItem* item)const
{
  Q_D(const qMRMLSegmentsModel);
  if (!d->SegmentationNode || !item)
    {
    return ""; //TODO
    }
  QVariant segmentID = item->data(qMRMLSegmentsModel::SegmentIDRole);
  if (!segmentID.isValid())
    {
    return ""; //TODO
    }
  return item->data(qMRMLSegmentsModel::SegmentIDRole).toString().toStdString();
}
//------------------------------------------------------------------------------
QStandardItem* qMRMLSegmentsModel::itemFromSegmentID(std::string segmentID, int column/*=0*/)const
{
  QModelIndex index = this->indexFromSegmentID(segmentID, column);
  QStandardItem* item = this->itemFromIndex(index);
  return item;
}

//------------------------------------------------------------------------------
QModelIndex qMRMLSegmentsModel::indexFromSegmentID(std::string segmentID, int column/*=0*/)const
{
  Q_D(const qMRMLSegmentsModel);

  QModelIndex itemIndex;
  if (segmentID.empty())
    {
    return itemIndex;
    }

  // Try to find the nodeIndex in the cache first // TODO
  //QMap<vtkIdType,QPersistentModelIndex>::iterator rowCacheIt = d->RowCache.find(itemID);
  //if (rowCacheIt==d->RowCache.end())
  //  {
  //  // Not found in cache, therefore it cannot be in the model
  //  return itemIndex;
  //  }
  //if (rowCacheIt.value().isValid())
  //  {
  //  // An entry found in the cache. If the item at the cached index matches the requested item ID then we use it.
  //  QStandardItem* item = this->itemFromIndex(rowCacheIt.value());
  //  if (item && item->data(qMRMLSegmentsModel::SegmentIDRole).toLongLong() == itemID)
  //    {
  //    // ID matched
  //    itemIndex = rowCacheIt.value();
  //    }
  //  }

  // The cache was not up-to-date. Do a slow linear search.
  if (!itemIndex.isValid())
    {
    QModelIndex startIndex = this->index(0, 0);
    // QAbstractItemModel::match doesn't browse through columns, we need to do it manually
    QModelIndexList itemIndexes = this->match(
      startIndex, SegmentIDRole, segmentID.c_str(), 1, Qt::MatchExactly | Qt::MatchRecursive);
    if (itemIndexes.size() == 0)
      {
      //d->RowCache.remove(itemID);
      return QModelIndex();
      }
    itemIndex = itemIndexes[0];
    //d->RowCache[itemID] = itemIndex; //TODO
    }

  if (column == 0)
    {
    // QAbstractItemModel::match only search through the first column
    return itemIndex;
    }

  // Add the QModelIndexes from the other columns
  const int row = itemIndex.row();
  QModelIndex nodeParentIndex = itemIndex.parent();
  if (column >= this->columnCount(nodeParentIndex))
    {
    qCritical() << Q_FUNC_INFO << ": Invalid column " << column;
    return QModelIndex();
    }

  return this->index(row, column, itemIndex.parent());;
}

//------------------------------------------------------------------------------
QModelIndexList qMRMLSegmentsModel::indexes(std::string segmentID) const
{
  QModelIndex startIndex = this->index(0, 0);
  // QAbstractItemModel::match doesn't browse through columns, we need to do it manually
  QModelIndexList itemIndexes = this->match(
    startIndex, SegmentIDRole, segmentID.c_str(), 1, Qt::MatchExactly | Qt::MatchRecursive);
  if (itemIndexes.size() != 1)
    {
    return QModelIndexList(); // If 0 it's empty, if >1 it's invalid (one item for each UID)
    }
  // Add the QModelIndexes from the other columns
  const int row = itemIndexes[0].row();
  QModelIndex itemParentIndex = itemIndexes[0].parent();
  for (int col = 1; col < this->columnCount(); ++col)
  {
    itemIndexes << this->index(row, col);
  }
  return itemIndexes;
}

//------------------------------------------------------------------------------
void qMRMLSegmentsModel::updateFromSegments()
{
  Q_D(qMRMLSegmentsModel);

  //d->RowCache.clear(); TODO

  // Enabled so it can be interacted with
  this->invisibleRootItem()->setFlags(Qt::ItemIsEnabled);

  if (!d->SegmentationNode)
    {
    // Remove all items
    const int oldColumnCount = this->columnCount();
    this->removeRows(0, this->rowCount());
    this->setColumnCount(oldColumnCount);
    return;
    }

  // Remove rows before populating
  this->invisibleRootItem()->removeRows(0, this->rowCount());

  // Populate subject hierarchy with the items
  std::vector<std::string> segmentIDs;
  d->SegmentationNode->GetSegmentation()->GetSegmentIDs(segmentIDs);
  for (std::string segmentID : segmentIDs)
    {
    this->insertSegment(segmentID);
    }

  //// Update expanded states (during inserting the update calls did not find valid indices, so
  //// expand and collapse statuses were not set in the tree view)
  //for (std::vector<vtkIdType>::iterator itemIt=allItemIDs.begin(); itemIt!=allItemIDs.end(); ++itemIt)
  //  {
  //  std::string segmentID = (*itemIt);
  //  // Expanded states are handled with the name column
  //  QStandardItem* item = this->itemFromSegmentID(itemID, this->nameColumn());
  //  this->updateItemDataFromSegment(item, itemID, this->nameColumn());
  //  }

  //emit segmentsUpdated();
}

//------------------------------------------------------------------------------
QStandardItem* qMRMLSegmentsModel::insertSegment(std::string segmentID, int row/*=-1*/ )
{
  Q_D(qMRMLSegmentsModel);

  QList<QStandardItem*> items;
  for (int col=0; col<this->columnCount(); ++col)
    {
    QStandardItem* newItem = new QStandardItem();
    this->updateItemFromSegment(newItem, segmentID, col);
    items.append(newItem);
    }

  if (row == -1)
  {
    row = d->SegmentationNode->GetSegmentation()->GetSegmentIndex(segmentID);
  }

  // Insert an invalid item in the cache to indicate that the subject hierarchy item is in the
  // model but we don't know its index yet. This is needed because a custom widget may be notified
  // abot row insertion before insertRow() returns (and the RowCache entry is added).
  //d->RowCache[itemID] = QModelIndex(); // TODO
  this->invisibleRootItem()->insertRow(row, items);
  //d->RowCache[itemID] = items[0]->index();

  return items[0];
}

//------------------------------------------------------------------------------
Qt::ItemFlags qMRMLSegmentsModel::segmentFlags(std::string segmentID, int column)const
{
  Q_D(const qMRMLSegmentsModel);

  Qt::ItemFlags flags;
  flags.setFlag(Qt::ItemIsEnabled);
  flags.setFlag(Qt::ItemIsSelectable);

  if (!d->SegmentationNode)
    {
    qCritical() << Q_FUNC_INFO << ": Invalid segmentation node";
    return flags;
    }

  if (column != this->visibilityColumn() && column != this->statusColumn())
    {
    flags.setFlag(Qt::ItemIsEditable);
    }

  return flags;
}

//------------------------------------------------------------------------------
void qMRMLSegmentsModel::updateItemFromSegment(QStandardItem* item, std::string segmentID, int column)
{
  Q_D(qMRMLSegmentsModel);
  // We are going to make potentially multiple changes to the item. We want to refresh
  // the subject hierarchy item only once, so we "block" the updates in onItemChanged().
  d->PendingItemModified = 0;

  Qt::ItemFlags flags = this->segmentFlags(segmentID, column);
  item->setFlags(flags);

  // TODO
  //// Set ID
  bool blocked = this->blockSignals(true);
  item->setData(segmentID.c_str(), qMRMLSegmentsModel::SegmentIDRole);
  this->blockSignals(blocked);

  //// Update item data for the current column
  this->updateItemDataFromSegment(item, segmentID, column);

  bool itemChanged = (d->PendingItemModified > 0);
  d->PendingItemModified = -1;

  if (itemChanged)
    {
    this->onItemChanged(item);
    }
}

//------------------------------------------------------------------------------
void qMRMLSegmentsModel::updateItemDataFromSegment(QStandardItem* item, std::string segmentID, int column)
{
  Q_D(qMRMLSegmentsModel);
  if (!d->SegmentationNode)
    {
    qCritical() << Q_FUNC_INFO << ": Invalid segmentation node";
    return;
    }

  vtkSegmentation* segmentation = d->SegmentationNode->GetSegmentation();
  if (!segmentation)
    {
    qCritical() << Q_FUNC_INFO << ": Invalid segmentation";
    return;
    }

  vtkSegment* segment = segmentation->GetSegment(segmentID);
  if (!segment)
    {
    qCritical() << Q_FUNC_INFO << ": Invalid segment";
    }

  if (column == this->nameColumn())
    {
    item->setText(segment->GetName());
    }
  else if (column == this->statusColumn())
    {
    int status = this->getStatus(segment);
    QIcon statusIcon = d->NotStartedIcon;
    QString statusTooltip = "";
    switch (status)
      {
      case InProgress:
        statusIcon = d->InProgressIcon;
        statusTooltip = "In progress";
        break;
      case Completed:
        statusIcon = d->CompletedIcon;
        statusTooltip = "Completed";
        break;
      case Flagged:
        statusIcon = d->FlaggedIcon;
        statusTooltip = "Flagged";
        break;
      }
      item->setIcon(statusIcon);
      item->setToolTip(statusTooltip);
      item->setData(status, StatusRole);
    }
  else
    {
    // Get segment display properties
    vtkMRMLSegmentationDisplayNode* displayNode = vtkMRMLSegmentationDisplayNode::SafeDownCast(d->SegmentationNode->GetDisplayNode());
    if (!displayNode)
      {
      qCritical() << Q_FUNC_INFO << ": Invalid segmentation display node";
      }

    vtkMRMLSegmentationDisplayNode::SegmentDisplayProperties properties;
    displayNode->GetSegmentDisplayProperties(segmentID, properties);

    if (column == this->colorColumn())
      {
      // Set terminology information from segment to item
      item->setData(segment->GetName(), qSlicerTerminologyItemDelegate::NameRole);
      item->setData(segment->GetNameAutoGenerated(), qSlicerTerminologyItemDelegate::NameAutoGeneratedRole);
      item->setData(segment->GetColorAutoGenerated(), qSlicerTerminologyItemDelegate::ColorAutoGeneratedRole);
      QString segmentTerminologyTagValue(d->getTerminologyUserDataForSegment(segment));
      if (segmentTerminologyTagValue != item->data(qSlicerTerminologyItemDelegate::TerminologyRole).toString())
        {
        item->setData(segmentTerminologyTagValue, qSlicerTerminologyItemDelegate::TerminologyRole);
        item->setToolTip(qMRMLSegmentsTableView::terminologyTooltipForSegment(segment));
        }
      // Set color
      double* colorArray = segment->GetColor();
      QColor color = QColor::fromRgbF(colorArray[0], colorArray[1], colorArray[2]);
      item->setData(color, Qt::DecorationRole);
      }
    else if (column == this->visibilityColumn())
      {
      // Have owner plugin give the visibility state and icon
      bool visible = properties.Visible && (properties.Visible3D || properties.Visible2DFill || properties.Visible2DOutline);
      QIcon visibilityIcon = d->HiddenIcon;
      if (visible)
        {
        visibilityIcon = d->VisibleIcon;
        }
      // It should be fine to set the icon even if it is the same, but due
      // to a bug in Qt (http://bugreports.qt.nokia.com/browse/QTBUG-20248),
      // it would fire a superfluous itemChanged() signal.
      if (item->data(VisibilityRole).isNull()
        || item->data(VisibilityRole).toInt() != visible)
        {
        item->setData(visible, VisibilityRole);
        item->setIcon(visibilityIcon);
        }
      }
    else if (column == this->opacityColumn())
      {
      QString displayedOpacityStr = QString::number(properties.Opacity3D, 'f', 2);
      item->setData(displayedOpacityStr, Qt::EditRole);
      }
    }
}

//------------------------------------------------------------------------------
int qMRMLSegmentsModel::getStatus(vtkSegment* segment)
{
  std::string value;
  if (!segment->GetTag(STATUS_TAG_NAME, value))
    {
    return NotStarted;
    }
  return QVariant(value.c_str()).toInt();
}

//------------------------------------------------------------------------------
void qMRMLSegmentsModel::updateSegmentFromItem(std::string segmentID, QStandardItem* item)
{
  Q_D(qMRMLSegmentsModel);
  //int wasModifying = d->SegmentationNode->StartModify(); //TODO: Add feature to item if there are performance issues
  // Can't call StartModfiy/EndModify currently, since SegmentID will be lost (likely because the calldata is a std::string::c_str).
  this->updateSegmentFromItemData(segmentID, item);
  //d->SegmentationNode->EndModify(wasModifying);
}

//------------------------------------------------------------------------------
void qMRMLSegmentsModel::updateSegmentFromItemData(std::string segmentID, QStandardItem* item)
{
  Q_D(qMRMLSegmentsModel);
  if (!d->SegmentationNode)
    {
    qCritical() << Q_FUNC_INFO << ": Invalid subject hierarchy";
    return;
    }

  // Name column
  if (item->column() == this->nameColumn())
    {
    vtkSegment* segment = d->SegmentationNode->GetSegmentation()->GetSegment(segmentID);
    if (!segment)
      {
      qCritical() << Q_FUNC_INFO << ": Segment with ID '" << segmentID.c_str() << "' not found in segmentation node " << d->SegmentationNode->GetName();
      return;
      }
    std::string name = item->text().toStdString();
    segment->SetName(name.c_str());
    }
  else if (item->column() == this->statusColumn())
    {
    vtkSegment* segment = d->SegmentationNode->GetSegmentation()->GetSegment(segmentID);
    if (!segment)
      {
      qCritical() << Q_FUNC_INFO << ": Segment with ID '" << segmentID.c_str() << "' not found in segmentation node " << d->SegmentationNode->GetName();
      return;
      }
    std::string status = item->data(StatusRole).toString().toStdString();
    segment->SetTag(STATUS_TAG_NAME, status);
    }
  else
    {
    // For all other columns we need the display node
    vtkMRMLSegmentationDisplayNode* displayNode = vtkMRMLSegmentationDisplayNode::SafeDownCast(
      d->SegmentationNode->GetDisplayNode());
    if (!displayNode)
      {
      qCritical() << Q_FUNC_INFO << ": No display node for segmentation!";
      return;
      }
    // Get display properties
    bool displayPropertyChanged = false;
    vtkMRMLSegmentationDisplayNode::SegmentDisplayProperties properties;
    displayNode->GetSegmentDisplayProperties(segmentID, properties);

    // Visibility column
    if (item->column() == this->visibilityColumn() && !item->data(VisibilityRole).isNull())
      {
      vtkMRMLSegmentationDisplayNode* displayNode = vtkMRMLSegmentationDisplayNode::SafeDownCast(d->SegmentationNode->GetDisplayNode());
      bool visible = item->data(VisibilityRole).toBool();
      displayNode->SetSegmentVisibility(segmentID, visible);
      }
    // Color column
    else if (item->column() == this->colorColumn())
      {
      vtkSegment* segment = d->SegmentationNode->GetSegmentation()->GetSegment(segmentID);
      if (!segment)
        {
        qCritical() << Q_FUNC_INFO << ": Segment with ID '" << segmentID.c_str() << "' not found in segmentation node " << d->SegmentationNode->GetName();
        return;
        }

      // Set terminology information to segment as tag
      QString terminologyString = item->data(qSlicerTerminologyItemDelegate::TerminologyRole).toString();
      segment->SetTag(vtkSegment::GetTerminologyEntryTagName(), terminologyString.toLatin1().constData());

      // Set color to segment if it changed
      QColor color = item->data(Qt::DecorationRole).value<QColor>();
      double* oldColorArray = segment->GetColor();
      QColor oldColor = QColor::fromRgbF(oldColorArray[0], oldColorArray[1], oldColorArray[2]);
      if (oldColor != color)
        {
        segment->SetColor(color.redF(), color.greenF(), color.blueF());
        }
      // Set color auto-generated flag
      segment->SetColorAutoGenerated(
        item->data(qSlicerTerminologyItemDelegate::ColorAutoGeneratedRole).toBool());

      // Set name if it changed
      QString nameFromColorItem = item->data(qSlicerTerminologyItemDelegate::NameRole).toString();
      if (nameFromColorItem.compare(segment->GetName()))
        {
        //emit segmentAboutToBeModified(segmentId);
        segment->SetName(nameFromColorItem.toLatin1().constData());
        }
      // Set name auto-generated flag
      segment->SetNameAutoGenerated(
        item->data(qSlicerTerminologyItemDelegate::NameAutoGeneratedRole).toBool());

      // Update tooltip
      item->setToolTip(qMRMLSegmentsTableView::terminologyTooltipForSegment(segment));
      }
    // Opacity changed
    else if (item->column() == this->opacityColumn())
      {
      QString opacity = item->data(Qt::EditRole).toString();
      QString currentOpacity = QString::number(properties.Opacity3D, 'f', 2);
      if (opacity != currentOpacity)
        {
        // Set to all kinds of opacities as they are combined on the UI
        properties.Opacity3D = opacity.toDouble();
        displayPropertyChanged = true;
        }
      }
    // Set changed properties to segmentation display node if a value has actually changed
    if (displayPropertyChanged)
      {
      displayNode->SetSegmentDisplayProperties(segmentID, properties);
      }
    }
}

//------------------------------------------------------------------------------
void qMRMLSegmentsModel::updateModelItems(std::string segmentID)
{
  Q_D(qMRMLSegmentsModel);

  QModelIndexList itemIndexes = this->indexes(segmentID);
  if (!itemIndexes.count())
    {
    // Can happen while the item is added, the plugin handler sets the owner plugin, which triggers
    // item modified before it can be inserted to the model
    return;
    }

  for (int currentIndex=0; currentIndex<itemIndexes.size(); ++currentIndex)
    {
    // Note: If this loop is changed to foreach update after reparenting stops working.
    //   Apparently foreach makes a deep copy of itemIndexes, and as the indices change after the
    //   first reparenting in the updateItemFromSegment for column 0, the old indices
    //   are being used for the subsequent columns, which yield new items, and reparenting is
    //   performed on those too. With regular for and [] operator, the updated indices are got,
    //   so reparenting is only performed once, which is the desired behavior.
    QModelIndex index = itemIndexes[currentIndex];
    QStandardItem* item = this->itemFromIndex(index);
    int oldRow = item->row();
    QStandardItem* oldParent = item->parent();

    this->updateItemFromSegment(item, segmentID, item->column());
    }
}

//-----------------------------------------------------------------------------
void qMRMLSegmentsModel::onEvent(
  vtkObject* caller, unsigned long event, void* clientData, void* callData )
{
  vtkMRMLSegmentationNode* segmentationNode = reinterpret_cast<vtkMRMLSegmentationNode*>(caller);
  qMRMLSegmentsModel* model = reinterpret_cast<qMRMLSegmentsModel*>(clientData);
  if (!model || !segmentationNode)
    {
    qCritical() << Q_FUNC_INFO << ": Invalid event parameters";
    return;
    }

  // Get item ID for subject hierarchy node events
  std::string segmentID = ""; // TODO
  if (callData)
    {
    const char* segmentIDPtr = reinterpret_cast<const char*>(callData);
    if (segmentIDPtr)
      {
      segmentID = segmentIDPtr;
      }
    }

  switch (event)
    {
    case vtkSegmentation::SegmentAdded:
      model->onSegmentAdded(segmentID);
      break;
    case vtkSegmentation::SegmentRemoved:
      model->onSegmentRemoved(segmentID);
      break;
    case vtkSegmentation::SegmentModified:
      model->onSegmentModified(segmentID);
      break;
    }
}

//------------------------------------------------------------------------------
void qMRMLSegmentsModel::onSegmentAdded(std::string segmentID)
{
  this->insertSegment(segmentID);
}

//------------------------------------------------------------------------------
void qMRMLSegmentsModel::onSegmentRemoved(std::string removedSegmentID)
{
  Q_D(qMRMLSegmentsModel);
  Q_UNUSED(removedSegmentID);

  QModelIndex index = this->indexFromSegmentID(removedSegmentID);
  this->removeRow(index.row());
}

//------------------------------------------------------------------------------
void qMRMLSegmentsModel::onSegmentModified(std::string segmentID)
{
  this->updateModelItems(segmentID);
}

//------------------------------------------------------------------------------
void qMRMLSegmentsModel::onItemChanged(QStandardItem* item)
{
  Q_D(qMRMLSegmentsModel);
  if (d->PendingItemModified >= 0)
    {
    ++d->PendingItemModified;
    return;
    }
  //// When a drag&drop occurs, the order of the items called with onItemChanged is
  //// random, it could be the item in column 1 then the item in column 0
  //if (d->DraggedSegmentsItems.count())
  //  {
  //  // Item changed will be triggered multiple times in course of the drag&drop event. Setting this flag
  //  // makes sure the final onItemChanged with the collected DraggedSegmentsItems is called only once.
  //  if (!d->DelayedItemChangedInvoked)
  //    {
  //    d->DelayedItemChangedInvoked = true;
  //    QTimer::singleShot(50, this, SLOT(delayedItemChanged()));
  //    }
  //  return;
  //  }

  this->updateSegmentFromItem(this->segmentIDFromItem(item), item);
}

//------------------------------------------------------------------------------
Qt::DropActions qMRMLSegmentsModel::supportedDropActions()const
{
  return Qt::MoveAction;
}

//------------------------------------------------------------------------------
int qMRMLSegmentsModel::nameColumn()const
{
  Q_D(const qMRMLSegmentsModel);
  return d->NameColumn;
}

//------------------------------------------------------------------------------
void qMRMLSegmentsModel::setNameColumn(int column)
{
  Q_D(qMRMLSegmentsModel);
  d->NameColumn = column;
  this->updateColumnCount();
}

//------------------------------------------------------------------------------
int qMRMLSegmentsModel::visibilityColumn()const
{
  Q_D(const qMRMLSegmentsModel);
  return d->VisibilityColumn;
}

//------------------------------------------------------------------------------
void qMRMLSegmentsModel::setVisibilityColumn(int column)
{
  Q_D(qMRMLSegmentsModel);
  d->VisibilityColumn = column;
  this->updateColumnCount();
}

//------------------------------------------------------------------------------
int qMRMLSegmentsModel::colorColumn()const
{
  Q_D(const qMRMLSegmentsModel);
  return d->ColorColumn;
}

//------------------------------------------------------------------------------
void qMRMLSegmentsModel::setColorColumn(int column)
{
  Q_D(qMRMLSegmentsModel);
  d->ColorColumn = column;
  this->updateColumnCount();
}

//------------------------------------------------------------------------------
int qMRMLSegmentsModel::opacityColumn()const
{
  Q_D(const qMRMLSegmentsModel);
  return d->OpacityColumn;
}

//------------------------------------------------------------------------------
void qMRMLSegmentsModel::setOpacityColumn(int column)
{
  Q_D(qMRMLSegmentsModel);
  d->OpacityColumn = column;
  this->updateColumnCount();
}


//------------------------------------------------------------------------------
int qMRMLSegmentsModel::statusColumn()const
{
  Q_D(const qMRMLSegmentsModel);
  return d->StatusColumn;
}

//------------------------------------------------------------------------------
void qMRMLSegmentsModel::setStatusColumn(int column)
{
  Q_D(qMRMLSegmentsModel);
  d->StatusColumn = column;
  this->updateColumnCount();
}

//------------------------------------------------------------------------------
void qMRMLSegmentsModel::updateColumnCount()
{
  Q_D(const qMRMLSegmentsModel);

  int max = this->maxColumnId();
  int oldColumnCount = this->columnCount();
  this->setColumnCount(max + 1);
  if (oldColumnCount == 0)
    {
    this->updateFromSegments();
    }
  else
    {
    // Update all items
    if (!d->SegmentationNode)
      {
      return;
      }
    std::vector<std::string> segmentIDs;
    d->SegmentationNode->GetSegmentation()->GetSegmentIDs(segmentIDs); //TODO
    for (std::vector<std::string>::iterator itemIt= segmentIDs.begin(); itemIt!= segmentIDs.end(); ++itemIt)
      {
      this->updateModelItems(*itemIt);
      }
    }
}

//------------------------------------------------------------------------------
int qMRMLSegmentsModel::maxColumnId()const
{
  Q_D(const qMRMLSegmentsModel);
  int maxId = -1;
  maxId = qMax(maxId, d->NameColumn);
  maxId = qMax(maxId, d->VisibilityColumn);
  maxId = qMax(maxId, d->ColorColumn);
  maxId = qMax(maxId, d->OpacityColumn);
  maxId = qMax(maxId, d->StatusColumn);
  return maxId;
}
