/*==============================================================================

  Program: 3D Slicer

  Copyright (c) Laboratory for Percutaneous Surgery (PerkLab)
  Queen's University, Kingston, ON, Canada. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Csaba Pinter, PerkLab, Queen's University
  and was supported through the Applied Cancer Research Unit program of Cancer Care
  Ontario with funds provided by the Ontario Ministry of Health and Long-Term Care

==============================================================================*/

// Segmentations includes
#include "qMRMLSegmentsTableView.h"
#include "ui_qMRMLSegmentsTableView.h"
#include "qMRMLSegmentsModel.h"

#include "qMRMLDoubleSpinBoxDelegate.h"

#include "vtkMRMLSegmentationNode.h"
#include "vtkMRMLSegmentationDisplayNode.h"
#include "vtkSegmentation.h"
#include "vtkSegment.h"

// Terminologies includes
#include "qSlicerTerminologyItemDelegate.h"
#include "vtkSlicerTerminologiesModuleLogic.h"
#include "vtkSlicerTerminologyEntry.h"
#include "vtkSlicerTerminologyCategory.h"
#include "vtkSlicerTerminologyType.h"

// MRML includes
#include <vtkMRMLScene.h>
#include <vtkMRMLLabelMapVolumeNode.h>
#include <vtkMRMLModelNode.h>
#include <vtkMRMLSliceNode.h>

// SlicerQt includes
#include <qSlicerApplication.h>
#include <qSlicerCoreApplication.h>
#include <qSlicerLayoutManager.h>
#include <qSlicerModuleManager.h>
#include <qSlicerAbstractCoreModule.h>
#include <qMRMLItemDelegate.h>
#include <qMRMLSliceWidget.h>

// VTK includes
#include <vtkWeakPointer.h>

// Qt includes
#include <QAction>
#include <QDebug>
#include <QKeyEvent>
#include <QStringList>
#include <QToolButton>
#include <QContextMenuEvent>
#include <QMenu>
#include <QModelIndex>

#define ID_PROPERTY "ID"
#define VISIBILITY_PROPERTY "Visible"

const std::string qMRMLSegmentsTableView::STATUS_TAG_NAME = "Segmentation.Status";

//-----------------------------------------------------------------------------
class qMRMLSegmentsTableViewPrivate: public Ui_qMRMLSegmentsTableView
{
  Q_DECLARE_PUBLIC(qMRMLSegmentsTableView);

protected:
  qMRMLSegmentsTableView* const q_ptr;
public:
  qMRMLSegmentsTableViewPrivate(qMRMLSegmentsTableView& object);
  void init();

  /// Sets table message and takes care of the visibility of the label
  void setMessage(const QString& message);

  /// Return the column index for a given string, -1 if not a valid header
  int columnIndex(QString label);

  /// Find name item of row corresponding to a segment ID
  QTableWidgetItem* findItemBySegmentID(QString segmentID);

  /// Get string to pass terminology information via table widget item
  QString getTerminologyUserDataForSegment(vtkSegment* segment);

public:
  /// Segmentation MRML node containing shown segments
  vtkWeakPointer<vtkMRMLSegmentationNode> SegmentationNode;

  /// Flag determining whether the long-press per-view segment visibility options are available
  bool AdvancedSegmentVisibility;

  QIcon VisibleIcon;
  QIcon InvisibleIcon;

  /// Currently, if we are requesting segment display information from the
  /// segmentation display node,  the display node may emit modification events.
  /// We make sure these events do not interrupt the update process by setting
  /// IsUpdatingWidgetFromMRML to true when an update is already in progress.
  bool IsUpdatingWidgetFromMRML;

  qMRMLSegmentsModel* Model;

private:
  QStringList ColumnLabels;
  QStringList HiddenSegmentIDs;
};

//-----------------------------------------------------------------------------
qMRMLSegmentsTableViewPrivate::qMRMLSegmentsTableViewPrivate(qMRMLSegmentsTableView& object)
  : q_ptr(&object)
  , SegmentationNode(nullptr)
  , AdvancedSegmentVisibility(false)
  , IsUpdatingWidgetFromMRML(false)
  , Model(nullptr)
{
}

