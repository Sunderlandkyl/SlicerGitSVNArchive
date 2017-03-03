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

// Segmentations includes
#include "vtkMRMLSegmentationNode.h"
#include "vtkMRMLSegmentationDisplayNode.h"
#include "vtkMRMLSegmentationStorageNode.h"

// SegmentationCore includes
#include "vtkOrientedImageData.h"
#include "vtkOrientedImageDataResample.h"
#include "vtkCalculateOversamplingFactor.h"
#include "vtkFractionalOperations.h"

// MRML includes
#include <vtkEventBroker.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLTransformNode.h>
#include <vtkMRMLStorageNode.h>
#include <vtkMRMLSubjectHierarchyNode.h>
#include <vtkMRMLScalarVolumeNode.h>

// VTK includes
#include <vtkBoundingBox.h>
#include <vtkCallbackCommand.h>
#include <vtkGeneralTransform.h>
#include <vtkHomogeneousTransform.h>
#include <vtkIntArray.h>
#include <vtkLookupTable.h>
#include <vtkMath.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>
#include <vtkStringArray.h>
#include <vtkMatrix4x4.h>
#include <vtkTransform.h>
#include <vtkDoubleArray.h>
#include <vtkFieldData.h>

// STD includes
#include <algorithm>

//----------------------------------------------------------------------------
vtkMRMLNodeNewMacro(vtkMRMLSegmentationNode);

//----------------------------------------------------------------------------
vtkMRMLSegmentationNode::vtkMRMLSegmentationNode()
{
  this->SegmentationModifiedCallbackCommand = vtkSmartPointer<vtkCallbackCommand>::New();
  this->SegmentationModifiedCallbackCommand->SetClientData(reinterpret_cast<void *>(this));
  this->SegmentationModifiedCallbackCommand->SetCallback(vtkMRMLSegmentationNode::SegmentationModifiedCallback);

  // Create empty segmentations object
  this->Segmentation = NULL;
  vtkSmartPointer<vtkSegmentation> segmentation = vtkSmartPointer<vtkSegmentation>::New();
  this->SetAndObserveSegmentation(segmentation);
}

//----------------------------------------------------------------------------
vtkMRMLSegmentationNode::~vtkMRMLSegmentationNode()
{
  this->SetAndObserveSegmentation(NULL);
  // Make sure this callback cannot call this object
  this->SegmentationModifiedCallbackCommand->SetClientData(NULL);
}

//----------------------------------------------------------------------------
void vtkMRMLSegmentationNode::WriteXML(ostream& of, int nIndent)
{
  Superclass::WriteXML(of, nIndent);
  if (this->Segmentation)
    {
    this->Segmentation->WriteXML(of, nIndent);
    }
}

//----------------------------------------------------------------------------
void vtkMRMLSegmentationNode::ReadXMLAttributes(const char** atts)
{
  // Read all MRML node attributes from two arrays of names and values
  int disabledModify = this->StartModify();

  Superclass::ReadXMLAttributes(atts);

  if (!this->Segmentation)
    {
    vtkSmartPointer<vtkSegmentation> segmentation = vtkSmartPointer<vtkSegmentation>::New();
    this->SetAndObserveSegmentation(segmentation);
    }
  this->Segmentation->ReadXMLAttributes(atts);

  this->EndModify(disabledModify);
}

//----------------------------------------------------------------------------
void vtkMRMLSegmentationNode::Copy(vtkMRMLNode *anode)
{
  vtkMRMLSegmentationNode* otherNode = vtkMRMLSegmentationNode::SafeDownCast(anode);
  if (!otherNode)
    {
    vtkErrorMacro("vtkMRMLSegmentationNode::Copy failed: invalid input node");
    return;
    }

  int wasModified = this->StartModify();

  Superclass::Copy(anode);

  if (otherNode->Segmentation)
    {
    if (!this->Segmentation)
      {
      vtkSmartPointer<vtkSegmentation> segmentation = vtkSmartPointer<vtkSegmentation>::New();
      this->SetAndObserveSegmentation(segmentation);
      }
    // Deep copy segmentation (containing the same segments from two segmentations is unstable)
    this->Segmentation->DeepCopy(otherNode->GetSegmentation());
    }
  else
    {
    this->SetAndObserveSegmentation(NULL);
    }

  this->EndModify(wasModified);
}

//----------------------------------------------------------------------------
void vtkMRMLSegmentationNode::DeepCopy(vtkMRMLNode* aNode)
{
  Copy(aNode);
}

//----------------------------------------------------------------------------
void vtkMRMLSegmentationNode::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os,indent);

  os << indent << "Segmentation:";
  if (this->Segmentation)
    {
    this->Segmentation->PrintSelf(os, indent.GetNextIndent());
    }
  else
    {
    os << " (invalid)\n";
    }
}

