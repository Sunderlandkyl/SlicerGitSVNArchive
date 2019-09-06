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
#include "vtkBinaryLabelmapToClosedSurfaceConversionRule.h"
#include "vtkSegmentation.h"

#include "vtkOrientedImageData.h"

// VTK includes
#include <vtkVersion.h> // must precede reference to VTK_MAJOR_VERSION
#include <vtkDecimatePro.h>
#if VTK_MAJOR_VERSION >= 9 || (VTK_MAJOR_VERSION >= 8 && VTK_MINOR_VERSION >= 2)
  #include <vtkDiscreteFlyingEdges3D.h>
#else
  #include <vtkDiscreteMarchingCubes.h>
#endif
#include <vtkGeometryFilter.h>
#include <vtkImageChangeInformation.h>
#include <vtkImageConstantPad.h>
#include <vtkImageThreshold.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPolyData.h>
#include <vtkPolyDataNormals.h>
#include <vtkThreshold.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkWindowedSincPolyDataFilter.h>
#include <vtkMatrix3x3.h>
#include <vtkReverseSense.h>
#include <vtkStringToNumeric.h>
#include <vtkStringArray.h>

//----------------------------------------------------------------------------
vtkSegmentationConverterRuleNewMacro(vtkBinaryLabelmapToClosedSurfaceConversionRule);

//----------------------------------------------------------------------------
vtkBinaryLabelmapToClosedSurfaceConversionRule::vtkBinaryLabelmapToClosedSurfaceConversionRule()
{
  this->ConversionParameters[GetDecimationFactorParameterName()] = std::make_pair("0.0",
    "Desired reduction in the total number of polygons. Range: 0.0 (no decimation) to 1.0 (as much simplification as possible)."
    " Value of 0.8 typically reduces data set size by 80% without losing too much details.");
  this->ConversionParameters[GetSmoothingFactorParameterName()] = std::make_pair("0.5",
    "Smoothing factor. Range: 0.0 (no smoothing) to 1.0 (strong smoothing).");
  this->ConversionParameters[GetComputeSurfaceNormalsParameterName()] = std::make_pair("1",
    "Compute surface normals. 1 (default) = surface normals are computed. "
    "0 = surface normals are not computed (slightly faster but produces less smooth surface display).");
  this->CurrentLabelValue = 1;
}

//----------------------------------------------------------------------------
vtkBinaryLabelmapToClosedSurfaceConversionRule::~vtkBinaryLabelmapToClosedSurfaceConversionRule()
= default;

//----------------------------------------------------------------------------
unsigned int vtkBinaryLabelmapToClosedSurfaceConversionRule::GetConversionCost(
    vtkDataObject* vtkNotUsed(sourceRepresentation)/*=nullptr*/,
    vtkDataObject* vtkNotUsed(targetRepresentation)/*=nullptr*/)
{
  // Rough input-independent guess (ms)
  return 500;
}

//----------------------------------------------------------------------------
vtkDataObject* vtkBinaryLabelmapToClosedSurfaceConversionRule::ConstructRepresentationObjectByRepresentation(std::string representationName)
{
  if ( !representationName.compare(this->GetSourceRepresentationName()) )
    {
    return (vtkDataObject*)vtkOrientedImageData::New();
    }
  else if ( !representationName.compare(this->GetTargetRepresentationName()) )
    {
    return (vtkDataObject*)vtkPolyData::New();
    }
  else
    {
    return nullptr;
    }
}

//----------------------------------------------------------------------------
vtkDataObject* vtkBinaryLabelmapToClosedSurfaceConversionRule::ConstructRepresentationObjectByClass(std::string className)
{
  if (!className.compare("vtkOrientedImageData"))
    {
    return (vtkDataObject*)vtkOrientedImageData::New();
    }
  else if (!className.compare("vtkPolyData"))
    {
    return (vtkDataObject*)vtkPolyData::New();
    }
  else
    {
    return nullptr;
    }
}