//-----------------------------------------------------------------------------
void qMRMLSegmentsTableViewPrivate::init()
{
  Q_Q(qMRMLSegmentsTableView);

  this->setupUi(q);

  this->Model = new qMRMLSegmentsModel(q);
  this->SegmentsTable->setModel(this->Model);

  this->VisibleIcon = QIcon(":/Icons/Small/SlicerVisible.png");
  this->InvisibleIcon = QIcon(":/Icons/Small/SlicerInvisible.png");

  this->setMessage(QString());

  // Set table header properties
  for (int i = 0; i < this->Model->columnCount(); ++i)
  {
    if (this->Model->visibilityColumn())
    {
      this->ColumnLabels << "Visible";
    }
    else if (this->Model->colorColumn())
    {
      this->ColumnLabels << "Color";
    }
    else if (this->Model->opacityColumn())
    {
      this->ColumnLabels << "Opacity";
    }
    else if (this->Model->nameColumn())
    {
      this->ColumnLabels << "Name";
    }
    else if (this->Model->statusColumn())
    {
      this->ColumnLabels << "Status";
    }
  }

  this->SegmentsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  //this->SegmentsTable->horizontalHeader()->setSectionResizeMode(this->Model->visibilityColumn(), QHeaderView::Fixed);
  //this->SegmentsTable->horizontalHeader()->setSectionResizeMode(this->Model->colorColumn(), QHeaderView::Fixed);
  //this->SegmentsTable->horizontalHeader()->setSectionResizeMode(this->Model->opacityColumn(), QHeaderView::Fixed);
  //this->SegmentsTable->horizontalHeader()->setSectionResizeMode(this->Model->statusColumn(), QHeaderView::Fixed);
  this->SegmentsTable->horizontalHeader()->setSectionResizeMode(this->Model->nameColumn(), QHeaderView::Stretch);
  this->SegmentsTable->horizontalHeader()->setStretchLastSection(0);

  // Select rows
  this->SegmentsTable->setSelectionBehavior(QAbstractItemView::SelectRows);

  // Unset read-only by default (edit triggers are double click and edit key press)
  q->setReadOnly(false);

  //// Make connections
  //QObject::connect(this->SegmentsTable, SIGNAL(itemChanged(QTableWidgetItem*)),
  //                 q, SLOT(onSegmentTableItemChanged(QTableWidgetItem*)));
  QObject::connect(this->SegmentsTable->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
                   q, SLOT(onSegmentSelectionChanged(QItemSelection,QItemSelection)));

  // Set item delegate to handle color and opacity changes
  qMRMLItemDelegate* itemDelegate = new qMRMLItemDelegate(this->SegmentsTable);
  this->SegmentsTable->setItemDelegateForColumn(this->Model->colorColumn(), new qSlicerTerminologyItemDelegate(this->SegmentsTable));
  this->SegmentsTable->setItemDelegateForColumn(this->Model->opacityColumn(), itemDelegate);
  this->SegmentsTable->installEventFilter(q);
}

//-----------------------------------------------------------------------------
int qMRMLSegmentsTableViewPrivate::columnIndex(QString label)
{
  if (!this->ColumnLabels.contains(label))
    {
    qCritical() << Q_FUNC_INFO << ": Invalid column label!";
    return -1;
    }
  return this->ColumnLabels.indexOf(label);
}

//-----------------------------------------------------------------------------
void qMRMLSegmentsTableViewPrivate::setMessage(const QString& message)
{
  this->SegmentsTableMessageLabel->setVisible(!message.isEmpty());
  this->SegmentsTableMessageLabel->setText(message);
}

//-----------------------------------------------------------------------------
QTableWidgetItem* qMRMLSegmentsTableViewPrivate::findItemBySegmentID(QString segmentID)
{
  Q_Q(qMRMLSegmentsTableView);
  //for (int row=0; row<this->SegmentsTable->rowCount(); ++row)
  //  {
  //  QTableWidgetItem* item = this->SegmentsTable->item(row, this->columnIndex("Name"));
  //  if (!item)
  //    {
  //    continue;
  //    }
  //  if (!item->data(q->IDRole).toString().compare(segmentID))
  //    {
  //    return item;
  //    }
  //  }
  // TODO: ask model
  return nullptr;
}

//------------------------------------------------------------------------------
QString qMRMLSegmentsTableViewPrivate::getTerminologyUserDataForSegment(vtkSegment* segment)
{
  if (!segment)
    {
    qCritical() << Q_FUNC_INFO << ": Invalid segment given";
    return QString();
    }

  std::string tagValue;
  return ( segment->GetTag(vtkSegment::GetTerminologyEntryTagName(), tagValue) ? QString(tagValue.c_str()) : QString() );
}

//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// qMRMLSegmentsTableView methods

//-----------------------------------------------------------------------------
qMRMLSegmentsTableView::qMRMLSegmentsTableView(QWidget* _parent)
  : qMRMLWidget(_parent)
  , d_ptr(new qMRMLSegmentsTableViewPrivate(*this))
{
  Q_D(qMRMLSegmentsTableView);
  d->init();
}

//-----------------------------------------------------------------------------
qMRMLSegmentsTableView::~qMRMLSegmentsTableView()
= default;

//-----------------------------------------------------------------------------
void qMRMLSegmentsTableView::setSegmentationNode(vtkMRMLNode* node)
{
  Q_D(qMRMLSegmentsTableView);

  vtkMRMLSegmentationNode* segmentationNode = vtkMRMLSegmentationNode::SafeDownCast(node);

  // Connect display modified event to population of the table
  //qvtkReconnect( d->SegmentationNode, segmentationNode, vtkMRMLDisplayableNode::DisplayModifiedEvent,
  //               this, SLOT( updateWidgetFromMRML() ) ); // TODO

  d->SegmentationNode = segmentationNode;
  d->Model->setSegmentationNode(d->SegmentationNode);
}