//----------------------------------------------------------------------------
void vtkMRMLSegmentationNode::SetAndObserveSegmentation(vtkSegmentation* segmentation)
{
  if (segmentation == this->Segmentation)
    {
    return;
    }

  // Remove segment event observations from previous segmentation
  if (this->Segmentation)
    {
    vtkEventBroker::GetInstance()->RemoveObservations(
      this->Segmentation, 0, this, this->SegmentationModifiedCallbackCommand);
    }

  this->SetSegmentation(segmentation);

  // Observe segment events in new segmentation
  if (this->Segmentation)
    {
    vtkEventBroker::GetInstance()->AddObservation(
      this->Segmentation, vtkSegmentation::MasterRepresentationModified, this, this->SegmentationModifiedCallbackCommand);
    vtkEventBroker::GetInstance()->AddObservation(
      this->Segmentation, vtkSegmentation::SegmentAdded, this, this->SegmentationModifiedCallbackCommand);
    vtkEventBroker::GetInstance()->AddObservation(
      this->Segmentation, vtkSegmentation::SegmentRemoved, this, this->SegmentationModifiedCallbackCommand);
    vtkEventBroker::GetInstance()->AddObservation(
      this->Segmentation, vtkSegmentation::SegmentModified, this, this->SegmentationModifiedCallbackCommand);
    vtkEventBroker::GetInstance()->AddObservation(
      this->Segmentation, vtkSegmentation::ContainedRepresentationNamesModified, this, this->SegmentationModifiedCallbackCommand);
    vtkEventBroker::GetInstance()->AddObservation(
      this->Segmentation, vtkSegmentation::RepresentationModified, this, this->SegmentationModifiedCallbackCommand);
    vtkEventBroker::GetInstance()->AddObservation(
      this->Segmentation, vtkSegmentation::SegmentsOrderModified, this, this->SegmentationModifiedCallbackCommand);
  }
}

//---------------------------------------------------------------------------
void vtkMRMLSegmentationNode::SegmentationModifiedCallback(vtkObject* vtkNotUsed(caller), unsigned long eid, void* clientData, void* callData)
{
  vtkMRMLSegmentationNode* self = reinterpret_cast<vtkMRMLSegmentationNode*>(clientData);
  if (!self)
    {
    return;
    }
  if (!self->Segmentation)
    {
    // this object is being deleted
    return;
    }
  switch (eid)
    {
    case vtkSegmentation::MasterRepresentationModified:
      self->OnMasterRepresentationModified();
      self->InvokeCustomModifiedEvent(eid, callData);
      break;
    case vtkSegmentation::RepresentationModified:
      self->StorableModifiedTime.Modified();
      self->InvokeCustomModifiedEvent(eid, callData);
      break;
    case vtkSegmentation::ContainedRepresentationNamesModified:
      self->StorableModifiedTime.Modified();
      self->InvokeCustomModifiedEvent(eid);
      break;
    case vtkSegmentation::SegmentAdded:
      self->StorableModifiedTime.Modified();
      self->OnSegmentAdded(reinterpret_cast<char*>(callData));
      self->InvokeCustomModifiedEvent(eid, callData);
      break;
    case vtkSegmentation::SegmentRemoved:
      self->StorableModifiedTime.Modified();
      self->OnSegmentRemoved(reinterpret_cast<char*>(callData));
      self->InvokeCustomModifiedEvent(eid, callData);
      break;
    case vtkSegmentation::SegmentModified:
      self->StorableModifiedTime.Modified();
      self->OnSegmentModified(reinterpret_cast<char*>(callData));
      self->InvokeCustomModifiedEvent(eid, callData);
      break;
    case vtkSegmentation::SegmentsOrderModified:
      self->StorableModifiedTime.Modified();
      self->InvokeCustomModifiedEvent(eid);
      break;
    default:
      vtkErrorWithObjectMacro(self, "vtkMRMLSegmentationNode::SegmentationModifiedCallback: Unknown event id "<<eid);
      return;
    }
}

//---------------------------------------------------------------------------
void vtkMRMLSegmentationNode::OnMasterRepresentationModified()
{
  // Reset supported write file types
  vtkMRMLSegmentationStorageNode* storageNode =  vtkMRMLSegmentationStorageNode::SafeDownCast(this->GetStorageNode());
  if (storageNode)
    {
    storageNode->ResetSupportedWriteFileTypes();
    }
}

//---------------------------------------------------------------------------
void vtkMRMLSegmentationNode::OnSegmentAdded(const char* vtkNotUsed(segmentId))
{
}

//---------------------------------------------------------------------------
void vtkMRMLSegmentationNode::OnSegmentRemoved(const char* vtkNotUsed(segmentId))
{
}

//---------------------------------------------------------------------------
void vtkMRMLSegmentationNode::OnSegmentModified(const char* vtkNotUsed(segmentId))
{
}