//----------------------------------------------------------------------------
bool vtkBinaryLabelmapToClosedSurfaceConversionRule::PreConvert(vtkSegmentation* segmentation, vtkSegment* segment)
{
  // Check validity of source and target representation objects
  vtkOrientedImageData* orientedBinaryLabelmap = vtkOrientedImageData::SafeDownCast(
    segment->GetRepresentation(vtkSegmentationConverter::GetBinaryLabelmapRepresentationName()));
  if (!orientedBinaryLabelmap)
    {
    vtkErrorMacro("Convert: Source representation is not oriented image data");
    return false;
    }
  vtkSmartPointer<vtkImageData> binaryLabelmap = vtkImageData::SafeDownCast(
    segment->GetRepresentation(vtkSegmentationConverter::GetBinaryLabelmapRepresentationName()));
    if (!binaryLabelmap.GetPointer())
      {
    vtkErrorMacro("Convert: Source representation is not data");
    return false;
      }

    this->CurrentSegment = segmentation->GetSegmentIdBySegment(segment);
    this->CurrentLabelValue = segment->GetLabelmapValue();

    vtkMTimeType inputMTime = 0;

    std::vector<std::string> mergedSegmentIDs;
    segmentation->GetMergedLabelmapSegmentIds(segment, mergedSegmentIDs, true);
    for (std::vector<std::string>::iterator segmentIDIt = mergedSegmentIDs.begin(); segmentIDIt != mergedSegmentIDs.end(); ++segmentIDIt)
      {
      std::string currentSegmentID = *segmentIDIt;
      if (this->InputLabelmaps.find(currentSegmentID) == this->InputLabelmaps.end())
        {
        this->InputMTime[currentSegmentID] = 0;
        }
      else
        {
        if (this->InputLabelmaps[currentSegmentID] == orientedBinaryLabelmap)
          {
          inputMTime = this->InputMTime[currentSegmentID];
          }
        }
      this->InputLabelmaps[currentSegmentID] = orientedBinaryLabelmap;
      this->InputMTime[currentSegmentID] = orientedBinaryLabelmap->GetMTime();
      }

  if (orientedBinaryLabelmap->GetMTime() <= inputMTime)
    {
    return true;
    }

  // Pad labelmap if it has non-background border voxels
    int* binaryLabelmapExtent = binaryLabelmap->GetExtent();
  if (binaryLabelmapExtent[0] > binaryLabelmapExtent[1]
    || binaryLabelmapExtent[2] > binaryLabelmapExtent[3]
    || binaryLabelmapExtent[4] > binaryLabelmapExtent[5])
    {
    // empty labelmap
    vtkDebugMacro("Convert: No polygons can be created, input image extent is empty");
    //closedSurfacePolyData->Reset();
    return true;
    }

  /// If input labelmap has non-background border voxels, then those regions remain open in the output closed surface.
  /// This function adds a 1 voxel padding to the labelmap in these cases.
  bool paddingNecessary = this->IsLabelmapPaddingNecessary(binaryLabelmap);
  if (paddingNecessary)
    {
    vtkSmartPointer<vtkImageConstantPad> padder = vtkSmartPointer<vtkImageConstantPad>::New();
    padder->SetInputData(binaryLabelmap);
    int extent[6] = { 0, -1, 0, -1, 0, -1 };
    binaryLabelmap->GetExtent(extent);
    // Set the output extent to the new size
    padder->SetOutputWholeExtent(extent[0] - 1, extent[1] + 1, extent[2] - 1, extent[3] + 1, extent[4] - 1, extent[5] + 1);
    padder->Update();
    binaryLabelmap = padder->GetOutput();
    }

  // Clone labelmap and set identity geometry so that the whole transform can be done in IJK space and then
  // the whole transform can be applied on the poly data to transform it to the world coordinate system
  vtkSmartPointer<vtkImageData> binaryLabelmapWithIdentityGeometry = vtkSmartPointer<vtkImageData>::New();
  binaryLabelmapWithIdentityGeometry->ShallowCopy(binaryLabelmap);
  binaryLabelmapWithIdentityGeometry->SetOrigin(0, 0, 0);
  binaryLabelmapWithIdentityGeometry->SetSpacing(1.0, 1.0, 1.0);

  // Get conversion parameters
  double decimationFactor = vtkVariant(this->ConversionParameters[GetDecimationFactorParameterName()].first).ToDouble();
  double smoothingFactor = vtkVariant(this->ConversionParameters[GetSmoothingFactorParameterName()].first).ToDouble();
  int computeSurfaceNormals = vtkVariant(this->ConversionParameters[GetComputeSurfaceNormalsParameterName()].first).ToInt();

  bool marchingCubesComputesSurfaceNormals = false;
#if VTK_MAJOR_VERSION >= 9 || (VTK_MAJOR_VERSION >= 8 && VTK_MINOR_VERSION >= 2)
  // Normals computation in vtkDiscreteFlyingEdges3D is faster than computing normals in a subsequent
  // vtkPolyDataNormals filter. However, if smoothing step is applied after vtkDiscreteFlyingEdges3D then
  // computing normals after smoothing provides smoother surfaces.
  marchingCubesComputesSurfaceNormals = (computeSurfaceNormals > 0) && (smoothingFactor <= 0);
  vtkSmartPointer<vtkDiscreteFlyingEdges3D> marchingCubes = vtkSmartPointer<vtkDiscreteFlyingEdges3D>::New();
#else
  vtkSmartPointer<vtkDiscreteMarchingCubes> marchingCubes = vtkSmartPointer<vtkDiscreteMarchingCubes>::New();
#endif
  marchingCubes->SetInputData(binaryLabelmapWithIdentityGeometry);
  marchingCubes->ComputeGradientsOff();
  marchingCubes->SetComputeNormals(marchingCubesComputesSurfaceNormals);
  marchingCubes->ComputeScalarsOn();

  int valueIndex = 0;
  for (std::vector<std::string>::iterator segmentIDIt = mergedSegmentIDs.begin(); segmentIDIt != mergedSegmentIDs.end(); ++segmentIDIt)
    {
    vtkSegment* currentSegment = segmentation->GetSegment(*segmentIDIt);
    double labelmapFillValue = currentSegment->GetLabelmapValue();
    marchingCubes->SetValue(valueIndex, labelmapFillValue);
    ++valueIndex;
    }

  vtkSmartPointer<vtkPolyData> convertedSegment = vtkSmartPointer<vtkPolyData>::New();

  // Run marching cubes
  marchingCubes->Update();
  vtkSmartPointer<vtkPolyData> processingResult = marchingCubes->GetOutput();
  if (processingResult->GetNumberOfPolys() == 0)
    {
    vtkDebugMacro("Convert: No polygons can be created, probably all voxels are empty");
    convertedSegment = nullptr;
    }

  for (std::vector<std::string>::iterator segmentIDIt = mergedSegmentIDs.begin(); segmentIDIt != mergedSegmentIDs.end(); ++segmentIDIt)
    {
    this->ConvertedSegments[*segmentIDIt] = convertedSegment;
    }

  if (!convertedSegment)
    {
    return true;
    }

  // Decimate
  if (decimationFactor > 0.0)
    {
    vtkSmartPointer<vtkDecimatePro> decimator = vtkSmartPointer<vtkDecimatePro>::New();
    decimator->SetInputData(processingResult);
    decimator->SetFeatureAngle(60);
    decimator->SplittingOff();
    decimator->PreserveTopologyOn();
    decimator->SetMaximumError(1);
    decimator->SetTargetReduction(decimationFactor);
    decimator->Update();
    processingResult = decimator->GetOutput();
    }

  if (smoothingFactor > 0)
    {
    vtkSmartPointer<vtkWindowedSincPolyDataFilter> smoother = vtkSmartPointer<vtkWindowedSincPolyDataFilter>::New();
    smoother->SetInputData(processingResult);
    smoother->SetNumberOfIterations(20); // based on VTK documentation ("Ten or twenty iterations is all the is usually necessary")
    // This formula maps:
    // 0.0  -> 1.0   (almost no smoothing)
    // 0.25 -> 0.1   (average smoothing)
    // 0.5  -> 0.01  (more smoothing)
    // 1.0  -> 0.001 (very strong smoothing)
    double passBand = pow(10.0, -4.0 * smoothingFactor);
    smoother->SetPassBand(passBand);
    smoother->BoundarySmoothingOff();
    smoother->FeatureEdgeSmoothingOff();
    smoother->NonManifoldSmoothingOn();
    smoother->NormalizeCoordinatesOn();
    smoother->Update();
    processingResult = smoother->GetOutput();
    }

  // Transform the result surface from labelmap IJK to world coordinate system
  vtkSmartPointer<vtkTransform> labelmapGeometryTransform = vtkSmartPointer<vtkTransform>::New();
  vtkSmartPointer<vtkMatrix4x4> labelmapImageToWorldMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
  orientedBinaryLabelmap->GetImageToWorldMatrix(labelmapImageToWorldMatrix);
  labelmapGeometryTransform->SetMatrix(labelmapImageToWorldMatrix);

  vtkSmartPointer<vtkTransformPolyDataFilter> transformPolyDataFilter = vtkSmartPointer<vtkTransformPolyDataFilter>::New();
  transformPolyDataFilter->SetInputData(processingResult);
  transformPolyDataFilter->SetTransform(labelmapGeometryTransform);

  // Determine if reference volume is in a left-handed coordinate system. If that is case, and normals are
  // calculated in the marching cubes step, then flipping the normals is needed
  bool flippedNormals = false;
  if (marchingCubesComputesSurfaceNormals)
    {
    vtkNew<vtkMatrix3x3> directionsMatrix;
    for (int i = 0; i < 3; ++i)
      {
      for (int j = 0; j < 3; ++j)
        {
        directionsMatrix->SetElement(i, j, labelmapImageToWorldMatrix->GetElement(i, j));
        }
      }
    if (directionsMatrix->Determinant() < 0.0)
      {
      flippedNormals = true;
      }
    }

  if (computeSurfaceNormals > 0 && !marchingCubesComputesSurfaceNormals)
    {
    vtkSmartPointer<vtkPolyDataNormals> polyDataNormals = vtkSmartPointer<vtkPolyDataNormals>::New();
    polyDataNormals->SetInputConnection(transformPolyDataFilter->GetOutputPort());
    polyDataNormals->ConsistencyOn(); // discrete marching cubes may generate inconsistent surface
    // We almost always perform smoothing, so splitting would not be able to preserve any sharp features
    // (and sharp edges would look like artifacts in the smooth surface).
    polyDataNormals->SplittingOff();
    polyDataNormals->Update();
    convertedSegment->ShallowCopy(polyDataNormals->GetOutput());
    }
  else if (computeSurfaceNormals > 0 && flippedNormals)
    {
    vtkNew<vtkReverseSense> flipNormals;
    flipNormals->SetInputConnection(transformPolyDataFilter->GetOutputPort());
    flipNormals->ReverseCellsOff();
    flipNormals->ReverseNormalsOn();
    flipNormals->Update();
    convertedSegment->ShallowCopy(flipNormals->GetOutput());
    }
  else
    {
    transformPolyDataFilter->Update();
    convertedSegment->ShallowCopy(transformPolyDataFilter->GetOutput());
    }

  return true;
}

