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

  This file was originally developed by Csaba Pinter, PerkLab, Queen's University
  and was supported through the Applied Cancer Research Unit program of Cancer Care
  Ontario with funds provided by the Ontario Ministry of Health and Long-Term Care

==============================================================================*/

// SegmentationCore includes
#include "vtkSegmentationLabelmapModifier.h"
#include "vtkSegmentationConverter.h"
#include "vtkOrientedImageData.h"
#include "vtkOrientedImageDataResample.h"

// VTK includes
#include <vtkAppendPolyData.h>
#include <vtkBoundingBox.h>
#include <vtkGeneralTransform.h>
#include <vtkImageCast.h>
#include <vtkImageConstantPad.h>
#include <vtkImageMask.h>
#include <vtkImageReslice.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPlaneSource.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkVersionMacros.h>
#include <vtkVector.h>

// STD includes
#include <algorithm>
#include <vector>

vtkStandardNewMacro(vtkSegmentationLabelmapModifier);

//----------------------------------------------------------------------------
vtkSegmentationLabelmapModifier::vtkSegmentationLabelmapModifier()
= default;

//----------------------------------------------------------------------------
vtkSegmentationLabelmapModifier::~vtkSegmentationLabelmapModifier()
= default;

//-----------------------------------------------------------------------------
bool vtkSegmentationLabelmapModifier::ModifySegmentationByLabelmap(vtkSegmentation * segmentation,
  std::vector<std::string> & segmentIds, vtkOrientedImageData * modifierLabelmapInput, vtkOrientedImageData * mask)
{
  if (!segmentation)
    {
    vtkErrorWithObjectMacro(nullptr, "Invalid segmentation!");
    return false;
    }

  if (!modifierLabelmapInput)
    {
    vtkErrorWithObjectMacro(nullptr, "Invalid modifier labelmap!");
    return false;
    }

  //vtkSmartPointer<vtkOrientedImageData> modifierLabelmap = modifierLabelmapInput;

  //// Apply mask to modifier labelmap if paint over is turned off
  //if (mask)
  //  {
  //  vtkOrientedImageDataResample::ApplyImageMask(modifierLabelmap, mask, 0/*this->m_EraseValue*/, true);
  //  }

  //int modificationExtent[6] = { 0 }; // TODO: Argument

  //// Copy the temporary padded modifier labelmap to the segment.
  //// Mask and threshold was already applied on modifier labelmap at this point if requested.
  //const int* extent = modificationExtent;
  //if (extent[0]>extent[1] || extent[2]>extent[3] || extent[4]>extent[5])
  //  {
  //  // invalid extent, it means we have to work with the entire modifier labelmap
  //  extent = nullptr;
  //  }

  //std::vector<std::string> allSegmentIDs;
  //segmentationNode->GetSegmentation()->GetSegmentIDs(allSegmentIDs);
  //// remove selected segment, that is already handled
  //allSegmentIDs.erase(std::remove(allSegmentIDs.begin(), allSegmentIDs.end(), segmentID), allSegmentIDs.end());

  //std::vector<std::string> visibleSegmentIDs;
  //vtkMRMLSegmentationDisplayNode* displayNode = vtkMRMLSegmentationDisplayNode::SafeDownCast(segmentationNode->GetDisplayNode());
  //if (displayNode)
  //  {
  //  for (std::vector<std::string>::iterator segmentIDIt = allSegmentIDs.begin(); segmentIDIt != allSegmentIDs.end(); ++segmentIDIt)
  //    {
  //    if (displayNode->GetSegmentVisibility(*segmentIDIt))
  //      {
  //      visibleSegmentIDs.push_back(*segmentIDIt);
  //      }
  //    }
  //  }

  //std::vector<std::string> segmentIDsToOverwrite;
  //switch (this->parameterSetNode()->GetOverwriteMode())
  //  {
  //  case vtkMRMLSegmentEditorNode::OverwriteNone:
  //    // nothing to overwrite
  //    break;
  //  case vtkMRMLSegmentEditorNode::OverwriteVisibleSegments:
  //    segmentIDsToOverwrite = visibleSegmentIDs;
  //    break;
  //  case vtkMRMLSegmentEditorNode::OverwriteAllSegments:
  //    segmentIDsToOverwrite = allSegmentIDs;
  //    break;
  //  }

  //if (bypassMasking)
  //  {
  //  segmentIDsToOverwrite.clear();
  //  }

  //vtkSegment* segment = segmentationNode->GetSegmentation()->GetSegment(segmentID);
  //std::vector<std::string> sharedSegmentIDs;
  //segmentationNode->GetSegmentation()->GetSegmentIDsSharingBinaryLabelmapRepresentation(segmentID, sharedSegmentIDs, false);

  //// Determine if there are any segments on the same layer that we should not overwrite
  //bool segmentsOnLayerShouldOverlap = false;
  //for (std::string sharedID : sharedSegmentIDs)
  //  {
  //  if (std::find(segmentIDsToOverwrite.begin(), segmentIDsToOverwrite.end(), sharedID) == segmentIDsToOverwrite.end())
  //    {
  //    segmentsOnLayerShouldOverlap = true;
  //    break;
  //    }
  //  }

  //// If there are segments on the same layer that we should not overwrite, determine if there are any under the modifier labelmap
  //if (segmentsOnLayerShouldOverlap)
  //  {
  //  std::vector<std::string> sharedSegmentsUnderModifier;
  //  vtkSlicerSegmentationsModuleLogic::GetSharedSegmentIDsInMask(segmentationNode, segmentID, modifierLabelmap,
  //    extent, sharedSegmentsUnderModifier, 0.0, false);

  //  bool separateSegmentID = false;
  //  for (std::string sharedSegmentID : sharedSegmentsUnderModifier)
  //    {
  //    std::vector<std::string>::iterator foundOverwriteIDIt = std::find(segmentIDsToOverwrite.begin(), segmentIDsToOverwrite.end(), sharedSegmentID);
  //    if (foundOverwriteIDIt == segmentIDsToOverwrite.end())
  //      {
  //      separateSegmentID = true;
  //      break;
  //      }
  //    }

  //  // We would overwrite a segment that should not be. Separate the segment to a separate layer
  //  if (separateSegmentID)
  //    {
  //    segmentationNode->GetSegmentation()->SeparateSegmentLabelmap(segmentationNode->GetSegmentation()->GetSegmentIdBySegment(segment));
  //    }
  //  }

  //for (std::string sharedSegmentID : sharedSegmentIDs)
  //  {
  //  std::vector<std::string>::iterator foundOverwriteIDIt = std::find(segmentIDsToOverwrite.begin(), segmentIDsToOverwrite.end(), sharedSegmentID);
  //  if (foundOverwriteIDIt != segmentIDsToOverwrite.end())
  //    {
  //    segmentIDsToOverwrite.erase(foundOverwriteIDIt);
  //    }
  //  }

  //// Create inverted binary labelmap
  //vtkSmartPointer<vtkImageThreshold> inverter = vtkSmartPointer<vtkImageThreshold>::New();
  //inverter->SetInputData(modifierLabelmap);
  //inverter->SetInValue(VTK_UNSIGNED_CHAR_MAX);
  //inverter->SetOutValue(m_EraseValue);
  //inverter->ThresholdByLower(0);
  //inverter->SetOutputScalarTypeToUnsignedChar();

  //if (modificationMode == qSlicerSegmentEditorAbstractEffect::ModificationModeSet)
  //  {
  //  vtkSmartPointer<vtkImageThreshold> segmentInverter = vtkSmartPointer<vtkImageThreshold>::New();
  //  segmentInverter->SetInputData(segment->GetRepresentation(segmentationNode->GetSegmentation()->GetMasterRepresentationName()));
  //  segmentInverter->SetInValue(m_EraseValue);
  //  segmentInverter->SetOutValue(VTK_UNSIGNED_CHAR_MAX);
  //  segmentInverter->ReplaceInOn();
  //  segmentInverter->ThresholdBetween(segment->GetLabelValue(), segment->GetLabelValue());
  //  segmentInverter->SetOutputScalarTypeToUnsignedChar();
  //  segmentInverter->Update();
  //  vtkNew<vtkOrientedImageData> invertedModifierLabelmap;
  //  invertedModifierLabelmap->ShallowCopy(segmentInverter->GetOutput());
  //  vtkNew<vtkMatrix4x4> imageToWorldMatrix;
  //  modifierLabelmap->GetImageToWorldMatrix(imageToWorldMatrix.GetPointer());
  //  invertedModifierLabelmap->SetGeometryFromImageToWorldMatrix(imageToWorldMatrix.GetPointer());
  //  if (!vtkSlicerSegmentationsModuleLogic::SetBinaryLabelmapToSegment(
  //    invertedModifierLabelmap.GetPointer(), segmentationNode, segmentID, vtkSlicerSegmentationsModuleLogic::MODE_MERGE_MIN))
  //    {
  //    qCritical() << Q_FUNC_INFO << ": Failed to remove modifier labelmap from selected segment";
  //    }
  //  if (!vtkSlicerSegmentationsModuleLogic::SetBinaryLabelmapToSegment(
  //    modifierLabelmap, segmentationNode, segmentID, vtkSlicerSegmentationsModuleLogic::MODE_MERGE_MASK, extent))
  //    {
  //    qCritical() << Q_FUNC_INFO << ": Failed to add modifier labelmap to selected segment";
  //    }
  //  }
  //else if (modificationMode == qSlicerSegmentEditorAbstractEffect::ModificationModeAdd)
  //  {
  //  if (!vtkSlicerSegmentationsModuleLogic::SetBinaryLabelmapToSegment(
  //    modifierLabelmap, segmentationNode, segmentID, vtkSlicerSegmentationsModuleLogic::MODE_MERGE_MASK, extent))
  //    {
  //    qCritical() << Q_FUNC_INFO << ": Failed to add modifier labelmap to selected segment";
  //    }
  //  }
  //else if (modificationMode == qSlicerSegmentEditorAbstractEffect::ModificationModeRemove
  //  || modificationMode == qSlicerSegmentEditorAbstractEffect::ModificationModeRemoveAll)
  //  {
  //  inverter->Update();
  //  vtkNew<vtkOrientedImageData> invertedModifierLabelmap;
  //  invertedModifierLabelmap->ShallowCopy(inverter->GetOutput());
  //  vtkNew<vtkMatrix4x4> imageToWorldMatrix;
  //  modifierLabelmap->GetImageToWorldMatrix(imageToWorldMatrix.GetPointer());
  //  invertedModifierLabelmap->SetGeometryFromImageToWorldMatrix(imageToWorldMatrix.GetPointer());
  //  if (!vtkSlicerSegmentationsModuleLogic::SetBinaryLabelmapToSegment(
  //    invertedModifierLabelmap.GetPointer(), segmentationNode, segmentID, vtkSlicerSegmentationsModuleLogic::MODE_MERGE_MIN, extent))
  //    {
  //    qCritical() << Q_FUNC_INFO << ": Failed to remove modifier labelmap from selected segment";
  //    }
  //  }

  //if (segment)
  //  {
  //  if (vtkSlicerSegmentationsModuleLogic::GetSegmentStatus(segment) == vtkSlicerSegmentationsModuleLogic::NotStarted)
  //    {
  //    vtkSlicerSegmentationsModuleLogic::SetSegmentStatus(segment, vtkSlicerSegmentationsModuleLogic::InProgress);
  //    }
  //  }

  //if (!segmentIDsToOverwrite.empty() &&
  //   ( modificationMode == qSlicerSegmentEditorAbstractEffect::ModificationModeSet
  //  || modificationMode == qSlicerSegmentEditorAbstractEffect::ModificationModeAdd
  //  || modificationMode == qSlicerSegmentEditorAbstractEffect::ModificationModeRemoveAll))
  //  {
  //  inverter->Update();
  //  vtkNew<vtkOrientedImageData> invertedModifierLabelmap;
  //  invertedModifierLabelmap->ShallowCopy(inverter->GetOutput());
  //  vtkNew<vtkMatrix4x4> imageToWorldMatrix;
  //  modifierLabelmap->GetImageToWorldMatrix(imageToWorldMatrix.GetPointer());
  //  invertedModifierLabelmap->SetGeometryFromImageToWorldMatrix(imageToWorldMatrix.GetPointer());

  //  std::map<vtkDataObject*, bool> erased;
  //  for (std::string currentSegmentID : segmentIDsToOverwrite)
  //    {
  //    vtkSegment* currentSegment = segmentationNode->GetSegmentation()->GetSegment(currentSegmentID);
  //    vtkDataObject* dataObject = currentSegment->GetRepresentation(vtkSegmentationConverter::GetBinaryLabelmapRepresentationName());
  //    if (erased[dataObject])
  //      {
  //      continue;
  //      }
  //    erased[dataObject] = true;

  //    vtkOrientedImageData* currentLabelmap = vtkOrientedImageData::SafeDownCast(dataObject);

  //    std::vector<std::string> dontOverwriteIDs;
  //    std::vector<std::string> currentSharedIDs;
  //    segmentationNode->GetSegmentation()->GetSegmentIDsSharingBinaryLabelmapRepresentation(currentSegmentID, currentSharedIDs, true);
  //    for (std::string sharedSegmentID : currentSharedIDs)
  //      {
  //      if (std::find(segmentIDsToOverwrite.begin(), segmentIDsToOverwrite.end(), sharedSegmentID) == segmentIDsToOverwrite.end())
  //        {
  //        dontOverwriteIDs.push_back(sharedSegmentID);
  //        }
  //      }

  //    vtkSmartPointer<vtkOrientedImageData> invertedModifierLabelmap2 = invertedModifierLabelmap;
  //    if (dontOverwriteIDs.size() > 0)
  //      {
  //      invertedModifierLabelmap2 = vtkSmartPointer<vtkOrientedImageData>::New();
  //      invertedModifierLabelmap2->DeepCopy(invertedModifierLabelmap);

  //      vtkNew<vtkOrientedImageData> maskImage;
  //      maskImage->CopyDirections(currentLabelmap);
  //      for (std::string dontOverwriteID : dontOverwriteIDs)
  //        {
  //        vtkSegment* dontOverwriteSegment = segmentationNode->GetSegmentation()->GetSegment(dontOverwriteID);
  //        vtkNew<vtkImageThreshold> threshold;
  //        threshold->SetInputData(currentLabelmap);
  //        threshold->ThresholdBetween(dontOverwriteSegment->GetLabelValue(), dontOverwriteSegment->GetLabelValue());
  //        threshold->SetInValue(1);
  //        threshold->SetOutValue(0);
  //        threshold->SetOutputScalarTypeToUnsignedChar();
  //        threshold->Update();
  //        maskImage->ShallowCopy(threshold->GetOutput());
  //        vtkOrientedImageDataResample::ApplyImageMask(invertedModifierLabelmap2, maskImage, VTK_UNSIGNED_CHAR_MAX, true);
  //        }
  //      }

  //    if (!vtkSlicerSegmentationsModuleLogic::SetBinaryLabelmapToSegment(
  //      invertedModifierLabelmap2, segmentationNode, currentSegmentID, vtkSlicerSegmentationsModuleLogic::MODE_MERGE_MIN, extent, true))
  //      {
  //      qCritical() << Q_FUNC_INFO << ": Failed to set modifier labelmap to segment " << (currentSegmentID.c_str());
  //      }
  //    }
  //  }
  //else if (modificationMode == qSlicerSegmentEditorAbstractEffect::ModificationModeRemove)
  //  {
  //  // In general, we don't try to "add back" areas to other segments when an area is removed from the selected segment.
  //  // The only exception is when we draw inside one specific segment. In that case erasing adds to the mask segment. It is useful
  //  // for splitting a segment into two by painting.
  //  if (this->parameterSetNode()->GetMaskMode() == vtkMRMLSegmentEditorNode::PaintAllowedInsideSingleSegment
  //    && this->parameterSetNode()->GetMaskSegmentID())
  //    {
  //    if (!vtkSlicerSegmentationsModuleLogic::SetBinaryLabelmapToSegment(
  //      modifierLabelmap, segmentationNode, this->parameterSetNode()->GetMaskSegmentID(), vtkSlicerSegmentationsModuleLogic::MODE_MERGE_MASK, extent))
  //      {
  //      qCritical() << Q_FUNC_INFO << ": Failed to remove modifier labelmap from segment " << this->parameterSetNode()->GetMaskSegmentID();
  //      }
  //    }
  //  }

  //// Make sure the segmentation node is under the same parent as the master volume
  //vtkMRMLScalarVolumeNode* masterVolumeNode = d->ParameterSetNode->GetMasterVolumeNode();
  //if (masterVolumeNode)
  //  {
  //  vtkMRMLSubjectHierarchyNode* shNode = vtkMRMLSubjectHierarchyNode::GetSubjectHierarchyNode(d->ParameterSetNode->GetScene());
  //  if (shNode)
  //    {
  //    vtkIdType segmentationId = shNode->GetItemByDataNode(segmentationNode);
  //    vtkIdType masterVolumeShId = shNode->GetItemByDataNode(masterVolumeNode);
  //    if (segmentationId && masterVolumeShId)
  //      {
  //      shNode->SetItemParent(segmentationId, shNode->GetItemParent(masterVolumeShId));
  //      }
  //    else
  //      {
  //      qCritical() << Q_FUNC_INFO << ": Subject hierarchy items not found for segmentation or master volume";
  //      }
  //    }
  //  }

  return false;
}
