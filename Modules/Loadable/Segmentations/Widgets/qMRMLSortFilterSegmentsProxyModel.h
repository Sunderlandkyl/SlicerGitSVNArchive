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

#ifndef __qMRMLSortFilterSegmentsProxyModel_h
#define __qMRMLSortFilterSegmentsProxyModel_h

// Segmentation includes
#include "qSlicerSegmentationsModuleWidgetsExport.h"

// Qt includes
#include <QSortFilterProxyModel>

// CTK includes
#include <ctkVTKObject.h>
#include <ctkPimpl.h>

class qMRMLSortFilterSegmentsProxyModelPrivate;
class vtkMRMLSegmentationNode;
class vtkMRMLScene;
class QStandardItem;

/// \ingroup Slicer_QtModules_Segmentations
class Q_SLICER_MODULE_SEGMENTATIONS_WIDGETS_EXPORT qMRMLSortFilterSegmentsProxyModel : public QSortFilterProxyModel
{
  Q_OBJECT
  QVTK_OBJECT

  /// Filter to show only items that contain the string in their names. Empty by default
  Q_PROPERTY(QString nameFilter READ nameFilter WRITE setNameFilter)
  /// Filter to show only items that contain an attribute with this name. Empty by default
  Q_PROPERTY(QString attributeNameFilter READ attributeNameFilter WRITE setAttributeNameFilter)
  /// Filter to show only items that contain an attribute with \sa attributeNameFilter (must be set)
  /// with this value. If empty, then existence of the attribute is enough to show
  /// Exact match is required. Empty by default
  Q_PROPERTY(QString attributeValueFilter READ attributeValueFilter WRITE setAttributeValueFilter)

  /// Filter to show segments with the state NotStarted
  Q_PROPERTY(bool showNotStarted READ showNotStarted WRITE setShowNotStarted)
  /// Filter to show segments with the state InProgress
  Q_PROPERTY(bool showInProgress READ showInProgress WRITE setShowInProgress)
  ///Filter to show segments with the state Completed
  Q_PROPERTY(bool showCompleted READ showCompleted WRITE setShowCompleted)
  /// Filter to show segments with the state Flagged
  Q_PROPERTY(bool showFlagged READ showFlagged WRITE setShowFlagged)

public:
  typedef QSortFilterProxyModel Superclass;
  qMRMLSortFilterSegmentsProxyModel(QObject *parent=nullptr);
  ~qMRMLSortFilterSegmentsProxyModel() override;

  Q_INVOKABLE vtkMRMLSegmentationNode* subjectHierarchyNode()const;
  Q_INVOKABLE vtkMRMLScene* mrmlScene()const;

  QString nameFilter()const;
  QString attributeNameFilter()const;
  QString attributeValueFilter()const;

  bool showNotStarted()const;
  bool showInProgress()const;
  bool showCompleted()const;
  bool showFlagged()const;

  /// Retrieve the associated subject hierarchy item ID from a model index
  Q_INVOKABLE std::string segmentIDFromIndex(const QModelIndex& index)const;

  /// Retrieve an index for a given a subject hierarchy item ID
  Q_INVOKABLE QModelIndex indexFromSegmentID(std::string itemID, int column=0)const;

  /// Returns true if the item in the row indicated by the given sourceRow and
  /// sourceParent should be included in the model; otherwise returns false.
  /// This method tests each item via \a filterAcceptsItem
  bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent)const override;

  /// Filters items to decide which to display in the view
  virtual bool filterAcceptsItem(std::string segmentID)const;

  Qt::ItemFlags flags(const QModelIndex & index)const override;

public slots:
  void setNameFilter(QString filter);
  void setAttributeNameFilter(QString filter);
  void setAttributeValueFilter(QString filter);
  void setShowNotStarted(bool show);
  void setShowInProgress(bool show);
  void setShowCompleted(bool show);
  void setShowFlagged(bool show);

protected:
  QStandardItem* sourceItem(const QModelIndex& index)const;

protected:
  QScopedPointer<qMRMLSortFilterSegmentsProxyModelPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qMRMLSortFilterSegmentsProxyModel);
  Q_DISABLE_COPY(qMRMLSortFilterSegmentsProxyModel);
};

#endif