//---------------------------------------------------------------------------
void qMRMLSegmentsTableView::setMRMLScene(vtkMRMLScene* newScene)
{
  Q_D(qMRMLSegmentsTableView);
  if (newScene == this->mrmlScene())
    {
    return;
    }

  this->qvtkReconnect(this->mrmlScene(), newScene, vtkMRMLScene::EndBatchProcessEvent, this, SLOT(endProcessing()));

  if (d->SegmentationNode && newScene != d->SegmentationNode->GetScene())
    {
    this->setSegmentationNode(nullptr);
    }

  Superclass::setMRMLScene(newScene);
  d->Model->setMRMLScene(this->mrmlScene());
}

//-----------------------------------------------------------------------------
vtkMRMLNode* qMRMLSegmentsTableView::segmentationNode()
{
  Q_D(qMRMLSegmentsTableView);

  return d->SegmentationNode;
}

//-----------------------------------------------------------------------------
//QTableView* qMRMLSegmentsTableView::tableView()
//{
//  Q_D(qMRMLSegmentsTableView);
//  return d->SegmentsTable;
//}

//-----------------------------------------------------------------------------
void qMRMLSegmentsTableView::onSegmentSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
  Q_D(qMRMLSegmentsTableView);
  if (d->SegmentsTable->signalsBlocked())
    {
    return;
    }
  emit selectionChanged(selected, deselected);
}

//-----------------------------------------------------------------------------
void qMRMLSegmentsTableView::onSegmentTableItemChanged(QTableWidgetItem* changedItem)
{
  Q_D(qMRMLSegmentsTableView);
  //TODO
  //d->setMessage(QString());

  //if (d->IsUpdatingWidgetFromMRML)
  //  {
  //  return;
  //  }
  //if (!changedItem || !d->SegmentationNode)
  //  {
  //  return;
  //  }

  //// All items contain the segment ID, get that
  //QString segmentId = changedItem->data(IDRole).toString();

  //// If segment name has been changed
  //if (changedItem->column() == d->columnIndex("Name"))
  //  {
  //  QString nameText(changedItem->text());
  //  vtkSegment* segment = d->SegmentationNode->GetSegmentation()->GetSegment(segmentId.toLatin1().constData());
  //  if (!segment)
  //    {
  //    qCritical() << Q_FUNC_INFO << ": Segment with ID '" << segmentId << "' not found in segmentation node " << d->SegmentationNode->GetName();
  //    return;
  //    }
  //  emit segmentAboutToBeModified(segmentId);
  //  segment->SetName(nameText.toLatin1().constData());

  //  // Turn off auto-generated flag for segment because it was manually entered
  //  segment->SetNameAutoGenerated(false);

  //  // Set name as item data for color item so that it can be propagated to terminology selector
  //  QTableWidgetItem* colorItem = changedItem->tableWidget()->item(changedItem->row(), d->columnIndex("Color"));
  //  d->IsUpdatingWidgetFromMRML = true;
  //  colorItem->setData(qSlicerTerminologyItemDelegate::NameRole, nameText);
  //  colorItem->setData(qSlicerTerminologyItemDelegate::NameAutoGeneratedRole, false);
  //  d->IsUpdatingWidgetFromMRML = false;
  //  }
  //// If visualization has been changed
  //else
  //  {
  //  // For all other columns we need the display node
  //  vtkMRMLSegmentationDisplayNode* displayNode = vtkMRMLSegmentationDisplayNode::SafeDownCast(
  //    d->SegmentationNode->GetDisplayNode() );
  //  if (!displayNode)
  //    {
  //    qCritical() << Q_FUNC_INFO << ": No display node for segmentation!";
  //    return;
  //    }
  //  // Get display properties
  //  vtkMRMLSegmentationDisplayNode::SegmentDisplayProperties properties;
  //  displayNode->GetSegmentDisplayProperties(segmentId.toLatin1().constData(), properties);

  //  bool displayPropertyChanged = false;

  //  // Terminology / color changed
  //  if (changedItem->column() == d->columnIndex("Color"))
  //    {
  //    vtkSegment* segment = d->SegmentationNode->GetSegmentation()->GetSegment(segmentId.toLatin1().constData());
  //    if (!segment)
  //      {
  //      qCritical() << Q_FUNC_INFO << ": Segment with ID '" << segmentId << "' not found in segmentation node " << d->SegmentationNode->GetName();
  //      return;
  //      }

  //    // Set terminology information to segment as tag
  //    QString terminologyString = changedItem->data(qSlicerTerminologyItemDelegate::TerminologyRole).toString();
  //    segment->SetTag(vtkSegment::GetTerminologyEntryTagName(), terminologyString.toLatin1().constData());

  //    // Set color to segment if it changed
  //    QColor color = changedItem->data(Qt::DecorationRole).value<QColor>();
  //    double* oldColorArray = segment->GetColor();
  //    QColor oldColor = QColor::fromRgbF(oldColorArray[0], oldColorArray[1], oldColorArray[2]);
  //    if (oldColor != color)
  //      {
  //      segment->SetColor(color.redF(), color.greenF(), color.blueF());
  //      }
  //    // Set color auto-generated flag
  //    segment->SetColorAutoGenerated(
  //      changedItem->data(qSlicerTerminologyItemDelegate::ColorAutoGeneratedRole).toBool() );

  //    // Set name if it changed
  //    QString nameFromColorItem = changedItem->data(qSlicerTerminologyItemDelegate::NameRole).toString();
  //    if (nameFromColorItem.compare(segment->GetName()))
  //      {
  //      emit segmentAboutToBeModified(segmentId);
  //      segment->SetName(nameFromColorItem.toLatin1().constData());
  //      }
  //    // Set name auto-generated flag
  //    segment->SetNameAutoGenerated(
  //      changedItem->data(qSlicerTerminologyItemDelegate::NameAutoGeneratedRole).toBool() );

  //    // Update tooltip
  //    changedItem->setToolTip(qMRMLSegmentsTableView::terminologyTooltipForSegment(segment));
  //    }
  //  // Opacity changed
  //  else if (changedItem->column() == d->columnIndex("Opacity"))
  //    {
  //    QString opacity = changedItem->data(Qt::EditRole).toString();
  //    QString currentOpacity = QString::number( properties.Opacity3D, 'f', 2);
  //    if (opacity != currentOpacity)
  //      {
  //      // Set to all kinds of opacities as they are combined on the UI
  //      properties.Opacity3D = opacity.toDouble();
  //      displayPropertyChanged = true;
  //      }
  //    }
  //  // Set changed properties to segmentation display node if a value has actually changed
  //  if (displayPropertyChanged)
  //    {
  //    displayNode->SetSegmentDisplayProperties(segmentId.toLatin1().constData(), properties);
  //    }
  //  }
}

