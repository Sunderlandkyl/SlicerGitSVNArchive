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
  and was supported through the Applied Cancer Research Unit program of Cancer Care
  Ontario with funds provided by the Ontario Ministry of Health and Long-Term Care

==============================================================================*/

#include "vtkFractionalLogicalOperations.h"

// VTK inlcludes
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkFieldData.h>
#include <vtkDoubleArray.h>
#include <vtkIntArray.h>
#include <vtkStringArray.h>

// Segmentation includes
#include <vtkOrientedImageData.h>
#include <vtkSegmentationConverter.h>
#include <vtkSegmentation.h>
#include <vtkOrientedImageDataResample.h>

#include <vtkNRRDWriter.h>

vtkStandardNewMacro(vtkFractionalLogicalOperations);

//----------------------------------------------------------------------------
vtkFractionalLogicalOperations::vtkFractionalLogicalOperations()
{
}

//----------------------------------------------------------------------------
vtkFractionalLogicalOperations::~vtkFractionalLogicalOperations()
{
}

//----------------------------------------------------------------------------
void vtkFractionalLogicalOperations::Invert(vtkOrientedImageData* labelmap)
{
  if (!labelmap)
    {
    std::cerr << "Invert: invalid labelmap" << std::endl;
    return;
    }


  int dimensions[3] = {0,0,0};
  labelmap->GetDimensions(dimensions);

  int extent[6] = {0, -1, 0, -1, 0, -1};
  labelmap->GetExtent(extent);

  if (extent[0] > extent[1] ||
      extent[2] > extent[3] ||
      extent[4] > extent[5])
    {
    std::cerr << "Invert: invalid extent" << std::endl;
    //vtkErrorMacro("Invert: invalid extent"); // TODO: static error macros?
    return;
    }

  double scalarRange[2] = {0.0, 1.0};
  //vtkFractionalLogicalOperations::GetScalarRange(labelmap, scalarRange); //TODO
  vtkDoubleArray* scalarRangeArray = vtkDoubleArray::SafeDownCast(
  labelmap->GetFieldData()->GetAbstractArray(vtkSegmentationConverter::GetScalarRangeFieldName()));
  if (scalarRangeArray && scalarRangeArray->GetNumberOfValues() == 2)
    {
    scalarRange[0] = scalarRangeArray->GetValue(0);
    scalarRange[1] = scalarRangeArray->GetValue(1);
    }

  void* labelmapPointer = labelmap->GetScalarPointerForExtent(extent);

  switch(labelmap->GetScalarType())
    {
    vtkTemplateMacro(InvertGeneric((VTK_TT*)labelmapPointer, dimensions, scalarRange));

    default:
      std::cerr << "Invert: invalid scalar type" << std::endl;
      //vtkErrorMacro("Invert: invalid extent"); // TODO: static error macros? TODO actual message
    }

}

//----------------------------------------------------------------------------
template <class T>
void vtkFractionalLogicalOperations::InvertGeneric(T* labelmapPointer, int dimensions[3], double scalarRange[2])
{
  if (!labelmapPointer)
  {
    std::cout << "InvertGeneric: invalid labelmap pointer" << std::endl;
    return;
  }

  int numberOfVoxels = dimensions[0]*dimensions[1]*dimensions[2];

  double min = scalarRange[0];
  double max = scalarRange[1];

  for (int i = 0; i < numberOfVoxels; ++i)
    {
    double invertedValue = max - (*labelmapPointer) + min;
    *labelmapPointer = invertedValue;
    ++labelmapPointer;
    }

}

//----------------------------------------------------------------------------
void vtkFractionalLogicalOperations::Union(vtkOrientedImageData* output, vtkOrientedImageData* a, vtkOrientedImageData* b)
{
  if (!output || !a || !b)
  {
    std::cerr << "Union: invalid vtkOrientedImageData" << std::endl;
    return;
  }

  vtkOrientedImageDataResample::MergeImage(a, b, output, vtkOrientedImageDataResample::OPERATION_MAXIMUM);
}

//----------------------------------------------------------------------------
void vtkFractionalLogicalOperations::Union(vtkOrientedImageData* output, vtkSegmentation* segmentation, vtkStringArray* segmentIds)
{
  if (!output)
  {
    std::cerr << "Union: invalid vtkOrientedImageData" << std::endl;
    return;
  }

  if (!segmentation)
  {
    std::cerr << "Union: invalid vtkSegmentation" << std::endl;
    return;
  }

  if (!segmentIds)
  {
    std::cerr << "Union: invalid vtkStringArray" << std::endl;
    return;
  }

  for (int i = 0; i < segmentIds->GetNumberOfValues(); ++i)
    {
    vtkOrientedImageData* fractionalLabelmap = vtkOrientedImageData::SafeDownCast(
      segmentation->GetSegmentRepresentation(segmentIds->GetValue(i), vtkSegmentationConverter::GetSegmentationFractionalLabelmapRepresentationName()));
    if (fractionalLabelmap)
      {
      vtkOrientedImageDataResample::MergeImage(output, fractionalLabelmap, output, vtkOrientedImageDataResample::OPERATION_MAXIMUM);
      }
    }
}