//---------------------------------------------------------------------------
void vtkMRMLSegmentationNode::OnSubjectHierarchyUIDAdded(
  vtkMRMLSubjectHierarchyNode* shNode, vtkIdType itemWithNewUID )
{
  if (!shNode || !this->Segmentation || !itemWithNewUID)
    {
    return;
    }
  // If already has geometry, then do not look for a new one
  if (!this->Segmentation->GetConversionParameter(vtkSegmentationConverter::GetReferenceImageGeometryParameterName()).empty())
    {
    return;
    }
  // If the new UID is empty string, then do not look for the segmentation's referenced UID in its UID list
  std::string itemUidValueStr = shNode->GetItemUID(itemWithNewUID, vtkMRMLSubjectHierarchyConstants::GetDICOMInstanceUIDName());
  if (itemUidValueStr.empty())
    {
    return;
    }

  // Get volume node from subject hierarchy item with new UID
  vtkMRMLScalarVolumeNode* referencedVolumeNode = vtkMRMLScalarVolumeNode::SafeDownCast(
    shNode->GetItemDataNode(itemWithNewUID) );
  if (!referencedVolumeNode)
    {
    // If associated node is not a volume, then return
    return;
    }

  // Get associated subject hierarchy item
  vtkIdType segmentationShItemID = shNode->GetItemByDataNode(this);
  if (!segmentationShItemID)
    {
    // If segmentation is not in subject hierarchy, then we cannot find its DICOM references
    return;
    }

  // Get DICOM references from segmentation subject hierarchy node
  std::string referencedInstanceUIDsAttribute = shNode->GetItemAttribute(
    segmentationShItemID, vtkMRMLSubjectHierarchyConstants::GetDICOMReferencedInstanceUIDsAttributeName() );
  if (referencedInstanceUIDsAttribute.empty())
    {
    // No references
    return;
    }

  // If the subject hierarchy node that got a new UID has a DICOM instance UID referenced
  // from this segmentation, then use its geometry as image geometry conversion parameter
  std::vector<std::string> referencedSopInstanceUids;
  vtkMRMLSubjectHierarchyNode::DeserializeUIDList(referencedInstanceUIDsAttribute, referencedSopInstanceUids);
  bool referencedVolumeFound = false;
  bool warningLogged = false;
  std::vector<std::string>::iterator uidIt;
  for (uidIt = referencedSopInstanceUids.begin(); uidIt != referencedSopInstanceUids.end(); ++uidIt)
    {
    // If we find the instance UID, then we set the geometry
    if (itemUidValueStr.find(*uidIt) != std::string::npos)
      {
      // Only set the reference once, but check all UIDs
      if (!referencedVolumeFound)
        {
        // Set reference image geometry parameter if volume node is found
        this->SetReferenceImageGeometryParameterFromVolumeNode(referencedVolumeNode);
        referencedVolumeFound = true;
        }
      }
    // If referenced UID is not contained in found node, then warn user
    else if (referencedVolumeFound && !warningLogged)
      {
      vtkWarningMacro("vtkMRMLSegmentationNode::OnSubjectHierarchyUIDAdded: Referenced volume for segmentation '"
        << this->Name << "' found (" << referencedVolumeNode->GetName() << "), but some referenced UIDs are not present in it! (maybe only partial volume was loaded?)");
      // Only log warning once for this node
      warningLogged = true;
      }
    }
}

//----------------------------------------------------------------------------
vtkMRMLStorageNode* vtkMRMLSegmentationNode::CreateDefaultStorageNode()
{
  return vtkMRMLSegmentationStorageNode::New();
}

//----------------------------------------------------------------------------
void vtkMRMLSegmentationNode::CreateDefaultDisplayNodes()
{
  if (vtkMRMLSegmentationDisplayNode::SafeDownCast(this->GetDisplayNode()))
    {
    // Display node already exists
    return;
    }
  if (this->GetScene() == NULL)
    {
    vtkErrorMacro("vtkMRMLSegmentationNode::CreateDefaultDisplayNodes failed: Scene is invalid");
    return;
    }
  vtkNew<vtkMRMLSegmentationDisplayNode> displayNode;
  this->GetScene()->AddNode(displayNode.GetPointer());
  this->SetAndObserveDisplayNodeID(displayNode->GetID());
}

//---------------------------------------------------------------------------
void vtkMRMLSegmentationNode::ApplyTransformMatrix(vtkMatrix4x4* transformMatrix)
{
  vtkTransform* transform = vtkTransform::New();
  transform->SetMatrix(transformMatrix);
  this->ApplyTransform(transform);
  transform->Delete();
}