//-----------------------------------------------------------------------------
void qMRMLSegmentsTableView::onVisibilityButtonClicked()
{
  Q_D(qMRMLSegmentsTableView);

  QToolButton* senderButton = qobject_cast<QToolButton*>(sender());
  if (!senderButton)
    {
    return;
    }

  bool visible = !senderButton->property(VISIBILITY_PROPERTY).toBool();

  // Set all visibility types to segment referenced by button toggled
  this->setSegmentVisibility(senderButton, visible, -1, -1, -1);

  // Change button icon
  senderButton->setProperty(VISIBILITY_PROPERTY, visible);
  if (visible)
    {
    senderButton->setIcon(d->VisibleIcon);
    }
  else
    {
    senderButton->setIcon(d->InvisibleIcon);
    }
}

//-----------------------------------------------------------------------------
void qMRMLSegmentsTableView::onVisibility3DActionToggled(bool visible)
{
  QAction* senderAction = qobject_cast<QAction*>(sender());
  if (!senderAction)
    {
    return;
    }

  // Set 3D visibility to segment referenced by action toggled
  this->setSegmentVisibility(senderAction, -1, visible, -1, -1);
}

//-----------------------------------------------------------------------------
void qMRMLSegmentsTableView::onVisibility2DFillActionToggled(bool visible)
{
  QAction* senderAction = qobject_cast<QAction*>(sender());
  if (!senderAction)
    {
    return;
    }

  // Set 2D fill visibility to segment referenced by action toggled
  this->setSegmentVisibility(senderAction, -1, -1, visible, -1);
}

//-----------------------------------------------------------------------------
void qMRMLSegmentsTableView::onVisibility2DOutlineActionToggled(bool visible)
{
  QAction* senderAction = qobject_cast<QAction*>(sender());
  if (!senderAction)
    {
    return;
    }

  // Set 2D outline visibility to segment referenced by action toggled
  this->setSegmentVisibility(senderAction, -1, -1, -1, visible);
}

//-----------------------------------------------------------------------------
void qMRMLSegmentsTableView::setSegmentVisibility(QObject* senderObject, int visible, int visible3D, int visible2DFill, int visible2DOutline)
{
  Q_D(qMRMLSegmentsTableView);

  if (!d->SegmentationNode)
  {
    qCritical() << Q_FUNC_INFO << " failed: segmentation node is not set";
    return;
  }

  QString segmentId = senderObject->property(ID_PROPERTY).toString();

  vtkMRMLSegmentationDisplayNode* displayNode = vtkMRMLSegmentationDisplayNode::SafeDownCast(
    d->SegmentationNode->GetDisplayNode() );
  if (!displayNode)
    {
    qCritical() << Q_FUNC_INFO << ": No display node for segmentation!";
    return;
    }
  vtkMRMLSegmentationDisplayNode::SegmentDisplayProperties properties;
  displayNode->GetSegmentDisplayProperties(segmentId.toLatin1().constData(), properties);

  // Change visibility to all modes
  bool valueChanged = false;
  if (visible == 0 || visible == 1)
    {
    properties.Visible = (bool)visible;

    // If overall visibility is explicitly set to true then enable all visibility options
    // to make sure that something is actually visible.
    if (properties.Visible && !properties.Visible3D && !properties.Visible2DFill && !properties.Visible2DOutline)
      {
      properties.Visible3D = true;
      properties.Visible2DFill = true;
      properties.Visible2DOutline = true;
      }

    valueChanged = true;
    }
  if (visible3D == 0 || visible3D == 1)
    {
    properties.Visible3D = (bool)visible3D;
    valueChanged = true;
    }
  if (visible2DFill == 0 || visible2DFill == 1)
    {
    properties.Visible2DFill = (bool)visible2DFill;
    valueChanged = true;
    }
  if (visible2DOutline == 0 || visible2DOutline == 1)
    {
    properties.Visible2DOutline = (bool)visible2DOutline;
    valueChanged = true;
    }

  // Set visibility to display node
  if (valueChanged)
    {
    displayNode->SetSegmentDisplayProperties(segmentId.toLatin1().constData(), properties);
    }
}