//----------------------------------------------------------------------------
void vtkFractionalLogicalOperations::ClearFractionalParameters(vtkOrientedImageData* input)
{
  if (!input)
  {
    std::cerr << "ClearFractionalParameters: invalid vtkOrientedImageData" << std::endl;
    return;
  }

  input->GetFieldData()->RemoveArray(vtkSegmentationConverter::GetScalarRangeFieldName());
  input->GetFieldData()->RemoveArray(vtkSegmentationConverter::GetThresholdValueFieldName());
  input->GetFieldData()->RemoveArray(vtkSegmentationConverter::GetInterpolationTypeFieldName());
}

//----------------------------------------------------------------------------
void vtkFractionalLogicalOperations::SetDefaultFractionalParameters(vtkOrientedImageData* input)
{
  if (!input)
  {
    std::cerr << "SetFractionalParameters: invalid vtkOrientedImageData" << std::endl;
    return;
  }

  vtkFractionalLogicalOperations::ClearFractionalParameters(input);

  double defaultScalarRange[2] = {-108.0, 108.0};
  double defaultThreshold = 0.0;
  vtkIdType defaultInterpolationType = VTK_LINEAR_INTERPOLATION;

  // Specify the scalar range of values in the labelmap
  vtkSmartPointer<vtkDoubleArray> scalarRangeArray = vtkSmartPointer<vtkDoubleArray>::New();
  scalarRangeArray->SetName(vtkSegmentationConverter::GetScalarRangeFieldName());
  scalarRangeArray->InsertNextValue(defaultScalarRange[0]);
  scalarRangeArray->InsertNextValue(defaultScalarRange[1]);
  input->GetFieldData()->AddArray(scalarRangeArray);

  vtkSmartPointer<vtkDoubleArray> thresholdArray = vtkSmartPointer<vtkDoubleArray>::New();
  thresholdArray->SetName(vtkSegmentationConverter::GetThresholdValueFieldName());
  thresholdArray->InsertNextValue(defaultThreshold);
  input->GetFieldData()->AddArray(thresholdArray);

  vtkSmartPointer<vtkIntArray> interpolationTypeArray = vtkSmartPointer<vtkIntArray>::New();
  interpolationTypeArray->SetName(vtkSegmentationConverter::GetInterpolationTypeFieldName());
  interpolationTypeArray->InsertNextValue(defaultInterpolationType);
  input->GetFieldData()->AddArray(interpolationTypeArray);

}

//----------------------------------------------------------------------------
void vtkFractionalLogicalOperations::CopyFractionalParameters(vtkOrientedImageData* input, vtkOrientedImageData* originalLabelmap)
{
  if (!input || !originalLabelmap)
  {
    std::cerr << "CopyFractionalParameters: invalid vtkOrientedImageData" << std::endl;
    return;
  }

  vtkFractionalLogicalOperations::ClearFractionalParameters(input);
  //vtkFractionalLogicalOperations::SetDefaultFractionalParameters(input);

  vtkFieldData* inputFieldData = input->GetFieldData();
  vtkFieldData* originalFieldData = originalLabelmap->GetFieldData();

  /*vtkDoubleArray* scalarRangeArray = vtkDoubleArray::SafeDownCast(originalFieldData->GetAbstractArray(vtkSegmentationConverter::GetScalarRangeFieldName()));
  if (scalarRangeArray && scalarRangeArray->GetNumberOfValues() == 2)
    {
    input->GetFieldData()->RemoveArray(vtkSegmentationConverter::GetScalarRangeFieldName());

    }
    */

  inputFieldData->AddArray(originalFieldData->GetAbstractArray(vtkSegmentationConverter::GetScalarRangeFieldName()));
  inputFieldData->AddArray(originalFieldData->GetAbstractArray(vtkSegmentationConverter::GetThresholdValueFieldName()));
  inputFieldData->AddArray(originalFieldData->GetAbstractArray(vtkSegmentationConverter::GetInterpolationTypeFieldName()));
}