//----------------------------------------------------------------------------
void vtkMRMLSegmentationNode::ApplyTransform(vtkAbstractTransform* transform)
{
  if (!this->Segmentation)
    {
    return;
    }

  // Apply transform on segmentation
  vtkSmartPointer<vtkTransform> linearTransform = vtkSmartPointer<vtkTransform>::New();
  if (vtkOrientedImageDataResample::IsTransformLinear(transform, linearTransform))
    {
    this->Segmentation->ApplyLinearTransform(transform);
    }
  else
    {
    this->Segmentation->ApplyNonLinearTransform(transform);
    }

  // Make sure preferred display representations exist after transformation
  // (it is invalidated in the process unless it is the master representation)
  char* preferredDisplayRepresentation2D = NULL;
  char* preferredDisplayRepresentation3D = NULL;
  vtkMRMLSegmentationDisplayNode* displayNode = vtkMRMLSegmentationDisplayNode::SafeDownCast(this->GetDisplayNode());
  if (displayNode)
  {
    preferredDisplayRepresentation2D = displayNode->GetPreferredDisplayRepresentationName2D();
    preferredDisplayRepresentation3D = displayNode->GetPreferredDisplayRepresentationName3D();
  }

  // Make sure preferred display representations exist after transformation
  // (it was invalidated in the process unless it is the master representation)
  if (displayNode)
    {
    if (preferredDisplayRepresentation2D)
      {
      this->Segmentation->CreateRepresentation(preferredDisplayRepresentation2D);
      }
    if (preferredDisplayRepresentation3D)
      {
      this->Segmentation->CreateRepresentation(preferredDisplayRepresentation3D);
      }
    // Need to set preferred representations again, as conversion sets them to the last converted one
    displayNode->SetPreferredDisplayRepresentationName2D(preferredDisplayRepresentation2D);
    displayNode->SetPreferredDisplayRepresentationName3D(preferredDisplayRepresentation3D);
    }
}

//----------------------------------------------------------------------------
bool vtkMRMLSegmentationNode::CanApplyNonLinearTransforms() const
{
  return true;
}

//---------------------------------------------------------------------------
// Global RAS in the form (Xmin, Xmax, Ymin, Ymax, Zmin, Zmax)
//---------------------------------------------------------------------------
void vtkMRMLSegmentationNode::GetRASBounds(double bounds[6])
{
  if (this->GetParentTransformNode() == NULL)
    {
    // Segmentation is not transformed
    this->GetBounds(bounds);
    }
  else
    {
    // Segmentation is transformed
    vtkNew<vtkGeneralTransform> segmentationToRASTransform;
    vtkMRMLTransformNode::GetTransformBetweenNodes(this->GetParentTransformNode(), NULL, segmentationToRASTransform.GetPointer());
    double bounds_Segmentation[6] = { 1, -1, 1, -1, 1, -1 };
    this->GetBounds(bounds_Segmentation);
    vtkOrientedImageDataResample::TransformBounds(bounds_Segmentation, segmentationToRASTransform.GetPointer(), bounds);
    }
}

//---------------------------------------------------------------------------
void vtkMRMLSegmentationNode::GetBounds(double bounds[6])
{
  vtkMath::UninitializeBounds(bounds);
  if (this->Segmentation)
    {
    this->Segmentation->GetBounds(bounds);
    }
}

