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

#ifndef __qMRMLSegmentsModel_p_h
#define __qMRMLSegmentsModel_p_h

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Slicer API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

// Qt includes
#include <QFlags>
#include <QMap>

// Segmentations includes
#include "qSlicerSegmentationsModuleWidgetsExport.h"

#include "qMRMLSegmentsModel.h"

// MRML includes
#include <vtkMRMLScene.h>
#include <vtkMRMLSegmentationNode.h>

// VTK includes
#include <vtkCallbackCommand.h>
#include <vtkSmartPointer.h>

class QStandardItemModel;

//------------------------------------------------------------------------------
// qMRMLSegmentsModelPrivate
//------------------------------------------------------------------------------
class Q_SLICER_MODULE_SEGMENTATIONS_WIDGETS_EXPORT qMRMLSegmentsModelPrivate
{
  Q_DECLARE_PUBLIC(qMRMLSegmentsModel);

protected:
  qMRMLSegmentsModel* const q_ptr;
public:
  qMRMLSegmentsModelPrivate(qMRMLSegmentsModel& object);
  virtual ~qMRMLSegmentsModelPrivate();
  void init();

  /// Convenience function to get name for subject hierarchy item
  QString segmentsItemName(std::string segmentID);

public:
  vtkSmartPointer<vtkCallbackCommand> CallBack;
  int PendingItemModified;

  int NameColumn;
  int VisibilityColumn;
  int ColorColumn;
  int OpacityColumn;
  int StatusColumn;

  QIcon VisibleIcon;
  QIcon HiddenIcon;

  QIcon NotStartedIcon;
  QIcon InProgressIcon;
  QIcon FlagForReviewIcon;
  QIcon CompletedIcon;

  /// Subject hierarchy node
  vtkWeakPointer<vtkMRMLSegmentationNode> SegmentationNode;
  /// MRML scene (to get new subject hierarchy node if the stored one is deleted)
  vtkWeakPointer<vtkMRMLScene> MRMLScene;

  QStandardItem* qMRMLSegmentsModelPrivate::insertSegment(std::string segmentID, int index);
};

#endif