//----------------------------------------------------------------------------
void vtkFractionalLogicalOperations::CopyFractionalParameters(vtkOrientedImageData* input, vtkSegmentation* segmentation)
{
  if (!input)
    {
    std::cerr << "CopyFractionalParameters: invalid vtkOrientedImageData" << std::endl;
    return;
    }

  if (!segmentation)
    {
    std::cerr << "CopyFractionalParameters: invalid vtkSegmentation" << std::endl;
    return;
    }

  std::vector<std::string> segmentIds = std::vector<std::string>();
  segmentation->GetSegmentIDs(segmentIds);

  bool foundCompleteParamters = false;

  vtkFieldData* inputFieldData = input->GetFieldData();
  for (std::vector<std::string>::iterator segmentIDIt = segmentIds.begin(); segmentIDIt != segmentIds.end(); ++segmentIDIt)
    {
    vtkFractionalLogicalOperations::ClearFractionalParameters(input);

    vtkOrientedImageData* originalLabelmap = vtkOrientedImageData::SafeDownCast(segmentation->GetSegmentRepresentation(*segmentIDIt, vtkSegmentationConverter::GetSegmentationFractionalLabelmapRepresentationName()));
    vtkFieldData* originalFieldData = originalLabelmap->GetFieldData();

    vtkDoubleArray* scalarRangeArray = vtkDoubleArray::SafeDownCast(originalFieldData->GetAbstractArray(vtkSegmentationConverter::GetScalarRangeFieldName()));
    if (scalarRangeArray && scalarRangeArray->GetNumberOfValues() == 2)
      {
      inputFieldData->AddArray(scalarRangeArray);
      }
    else
      {
      continue;
      }

    vtkDoubleArray* thresholdArray = vtkDoubleArray::SafeDownCast(originalFieldData->GetAbstractArray(vtkSegmentationConverter::GetThresholdValueFieldName()));
    if (thresholdArray && thresholdArray->GetNumberOfValues() == 1)
      {
      inputFieldData->AddArray(thresholdArray);
      }
    else
      {
      continue;
      }

    vtkDoubleArray* interpolationTypeArray = vtkDoubleArray::SafeDownCast(originalFieldData->GetAbstractArray(vtkSegmentationConverter::GetInterpolationTypeFieldName()));
    if (interpolationTypeArray && interpolationTypeArray->GetNumberOfValues() == 1)
      {
      inputFieldData->AddArray(interpolationTypeArray);
      }
    else
      {
      continue;
      }

    foundCompleteParamters = true;
    }

  if (!foundCompleteParamters)
    {
    vtkFractionalLogicalOperations::SetDefaultFractionalParameters(input);
    }
}

/*//----------------------------------------------------------------------------
void vtkFractionalLogicalOperations::GetScalarRange(vtkOrientedImageData* input, double scalarRange[2])
{
  if (!input)
    {
    std::cerr << "GetScalarRange: invalid vtkOrientedImageData" << std::endl;
    }

  if (!scalarRange)
    {
    std::cerr << "GetScalarRange: invalid double array" << std::endl;
    }

  vtkDoubleArray* scalarRangeArray = vtkDoubleArray::SafeDownCast(
  input->GetFieldData()->GetAbstractArray(vtkSegmentationConverter::GetScalarRangeFieldName()));
  if (scalarRangeArray && scalarRangeArray->GetNumberOfValues() == 2)
    {
    scalarRange[0] = scalarRangeArray->GetValue(0);
    scalarRange[1] = scalarRangeArray->GetValue(1);
    }

}

//----------------------------------------------------------------------------
void vtkFractionalLogicalOperations::GetThreshold(vtkOrientedImageData* input, double threshold[1])
{
  if (!input)
    {
    std::cerr << "GetScalarRange: invalid vtkOrientedImageData" << std::endl;
    }

  if (!scalarRange)
    {
    std::cerr << "GetScalarRange: invalid double array" << std::endl;
    }

  vtkDoubleArray* scalarRangeArray = vtkDoubleArray::SafeDownCast(
  input->GetFieldData()->GetAbstractArray(vtkSegmentationConverter::GetScalarRangeFieldName()));
  if (scalarRangeArray && scalarRangeArray->GetNumberOfValues() == 2)
    {
    scalarRange[0] = scalarRangeArray->GetValue(0);
    scalarRange[1] = scalarRangeArray->GetValue(1);
    }

}*/

//----------------------------------------------------------------------------
void vtkFractionalLogicalOperations::Write(vtkImageData* image, const char* name)
{
  vtkNew<vtkNRRDWriter> writer;
  writer->SetInputData(image);
  writer->SetFileName(name);
  writer->Update();
}