//---------------------------------------------------------------------------
bool vtkMRMLSegmentationNode::GenerateMergedLabelmap(
  vtkOrientedImageData* mergedImageData,
  int extentComputationMode,
  vtkOrientedImageData* mergedLabelmapGeometry/*=NULL*/,
  const std::vector<std::string>& segmentIDs/*=std::vector<std::string>()*/
  )
{
  if (!mergedImageData)
    {
    vtkErrorMacro("GenerateMergedLabelmap: Invalid image data");
    return false;
    }
  // If segmentation is missing or empty then we cannot create a merged image data
  if (!this->Segmentation)
    {
    vtkErrorMacro("GenerateMergedLabelmap: Invalid segmentation");
    return false;
    }
  std::cout << "Generating" <<std::endl;
  bool masterRepresentationIsFractionalLabelmap = this->GetSegmentation()->GetMasterRepresentationName() == vtkSegmentationConverter::GetSegmentationFractionalLabelmapRepresentationName();

  if (!( this->Segmentation->ContainsRepresentation(vtkSegmentationConverter::GetSegmentationBinaryLabelmapRepresentationName()) && !masterRepresentationIsFractionalLabelmap ) &&
      !( this->Segmentation->ContainsRepresentation(vtkSegmentationConverter::GetSegmentationFractionalLabelmapRepresentationName()) && masterRepresentationIsFractionalLabelmap))
    {
    vtkErrorMacro("GenerateMergedLabelmap: Segmentation does not contain a labelmap representation");
    return false;
    }

  // If segment IDs list is empty then include all segments
  std::vector<std::string> mergedSegmentIDs;
  if (segmentIDs.empty())
    {
    this->Segmentation->GetSegmentIDs(mergedSegmentIDs);
    }
  else
    {
    mergedSegmentIDs = segmentIDs;
    }

  // Determine common labelmap geometry that will be used for the merged labelmap
  vtkSmartPointer<vtkMatrix4x4> mergedImageToWorldMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
  vtkSmartPointer<vtkOrientedImageData> commonGeometryImage;
  if (mergedLabelmapGeometry)
    {
    // Use merged labelmap geometry if provided
    commonGeometryImage = mergedLabelmapGeometry;
    mergedLabelmapGeometry->GetImageToWorldMatrix(mergedImageToWorldMatrix);
    }
  else
    {
    commonGeometryImage = vtkSmartPointer<vtkOrientedImageData>::New();
    std::string commonGeometryString = this->Segmentation->DetermineCommonLabelmapGeometry(extentComputationMode, mergedSegmentIDs);
    if (commonGeometryString.empty())
      {
      // This can occur if there are only empty segments in the segmentation
      mergedImageToWorldMatrix->Identity();
      return true;
      }
    vtkSegmentationConverter::DeserializeImageGeometry(commonGeometryString, commonGeometryImage);
    }

  commonGeometryImage->GetImageToWorldMatrix(mergedImageToWorldMatrix);
  int referenceDimensions[3] = {0,0,0};
  commonGeometryImage->GetDimensions(referenceDimensions);
  int referenceExtent[6] = {0,-1,0,-1,0,-1};
  commonGeometryImage->GetExtent(referenceExtent);

  // Allocate image data if empty or if reference extent changed
  int imageDataExtent[6] = {0,-1,0,-1,0,-1};
  mergedImageData->GetExtent(imageDataExtent);
  if ( mergedImageData->GetScalarType() != VTK_SHORT
    || imageDataExtent[0] != referenceExtent[0] || imageDataExtent[1] != referenceExtent[1] || imageDataExtent[2] != referenceExtent[2]
    || imageDataExtent[3] != referenceExtent[3] || imageDataExtent[4] != referenceExtent[4] || imageDataExtent[5] != referenceExtent[5] )
    {
    if (mergedImageData->GetPointData()->GetScalars() && mergedImageData->GetScalarType() != VTK_SHORT)
      {
      vtkWarningMacro("GenerateMergedLabelmap: Merged image data scalar type is not short. Allocating using short.");
      }
    mergedImageData->SetExtent(referenceExtent);
    mergedImageData->AllocateScalars(VTK_SHORT, 1);
    }
  mergedImageData->SetImageToWorldMatrix(mergedImageToWorldMatrix);

  // Paint the image data background first
  short* mergedImagePtr = (short*)mergedImageData->GetScalarPointerForExtent(referenceExtent);
  if (!mergedImagePtr)
    {
    // Setting the extent may invoke this function again via ImageDataModified, in which case the pointer is NULL
    return false;
    }

  const short backgroundColorIndex = 0;

  double scalarRange[2] = {0.0, 1.0};
  if (masterRepresentationIsFractionalLabelmap)
    {
    vtkDoubleArray* scalarRangeArray = vtkDoubleArray::SafeDownCast(
      this->Segmentation->GetNthSegment(0)->GetRepresentation(
      vtkSegmentationConverter::GetSegmentationFractionalLabelmapRepresentationName())->GetFieldData()->GetAbstractArray(vtkSegmentationConverter::GetScalarRangeFieldName()));
    if (scalarRangeArray && scalarRangeArray->GetNumberOfValues() == 2)
      {
      scalarRange[0] = scalarRangeArray->GetValue(0);
      scalarRange[1] = scalarRangeArray->GetValue(1);
      }
    }

  if (masterRepresentationIsFractionalLabelmap)
    {
    vtkOrientedImageDataResample::FillImage(mergedImageData, scalarRange[0]);
    }
  else
    {
    vtkOrientedImageDataResample::FillImage(mergedImageData, backgroundColorIndex);
    }

  // Skip the rest if there are no segments
  if (this->Segmentation->GetNumberOfSegments() == 0)
    {
    return true;
    }

  // Create merged labelmap
  short colorIndex = backgroundColorIndex + 1;
  for (std::vector<std::string>::iterator segmentIdIt = mergedSegmentIDs.begin(); segmentIdIt != mergedSegmentIDs.end(); ++segmentIdIt, ++colorIndex)
    {
    std::string currentSegmentId = *segmentIdIt;
    vtkSegment* currentSegment = this->Segmentation->GetSegment(currentSegmentId);
    if (!currentSegment)
      {
      vtkWarningMacro("GenerateMergedLabelmap: Segment not found: " << currentSegmentId);
      continue;
      }

    // Get labelmap from segment
    vtkOrientedImageData* representationLabelmap;

    if (masterRepresentationIsFractionalLabelmap)
      {
      representationLabelmap = vtkOrientedImageData::SafeDownCast(
        currentSegment->GetRepresentation(vtkSegmentationConverter::GetSegmentationFractionalLabelmapRepresentationName()) );
      }
    else
      {
      representationLabelmap = vtkOrientedImageData::SafeDownCast(
        currentSegment->GetRepresentation(vtkSegmentationConverter::GetSegmentationBinaryLabelmapRepresentationName()) );
      }

    // If labelmap is empty then skip
    if (representationLabelmap->IsEmpty())
      {
      continue;
      }

    // Set oriented image data used for merging to the representation (may change later if resampling is needed)
    vtkOrientedImageData* labelmap = representationLabelmap;

    // If labelmap geometries (origin, spacing, and directions) do not match reference then resample temporarily
    vtkSmartPointer<vtkOrientedImageData> resampledLabelmap;
    if (!vtkOrientedImageDataResample::DoGeometriesMatch(commonGeometryImage, representationLabelmap))
      {
      resampledLabelmap = vtkSmartPointer<vtkOrientedImageData>::New();

      // Resample segment labelmap for merging
      if (!vtkOrientedImageDataResample::ResampleOrientedImageToReferenceGeometry(representationLabelmap, mergedImageToWorldMatrix, resampledLabelmap, masterRepresentationIsFractionalLabelmap))
        {
        continue;
        }

      // Use resampled labelmap for merging
      labelmap = resampledLabelmap;
      }

      // Copy image data voxels into merged labelmap with the proper color index
      vtkOrientedImageDataResample::ModifyImage(
            mergedImageData,
            labelmap,
            masterRepresentationIsFractionalLabelmap ? vtkOrientedImageDataResample::OPERATION_FRACTIONAL_ADDITION : vtkOrientedImageDataResample::OPERATION_MASKING,
            NULL,
            0,
            colorIndex,
            masterRepresentationIsFractionalLabelmap ? scalarRange[0] : VTK_DOUBLE_MIN,
            masterRepresentationIsFractionalLabelmap ? scalarRange[1] : VTK_DOUBLE_MAX);
      }

  if (masterRepresentationIsFractionalLabelmap)
    {
    // Specify the scalar range of values in the labelmap
    vtkSmartPointer<vtkDoubleArray> scalarRangeArray = vtkSmartPointer<vtkDoubleArray>::New();
    scalarRangeArray->SetName(vtkSegmentationConverter::GetScalarRangeFieldName());
    scalarRangeArray->InsertNextValue(scalarRange[0]);
    scalarRangeArray->InsertNextValue(scalarRange[1]);
    mergedImageData->GetFieldData()->AddArray(scalarRangeArray);
    //TODO
    // Specify the surface threshold value for visualization
    vtkSmartPointer<vtkDoubleArray> thresholdValueArray = vtkSmartPointer<vtkDoubleArray>::New();
    thresholdValueArray->SetName(vtkSegmentationConverter::GetThresholdValueFieldName());
    thresholdValueArray->InsertNextValue(0);
    mergedImageData->GetFieldData()->AddArray(thresholdValueArray);
    //TODO
    // Specify the interpolation type for visualization
    vtkSmartPointer<vtkIntArray> interpolationTypeArray = vtkSmartPointer<vtkIntArray>::New();
    interpolationTypeArray->SetName(vtkSegmentationConverter::GetInterpolationTypeFieldName());
    interpolationTypeArray->InsertNextValue(VTK_LINEAR_INTERPOLATION);
    mergedImageData->GetFieldData()->AddArray(interpolationTypeArray);
    }

  return true;
}