//-----------------------------------------------------------------------------
int qMRMLSegmentsTableView::segmentCount() const
{
  Q_D(const qMRMLSegmentsTableView);

  return d->Model->rowCount();
}

//-----------------------------------------------------------------------------
QStringList qMRMLSegmentsTableView::selectedSegmentIDs()
{
  Q_D(qMRMLSegmentsTableView);
  if (!d->SegmentsTable->selectionModel()->hasSelection())
    {
    return QStringList();
    }

  QModelIndexList selectedModelIndices = d->SegmentsTable->selectionModel()->selectedRows();
  QStringList selectedSegmentIds;
  foreach (QModelIndex selectedModelIndex, selectedModelIndices)
    {
    std::string segmentID = d->Model->segmentIDFromIndex(selectedModelIndex);
    selectedSegmentIds << segmentID.c_str();
    }

  return selectedSegmentIds;
}

//-----------------------------------------------------------------------------
void qMRMLSegmentsTableView::setSelectedSegmentIDs(QStringList segmentIDs)
{
  Q_D(qMRMLSegmentsTableView);

  if (!d->SegmentationNode && !segmentIDs.empty())
    {
    qCritical() << Q_FUNC_INFO << " failed: segmentation node is not set";
    return;
    }

  for (QString segmentID : segmentIDs)
  {
    QModelIndex index = d->Model->indexFromSegmentID(segmentID.toStdString());
    QFlags<QItemSelectionModel::SelectionFlag> flags = QFlags<QItemSelectionModel::SelectionFlag>();
    flags << QItemSelectionModel::SelectionFlag::Select;
    d->SegmentsTable->selectionModel()->select(index, flags);
  }

  // Deselect items that don't have to be selected anymore
  for (int row = 0; row < d->Model->rowCount(); ++row)
    {
    QModelIndex index = d->Model->index(row, d->Model->nameColumn());
    std::string segmentID = d->Model->segmentIDFromIndex(index);
    if (segmentID.empty())
      {
      // invalid item, canot determine selection state
      continue;
      }

    if (segmentIDs.contains(segmentID.c_str()))
      {
      // selected
      continue;
      }

    QFlags<QItemSelectionModel::SelectionFlag> flags = QFlags<QItemSelectionModel::SelectionFlag>();
    flags << QItemSelectionModel::SelectionFlag::Deselect;
    d->SegmentsTable->selectionModel()->select(index, flags);
    }
}

//-----------------------------------------------------------------------------
void qMRMLSegmentsTableView::clearSelection()
{
  Q_D(qMRMLSegmentsTableView);
  d->SegmentsTable->clearSelection();
}

//------------------------------------------------------------------------------
bool qMRMLSegmentsTableView::eventFilter(QObject* target, QEvent* event)
{
  Q_D(qMRMLSegmentsTableView);
  if (target == d->SegmentsTable)
    {
    // Prevent giving the focus to the previous/next widget if arrow keys are used
    // at the edge of the table (without this: if the current cell is in the top
    // row and user press the Up key, the focus goes from the table to the previous
    // widget in the tab order)
    if (event->type() == QEvent::KeyPress)
      {
      QKeyEvent* keyEvent = static_cast<QKeyEvent *>(event);
      QAbstractItemModel* model = d->SegmentsTable->model();
      QModelIndex currentIndex = d->SegmentsTable->currentIndex();

      if (model && (
        (keyEvent->key() == Qt::Key_Left && currentIndex.column() == 0)
        || (keyEvent->key() == Qt::Key_Up && currentIndex.row() == 0)
        || (keyEvent->key() == Qt::Key_Right && currentIndex.column() == model->columnCount() - 1)
        || (keyEvent->key() == Qt::Key_Down && currentIndex.row() == model->rowCount() - 1)))
        {
        return true;
        }
      }
    }
  return this->QWidget::eventFilter(target, event);
}

//------------------------------------------------------------------------------
void qMRMLSegmentsTableView::endProcessing()
{
}

//------------------------------------------------------------------------------
void qMRMLSegmentsTableView::setSelectionMode(int mode)
{
  Q_D(qMRMLSegmentsTableView);
  d->SegmentsTable->setSelectionMode(static_cast<QAbstractItemView::SelectionMode>(mode));
}