//----------------------------------------------------------------------------
bool vtkBinaryLabelmapToClosedSurfaceConversionRule::Convert(vtkDataObject* sourceRepresentation, vtkDataObject* targetRepresentation)
{
  vtkPolyData* closedSurfacePolyData = vtkPolyData::SafeDownCast(targetRepresentation);
  if (!closedSurfacePolyData)
    {
    vtkErrorMacro("Convert: Target representation is not poly data");
    return false;
    }

  // TODO: How to know what value to threshold from the polydata
  vtkNew<vtkThreshold> threshold;
  threshold->SetInputData(this->ConvertedSegments[this->CurrentSegment]);
  threshold->ThresholdBetween(this->CurrentLabelValue, this->CurrentLabelValue);

  vtkNew<vtkGeometryFilter> geometryFilter;
  geometryFilter->SetInputConnection(threshold->GetOutputPort());
  geometryFilter->Update();
  closedSurfacePolyData->ShallowCopy(geometryFilter->GetOutput());

  return true;
}

//----------------------------------------------------------------------------
template<class ImageScalarType>
void IsLabelmapPaddingNecessaryGeneric(vtkImageData* binaryLabelmap, bool &paddingNecessary)
{
  if (!binaryLabelmap)
    {
    paddingNecessary = false;
    return;
    }

  // Check if there are non-zero voxels in the labelmap
  int extent[6] = {0,-1,0,-1,0,-1};
  binaryLabelmap->GetExtent(extent);
  int dimensions[3] = {0, 0, 0};
  binaryLabelmap->GetDimensions(dimensions);

  ImageScalarType* imagePtr = (ImageScalarType*)binaryLabelmap->GetScalarPointerForExtent(extent);

  for (int i=0; i<dimensions[0]; ++i)
    {
    for (int j=0; j<dimensions[1]; ++j)
      {
      for (int k=0; k<dimensions[2]; ++k)
        {
        if (i!=0 && i!=dimensions[0]-1 && j!=0 && j!=dimensions[1]-1 && k!=0 && k!=dimensions[2]-1)
          {
          // Skip non-border voxels
          continue;
          }
        int voxelValue = 0;
        voxelValue = (*(imagePtr + i + j*dimensions[0] + k*dimensions[0]*dimensions[1]));

        if (voxelValue != 0)
          {
          paddingNecessary = true;
          return;
          }
        }
      }
    }

  paddingNecessary = false;
  return;
}

//----------------------------------------------------------------------------
bool vtkBinaryLabelmapToClosedSurfaceConversionRule::IsLabelmapPaddingNecessary(vtkImageData* binaryLabelmap)
{
  if (!binaryLabelmap)
    {
    return false;
    }

  bool paddingNecessary = false;

  switch (binaryLabelmap->GetScalarType())
    {
    vtkTemplateMacro(IsLabelmapPaddingNecessaryGeneric<VTK_TT>( binaryLabelmap, paddingNecessary ));
    default:
      vtkErrorWithObjectMacro(binaryLabelmap, "IsLabelmapPaddingNecessary: Unknown image scalar type!");
      return false;
    }

  return paddingNecessary;
}