//---------------------------------------------------------------------------
bool vtkMRMLSegmentationNode::GenerateMergedLabelmapForAllSegments(vtkOrientedImageData* mergedImageData, int extentComputationMode,
  vtkOrientedImageData* mergedLabelmapGeometry /*=NULL*/, vtkStringArray* segmentIDs /*=NULL*/)
{
  std::vector<std::string> segmentIDsVector;
  if (segmentIDs)
    {
    for (int i = 0; i < segmentIDs->GetNumberOfValues(); i++)
      {
      segmentIDsVector.push_back(segmentIDs->GetValue(i));
      }
    }
  return this->GenerateMergedLabelmap(mergedImageData, extentComputationMode, mergedLabelmapGeometry, segmentIDsVector);
}

//---------------------------------------------------------------------------
vtkIdType vtkMRMLSegmentationNode::GetSegmentSubjectHierarchyItem(std::string segmentID, vtkMRMLSubjectHierarchyNode* shNode)
{
  if (!shNode)
    {
    vtkErrorMacro("GetSegmentSubjectHierarchyItem: Invalid subject hierarchy");
    return vtkMRMLSubjectHierarchyNode::INVALID_ITEM_ID;
    }

  vtkIdType segmentationvtkIdType = shNode->GetItemByDataNode(this);
  if (!segmentationvtkIdType)
    {
    return vtkMRMLSubjectHierarchyNode::INVALID_ITEM_ID;
    }

  // Find child item of segmentation subject hierarchy item that has the requested segment ID
  std::vector<vtkIdType> segmentationChildItemIDs;
  shNode->GetItemChildren(segmentationvtkIdType, segmentationChildItemIDs, false);

  std::vector<vtkIdType>::iterator childIt;
  for (childIt=segmentationChildItemIDs.begin(); childIt!=segmentationChildItemIDs.end(); ++childIt)
    {
    vtkIdType childItemID = (*childIt);
    std::string childSegmentID = shNode->GetItemAttribute(childItemID, vtkMRMLSegmentationNode::GetSegmentIDAttributeName());
    if (!childSegmentID.empty() && !childSegmentID.compare(segmentID))
      {
      return childItemID;
      }
    }

  return vtkMRMLSubjectHierarchyNode::INVALID_ITEM_ID;
}