//------------------------------------------------------------------------------
void qMRMLSegmentsTableView::setHeaderVisible(bool visible)
{
  Q_D(qMRMLSegmentsTableView);
  d->SegmentsTable->horizontalHeader()->setVisible(visible);
}

//------------------------------------------------------------------------------
void qMRMLSegmentsTableView::setVisibilityColumnVisible(bool visible)
{
  Q_D(qMRMLSegmentsTableView);
  d->SegmentsTable->setColumnHidden(d->columnIndex("Visible"), !visible);
}

//------------------------------------------------------------------------------
void qMRMLSegmentsTableView::setColorColumnVisible(bool visible)
{
  Q_D(qMRMLSegmentsTableView);
  d->SegmentsTable->setColumnHidden(d->columnIndex("Color"), !visible);
}

//------------------------------------------------------------------------------
void qMRMLSegmentsTableView::setOpacityColumnVisible(bool visible)
{
  Q_D(qMRMLSegmentsTableView);
  d->SegmentsTable->setColumnHidden(d->columnIndex("Opacity"), !visible);
}

//------------------------------------------------------------------------------
void qMRMLSegmentsTableView::setReadOnly(bool aReadOnly)
{
  Q_D(qMRMLSegmentsTableView);
  if (aReadOnly)
    {
    d->SegmentsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    }
  else
    {
    d->SegmentsTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    }
}

//------------------------------------------------------------------------------
int qMRMLSegmentsTableView::selectionMode()
{
  Q_D(qMRMLSegmentsTableView);
  return d->SegmentsTable->selectionMode();
}

//------------------------------------------------------------------------------
bool qMRMLSegmentsTableView::headerVisible()
{
  Q_D(qMRMLSegmentsTableView);
  return d->SegmentsTable->horizontalHeader()->isVisible();
}

//------------------------------------------------------------------------------
bool qMRMLSegmentsTableView::visibilityColumnVisible()
{
  Q_D(qMRMLSegmentsTableView);
  return !d->SegmentsTable->isColumnHidden(d->columnIndex("Visible"));
}

//------------------------------------------------------------------------------
bool qMRMLSegmentsTableView::colorColumnVisible()
{
  Q_D(qMRMLSegmentsTableView);
  return !d->SegmentsTable->isColumnHidden(d->columnIndex("Color"));
}

//------------------------------------------------------------------------------
bool qMRMLSegmentsTableView::opacityColumnVisible()
{
  Q_D(qMRMLSegmentsTableView);
  return !d->SegmentsTable->isColumnHidden(d->columnIndex("Opacity"));
}

//------------------------------------------------------------------------------
bool qMRMLSegmentsTableView::readOnly()
{
  Q_D(qMRMLSegmentsTableView);
  return (d->SegmentsTable->editTriggers() == QAbstractItemView::NoEditTriggers);
}

//------------------------------------------------------------------------------
void qMRMLSegmentsTableView::contextMenuEvent(QContextMenuEvent* event)
{
  QMenu* contextMenu = new QMenu(this);

  QAction* showOnlySelectedAction = new QAction("Show only selected segments", this);
  QObject::connect(showOnlySelectedAction, SIGNAL(triggered()), this, SLOT(showOnlySelectedSegments()));
  contextMenu->addAction(showOnlySelectedAction);

  contextMenu->addSeparator();

  QAction* jumpSlicesAction = new QAction("Jump slices", this);
  QObject::connect(jumpSlicesAction, SIGNAL(triggered()), this, SLOT(jumpSlices()));
  contextMenu->addAction(jumpSlicesAction);

  contextMenu->addSeparator();

  QAction* moveUpAction = new QAction("Move segment up", this);
  QObject::connect(moveUpAction, SIGNAL(triggered()), this, SLOT(moveSelectedSegmentsUp()));
  contextMenu->addAction(moveUpAction);

  QAction* moveDownAction = new QAction("Move segment down", this);
  QObject::connect(moveDownAction, SIGNAL(triggered()), this, SLOT(moveSelectedSegmentsDown()));
  contextMenu->addAction(moveDownAction);

  contextMenu->popup(event->globalPos());
}

//------------------------------------------------------------------------------
void qMRMLSegmentsTableView::showOnlySelectedSegments()
{
  QStringList selectedSegmentIDs = this->selectedSegmentIDs();
  if (selectedSegmentIDs.size() == 0)
    {
    qWarning() << Q_FUNC_INFO << ": No segment selected";
    return;
    }

  Q_D(qMRMLSegmentsTableView);
  if (!d->SegmentationNode)
    {
    qCritical() << Q_FUNC_INFO << ": No current segmentation node";
    return;
    }
  vtkMRMLSegmentationDisplayNode* displayNode = vtkMRMLSegmentationDisplayNode::SafeDownCast(
    d->SegmentationNode->GetDisplayNode() );
  if (!displayNode)
    {
    qCritical() << Q_FUNC_INFO << ": No display node for segmentation " << d->SegmentationNode->GetName();
    return;
    }

  // Hide all segments except the selected ones
  int disabledModify = displayNode->StartModify();
  QStringList displayedSegmentIDs = this->displayedSegmentIDs();
  foreach (QString segmentId, displayedSegmentIDs)
    {
    bool visible = false;
    if (selectedSegmentIDs.contains(segmentId))
      {
      visible = true;
      }

    displayNode->SetSegmentVisibility(segmentId.toLatin1().constData(), visible);
    }
  displayNode->EndModify(disabledModify);
}

