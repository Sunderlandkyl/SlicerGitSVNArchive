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

  This file was originally developed by Kyle Sunderland, PerkLab, Queen's University
  and was supported through the Applied Cancer Research Unit program of Cancer Care
  Ontario with funds provided by the Ontario Ministry of Health and Long-Term Care

==============================================================================*/

#ifndef __qMRMLSegmentsModel_h
#define __qMRMLSegmentsModel_h

// Qt includes
#include <QStandardItemModel>

// CTK includes
#include <ctkPimpl.h>
#include <ctkVTKObject.h>

// Segments includes
#include "qSlicerSegmentationsModuleWidgetsExport.h"

class qMRMLSegmentsModelPrivate;
class vtkMRMLSegmentationNode;
class vtkMRMLNode;
class vtkMRMLScene;

/// \brief Item model for segments
///
/// TODO: Adopt for segments
/// It is associated to the pseudo-singleton subject hierarchy node, and it creates one model item
/// for each subject hierarchy item. It handles reparenting, reordering, etc.
/// The whole model is regenerated when the Modified event is invoked on the subject hierarchy node,
/// but only the individual items are updated when per-item events are invoked (such as
/// vtkMRMLSegmentsNode::SegmentsItemModifiedEvent)
///
class Q_SLICER_MODULE_SEGMENTATIONS_WIDGETS_EXPORT qMRMLSegmentsModel : public QStandardItemModel
{
  Q_OBJECT
  QVTK_OBJECT

  /// Control in which column the data MRML node names or if not assigned then subject hierarchy
  /// item names are displayed (Qt::DisplayRole).
  /// The icons corresponding to the role provided by the owner subject hierarchy plugin is also
  /// displayed in this column (Qt::DecorationRole).
  /// A value of -1 hides it. First column (0) by default.
  /// If no property is set in a column, nothing is displayed.
  Q_PROPERTY (int nameColumn READ nameColumn WRITE setNameColumn)
  /// Control in which column data MRML node visibility are displayed (Qt::DecorationRole).
  /// A value of -1 (default) hides the column
  Q_PROPERTY (int visibilityColumn READ visibilityColumn WRITE setVisibilityColumn)
  /// Control in which column data MRML node color is displayed.
  /// A value of -1 (default) hides the column
  Q_PROPERTY(int colorColumn READ colorColumn WRITE setColorColumn)
  //TODO
  Q_PROPERTY (int opacityColumn READ opacityColumn WRITE setOpacityColumn)
  //TODO
  Q_PROPERTY(int statusColumn READ statusColumn WRITE setStatusColumn)

public:
  typedef QStandardItemModel Superclass;
  qMRMLSegmentsModel(QObject *parent=nullptr);
  ~qMRMLSegmentsModel() override;

  enum ItemDataRole
    {
    /// Unique ID of the item, typed vtkIdType
    SegmentsItemIDRole = Qt::UserRole + 1,
    /// Integer that contains the visibility property of an item.
    /// It is closely related to the item icon.
    VisibilityRole,
    /// Must stay the last enum in the list.
    LastRole
    };

  int nameColumn()const;
  void setNameColumn(int column);

  int visibilityColumn()const;
  void setVisibilityColumn(int column);

  int colorColumn()const;
  void setColorColumn(int column);

  int opacityColumn()const;
  void setOpacityColumn(int column);

  int statusColumn()const;
  void setStatusColumn(int column);

  Qt::DropActions supportedDropActions()const override;

  Q_INVOKABLE virtual void setMRMLScene(vtkMRMLScene* scene);
  Q_INVOKABLE vtkMRMLScene* mrmlScene()const;

  vtkMRMLSegmentationNode* segmentationNode()const;

  std::string segmentIDFromIndex(const QModelIndex &index)const;
  std::string segmentIDFromItem(QStandardItem* item)const;
  QModelIndex indexFromSegmentID(std::string segmentID, int column=0)const;
  QStandardItem* itemFromSegmentID(std::string segmentID, int column=0)const;

  /// Return all the QModelIndexes (all the columns) for a given subject hierarchy item
  QModelIndexList indexes(std::string segmentID)const;

  virtual void setSegmentationNode(vtkMRMLSegmentationNode* segmentation);

signals:
  /// Signal requesting selecting items in the tree
  void requestSelectItems(QList<vtkIdType> itemIDs);
  /// Triggers invalidating the sort filter proxy model

protected slots:
  virtual void onSegmentAdded(std::string segmentID);
  virtual void onSegmentRemoved(std::string segmentID);
  virtual void onSegmentModified(std::string segmentID);

  virtual void onMRMLSceneImported(vtkMRMLScene* scene);
  virtual void onMRMLSceneClosed(vtkMRMLScene* scene);
  virtual void onMRMLSceneStartBatchProcess(vtkMRMLScene* scene);
  virtual void onMRMLSceneEndBatchProcess(vtkMRMLScene* scene);
  virtual void onMRMLNodeRemoved(vtkMRMLNode* node);

  virtual void onItemChanged(QStandardItem* item);

  /// Recompute the number of columns in the model. Called when a [some]Column property is set.
  /// Needs maxColumnId() to be reimplemented in subclasses
  void updateColumnCount();

protected:
  qMRMLSegmentsModel(qMRMLSegmentsModelPrivate* pimpl, QObject *parent=nullptr);

  virtual void updateFromSegments();
  virtual QStandardItem* insertSegment(std::string segmentID, int row=-1);

  virtual QFlags<Qt::ItemFlag> segmentFlags(std::string segmentID, int column)const;

  virtual void updateItemFromSegment(
    QStandardItem* item, std::string segmentID, int column );
  virtual void updateItemDataFromSegment(
    QStandardItem* item, std::string segmentID, int column );
  virtual void updateSegmentFromItem(std::string segmentID, QStandardItem* item );
  virtual void updateSegmentFromItemData(std::string segmentID, QStandardItem* item );

  /// Update the model items associated with the subject hierarchy item
  void updateModelItems(std::string segmentID);

  static void onEvent(vtkObject* caller, unsigned long event, void* clientData, void* callData);

  /// Must be reimplemented in subclasses that add new column types
  virtual int maxColumnId()const;

protected:
  QScopedPointer<qMRMLSegmentsModelPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qMRMLSegmentsModel);
  Q_DISABLE_COPY(qMRMLSegmentsModel);
};

void printStandardItem(QStandardItem* item, const QString& offset);

#endif