//---------------------------------------------------------------------------
void vtkMRMLSegmentationNode::SetReferenceImageGeometryParameterFromVolumeNode(vtkMRMLScalarVolumeNode* volumeNode)
{
  if (!volumeNode || !volumeNode->GetImageData())
    {
    return;
    }
  if (!this->Segmentation)
    {
    vtkSmartPointer<vtkSegmentation> segmentation = vtkSmartPointer<vtkSegmentation>::New();
    this->SetAndObserveSegmentation(segmentation);
    }

  // Get serialized geometry of selected volume
  vtkSmartPointer<vtkMatrix4x4> volumeIjkToRasMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
  volumeNode->GetIJKToRASMatrix(volumeIjkToRasMatrix);

  // If there is a linear transform between the reference volume and segmentation then transform the geometry
  // to be aligned with the reference volume.
  if (volumeNode->GetParentTransformNode() != this->GetParentTransformNode())
    {
    vtkSmartPointer<vtkGeneralTransform> volumeToSegmentationTransform = vtkSmartPointer<vtkGeneralTransform>::New();
    vtkMRMLTransformNode::GetTransformBetweenNodes(volumeNode->GetParentTransformNode(), this->GetParentTransformNode(), volumeToSegmentationTransform);
    if (vtkMRMLTransformNode::IsGeneralTransformLinear(volumeToSegmentationTransform))
      {
      vtkNew<vtkMatrix4x4> volumeToSegmentationMatrix;
      vtkMRMLTransformNode::GetMatrixTransformBetweenNodes(volumeNode->GetParentTransformNode(), this->GetParentTransformNode(), volumeToSegmentationMatrix.GetPointer());
      vtkMatrix4x4::Multiply4x4(volumeToSegmentationMatrix.GetPointer(), volumeIjkToRasMatrix, volumeIjkToRasMatrix);
      }
    }

  std::string serializedImageGeometry = vtkSegmentationConverter::SerializeImageGeometry(
    volumeIjkToRasMatrix, volumeNode->GetImageData() );

  // Set parameter
  this->Segmentation->SetConversionParameter(
    vtkSegmentationConverter::GetReferenceImageGeometryParameterName(), serializedImageGeometry);

  // Set node reference from segmentation to reference geometry volume
  this->SetNodeReferenceID(
    vtkMRMLSegmentationNode::GetReferenceImageGeometryReferenceRole().c_str(), volumeNode->GetID() );
}

//---------------------------------------------------------------------------
std::string vtkMRMLSegmentationNode::AddSegmentFromClosedSurfaceRepresentation(vtkPolyData* polyData,
  std::string segmentName/* ="" */, double color[3] /* =NULL */,
  std::string vtkNotUsed(segmentId)/* ="" */)
{
  if (!this->Segmentation)
    {
    vtkErrorMacro("AddSegmentFromClosedSurfaceRepresentation: Invalid segmentation");
    return "";
    }
  vtkNew<vtkSegment> newSegment;
  if (!segmentName.empty())
    {
    newSegment->SetName(segmentName.c_str());
    }
  if (color!=NULL)
    {
    newSegment->SetColor(color);
    }
  newSegment->AddRepresentation(vtkSegmentationConverter::GetSegmentationClosedSurfaceRepresentationName(), polyData);
  if (!this->Segmentation->AddSegment(newSegment.GetPointer()))
    {
    return "";
    }
  return this->Segmentation->GetSegmentIdBySegment(newSegment.GetPointer());
}

//---------------------------------------------------------------------------
std::string vtkMRMLSegmentationNode::AddSegmentFromBinaryLabelmapRepresentation(vtkOrientedImageData* imageData,
  std::string segmentName/* ="" */, double color[3] /* =NULL */,
  std::string vtkNotUsed(segmentId)/* ="" */)
{
  if (!this->Segmentation)
    {
    vtkErrorMacro("AddSegmentFromBinaryLabelmapRepresentation: Invalid segmentation");
    return "";
    }
  vtkNew<vtkSegment> newSegment;
  if (!segmentName.empty())
    {
    newSegment->SetName(segmentName.c_str());
    }
  if (color != NULL)
    {
    newSegment->SetColor(color);
    }
  newSegment->AddRepresentation(vtkSegmentationConverter::GetSegmentationBinaryLabelmapRepresentationName(), imageData);
  if (!this->Segmentation->AddSegment(newSegment.GetPointer()))
    {
    return "";
    }
  return this->Segmentation->GetSegmentIdBySegment(newSegment.GetPointer());
}