//------------------------------------------------------------------------------
void qMRMLSegmentsTableView::jumpSlices()
{
  QStringList selectedSegmentIDs = this->selectedSegmentIDs();
  if (selectedSegmentIDs.size() == 0)
    {
    qWarning() << Q_FUNC_INFO << ": No segment selected";
    return;
    }

  Q_D(qMRMLSegmentsTableView);
  if (!d->SegmentationNode)
    {
    qCritical() << Q_FUNC_INFO << ": No current segmentation node";
    return;
    }

  double* segmentCenterPosition = d->SegmentationNode->GetSegmentCenterRAS(selectedSegmentIDs[0].toLatin1().constData());
  if (!segmentCenterPosition)
    {
    return;
    }

  qSlicerLayoutManager* layoutManager = qSlicerApplication::application()->layoutManager();
  if (!layoutManager)
    {
    // application is closing
    return;
    }
  foreach(QString sliceViewName, layoutManager->sliceViewNames())
    {
    // Check if segmentation is visible in this view
    qMRMLSliceWidget* sliceWidget = layoutManager->sliceWidget(sliceViewName);
    vtkMRMLSliceNode* sliceNode = sliceWidget->mrmlSliceNode();
    if (!sliceNode || !sliceNode->GetID())
      {
      continue;
      }
    bool visibleInView = false;
    int numberOfDisplayNodes = d->SegmentationNode->GetNumberOfDisplayNodes();
    for (int displayNodeIndex = 0; displayNodeIndex < numberOfDisplayNodes; displayNodeIndex++)
      {
      vtkMRMLDisplayNode* segmentationDisplayNode = d->SegmentationNode->GetNthDisplayNode(displayNodeIndex);
      if (segmentationDisplayNode->IsDisplayableInView(sliceNode->GetID()))
        {
        visibleInView = true;
        break;
        }
      }
    if (!visibleInView)
      {
      continue;
      }
    sliceNode->JumpSliceByCentering(segmentCenterPosition[0], segmentCenterPosition[1], segmentCenterPosition[2]);
    }
}

//------------------------------------------------------------------------------
void qMRMLSegmentsTableView::moveSelectedSegmentsUp()
{
  QStringList selectedSegmentIDs = this->selectedSegmentIDs();
  if (selectedSegmentIDs.size() == 0)
    {
    qWarning() << Q_FUNC_INFO << ": No segment selected";
    return;
    }

  Q_D(qMRMLSegmentsTableView);
  if (!d->SegmentationNode)
    {
    qCritical() << Q_FUNC_INFO << ": No current segmentation node";
    return;
    }
  vtkSegmentation* segmentation = d->SegmentationNode->GetSegmentation();

  QList<int> segmentIndices;
  foreach (QString segmentId, selectedSegmentIDs)
    {
    segmentIndices << segmentation->GetSegmentIndex(segmentId.toLatin1().constData());
    }
  int minIndex = *(std::min_element(segmentIndices.begin(), segmentIndices.end()));
  if (minIndex == 0)
    {
    qDebug() << Q_FUNC_INFO << ": Cannot move top segment up";
    return;
    }
  for (int i=0; i<selectedSegmentIDs.count(); ++i)
    {
    segmentation->SetSegmentIndex(selectedSegmentIDs[i].toLatin1().constData(), segmentIndices[i]-1);
    }
}

//------------------------------------------------------------------------------
void qMRMLSegmentsTableView::moveSelectedSegmentsDown()
{
  QStringList selectedSegmentIDs = this->selectedSegmentIDs();
  if (selectedSegmentIDs.size() == 0)
    {
    qWarning() << Q_FUNC_INFO << ": No segment selected";
    return;
    }

  Q_D(qMRMLSegmentsTableView);
  if (!d->SegmentationNode)
    {
    qCritical() << Q_FUNC_INFO << ": No current segmentation node";
    return;
    }
  vtkSegmentation* segmentation = d->SegmentationNode->GetSegmentation();

  QList<int> segmentIndices;
  foreach (QString segmentId, selectedSegmentIDs)
    {
    segmentIndices << segmentation->GetSegmentIndex(segmentId.toLatin1().constData());
    }
  int maxIndex = *(std::max_element(segmentIndices.begin(), segmentIndices.end()));
  if (maxIndex == segmentation->GetNumberOfSegments()-1)
    {
    qDebug() << Q_FUNC_INFO << ": Cannot move bottom segment down";
    return;
    }
  for (int i=selectedSegmentIDs.count()-1; i>=0; --i)
    {
    segmentation->SetSegmentIndex(selectedSegmentIDs[i].toLatin1().constData(), segmentIndices[i]+1);
    }
}