//---------------------------------------------------------------------------
void vtkMRMLSegmentationNode::RemoveSegment(const std::string& segmentID)
{
  if (!this->Segmentation)
    {
    vtkErrorMacro("RemoveSegment: Invalid segmentation");
    return;
    }
  this->Segmentation->RemoveSegment(segmentID);
}

//---------------------------------------------------------------------------
bool vtkMRMLSegmentationNode::CreateBinaryLabelmapRepresentation()
{
  if (!this->Segmentation)
    {
    vtkErrorMacro("CreateBinaryLabelmapRepresentation: Invalid segmentation");
    return false;
    }
  return this->Segmentation->CreateRepresentation(vtkSegmentationConverter::GetSegmentationBinaryLabelmapRepresentationName());
}

//---------------------------------------------------------------------------
void vtkMRMLSegmentationNode::RemoveBinaryLabelmapRepresentation()
{
  if (!this->Segmentation)
    {
    vtkErrorMacro("RemoveBinaryLabelmapRepresentation: Invalid segmentation");
    return;
    }
  this->Segmentation->RemoveRepresentation(vtkSegmentationConverter::GetSegmentationBinaryLabelmapRepresentationName());
}

//---------------------------------------------------------------------------
vtkOrientedImageData* vtkMRMLSegmentationNode::GetBinaryLabelmapRepresentation(const std::string segmentId)
{
  if (!this->Segmentation)
    {
    vtkErrorMacro("GetBinaryLabelmapRepresentation: Invalid segmentation");
    return NULL;
    }
  vtkSegment* segment = this->Segmentation->GetSegment(segmentId);
  if (!segment)
    {
    vtkErrorMacro("GetBinaryLabelmapRepresentation: Invalid segment");
    return NULL;
    }
  return vtkOrientedImageData::SafeDownCast(segment->GetRepresentation(vtkSegmentationConverter::GetSegmentationBinaryLabelmapRepresentationName()));
}

//---------------------------------------------------------------------------
bool vtkMRMLSegmentationNode::CreateClosedSurfaceRepresentation()
{
  if (!this->Segmentation)
    {
    vtkErrorMacro("CreateClosedSurfaceRepresentation: Invalid segmentation");
    return false;
    }
  return this->Segmentation->CreateRepresentation(vtkSegmentationConverter::GetSegmentationClosedSurfaceRepresentationName());
}

//---------------------------------------------------------------------------
void vtkMRMLSegmentationNode::RemoveClosedSurfaceRepresentation()
{
  if (!this->Segmentation)
    {
    vtkErrorMacro("RemoveClosedSurfaceRepresentation: Invalid segmentation");
    return;
    }
  this->Segmentation->RemoveRepresentation(vtkSegmentationConverter::GetSegmentationClosedSurfaceRepresentationName());
}

//---------------------------------------------------------------------------
vtkPolyData* vtkMRMLSegmentationNode::GetClosedSurfaceRepresentation(const std::string segmentId)
{
  if (!this->Segmentation)
    {
    vtkErrorMacro("GetClosedSurfaceRepresentation: Invalid segmentation");
    return NULL;
    }
  vtkSegment* segment = this->Segmentation->GetSegment(segmentId);
  if (!segment)
    {
    vtkErrorMacro("GetClosedSurfaceRepresentation: Invalid segment");
    return NULL;
    }
  return vtkPolyData::SafeDownCast(segment->GetRepresentation(vtkSegmentationConverter::GetSegmentationClosedSurfaceRepresentationName()));
}

//---------------------------------------------------------------------------
bool vtkMRMLSegmentationNode::SetMasterRepresentationToBinaryLabelmap()
{
  if (!this->Segmentation)
    {
    vtkErrorMacro("SetMasterRepresentationToBinaryLabelmap: Invalid segmentation");
    return false;
    }
  this->Segmentation->SetMasterRepresentationName(vtkSegmentationConverter::GetSegmentationBinaryLabelmapRepresentationName());
  return true;
}

//---------------------------------------------------------------------------
bool vtkMRMLSegmentationNode::SetMasterRepresentationToClosedSurface()
{
  if (!this->Segmentation)
    {
    vtkErrorMacro("SetMasterRepresentationToClosedSurface: Invalid segmentation");
    return false;
    }
  this->Segmentation->SetMasterRepresentationName(vtkSegmentationConverter::GetSegmentationClosedSurfaceRepresentationName());
  return true;
}