// --------------------------------------------------------------------------
QString qMRMLSegmentsTableView::terminologyTooltipForSegment(vtkSegment* segment)
{
  if (!segment)
    {
    qCritical() << Q_FUNC_INFO << ": Invalid segment given";
    return QString();
    }

  // Get terminologies module logic
  vtkSlicerTerminologiesModuleLogic* terminologiesLogic = nullptr;
  qSlicerAbstractCoreModule* terminologiesModule = qSlicerCoreApplication::application()->moduleManager()->module("Terminologies");
  if (terminologiesModule)
    {
    terminologiesLogic = vtkSlicerTerminologiesModuleLogic::SafeDownCast(terminologiesModule->logic());
    }
  else
    {
    qCritical() << Q_FUNC_INFO << ": Terminologies module is not found";
    return QString();
    }

  std::string serializedTerminology("");
  if (!segment->GetTag(vtkSegment::GetTerminologyEntryTagName(), serializedTerminology))
    {
    return QString("No terminology information");
    }
  vtkSmartPointer<vtkSlicerTerminologyEntry> terminologyEntry = vtkSmartPointer<vtkSlicerTerminologyEntry>::New();
  if (!terminologiesLogic->DeserializeTerminologyEntry(serializedTerminology, terminologyEntry))
    {
    return QString("Invalid terminology information");
    }

  return QString(terminologiesLogic->GetInfoStringFromTerminologyEntry(terminologyEntry).c_str());
}

// --------------------------------------------------------------------------
void qMRMLSegmentsTableView::setHideSegments(const QStringList& segmentIDs)
{
  Q_D(qMRMLSegmentsTableView);
  d->HiddenSegmentIDs = segmentIDs;
}

// --------------------------------------------------------------------------
QStringList qMRMLSegmentsTableView::hideSegments()const
{
  Q_D(const qMRMLSegmentsTableView);
  return d->HiddenSegmentIDs;
}

// --------------------------------------------------------------------------
QStringList qMRMLSegmentsTableView::displayedSegmentIDs()const
{
  Q_D(const qMRMLSegmentsTableView);
  QStringList displayedSegmentIDs;
  std::vector< std::string > segmentIDs;
  vtkSegmentation* segmentation = d->SegmentationNode->GetSegmentation();
  if (segmentation)
  {
    segmentation->GetSegmentIDs(segmentIDs);
  }
  for (std::vector< std::string >::const_iterator segmentIdIt = segmentIDs.begin(); segmentIdIt != segmentIDs.end(); ++segmentIdIt)
  {
    if (d->HiddenSegmentIDs.contains(segmentIdIt->c_str()))
    {
      continue;
    }
    displayedSegmentIDs.append(segmentIdIt->c_str());
  }
  return displayedSegmentIDs;
}



//------------------------------------------------------------------------------
bool qMRMLSegmentsTableView::clickDecoration(QMouseEvent* e)
{
  Q_D(qMRMLSegmentsTableView);

  // TODO visibility click, or add boolean to mrml

  //QModelIndex index = d->SegmentsTable->indexAt(e->pos());
  //QStyleOptionViewItem opt = d->SegmentsTable->viewOptions();
  //opt.rect = this->visualRect(index);
  //qobject_cast<qMRMLItemDelegate*>(this->itemDelegate())->initStyleOption(&opt, index);
  //QRect decorationElement = this->style()->subElementRect(QStyle::SE_ItemViewItemDecoration, &opt, this);
  //if (!decorationElement.contains(e->pos()))
  //{
  //  // Mouse event is not within an item decoration
  //  return false;
  //}

  //QModelIndex sourceIndex = this->sortFilterProxyModel()->mapToSource(index);
  //if (!(sourceIndex.flags() & Qt::ItemIsEnabled))
  //{
  //  // Item is disabled
  //  return false;
  //}

  //// Visibility and color columns
  //if (sourceIndex.column() == this->model()->visibilityColumn()
  //  || sourceIndex.column() == this->model()->colorColumn())
  //{
  //  vtkIdType itemID = d->SortFilterModel->subjectHierarchyItemFromIndex(index);
  //  if (!itemID)
  //  {
  //    // Valid item is needed for visibility actions
  //    return false;
  //  }

  //  if (e->button() == Qt::LeftButton && sourceIndex.column() == this->model()->visibilityColumn())
  //  {
  //    // Toggle simple visibility
  //    this->toggleSubjectHierarchyItemVisibility(itemID);
  //  }
  //  else if (e->button() == Qt::RightButton)
  //  {
  //    // If multiple items are selected then show the node menu instead of the visibility menu
  //    if (d->SelectedItems.size() > 1)
  //    {
  //      this->populateContextMenuForItem(itemID);
  //      d->NodeMenu->exec(e->globalPos());
  //    }
  //    else
  //    {
  //      // Populate then show visibility context menu if only one item is selected
  //      this->populateVisibilityContextMenuForItem(itemID);
  //      d->VisibilityMenu->exec(e->globalPos());
  //    }
  //  }

  //  return true;
  //}

  return false;
}